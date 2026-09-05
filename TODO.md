# TODO

Things noticed during the v1.0.0 review that weren't in scope for a bug
pass, plus ideas for where this could go next.

## Hardening (found, not fixed - deliberate design or bigger effort)

- `Request::findLocation()` matches locations by walking the location
  map in reverse alphabetical order and returning the first prefix
  match. It happens to work for typical configs (more specific paths
  sort after `/`), but it's not real longest-prefix matching like
  nginx does. Two sibling locations with an unlucky alphabetical
  order would resolve wrong.
- `Response`'s copy assignment shallow-copies the `env` (CGI
  environment) pointer. Not observed to cause a double-free in
  practice (map entries aren't copied after insertion), but it's
  fragile - a `Response` copied while `_isCGI` is set would double-free.
- `Response::env`, `pid` aren't initialized in the default constructor.
  Harmless today (nothing reads them before `fillEnv()`/`CGI()` set
  them), but relies on that call order never changing.
- CGI interpreter paths (`/usr/bin/php-cgi`, `/usr/bin/python3`) are
  hardcoded. Should be configurable per-location instead.
- `conf/mime.types` and `conf/default.conf` are opened with paths
  relative to the current working directory, so the server only works
  correctly launched from the project root. Resolve relative to the
  binary's own path instead.
- `Server::mimeTypes()` reparses `conf/mime.types` from disk once per
  `server {}` block. Harmless at startup, just wasteful.

## Features for a v2

- Keep-alive / HTTP pipelining. Every response currently sends
  `Connection: close`; supporting persistent connections would cut
  latency for real clients (browsers, curl with `-K`, benchmarking
  tools).
- IPv6 (`AF_INET6` / dual-stack sockets).
- TLS (even just a `listen 443 ssl` directive backed by OpenSSL).
- Access logging (method, path, status, size, timing) to a file, not
  just the startup banner to stdout.
- Config validation pass at startup that reports *all* errors found
  (missing roots, bad directives, port clashes) instead of stopping at
  the first one - much nicer when writing a config from scratch.
- Graceful shutdown on SIGINT/SIGTERM (close listening sockets, let
  in-flight responses finish) instead of an abrupt kill.

## Build / tooling

- Add a small test suite. Right now correctness is verified manually
  with curl - a scripted suite (even just a shell script driving curl
  + diff against expected output) would catch regressions like the
  ones fixed in v1.0.0 automatically.
- Consider a `.clang-format` / consistent brace style - the codebase
  mixes a couple of different conventions (`e.g. cout << "x"` vs
  `cout << "x"\n`, mixed indentation in a few spots).

## Packaging

Not a pip/npm-shaped project - it's a Linux-only C++98 systems binary
that talks directly to raw sockets, `epoll`, and `fork`/`execve`.
There's no interpreter or package registry it fits into. For this
kind of project:

- **Makefile (current)** is the right primary build for now - it's
  what 42-style C++ projects use, has no dependencies, and is fine for
  a single-binary CLI tool.
- **CMake** is worth adding *if* this grows past one Makefile-friendly
  target (e.g. a test suite, optional TLS, splitting into a library +
  CLI) or you want IDE integration (CLion, VS Code CMake Tools) and
  easier cross-compiling. Can coexist with the Makefile - say the
  word and it's a small addition.
- **Distro packaging** (a Debian package, a Homebrew formula) is
  premature at this stage - that's usually worth doing once there's
  an actual install target (`/usr/local/bin`, a config file location
  under `/etc`, etc.) and users installing it outside of `git clone`.
