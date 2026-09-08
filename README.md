*This project has been created as part of the 42 curriculum by bahbibe, bmakhlou, mahansal.*

# webserv

A HTTP/1.1 server written in C++98, built around a single-threaded
`epoll` event loop. Handles static file serving, directory listings,
file uploads (`multipart/form-data`, chunked and Content-Length
bodies), CGI (PHP/Python), and an nginx-style config file.

## Description

`webserv` implements enough of HTTP/1.1 to serve a real static
website and CGI applications to a standard web browser or `curl`,
without any external HTTP or Boost library: raw POSIX sockets, a
single non-blocking `epoll` instance driving all client I/O (listen,
read and write alike), and `fork()`/`execve()` for CGI. Its
behaviour is driven entirely by an nginx-style configuration file
(server blocks, location blocks, per-route method/redirect/upload/
CGI/autoindex rules).

## Instructions

```
make        # build ./webserv
make re     # rebuild from scratch
make clean  # remove object files
make fclean # remove object files and the binary
make run    # build, run with the default config, then clean
make leaks  # build and run under valgrind (leak-check=full)
make test   # build, then run tests/run_tests.sh (end-to-end, needs curl)
```

Requires a C++98 compiler (`c++`) and Linux (uses `epoll`). `make test`
additionally needs `curl` and `python3` on `PATH`.

The server reads `conf/mime.types` and the default config at
`conf/default.conf` relative to the `webserv` binary's own location,
so it can be run from any working directory as long as those two
files stay in a `conf/` folder next to the binary.

```
./webserv [config_file]
```

If no config file is given, `conf/default.conf` is used.

## Resources

Classic references consulted while building and hardening this
project:

- RFC 9110 (HTTP Semantics), RFC 9111 (HTTP Caching), RFC 9112
  (HTTP/1.1) — the current HTTP specification set, used to check
  status code usage, header semantics, and connection/keep-alive
  behaviour against the letter of the spec.
- RFC 3875 (The Common Gateway Interface, CGI/1.1) — CGI
  meta-variable set and request/response framing.
- [nginx](https://nginx.org/en/docs/) documentation — the `server {}`
  / `location {}` config block style this project's config format is
  modeled on.

AI assistance (Claude Code) was used throughout this project's
hardening pass, under direct human direction with every design
decision explicitly reviewed and confirmed before implementation:
end-to-end code review against the 42 subject and the HTTP RFCs
above; fixing bugs found that way (multipart parsing, path-traversal
validation, epoll busy-spin/timeout handling, memory leaks caught via
`valgrind`); implementing new features (HEAD method, graceful
shutdown, access logging, HTTP/1.1 keep-alive, configurable CGI
interpreter paths, non-blocking sockets with partial-write handling);
and writing/extending the end-to-end test suite (`tests/run_tests.sh`).
Every change was built with `-Wall -Wextra -Werror -std=c++98`, run
through the test suite, and manually verified against a real running
server (`curl`, `valgrind`) before being committed.

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
```

Multiple `server` blocks are supported (virtual hosting by
`server_name` on a shared `host:port`, and independent listeners on
different ports).

### Server-level directives

| Directive | Meaning |
|---|---|
| `host` | IPv4 or IPv6 address to bind (`localhost` is normalized to `127.0.0.1`); an IPv6 `host` binds IPv6-only (no dual-stack), so listen on both families with two `server` blocks on the same port |
| `listen` | Port to bind (defaults to 80 if empty) |
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

- Single-process, single-threaded: CGI scripts fork a child but the
  parent event loop blocks on each `epoll_wait` cycle while polling
  CGI completion, and a CGI process is killed after 5 seconds.
- CGI stdio is wired up via `freopen()` onto temp files rather than
  `pipe()`/`dup2()`, so it doesn't need its own non-blocking I/O
  handling (temp files are exempt from the single-poll requirement).
  See "Possible future work" below.
- No HTTP pipelining: a client must read each response before sending
  the next request on the same connection (see "Keep-alive" above).
- `POST` always closes the connection instead of being keep-alive
  eligible, to avoid an unread request body being left on the socket
  after an early-rejection error path.
- No HTTPS/TLS.
- A handful of functions used (`remove()`, `realpath()`, `readlink()`,
  `inet_ntop()`) aren't on the 42 subject's whitelisted external-
  function list, and there's no whitelisted replacement for file
  deletion, path resolution, or address formatting at all - these are
  used anyway since the alternative is no DELETE, no path-traversal
  check, or no IPv6 client logging.

## Out of scope (by design)

Considered and deliberately not implemented, rather than gaps waiting
to be filled:

- **HTTP/1.0.** The 42 subject explicitly calls it "a reference
  point, not enforced." HTTP/1.0 has no persistent connections by
  default (the inverse of 1.1, which this server already implements
  correctly with keep-alive), so supporting it would mean a second,
  parallel connection-handling code path for a strictly older
  protocol version - real added complexity for zero behavior this
  server doesn't already cover better under 1.1.
- **HTTP/2 (or HTTP/3/QUIC).** Not a support flag on top of the
  current server - a different wire protocol entirely (binary
  framing, stream multiplexing, header compression, and for HTTP/3 a
  UDP-based transport instead of TCP). Out of scope for a learning-
  focused HTTP/1.1 server.

## Possible future work

- Rearchitect CGI stdio from `freopen()`-on-temp-files to
  `pipe()`+`dup2()`, registering the pipe fds on the same shared
  `epoll` instance non-blocking like every other fd. Bigger than it
  sounds: temp files are explicitly exempt from the single-poll
  requirement, pipes aren't, so this touches the event loop, not just
  `Response::CGI()`.
- A folder/function reorganization pass (the codebase has grown a lot
  of features into a small number of files this session) - best done
  as its own change, separate from any behavior change, so a
  regression is never "was it the rename or the rewrite?"
- POST keep-alive: needs guaranteed full-body draining on every error
  path (or an explicit drain step before reuse) so a rejected POST
  can't leave unread bytes on a connection about to be reused.
- Per-location `client_max_body_size` override (currently server-level
  only).
- A config reload on `SIGHUP` instead of requiring a restart.
