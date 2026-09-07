#!/bin/sh
# Small end-to-end test suite: builds webserv, runs it against a real
# throwaway config, and drives it with curl. Not a unit test suite -
# this project has no test hooks into individual classes, so it tests
# the one thing that actually matters: observable HTTP behavior.
#
# Usage: tests/run_tests.sh
# Exit code: 0 if all checks pass, 1 otherwise.

set -u

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
ROOT_DIR=$(cd "$SCRIPT_DIR/.." && pwd)
PORT=8765
BASE_URL="http://127.0.0.1:$PORT"
WORK_DIR=$(mktemp -d)
SERVER_PID=""
PASS=0
FAIL=0

# The server writes access.log next to its own binary, i.e. inside
# ROOT_DIR - not inside WORK_DIR. Back up/restore so a test run never
# leaves the repo's working tree dirty.
ACCESS_LOG="$ROOT_DIR/access.log"
ACCESS_LOG_BACKUP=""
if [ -f "$ACCESS_LOG" ]; then
    ACCESS_LOG_BACKUP=$(mktemp)
    cp "$ACCESS_LOG" "$ACCESS_LOG_BACKUP"
fi

cleanup()
{
    if [ -n "$SERVER_PID" ]; then
        kill "$SERVER_PID" >/dev/null 2>&1
        wait "$SERVER_PID" 2>/dev/null
    fi
    if [ -n "$ACCESS_LOG_BACKUP" ]; then
        mv "$ACCESS_LOG_BACKUP" "$ACCESS_LOG"
    else
        rm -f "$ACCESS_LOG"
    fi
    rm -rf "$WORK_DIR"
}
trap cleanup EXIT INT TERM

fail()
{
    echo "FAIL: $1"
    FAIL=$((FAIL + 1))
}

pass()
{
    echo "PASS: $1"
    PASS=$((PASS + 1))
}

assert_status()
{
    name=$1
    expected=$2
    shift 2
    actual=$(curl -s -o /dev/null -w '%{http_code}' "$@")
    if [ "$actual" = "$expected" ]; then
        pass "$name (got $actual)"
    else
        fail "$name (expected $expected, got $actual)"
    fi
}

assert_header_contains()
{
    name=$1
    header=$2
    needle=$3
    shift 3
    actual=$(curl -s -D - -o /dev/null "$@" | grep -i "^$header:" | tr -d '\r')
    case "$actual" in
        *"$needle"*) pass "$name" ;;
        *) fail "$name (header '$header' was '$actual', expected to contain '$needle')" ;;
    esac
}

assert_empty_body()
{
    name=$1
    shift
    size=$(curl -s "$@" | wc -c)
    if [ "$size" -eq 0 ]; then
        pass "$name (0 body bytes)"
    else
        fail "$name (expected 0 body bytes, got $size)"
    fi
}

# --- build ---

make -s -C "$ROOT_DIR" re >"$WORK_DIR/build.log" 2>&1
if [ $? -ne 0 ]; then
    echo "build failed:"
    cat "$WORK_DIR/build.log"
    rm -rf "$WORK_DIR"
    exit 1
fi

# --- fixtures ---

mkdir -p "$WORK_DIR/WWW/uploads" "$WORK_DIR/WWW/readonly" "$WORK_DIR/WWW/cgi_out"
echo "test index" > "$WORK_DIR/WWW/index.html"
echo "readonly content" > "$WORK_DIR/WWW/readonly/index.html"
python3 -c "print('X' * 3000)" > "$WORK_DIR/upload_source.txt"

echo "this file is never actually run - cgi_path below points at the fake interpreter" \
    > "$WORK_DIR/WWW/hello.py"
chmod +x "$WORK_DIR/WWW/hello.py"
cat > "$WORK_DIR/fake_interp.sh" <<'PYEOF'
#!/bin/sh
printf 'Content-Type: text/plain\r\n\r\nCUSTOM INTERPRETER WAS USED'
PYEOF
chmod +x "$WORK_DIR/fake_interp.sh"

