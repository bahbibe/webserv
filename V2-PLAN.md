# webserv v2 — modernization plan

The project this was built for was dropped from the 42 curriculum and
replaced. The C++98 constraint and the subject's whitelisted-function
list no longer apply. This is the plan for a major, non-backward-
constrained rewrite aimed at making the project genuinely production-
worthy, not just subject-compliant.

**Rule for this effort:** stays on the `v2` branch. No merge to `main`
until the whole plan below is implemented and tested — this is one
continuous branch, not the usual per-feature branch/merge/release
cycle used everywhere else in this project's history.

## Status

- **Phase 0 (CMake/C++20 build migration) — done.**
- **Phase 1 (RAII/type cleanup) — done**, plus a wrap-up pass beyond
  the original scope: `DELETE` rewritten onto `std::filesystem`
  (found and fixed a real permission-check bug along the way),
  `Response.cpp` split (autoindex/default-error-page generation into
  `DefaultPages.cpp`), `Request::toLowerCase`/`trim` modernized (fixed
  a real UB risk in the old `toLowerCase`), `Webserver::brackets()`
  relocated into `Config.cpp`.
- **Phase 2 (structured logging) — done** for the diagnostic-message
  half: spdlog is in (CMake `FetchContent`, header-only), every real
  cout/cerr diagnostic converted with proper levels, dead ANSI macros
  and three dead debug-dump functions removed along the way.
  `access.log` deliberately untouched, per plan. Known rough edge:
  fatal `WebservException` messages still carry an old embedded
  "Error: " ANSI prefix alongside spdlog's own level tag — cosmetic,
  not fixed yet.
- **Phase 3 (CGI: pipes instead of temp files) — done.** `freopen()`-
  on-temp-files replaced with real non-blocking `pipe()`+`dup2()`+
  `fork()`+`execve()`, pipe fds routed through the shared epoll loop
  via a new `_cgiFdToClient` map. Verified against the full stress
  matrix (large output with forced backpressure, large POST body,
  crash/timeout before and after header commit, HEAD, autoindex-CGI,
  keep-alive, client disconnect mid-stream) plus `valgrind
  --track-fds=yes` showing zero leaked fds and zero leaked heap.
- **Phase 4 (TLS) — not started.**
- **Phase 5 (final hardening pass, README rewrite, merge) — not started.**

## Decisions made

- **C++20.** GCC 13.3.0 (present on this machine) supports it fully.
  Not committing to using every new feature for its own sake — using
  what actually simplifies real code (RAII wrappers, smart pointers,
  `std::optional`, `std::filesystem`, designated initializers where
  they help readability).
- **Concurrency model: unchanged.** Single-threaded epoll reactor,
  same as today — this is the same pattern one nginx worker process
  uses. CGI already forks, so the process was never single-tasking.
  A thread pool was considered and rejected: this server is I/O-bound
  (sockets, disk, CGI exec), so the throughput gain is limited without
  also reworking file I/O to be async, and it would add real
  synchronization risk across `Request`/`Response`/`Server` state that
  isn't there today. If more throughput is ever needed, scale via
  multiple worker *processes* (nginx's actual model), not in-process
  threads — not part of this plan, noted for later.
- **Dependencies: targeted.** OpenSSL (TLS — the single biggest real
  gap found this session) and a small structured-logging library
  (spdlog, header-only-friendly). Nothing else added speculatively.
- **Build system: CMake.** Needed cleanly once OpenSSL is a
  dependency; better dependency discovery and test integration than
  hand-wiring `-lssl -lcrypto` into the Makefile.
- **Testing: unchanged approach.** Keep the curl-based end-to-end
  suite (`tests/run_tests.sh`) as the primary regression gate, same as
  every feature this session — no new unit-test framework added (kept
  dependency footprint targeted, not broader).

## Environment note

`cmake` and `libssl-dev` are not installed on this machine as of this
plan being written. The user is installing them independently. Phase
0 (below) is blocked on that — nothing in this plan gets marked done
without an actual successful build + full test-suite run, matching
how every change in this project has been verified all along.

## Known pain points in the current (v1) codebase this plan addresses

- CGI's environment array is manually managed with raw `new[]`/
  `delete[]` (`Response`/`Cgi.cpp`) — no RAII, cleanup duplicated
  across every error path.
- `Location*` stored as raw pointers in `map<string, Location*>`
  (`Server.hpp`), manually `delete`d in the destructor and in
  `operator=` — should be `unique_ptr<Location>`.
- Duplicate-directive detection reads a `t_dir` struct through an
  `int*` cast and scans it as an array (`duplicateDirective()`,
  `ConfigUtils.cpp`) — works, but relies on struct layout/no padding
  assumptions instead of being an explicit, obviously-correct check.
- Two near-identical hand-rolled `ServerException` classes
  (`Server.hpp`, `webserv.hpp`) — should be one shared exception
  hierarchy.
- Config-validation errors go through a global `vector<string>
  configErrors` and a free function `addConfigError()` — works, but
  is unscoped global mutable state; a `ConfigValidator` type that
  owns its own result list would be cleaner.
- No RAII for file descriptors — every close() path (accept failure,
  fcntl failure, epoll_ctl failure, connection teardown) repeats
  manual `close()` calls. A small `FdGuard`/`UniqueFd` wrapper removes
  the duplication and the "did I forget a close() on this new error
  path" risk that's bitten this project before (the client-fd leak
  fixed earlier this session).
