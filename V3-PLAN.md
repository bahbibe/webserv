# webserv v3 - filesystem-standard config, install script, daemon mode

## Context

Getting the showcase demo running (see the "Add an example site and fix
default.conf" / "Fix CGI script path" commits on `main`) surfaced how far
this project's config/deployment story is from anything you'd actually run
as a system service: the shipped config hardcoded a path from the original
author's machine, the config file's own location is resolved relative to
the binary (via `/proc/self/exe`) but every path *written inside* the
config resolves relative to the launch directory instead, there's no
install path at all (you run `./webserv` from wherever you happen to have
cloned it), and there's no daemon/service story - no pidfile, no systemd
unit, nothing.

nginx, Apache, and every other real HTTP server ship a filesystem layout
(`/etc/nginx/`, `/var/www/`, `/var/log/nginx/`), an install path, and a
supervised-daemon story. This plan gets webserv to the same place.

## Status

- **Phase 1 (config resolution order) - done.** Explicit CLI arg still
  always wins; `/etc/webserv/webserv.conf` preferred over the
  binary-relative `conf/default.conf` fallback whenever that directory
  exists; mime.types follows the same tier. Verified live against a
  throwaway `/etc/webserv/{webserv.conf,mime.types}` (created and
  removed by hand): all three precedence levels confirmed, zero
  regression for the no-system-install case (every existing invocation
  of this project).
- **Phase 2 (main-context directives: `pid`, `error_log`) - done.**
  `parseGlobalDirectives()` recognizes both at depth 0 and reports
  anything else unrecognized - closes the "stray top-level line was
  silently ignored" gap. Found and fixed two related bugs in
  `Server::parseServer()` along the way (misreading a leading global
  directive as an invalid server-level one; double-reporting a
  genuinely invalid one) and a call-ordering bug in `main.cpp`
  (`error_log` has to be wired up before `setupSocket()`'s own
  logging). Full regression suite (28/28) unaffected.
- **Phase 3 (install.sh, systemd unit) - done.** `scripts/install.sh`/
  `uninstall.sh`, `scripts/webserv.service` (`Type=simple`, no
  double-fork needed), `conf/webserv.conf.install` (FHS paths,
  absolute throughout). Also closed a gap Phase 1 left (access log
  path wasn't tier-aware) and, while testing in a container, found
  and fixed a real bug retroactively affecting Phase 1 too: the
  system-tier switch ran unconditionally based on whether
  `/etc/webserv/` existed on disk, independent of whether an explicit
  config path was given - meaning any machine with that directory
  present (including this project's own `tests/run_tests.sh`) got its
  access log and mime.types silently redirected regardless of the
  config actually in use. Fixed by scoping all system-tier logic to
  only the code path that runs when no CLI argument was given.
  Verified in a disposable Ubuntu 24.04 Docker container - full
  install → auto-resolved config → static/CGI/upload/404 all correct,
  logs landing in the right place, non-clobber re-install, uninstall
  prompts. `systemctl start/stop/reload` itself needs a real systemd
  PID 1 (not available in a plain container) and is not verified end
  to end - the unit is 8 lines of standard boilerplate, low risk, but
  flagged rather than overclaimed. `tests/run_tests.sh` (28/28)
  reproduced the bug once, confirmed the fix once.
- **Phase 4 (docs, hardening pass) - done.** README: rewrote the
  config-resolution paragraph for the real three-tier order, added a
  "Main-context directives" table and a "Production deployment"
  section, updated "Known limitations"/"Possible future work" for the
  root-only systemd service. Five new automated checks in
  `tests/run_tests.sh` formalize what Phases 1-3 verified by hand
  where that's actually appropriate (global-directive validation,
  pid/error_log behavior) - config resolution order and install.sh
  stay hand-verified in a container, since they need root and real
  system paths the regular suite can't assume. Full suite: 33/33.
  **All four phases done - ready to merge, pending final go-ahead.**

## Rule for this effort

Same model as v2: one dedicated branch (`v3`), phases land as their own
commits, no merge to `main` until the whole story - config resolution,
global directives, install script, systemd unit - works end to end. A
half-finished "installs a config format that doesn't exist yet" state
would leave `main` worse off than not starting.

## Decisions made

- **Daemon control: systemd unit only**, not a self-contained double-fork
  daemon with `-s stop/reload` flags. Under systemd `Type=simple`, the
  process just runs in the foreground and systemd tracks the PID directly
  from the fork it launched - no double-fork/`setsid()`/fd-redirection
  dance needed at all. That machinery exists in nginx because nginx
  predates systemd by over a decade; building it here would be legacy
  weight with no one to serve it. `SIGTERM`/`SIGINT` (graceful shutdown)
  and `SIGHUP` (currently: reopen the access log) already do exactly what
  systemd's default stop signal and a typical `ExecReload=` expect -
  nothing new needed there either.
- **Privilege drop deferred to v4.** A `user`/`group` directive (bind a
  privileged port as root, then `setuid()`/`setgid()` before the event
  loop starts) is real, security-sensitive code - ordering mistakes in
  that handoff are a classic privilege-escalation bug class. Not bundling
  it into a config/packaging release; it deserves its own careful pass
  with its own test matrix. V3's systemd unit runs as root and documents
  that plainly as a known limitation, same as running `./webserv` on
  port 80 today already requires root.
- **Packaging: a plain `install.sh`, not a CPack `.deb`.** CPack would be
  low extra effort on top of the existing CMake build, but it's a
  packaging surface (versioning, changelog, postinst/prerm scripts) this
  project doesn't need yet. A script that copies files into place is
  transparent and easy to read before running as root, which matters more
  here than `apt install ./webserv.deb` convenience.
- **No nginx-style `http {}` wrapper block.** Real directives (`pid`,
  `error_log`) live in a new flat "main context" - lines outside any
  `server {}` block - rather than introducing an intermediate `http {}`
  block every existing config would need to be wrapped in. Keeps
  `conf/default.conf` and every test config backward compatible; nothing
  about the current `server { ... }` syntax changes.
- **Two config templates, not one.** `conf/default.conf` stays exactly
  what it is now - a repo-relative dev/demo config for running straight
  out of a git checkout. A separate template (`conf/webserv.conf.install`
  or similar - exact name TBD) uses FHS paths (`/var/www/webserv`,
  `/var/log/webserv/`) and is what `install.sh` actually deploys. Trying
  to make one config file serve both purposes is exactly the kind of
  hardcoded-path confusion this plan exists to fix.

## Known gap this plan also closes

Config validation currently only looks at `server { ... }` blocks -
`Webserver::brackets()` walks the file purely for bracket balance, and
`Server::parseServer()` only runs per already-recognized server block. A
stray line outside any `server {}` block today isn't an error at all -
it's silently ignored. `parseGlobalDirectives()` (Phase 2 below) closes
this: every top-level directive is either recognized or reported, same
"collect every error before starting" discipline the rest of the config
parser already follows.

## Phases

### Phase 1 - config resolution order

- `main.cpp`'s `resolveConfDir()` currently always resolves relative to
  the binary's own location. New precedence, checked in order:
  1. Explicit CLI argument (`./webserv path/to/conf`) - unchanged, always
     wins, exactly like today.
  2. `/etc/webserv/webserv.conf`, if it exists - the installed path.
  3. `<binary-dir>/conf/default.conf` - unchanged fallback, keeps the
     existing "run straight from a git checkout" workflow working exactly
     as it does now.
- `mime.types` resolution follows the same precedence
  (`/etc/webserv/mime.types` before the binary-relative fallback).
- Behavior-preserving for every existing use of this project: nothing
  changes for `./webserv [config_file]` run from a checkout unless
  `/etc/webserv/webserv.conf` actually exists on that machine.

### Phase 2 - main context directives

- New directives valid only *outside* any `server {}` block:
  - `pid <path>;` - optional; if set, the PID is written there on start
    and removed on clean shutdown. Not required for systemd `Type=simple`
    (systemd already knows the PID), kept for compatibility with anything
    else that expects a PID file (log rotation scripts, monitoring).
  - `error_log <path> [level];` - adds a file sink to the existing spdlog
    setup at the given level (`debug`/`info`/`warn`/`error`, matching
    spdlog's own level names) alongside (or instead of, TBD) the current
    stdout sink. Under systemd, stdout already goes to the journal, so
    this matters most for non-systemd runs.
- `parseGlobalDirectives()`: a new pass, run once before the existing
  `Webserver::brackets()`/`parseServer()` loop, that walks only the lines
  outside any `{}` block. Recognizes `pid`/`error_log`; anything else
  unrecognized at the top level is a config error (closes the silent-
  ignore gap noted above). Wired into the existing `ConfigValidator` so
  it reports alongside every other config problem, not separately.

### Phase 3 - install.sh and the systemd unit

- `scripts/install.sh` (needs root; must be tested in a container, not
  against a real machine - see Testing below):
  - Builds via CMake if `./webserv` isn't already built.
  - Creates `/etc/webserv/`, `/var/www/webserv/`, `/var/log/webserv/` if
    missing.
  - Installs the binary to `/usr/local/sbin/webserv` (`sbin`, not `bin` -
    it's a system service binary, matching nginx's own
    `/usr/sbin/nginx`).
  - Copies `mime.types` and the install config template into
    `/etc/webserv/` - **only if not already present**, so re-running the
    installer (an upgrade) never clobbers a config someone has already
    edited.
  - Copies the example `WWW/` site into `/var/www/webserv/` - same
    non-clobber rule, first install only.
  - Installs `webserv.service` into `/etc/systemd/system/` and runs
    `systemctl daemon-reload`.
- `scripts/uninstall.sh`: removes the binary and the systemd unit.
  Prompts before touching `/etc/webserv/` or `/var/log/webserv/` - config
  and logs are exactly the kind of thing people expect to survive an
  uninstall unless they explicitly ask otherwise.
- `webserv.service`:
  ```
  [Unit]
  Description=webserv HTTP/HTTPS server
  After=network.target

  [Service]
  Type=simple
  ExecStart=/usr/local/sbin/webserv
  ExecReload=/bin/kill -HUP $MAINPID
  Restart=on-failure

  [Install]
  WantedBy=multi-user.target
  ```
  (Exact contents will firm up during implementation - shown here to fix
  the shape: no `User=`/`Group=` yet, per the privilege-drop decision
  above.)

### Phase 4 - docs and hardening pass

- README gets a "Production deployment" section: what `install.sh` does,
  the resulting directory layout (a small table - `/etc/webserv`,
  `/var/www/webserv`, `/var/log/webserv`, the systemd unit path), and the
  `systemctl start/stop/status/restart webserv` / `systemctl reload
  webserv` command set.
- Full regression sweep: the existing 26/28-check suite must keep passing
  completely unmodified (it never touches `/etc` or the install path).
  New coverage needed:
  - Config resolution order (explicit arg beats `/etc/webserv`, which
    beats the binary-relative default).
  - Global directive parsing: valid `pid`/`error_log`, invalid values,
    and - the actual bug being fixed - an unrecognized top-level
    directive now produces a config error instead of silently doing
    nothing.
  - `install.sh`/`uninstall.sh` run inside a throwaway container, not
    against this machine directly - see Testing.

## Testing

Same discipline as every phase this project has gone through: build
warning-free, run the full test suite, and manually verify against a real
running server before moving on - with one addition specific to this
plan. `install.sh` writes to `/etc`, `/var/www`, `/var/log`, and
`/usr/local/sbin`, and installs a systemd unit - real system state, not a
throwaway build directory. That gets tested inside a disposable container
(or VM), never by running the installer against the machine this session
is actually running on, and only after the user explicitly confirms doing
so - matching the standing rule about destructive/system-affecting
actions.

## Explicitly not in scope

- **Privilege drop** (`user`/`group` directive) - v4, per the decision
  above.
- **Full config reload on `SIGHUP`** - already a tracked TODO
  independent of this plan (`SIGHUP` reopening the access log is a
  narrower, already-shipped feature; rebuilding servers/sockets/locations
  live without dropping connections is a separate, larger piece of work).
  `ExecReload=` in the systemd unit calls the same `SIGHUP` this project
  already handles - if/when full reload ships, this unit needs no changes.
- **An nginx-style `http {}` block** - per the decision above, deliberately
  not introduced.
- **Windows/macOS support** - `/etc`, `/var/log`, systemd are all
  Linux-specific by design; this project has been Linux-only (`epoll`)
  from the start.
