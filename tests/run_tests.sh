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

cleanup()
{
    if [ -n "$SERVER_PID" ]; then
        kill "$SERVER_PID" >/dev/null 2>&1
        wait "$SERVER_PID" 2>/dev/null
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

# --- build ---

make -s -C "$ROOT_DIR" re >"$WORK_DIR/build.log" 2>&1
if [ $? -ne 0 ]; then
    echo "build failed:"
    cat "$WORK_DIR/build.log"
    rm -rf "$WORK_DIR"
    exit 1
fi

# --- fixtures ---

mkdir -p "$WORK_DIR/WWW/uploads" "$WORK_DIR/WWW/readonly"
echo "test index" > "$WORK_DIR/WWW/index.html"
echo "readonly content" > "$WORK_DIR/WWW/readonly/index.html"
python3 -c "print('X' * 3000)" > "$WORK_DIR/upload_source.txt"

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

assert_status "path traversal outside root -> 403" 403 \
    --path-as-is "$BASE_URL/../../../../../../../../etc/passwd"

assert_status "GET /readonly/ -> 200" 200 "$BASE_URL/readonly/"
assert_status "DELETE on GET-only location -> 405" 405 -X DELETE "$BASE_URL/readonly/index.html"

assert_status "return directive -> 301" 301 "$BASE_URL/old"
assert_header_contains "301 Location header" "Location" "example.com/new" "$BASE_URL/old"

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

echo ""
echo "$PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ]