cat > "$WORK_DIR/test.conf" <<EOF
server {
    host 127.0.0.1
    listen $PORT
    root $WORK_DIR/WWW
    index index.html
    client_max_body_size 1000000
    autoindex off

    location /readonly {
        root $WORK_DIR/WWW/readonly
        allow GET
        index index.html
    }

    location /old {
        return http://example.com/new
    }

    location / {
        root $WORK_DIR/WWW
        index index.html
        allow GET POST DELETE
        upload on
        upload_path $WORK_DIR/WWW/uploads
        cgi on
        cgi_upload_path $WORK_DIR/WWW/cgi_out
        cgi_path py $WORK_DIR/fake_interp.sh
    }
}
EOF

# --- start server ---

"$ROOT_DIR/webserv" "$WORK_DIR/test.conf" >"$WORK_DIR/server.log" 2>&1 &
SERVER_PID=$!

i=0
until curl -s -o /dev/null "$BASE_URL/" 2>/dev/null; do
    i=$((i + 1))
    if [ "$i" -gt 50 ]; then
        echo "server never came up:"
        cat "$WORK_DIR/server.log"
        exit 1
    fi
    sleep 0.1
done

# --- tests ---

assert_status "GET / -> 200" 200 "$BASE_URL/"
assert_status "GET /missing.html -> 404" 404 "$BASE_URL/missing.html"

assert_status "HEAD / -> 200" 200 -X HEAD "$BASE_URL/"
assert_empty_body "HEAD / has no body" -X HEAD "$BASE_URL/"
assert_status "HEAD /missing.html -> 404" 404 -X HEAD "$BASE_URL/missing.html"
assert_empty_body "HEAD /missing.html has no body" -X HEAD "$BASE_URL/missing.html"
assert_status "HEAD on GET-only location -> 200, not 405" 200 -X HEAD "$BASE_URL/readonly/index.html"

cgi_body=$(curl -s "$BASE_URL/hello.py")
if [ "$cgi_body" = "CUSTOM INTERPRETER WAS USED" ]; then
    pass "cgi_path directive overrides the default interpreter"
else
    fail "cgi_path directive not honored (got: '$cgi_body')"
fi

if [ -f "$ACCESS_LOG" ] \
    && grep -Eq '^\[.*\] 127\.0\.0\.1 GET / 200 [0-9]+ [0-9]+ms$' "$ACCESS_LOG" \
    && grep -Eq '^\[.*\] 127\.0\.0\.1 HEAD / 200 0 [0-9]+ms$' "$ACCESS_LOG"; then
    pass "access.log records requests with correct format (HEAD shows 0 bytes)"
else
    fail "access.log missing expected entries: $(cat "$ACCESS_LOG" 2>/dev/null || echo 'file not found')"
fi

assert_status "path traversal outside root -> 403" 403 \
    --path-as-is "$BASE_URL/../../../../../../../../etc/passwd"

assert_status "GET /readonly/ -> 200" 200 "$BASE_URL/readonly/"
assert_status "DELETE on GET-only location -> 405" 405 -X DELETE "$BASE_URL/readonly/index.html"

assert_status "return directive -> 301" 301 "$BASE_URL/old"
assert_header_contains "301 Location header" "Location" "example.com/new" "$BASE_URL/old"

# --- keep-alive ---

ka_trace=$(curl -s -v -o /dev/null "$BASE_URL/" "$BASE_URL/" 2>&1)
if echo "$ka_trace" | grep -qi "Re-using existing connection"; then
    pass "two GETs reuse the same connection (keep-alive)"
else
    fail "two GETs did not reuse the connection"
fi
if echo "$ka_trace" | grep -qi "^< connection: keep-alive"; then
    pass "keep-alive GET response sends connection: keep-alive"
else
    fail "keep-alive GET response missing connection: keep-alive header"
fi

close_trace=$(curl -s -v -o /dev/null -H "Connection: close" "$BASE_URL/" "$BASE_URL/" 2>&1)
if echo "$close_trace" | grep -qi "Re-using existing connection"; then
    fail "Connection: close was not honored - connection got reused anyway"
else
    pass "Connection: close is honored (no reuse)"
fi

