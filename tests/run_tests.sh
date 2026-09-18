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

# Incremental on purpose - not a clean rebuild every run. cmake/make
# already know what's stale; wiping build/ here forced a full
# FetchContent re-clone of spdlog+doctest and a from-scratch compile
# on every single invocation of this script, which combined with the
# unbounded -j below (no number = as many parallel compiles as make
# feels like, not even capped to core count) was enough to push a
# 15GB machine into swap. -j capped at 4: fast enough for a local dev
# loop without trying to run dozens of cc1plus instances at once.
cmake -S "$ROOT_DIR" -B "$ROOT_DIR/build" >"$WORK_DIR/build.log" 2>&1 \
    && cmake --build "$ROOT_DIR/build" -j4 >>"$WORK_DIR/build.log" 2>&1
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

# A sibling directory sharing WWW's name as a raw string prefix - not
# nested under it at all - for the sibling-directory traversal
# regression test below.
mkdir -p "$WORK_DIR/WWW-secret"
echo "TOP SECRET - outside the server root entirely" > "$WORK_DIR/WWW-secret/secret.txt"

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

# A real, previously-exploitable gap: validatePath() used to compare
# the canonicalized request path against root with a raw string
# prefix match, which treats any sibling whose name happens to start
# with root's own name - WWW-secret next to WWW - as being "inside"
# root, since the string "WWW-secret" starts with the string "WWW".
# Confirmed live before the fix: this returned 200 with the sibling
# file's real content instead of 403.
assert_status "sibling directory sharing root's name as a string prefix is still outside root -> 403" 403 \
    --path-as-is "$BASE_URL/../WWW-secret/secret.txt"

assert_status "GET /readonly/ -> 200" 200 "$BASE_URL/readonly/"
assert_status "DELETE on GET-only location -> 405" 405 -X DELETE "$BASE_URL/readonly/index.html"

# Real gap: findLocation() used to match on a raw string prefix, so a
# target like /readonlyExtra (not actually under /readonly at all)
# would incorrectly match the /readonly location instead of falling
# through to / - observably different here, since /readonly only
# allows GET (405 on DELETE) while / allows DELETE too. Confirms the
# fix respects a real path boundary, not just a shared prefix.
assert_status "DELETE /readonlyExtra falls through to / (path boundary), not /readonly's 405" 404 \
    -X DELETE "$BASE_URL/readonlyExtra"

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
    pass "successful Content-Length POST reuses the connection (keep-alive)"
else
    fail "successful POST did not reuse the connection"
fi

# A chunked body's length isn't known up front, but the chunk framing
# itself marks its own end (the zero-size last-chunk, then the
# trailer part's terminating CRLF - see Chunks::parseTrailer()) just
# as unambiguously as a declared Content-Length does, so a
# successfully-parsed chunked POST is keep-alive eligible too.
chunked_ka_trace=$(curl -s -v -o /dev/null -H "Transfer-Encoding: chunked" --data-binary "@$WORK_DIR/upload_source.txt" "$BASE_URL/" \
    --next -H "Transfer-Encoding: chunked" --data-binary "@$WORK_DIR/upload_source.txt" "$BASE_URL/" 2>&1)
if echo "$chunked_ka_trace" | grep -qi "Re-using existing connection"; then
    pass "successful chunked POST reuses the connection (keep-alive)"
else
    fail "successful chunked POST did not reuse the connection"
fi

