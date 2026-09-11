# webserv

An HTTP/1.1 (and HTTPS) server in C++20, built around a single-threaded
`epoll` event loop. Handles static file serving, directory listings,
file uploads (`multipart/form-data`, chunked and Content-Length
bodies), CGI (PHP/Python) over real non-blocking pipes, TLS
termination, and an nginx-style config file.

Originally built as a 42 School project (C++98, a restricted function
whitelist, no external libraries). That project was retired from the
42 curriculum, which removed those constraints - this is a from-scratch
modernization: C++20, RAII throughout, `std::filesystem`, structured
logging, real CGI pipes instead of temp files, and OpenSSL-backed TLS,
aimed at being genuinely production-worthy rather than just
subject-compliant.

## Description

`webserv` implements enough of HTTP/1.1 to serve a real static
website and CGI applications to a standard web browser or `curl`: raw
POSIX sockets, a single non-blocking `epoll` instance driving all
client I/O (listen, read and write alike, plaintext or TLS), and
`fork()`/`execve()` with non-blocking pipes for CGI. Its behavior is
driven entirely by an nginx-style configuration file (server blocks,
location blocks, per-route method/redirect/upload/CGI/autoindex
rules, and per-server TLS certificates).

## Build

```
cmake -S . -B build
cmake --build build -j
```

Produces `./webserv` at the repo root (not inside `build/`), so every
existing invocation and script below still works unchanged.

Requires:

- A C++20 compiler (tested with GCC 13).
- CMake 3.16+.
- OpenSSL development headers (`libssl-dev` on Debian/Ubuntu,
  `openssl-devel` on Fedora/RHEL) - TLS support links against it.
- `curl` and `python3` on `PATH` to run the test suite.