- `DELETE`'s recursive directory removal is a hand-rolled
  `opendir`/`readdir` walk (`Response::DELETE`) — `std::filesystem::
  remove_all` replaces it outright, and also finally resolves the
  "DELETE is mandatory but no whitelisted function can delete a file"
  contradiction found earlier this session, since it's standard
  library, not a syscall wrapper.
- CGI stdio uses `freopen()` on temp files instead of `pipe()` +
  `dup2()` (already a known TODO item — the "not whitelisted" reason
  for avoiding pipes doesn't apply anymore, but the pipes are still
  the *right* design regardless: real bidirectional I/O instead of
  going through the filesystem).
- Logging is `cout`/`cerr` with hand-rolled ANSI color macros
  (`ERR`, `YELLOW`, etc. in `webserv.hpp`) - no levels, no structured
  fields, can't be redirected/filtered independently of stdout.
- No TLS at all.

## Phases

Each phase lands as its own set of commits on `v2`, built and run
through the full test suite (plus valgrind where relevant) before
moving to the next — same rigor as every change made to this project
so far, just without an intermediate merge to `main`.

### Phase 0 — build system migration (behavior-preserving)

Port the existing, working v1 logic to a CMake build under C++20,
with **zero functional changes** — same discipline as the folder
restructure: prove the port itself introduced nothing new before any
modernization starts.

- `CMakeLists.txt` at the repo root, C++20, same warning flags
  (`-Wall -Wextra -Werror` at minimum, consider `-Wpedantic`).
- Same source layout (`src/Server`, `src/Request`, `src/Response`,
  `main.cpp`) initially — no reorganization bundled with the build
  migration, for the same "was it the move or the rewrite" reason the
  folder restructure was kept separate from feature work.
- `tests/run_tests.sh` updated to build via CMake instead of `make`.
- Full 26-check suite must pass, identical to today, before Phase 0
  is considered done.
- **Blocked** until `cmake` is installed (see Environment note above).

### Phase 1 — RAII and type cleanup (behavior-preserving)

- `unique_ptr<Location>` in `Server`'s location map.
- A small `UniqueFd` RAII wrapper for socket/pipe file descriptors,
  used everywhere a raw `int` fd is manually `close()`'d today.
- Replace the CGI envp raw `new[]`/`delete[]` array with a type that
  owns its C-string lifetimes and exposes a `char**` only at the
  `execve()` call site.
- Replace `t_dir`'s int-array duplicate-detection with an explicit,
  named check (no reinterpret-through-a-struct).
- Collapse `Server::ServerException`/`Webserver::ServerException`
  into one shared exception type.
- `ConfigValidator`-style type replacing the global `configErrors`
  vector + `addConfigError()` free function.
- Every step verified against the full test suite + valgrind, same as
  every fix this session — this phase touches memory-management code
  directly, so it gets the most scrutiny.

### Phase 2 — structured logging

- Introduce spdlog for diagnostic/error logging (replacing the
  `cout`/`cerr` + ANSI-macro pattern), with levels (debug/info/warn/
  error) and a configurable sink.
- Keep `access.log`'s existing line format as its own dedicated
  logger/sink — it's a stable, documented format
  (`tests/run_tests.sh` and the README both describe it) and changing
  it isn't part of this phase's goal. `SIGHUP` log-reopen behavior
  carries over unchanged.

### Phase 3 — CGI: pipes instead of temp files

- Replace `freopen()`-on-temp-files with `pipe()` + `dup2()` for CGI
  stdin/stdout, matching the subject's originally-suggested pattern
  (irrelevant now that it's not graded, but it's still the technically
  right design: no filesystem round-trip, no temp-file cleanup races).
- Register the pipe fds on the same shared epoll instance, non-
  blocking, following the exact partial-read/-write discipline
  already built for client sockets this session (offset tracking, no
  errno inspection, resume on next readiness tick).
- This is the part of the "v2" work most likely to introduce new
  bugs — gets the heaviest testing pass: large CGI output (bigger
  than one pipe buffer, forcing real backpressure), slow CGI input
  consumption, and the existing CGI success/error/timeout/chdir test
  matrix repeated against the new implementation.

### Phase 4 — TLS

- OpenSSL-backed TLS termination.
- New config directives: `listen <port> ssl`, `ssl_certificate`,
  `ssl_certificate_key`.
- Non-blocking handshake as an explicit state in the connection
  object, driven from the same epoll loop (accept → TLS handshake in
  progress → established → normal request/response flow), not a
  blocking `SSL_accept()` call.
- Tested with both `curl` against a self-signed cert and
  `openssl s_client` for protocol-level verification.

### Phase 5 — final hardening pass and merge readiness

- Re-run every existing regression test (26-check suite, the
  Slowloris/header-cap/connection-cap/CGI-timeout live tests from
  earlier this session, valgrind across all of it) against the fully
  modernized codebase.
- README rewritten for v2 (new build instructions, TLS config
  documentation, updated "Known limitations"/"Possible future work").
- Only once all of the above is green: merge `v2` into `main`.

## Explicitly not in scope for this plan

- Multi-worker-process scaling (noted above as a possible later step,
  not part of making this "prod ready" in the sense of correctness/
  security — it's a scaling change, not a correctness one).
- A unit-test framework (Catch2/GoogleTest) — the existing e2e suite
  stays authoritative, per the "targeted dependencies" decision.
- HTTP/2, HTTP/3/QUIC, HTTP pipelining — already established
  non-goals for this project (see README's "Out of scope" section);
  nothing about dropping the 42 constraints changes that reasoning.