# A POST rejected early (413, body too large) still has to have its
# whole declared Content-Length drained off the wire before the
# connection can be reused - otherwise the leftover bytes get parsed
# as the start of the next request. Drive this over a raw socket so
# the drain and the next request share one real TCP connection,
# which curl's --next can't guarantee.
drain_result=$(python3 - "$PORT" <<'PYEOF'
import socket, sys, time

def recv_one_response(s, timeout=15):
    s.settimeout(timeout)
    buf = b""
    while b"\r\n\r\n" not in buf:
        chunk = s.recv(4096)
        if not chunk:
            return buf
        buf += chunk
    head, rest = buf.split(b"\r\n\r\n", 1)
    if b"chunked" not in head.lower():
        return buf
    body = rest
    while b"\r\n0\r\n\r\n" not in body:
        chunk = s.recv(4096)
        if not chunk:
            break
        body += chunk
    return head + b"\r\n\r\n" + body

port = int(sys.argv[1])
s = socket.create_connection(("127.0.0.1", port), timeout=5)
total_len = 1100000
head_chunk = b"x" * 50000
req = (b"POST / HTTP/1.1\r\nHost: x\r\nContent-Type: application/octet-stream\r\n"
       b"Content-Length: " + str(total_len).encode() + b"\r\n\r\n") + head_chunk
s.sendall(req)
resp1 = recv_one_response(s)
s.sendall(b"y" * (total_len - len(head_chunk)))
time.sleep(1)
s.sendall(b"GET / HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n")
resp2 = recv_one_response(s)
s.close()
print(resp1.split(b"\r\n")[0].decode(errors="replace"))
print(resp2.split(b"\r\n")[0].decode(errors="replace"))
PYEOF
)
drain_status1=$(echo "$drain_result" | sed -n '1p')
drain_status2=$(echo "$drain_result" | sed -n '2p')
if [ "$drain_status1" = "HTTP/1.1 413 Content Too Large" ] && [ "$drain_status2" = "HTTP/1.1 200 OK" ]; then
    pass "oversized POST body is drained so the reused connection's next request parses cleanly"
else
    fail "drain-then-reuse produced status1='$drain_status1' status2='$drain_status2'"
fi

# Same idea, chunked: an oversized chunk still has to be drained -
# correctly walking chunk framing, not just counting bytes, since
# there's no declared total length to count against - before the
# connection can be reused.
chunked_drain_result=$(python3 - "$PORT" <<'PYEOF'
import socket, sys, time

def recv_one_response(s, timeout=15):
    s.settimeout(timeout)
    buf = b""
    while b"\r\n\r\n" not in buf:
        chunk = s.recv(4096)
        if not chunk:
            return buf
        buf += chunk
    head, rest = buf.split(b"\r\n\r\n", 1)
    if b"chunked" not in head.lower():
        return buf
    body = rest
    while b"\r\n0\r\n\r\n" not in body:
        chunk = s.recv(4096)
        if not chunk:
            break
        body += chunk
    return head + b"\r\n\r\n" + body

port = int(sys.argv[1])
s = socket.create_connection(("127.0.0.1", port), timeout=5)

# The client_max_body_size check only runs once there's actually some
# content buffered for the oversized chunk (Chunks::writeContent()
# bails out immediately on an empty buffer, before reaching the size
# check) - so the chunk's size line alone isn't enough to trigger it
# in the same read the way a declared Content-Length is; head_chunk
# has to carry real content bytes along with it, same reason the
# Content-Length version above sends real bytes up front too.
second_chunk_len = 1100000
head_chunk = b"b" * 50000
req_head = (b"POST / HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\n"
    + format(second_chunk_len, "x").encode() + b"\r\n" + head_chunk)
s.sendall(req_head)
resp1 = recv_one_response(s)

# The server already rejected this with a 413 - but it still needs
# the rest of what the client declared to correctly find where this
# chunked body actually ends on the wire, same as the Content-Length
# case above.
s.sendall(b"b" * (second_chunk_len - len(head_chunk)) + b"\r\n0\r\n\r\n")
time.sleep(1)
s.sendall(b"GET / HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n")
resp2 = recv_one_response(s)
s.close()
print(resp1.split(b"\r\n")[0].decode(errors="replace"))
print(resp2.split(b"\r\n")[0].decode(errors="replace"))
PYEOF
)
chunked_drain_status1=$(echo "$chunked_drain_result" | sed -n '1p')
chunked_drain_status2=$(echo "$chunked_drain_result" | sed -n '2p')
if [ "$chunked_drain_status1" = "HTTP/1.1 413 Content Too Large" ] && [ "$chunked_drain_status2" = "HTTP/1.1 200 OK" ]; then
    pass "oversized chunked POST body is drained so the reused connection's next request parses cleanly"