[spdlog](https://github.com/gabime/spdlog) is fetched and built
automatically by CMake (`FetchContent`) - no separate install needed.

```
rm -rf build && cmake -S . -B build && cmake --build build -j   # clean rebuild
bash tests/run_tests.sh                                          # end-to-end suite
valgrind --leak-check=full ./webserv [config_file]               # manual leak check
```

## Usage

```
./webserv [config_file]
```

If no config file is given, `conf/default.conf` is used. The server
reads `conf/mime.types` and the default config relative to the
`webserv` binary's own location, so it can be run from any working
directory as long as those two files stay in a `conf/` folder next to
the binary.

## Resources

Classic references consulted while building this project:

- RFC 9110 (HTTP Semantics), RFC 9111 (HTTP Caching), RFC 9112
  (HTTP/1.1) - the current HTTP specification set, used to check
  status code usage, header semantics, and connection/keep-alive
  behavior against the letter of the spec.
- RFC 3875 (The Common Gateway Interface, CGI/1.1) - CGI
  meta-variable set and request/response framing.
- RFC 8446 (TLS 1.3) and the [OpenSSL](https://www.openssl.org/docs/)
  API documentation - non-blocking `SSL_accept()`/`SSL_read()`/
  `SSL_write()` state handling.
- [nginx](https://nginx.org/en/docs/) documentation - the `server {}`
  / `location {}` config block style this project's config format is
  modeled on.

AI assistance (Claude Code) was used throughout this project's
hardening pass and the v2 modernization, under direct human direction
with every design decision explicitly reviewed and confirmed before
implementation: end-to-end code review against the HTTP RFCs above;
fixing bugs found that way (multipart parsing, path-traversal
validation, epoll busy-spin/timeout handling, memory leaks caught via
`valgrind`); implementing features (HEAD method, graceful shutdown,
access logging, HTTP/1.1 keep-alive, configurable CGI interpreter
paths, non-blocking sockets with partial-write handling); the v2
rewrite itself (CMake/C++20 migration, RAII cleanup, structured
logging, real non-blocking CGI pipes, TLS); and writing/extending the
end-to-end test suite (`tests/run_tests.sh`). Every change was built
warning-free (`-Wall -Wextra -Werror`), run through the test suite,
and manually verified against a real running server (`curl`,
`openssl s_client`, `valgrind`) before being committed.

## Access log

Every completed or aborted connection is appended (one line, flushed
immediately) to `access.log` next to the binary:

```
[2026-09-05 05:54:38] 127.0.0.1 GET / 200 11 0ms
[2026-09-05 05:54:38] 127.0.0.1 GET /missing.html 404 127 0ms
[2026-09-05 05:54:38] 127.0.0.1 HEAD / 200 0 0ms
[2026-09-05 05:54:51] 127.0.0.1 - - - 0 300ms
```

Fields: timestamp, client IP, method, path, status code, response
body size in bytes (headers and chunk framing excluded; always 0 for
`HEAD`), and total connection duration. A connection that closed
before a request could be parsed (e.g. an idle client that just
disconnects) logs `-` for method/path/status.

If the log file can't be opened (e.g. no write permission next to the
binary), the server prints a warning and keeps serving without
logging - it doesn't fail to start over this.

### Log rotation

`SIGHUP` closes and reopens `access.log` at the same path, without
restarting - the standard logrotate pattern: rotate (rename)
`access.log`, send `SIGHUP`, and the server picks up a fresh file
while whatever was renamed stays exactly as it was:

```
mv access.log access.log.1
kill -HUP $(pgrep webserv)
```

## Config file

The whole config file is validated before anything binds a socket:
every bad directive value, unknown directive, missing root path,
duplicate directive, and bind/listen failure (including a port
already in use) across the *entire* file is collected and reported
together, not just the first one hit - useful when writing a config
from scratch. If any errors are found, the server prints all of them
and exits without starting; nothing listens unless the whole file is
clean. A malformed `{`/`}` structure is the one thing that still
aborts immediately, since nothing past that point can be parsed
reliably.

Config files use an nginx-like block syntax:

```
server {
    host 127.0.0.1
    listen 8080
    server_name example.com

    root /path/to/site
    index index.html
    error_page 404 /path/to/404.html
    client_max_body_size 1000000
    autoindex off

    location /uploads {
        root /path/to/site/uploads
        allow GET POST DELETE
        upload on
        upload_path /path/to/site/uploads
        cgi on
        cgi_upload_path /path/to/site/uploads/cgi
        autoindex on
    }
}

server {
    host 127.0.0.1
    listen 8443 ssl
    ssl_certificate /path/to/cert.pem
    ssl_certificate_key /path/to/key.pem

    root /path/to/site
    index index.html
}
```

Multiple `server` blocks are supported (virtual hosting by
`server_name` on a shared `host:port`, and independent listeners on
different ports - plaintext and TLS listeners can coexist on
different ports in the same config).

### Server-level directives

| Directive | Meaning |
|---|---|
| `host` | IPv4 or IPv6 address to bind (`localhost` is normalized to `127.0.0.1`); an IPv6 `host` binds IPv6-only (no dual-stack), so listen on both families with two `server` blocks on the same port |
| `listen <port> [ssl]` | Port to bind (defaults to 80 if empty); the `ssl` keyword makes this a TLS listener - requires `ssl_certificate` and `ssl_certificate_key` |
| `ssl_certificate` | Path to a PEM certificate file (required if `listen ... ssl`) |
| `ssl_certificate_key` | Path to the matching PEM private key (required if `listen ... ssl`) |
| `server_name` | One or more virtual host names |
| `root` | Filesystem root for this server; must exist |
| `index` | Default file(s) served for a directory request |
| `error_page <code> <path>` | Custom page for a status code (repeatable) |
| `client_max_body_size` | Max request body size in bytes (0 = unlimited) |
| `autoindex` | `on`/`off`, directory listing when no index file is found |
| `location <path> { ... }` | Per-path overrides, see below |

### Location-level directives

`root`, `index`, `autoindex` override the server-level value for
requests under that path. Additional directives:

| Directive | Meaning |
|---|---|
| `allow` | Space-separated list of allowed methods (`GET`, `POST`, `DELETE`) |
| `upload on\|off` | Allow file uploads for `POST` requests |
| `upload_path` | Where uploaded files are written |
| `cgi on\|off` | Enable CGI execution (`.php` via `/usr/bin/php-cgi`, `.py` via `/usr/bin/python3` by default - see `cgi_path`) |
| `cgi_upload_path` | Where CGI-received request bodies are staged |
| `cgi_path <ext> <interpreter>` | Override the interpreter for an extension, e.g. `cgi_path py /usr/local/bin/python3.12`. Repeatable, one line per extension. |
| `return <url>` | Issue a 301 redirect to `<url>` |

## Supported HTTP behavior

- Methods: `GET`, `HEAD`, `POST`, `DELETE`. `HEAD` behaves exactly like
  `GET` (same status, same headers) but never sends a body; it's
  permitted anywhere `GET` is, so `allow` directives don't need to
  list it separately.
- HTTP/1.1 only (`505` on any other version).
- TLS 1.2 minimum (TLS 1.3 negotiated where the client supports it),
  OpenSSL-backed, fully non-blocking through the same `epoll` loop as
  everything else - not a blocking `SSL_accept()`/`SSL_read()`/
  `SSL_write()` anywhere.
- Keep-alive: `GET`/`HEAD`/`DELETE` responses reuse the connection for
  the next request unless the client sends `Connection: close` (no
  pipelining - the client must read each response before sending the
  next request). `POST` always closes the connection after
  responding, regardless of status.
- Request bodies via `Content-Length`, `Transfer-Encoding: chunked`,
  or `multipart/form-data`.
- Every response carries a `Date` header (RFC 9110 6.6.1).
- Status codes returned: 200, 201, 204, 301, 400, 403, 404, 405, 408,
  409, 411, 413, 414, 500, 501, 504, 505.
- Idle connections are dropped with a `408` after 10 seconds without a
  complete request, or after 30 seconds total regardless of activity
  (a client trickling bytes just often enough to keep resetting the
  idle timer still gets cut off).
- The header block (request line + headers, before the body) is
  capped at 8192 bytes; over that is a `400` and the connection is
  closed.
- Up to 512 concurrent connections; beyond that, new connections are
  accepted and immediately closed rather than queued.
- Directory requests without a trailing slash are redirected (301);
  directory requests are served from `index`, or an autoindex listing
  if `autoindex on` and no index file is found.
- `DELETE` removes files and, recursively, directories.
- Path traversal outside a location's configured root is rejected
  with `403`.
- CGI stdin/stdout run over real non-blocking pipes registered on the
  same `epoll` instance - no filesystem round-trip. A CGI process that
  doesn't respond within 5 seconds is killed; if that happens before
  any output was sent, the client gets a clean `504`, otherwise the
  partial response is closed cleanly (matches real reverse-proxy
  behavior - once a 200 stream has started, a crash truncates it
  rather than retroactively becoming an error page).
- `SIGINT`/`SIGTERM` (e.g. Ctrl-C) trigger a graceful shutdown: the
  server stops accepting new connections immediately, finishes any
  requests already in flight (up to a 5 second grace period, after
  which remaining connections are force-closed), then exits.
- Sockets (listening and client) are non-blocking; all I/O is driven
  through a single shared `epoll` instance, and partial/would-block
  writes are resumed on the next writable tick rather than dropped.
- A thrown exception while servicing one connection (e.g. an
  allocation failure under memory pressure) closes just that
  connection - it doesn't take the whole server down.

## Known limitations

- Single-process, single-threaded reactor (the same model one nginx
  worker process uses): CGI forks a child process, but the event loop
  itself doesn't use additional threads. I/O-bound workloads (sockets,
  disk, CGI exec) don't benefit much from in-process threading without
  also reworking file I/O to be async, and multiple worker
  *processes* (nginx's actual scaling model) would be a separate,
  larger change.
- No HTTP pipelining: a client must read each response before sending
  the next request on the same connection (see "Keep-alive" above).
- `POST` always closes the connection instead of being keep-alive
  eligible, to avoid an unread request body being left on the socket
  after an early-rejection error path.
- No SNI / multiple TLS certificates on one listener - one certificate
  per `server` block, matched by which listening socket accepted the
  connection, not by the TLS ClientHello's server name.

## Out of scope (by design)

Considered and deliberately not implemented, rather than gaps waiting
to be filled:

- **HTTP/1.0.** No persistent connections by default (the inverse of
  1.1, which this server already implements correctly with
  keep-alive), so supporting it would mean a second, parallel
  connection-handling code path for a strictly older protocol
  version - real added complexity for zero behavior this server
  doesn't already cover better under 1.1.
- **HTTP/2 (or HTTP/3/QUIC).** Not a support flag on top of the
  current server - a different wire protocol entirely (binary
  framing, stream multiplexing, header compression, and for HTTP/3 a
  UDP-based transport instead of TCP). Out of scope for a
  learning-focused HTTP/1.1 server.

## Possible future work

- POST keep-alive: needs guaranteed full-body draining on every error
  path (or an explicit drain step before reuse) so a rejected POST
  can't leave unread bytes on a connection about to be reused.
- Per-location `client_max_body_size` override (currently server-level
  only).
- A full config reload on `SIGHUP` (re-parse the file, rebuild live
  servers/locations/sockets without dropping connections) instead of
  requiring a restart - `SIGHUP` currently only reopens the access log
  (see "Log rotation" above), which was deliberately scoped smaller.
- SNI-based certificate selection for multiple TLS virtual hosts on
  one listener.
- Multi-worker-process scaling (nginx's actual model) if throughput
  ever becomes the bottleneck - a scaling change, not a correctness
  one, and not part of this project's current scope.