post_trace=$(curl -s -v -o /dev/null -d "x=1" "$BASE_URL/" "$BASE_URL/" 2>&1)
if echo "$post_trace" | grep -qi "Re-using existing connection"; then
    fail "POST connection was reused (should always close, to avoid body-drain hazards on error paths)"
else
    pass "POST always closes the connection"
fi

assert_status "multipart upload -> 201" 201 -F "file=@$WORK_DIR/upload_source.txt" "$BASE_URL/"
uploaded=$(ls -t "$WORK_DIR/WWW/uploads"/*.txt 2>/dev/null | head -1)
if [ -n "$uploaded" ] && diff -q "$WORK_DIR/upload_source.txt" "$uploaded" >/dev/null 2>&1; then
    pass "multipart upload content matches byte-for-byte"
else
    fail "multipart upload content mismatch (uploaded: ${uploaded:-none})"
fi

assert_status "chunked upload -> 201" 201 \
    -H "Transfer-Encoding: chunked" --data-binary "@$WORK_DIR/upload_source.txt" "$BASE_URL/"
chunked=$(ls -t "$WORK_DIR/WWW/uploads"/*.bin 2>/dev/null | head -1)
if [ -n "$chunked" ] && diff -q "$WORK_DIR/upload_source.txt" "$chunked" >/dev/null 2>&1; then
    pass "chunked upload content matches byte-for-byte"
else
    fail "chunked upload content mismatch (uploaded: ${chunked:-none})"
fi

if [ -n "$uploaded" ]; then
    rel=${uploaded#"$WORK_DIR/WWW"}
    assert_status "DELETE uploaded file -> 204" 204 -X DELETE "$BASE_URL$rel"
    assert_status "GET deleted file -> 404" 404 "$BASE_URL$rel"
fi


# --- graceful shutdown (own server instance: this test kills it) ---

SHUTDOWN_PORT=8766
cat > "$WORK_DIR/slow.py" <<'PYEOF'
#!/usr/bin/env python3
import time
time.sleep(1)
print("Content-Type: text/plain\r\n\r\nslow cgi done")
PYEOF
chmod +x "$WORK_DIR/slow.py"
cp "$WORK_DIR/slow.py" "$WORK_DIR/WWW/slow.py"
mkdir -p "$WORK_DIR/cgi"

cat > "$WORK_DIR/shutdown.conf" <<EOF
server {
    host 127.0.0.1
    listen $SHUTDOWN_PORT
    root $WORK_DIR/WWW
    index index.html
    location / {
        root $WORK_DIR/WWW
        allow GET
        cgi on
        cgi_upload_path $WORK_DIR/cgi
    }
}
EOF

"$ROOT_DIR/webserv" "$WORK_DIR/shutdown.conf" >"$WORK_DIR/shutdown_server.log" 2>&1 &
SHUTDOWN_PID=$!
i=0
until curl -s -o /dev/null "http://127.0.0.1:$SHUTDOWN_PORT/" 2>/dev/null; do
    i=$((i + 1))
    if [ "$i" -gt 50 ]; then
        fail "graceful shutdown server never came up"
        break
    fi
    sleep 0.1
done

curl -s -o "$WORK_DIR/shutdown_body.txt" -w '%{http_code}' \
    "http://127.0.0.1:$SHUTDOWN_PORT/slow.py" >"$WORK_DIR/shutdown_code.txt" &
sleep 0.2
kill -TERM "$SHUTDOWN_PID" 2>/dev/null
wait "$SHUTDOWN_PID" 2>/dev/null

code=$(cat "$WORK_DIR/shutdown_code.txt" 2>/dev/null)
body=$(cat "$WORK_DIR/shutdown_body.txt" 2>/dev/null)
if [ "$code" = "200" ] && [ "$body" = "slow cgi done" ]; then
    pass "graceful shutdown waits for in-flight CGI request to finish"
else
    fail "graceful shutdown waits for in-flight CGI request (code='$code' body='$body')"
fi
if kill -0 "$SHUTDOWN_PID" 2>/dev/null; then
    fail "server process still running after graceful shutdown"
    kill -9 "$SHUTDOWN_PID" 2>/dev/null
else
    pass "server process exited after graceful shutdown"
fi

echo ""
echo "$PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ]