else
    fail "chunked drain-then-reuse produced status1='$chunked_drain_status1' status2='$chunked_drain_status2'"
fi

# Regression case: a request declaring both Content-Length and
# Transfer-Encoding: chunked is correctly rejected (400, RFC 9112
# 6.1), but chunked-drain setup used to only ever consume *new*
# socket reads - the chunked body bytes that arrived in the very same
# read as the headers (the common case for a small request) were
# never fed to the drain, so the connection just sat there until the
# unrelated idle-timeout scan force-closed it ~10s later. Assert the
# connection reuses in well under that, not that it merely eventually
# recovers.
cl_te_result=$(python3 - "$PORT" <<'PYEOF'
import socket, sys, time

def recv_one_response(s, timeout=15):
    s.settimeout(timeout)
    buf = b""
    while b"\r\n\r\n" not in buf:
        chunk = s.recv(4096)
        if not chunk:
            return buf
        buf += chunk
    head, rest = buf.split(b"\r\n\r\n", 1)
    if b"chunked" not in head.lower():
        return buf
    body = rest
    while b"\r\n0\r\n\r\n" not in body:
        chunk = s.recv(4096)
        if not chunk:
            break
        body += chunk
    return head + b"\r\n\r\n" + body

port = int(sys.argv[1])
s = socket.create_connection(("127.0.0.1", port), timeout=5)
chunked_body = b"1a\r\n" + b"A" * 26 + b"\r\n0\r\n\r\n"
req = (b"POST /uploads HTTP/1.1\r\nHost: x\r\nContent-Length: 5\r\n"
    b"Transfer-Encoding: chunked\r\nConnection: keep-alive\r\n\r\n") + chunked_body
t0 = time.time()
s.sendall(req)
resp1 = recv_one_response(s)
s.sendall(b"GET / HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n")
resp2 = recv_one_response(s)
elapsed = time.time() - t0
s.close()
print(resp1.split(b"\r\n")[0].decode(errors="replace"))
print(resp2.split(b"\r\n")[0].decode(errors="replace"))
print(f"{elapsed:.2f}")
PYEOF
)
cl_te_status1=$(echo "$cl_te_result" | sed -n '1p')
cl_te_status2=$(echo "$cl_te_result" | sed -n '2p')
cl_te_elapsed=$(echo "$cl_te_result" | sed -n '3p')
cl_te_fast=$(awk -v e="$cl_te_elapsed" 'BEGIN { print (e < 5) ? 1 : 0 }')
if [ "$cl_te_status1" = "HTTP/1.1 400 Bad Request" ] && [ "$cl_te_status2" = "HTTP/1.1 200 OK" ] && [ "$cl_te_fast" = "1" ]; then
    pass "Content-Length + Transfer-Encoding conflict reuses the connection immediately, not after a timeout (${cl_te_elapsed}s)"
else
    fail "CL/TE conflict drain produced status1='$cl_te_status1' status2='$cl_te_status2' elapsed=${cl_te_elapsed}s"
fi

assert_status "multipart upload -> 201" 201 -F "file=@$WORK_DIR/upload_source.txt" "$BASE_URL/"
uploaded=$(ls -t "$WORK_DIR/WWW/uploads"/*.txt 2>/dev/null | head -1)
if [ -n "$uploaded" ] && diff -q "$WORK_DIR/upload_source.txt" "$uploaded" >/dev/null 2>&1; then
    pass "multipart upload content matches byte-for-byte"
else
    fail "multipart upload content mismatch (uploaded: ${uploaded:-none})"
fi

# Regression case: a multipart upload abandoned mid-transfer (client
# disconnects after Boundaries::createFile() already opened the
# output file, before the closing boundary ever arrives) used to leak
# the file descriptor forever - Boundaries had no destructor, and
# nothing else in the abrupt-disconnect path closed it. Found via
# fuzzing Boundaries::parseBoundary() (tests/fuzz/fuzz_boundaries.cpp)
# turning up an ASan leak report, then reproduced live against the
# real server via /proc/<pid>/fd (5 abandoned uploads -> 5 leaked
# fds before the fix).
fd_count() { ls "/proc/$SERVER_PID/fd" 2>/dev/null | wc -l; }
fds_before_abandon=$(fd_count)
python3 - "$PORT" <<'PYEOF'
import socket, sys, time
port = int(sys.argv[1])
boundary = "----WebKitFormBoundaryLEAK"
body = (
    f"--{boundary}\r\n"
    f"Content-Disposition: form-data; name=\"file\"; filename=\"a.txt\"\r\n"
    f"Content-Type: text/plain\r\n\r\n"
    f"partial file content, never terminated"
).encode()
for _ in range(5):
    s = socket.create_connection(("127.0.0.1", port), timeout=5)
    req = (
        f"POST /uploads HTTP/1.1\r\nHost: 127.0.0.1\r\n"
        f"Content-Type: multipart/form-data; boundary={boundary}\r\n"
        f"Content-Length: {len(body) + 100}\r\nConnection: keep-alive\r\n\r\n"
    ).encode() + body
    s.sendall(req)
    time.sleep(0.2)
    s.close()
PYEOF
sleep 1
fds_after_abandon=$(fd_count)
if [ "$fds_after_abandon" -le "$fds_before_abandon" ]; then
    pass "abandoned multipart uploads don't leak file descriptors ($fds_before_abandon -> $fds_after_abandon)"
else
    fail "file descriptor leak: $fds_before_abandon -> $fds_after_abandon after 5 abandoned uploads"
fi
# Leftover partial files from the abandoned uploads above are harmless
# (WORK_DIR is wiped by the cleanup trap on exit) - not removed here
# on purpose, since a broad glob would also catch the real upload
# file "multipart upload -> 201" already left behind, which the
# DELETE test below still needs.

assert_status "chunked upload -> 201" 201 \
    -H "Transfer-Encoding: chunked" --data-binary "@$WORK_DIR/upload_source.txt" "$BASE_URL/"
chunked=$(ls -t "$WORK_DIR/WWW/uploads"/*.bin 2>/dev/null | head -1)
if [ -n "$chunked" ] && diff -q "$WORK_DIR/upload_source.txt" "$chunked" >/dev/null 2>&1; then
    pass "chunked upload content matches byte-for-byte"
else
    fail "chunked upload content mismatch (uploaded: ${chunked:-none})"
fi

# Regression case: the automatic "directory without a trailing slash"
# redirect used to fire for every method, not just GET/HEAD - a POST
# to a directory-shaped target (like the README's own quick-start
# example, `curl -F ... http://.../uploads`, no trailing slash) still
# wrote the file to disk (that happens during request parsing,
# independent of the response) but told the client 301 instead of
# 201, with no reliable way for the client to know the upload had
# already happened. /uploads here is a real on-disk directory under
# the / location's own root (upload on applies to it too), so this
# exercises the exact same is_adir()-without-trailing-slash path.
assert_status "POST to a directory path with no trailing slash still uploads (not a 301)" 201 \
    -F "file=@$WORK_DIR/upload_source.txt" "$BASE_URL/uploads"

if [ -n "$uploaded" ]; then
    rel=${uploaded#"$WORK_DIR/WWW"}
    assert_status "DELETE uploaded file -> 204" 204 -X DELETE "$BASE_URL$rel"
    assert_status "GET deleted file -> 404" 404 "$BASE_URL$rel"
fi


# --- main-context directives: pid, error_log (v3) ---
# Config resolution order (/etc/webserv/ vs the binary-relative
# fallback) and scripts/install.sh are deliberately not covered here -
# both need real system paths and root, and are tested by hand inside
# a disposable container instead (see V3-PLAN.md).

cat > "$WORK_DIR/bad-directive.conf" <<EOF
worker_processes 4;

server {
    host 127.0.0.1
    listen 8768
    root $WORK_DIR/WWW
    index index.html
}
EOF
bad_directive_output=$("$ROOT_DIR/webserv" "$WORK_DIR/bad-directive.conf" 2>&1)
bad_directive_status=$?
if [ "$bad_directive_status" -ne 0 ] && echo "$bad_directive_output" | grep -q "Invalid directive at top level: worker_processes"; then
    pass "unrecognized top-level directive is a config error, not silently ignored"
else
    fail "unrecognized top-level directive not rejected (status=$bad_directive_status output='$bad_directive_output')"
fi

cat > "$WORK_DIR/bad-errorlog.conf" <<EOF
error_log $WORK_DIR/bad.log notalevel

server {
    host 127.0.0.1
    listen 8768
    root $WORK_DIR/WWW
    index index.html
}
EOF
bad_level_output=$("$ROOT_DIR/webserv" "$WORK_DIR/bad-errorlog.conf" 2>&1)
bad_level_status=$?
if [ "$bad_level_status" -ne 0 ] && echo "$bad_level_output" | grep -q "Invalid error_log level: notalevel"; then
    pass "invalid error_log level is a config error"
else
    fail "invalid error_log level not rejected (status=$bad_level_status output='$bad_level_output')"
fi

# The drop itself (V5-PLAN.md Phase 1) needs real root and is
# container-verified separately - this case only needs the config
# validation, which runs the same regardless of privilege.
cat > "$WORK_DIR/bad-user.conf" <<EOF
user this_account_does_not_exist

server {
    host 127.0.0.1
    listen 8768
    root $WORK_DIR/WWW
    index index.html
}
EOF
bad_user_output=$("$ROOT_DIR/webserv" "$WORK_DIR/bad-user.conf" 2>&1)
bad_user_status=$?
if [ "$bad_user_status" -ne 0 ] && echo "$bad_user_output" | grep -q "no such account: this_account_does_not_exist"; then
    pass "user pointed at a nonexistent account is a config error"
else
    fail "user pointed at a nonexistent account not rejected (status=$bad_user_status output='$bad_user_output')"
fi

GLOBAL_PORT=8767
mkdir -p "$WORK_DIR/logdir"
cat > "$WORK_DIR/global.conf" <<EOF
pid $WORK_DIR/webserv.pid
error_log $WORK_DIR/logdir/error.log debug

server {
    host 127.0.0.1
    listen $GLOBAL_PORT
    root $WORK_DIR/WWW
    index index.html
}
EOF

"$ROOT_DIR/webserv" "$WORK_DIR/global.conf" >"$WORK_DIR/global_stdout.log" 2>&1 &
GLOBAL_PID=$!
i=0
until curl -s -o /dev/null "http://127.0.0.1:$GLOBAL_PORT/" 2>/dev/null; do
    i=$((i + 1))
    if [ "$i" -gt 50 ]; then
        fail "pid/error_log test server never came up"
        break
    fi
    sleep 0.1
done

pidfile_content=$(cat "$WORK_DIR/webserv.pid" 2>/dev/null)
if [ "$pidfile_content" = "$GLOBAL_PID" ]; then
    pass "pid directive writes a pidfile matching the actual process"
else
    fail "pid directive: pidfile='$pidfile_content' actual pid='$GLOBAL_PID'"
fi

curl -s -o /dev/null "http://127.0.0.1:$GLOBAL_PORT/"
if [ ! -s "$WORK_DIR/global_stdout.log" ] && grep -q "Listening on" "$WORK_DIR/logdir/error.log" 2>/dev/null; then
    pass "error_log redirects diagnostics to the file, stdout stays clean"
else
    fail "error_log did not redirect correctly (stdout: $(cat "$WORK_DIR/global_stdout.log" 2>/dev/null))"
fi

kill -TERM "$GLOBAL_PID" 2>/dev/null
wait "$GLOBAL_PID" 2>/dev/null
if [ ! -e "$WORK_DIR/webserv.pid" ]; then
    pass "pidfile removed on graceful shutdown"
else
    fail "pidfile still present after graceful shutdown"
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
