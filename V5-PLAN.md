# webserv v5 - privilege drop

## Execution order (reorganized - see V4-PLAN.md)

This plan's original Phase 1 (the `user` directive - see below) is
superseded by `V4-PLAN.md`'s parser rewrite, not abandoned: the
`getpwnam()`/`getgrnam()` validation it wrote carries over into the
new parser essentially unchanged, only the line-scanning it was
sitting in gets replaced. `v4` lands first (parser hardening,
independent of this branch), `v5` resumes after, rebased onto the new
parser, starting from what was Phase 2 below - renumbered Phase 1 now
that directive-parsing isn't this branch's own work anymore. This
plan was originally drafted and started as "v4," before parser
hardening existed as a plan at all - renumbered to v5 once it became
clear parser hardening needed to release *first* (see
`V4-PLAN.md`'s own "Execution order"), so that the version numbers
match actual release order instead of drafting order. Scope and
decisions below are unchanged from the original v4 draft; only the
number and the phase numbering within this plan moved.

## Context

v3 shipped a real systemd install, but the service still runs as root
the entire time - `listen 80`/`443` needs root to bind, and nothing
afterward gives that up. Every line of untrusted network input this
server ever touches (HTTP parsing, file serving, uploads, and
`fork()`+`execve()` for CGI) runs with full root privileges as a
result. This was called out explicitly in v3 (`README.md`'s "Known
limitations", `V3-PLAN.md`'s decisions) as deliberately deferred, not
overlooked - it's real, security-sensitive code, and it deserved its
own pass rather than being bolted onto a config/packaging release.

A `user` directive (own branch, own phases, same discipline as v2 and
v3): bind privileged ports as root, then drop to an unprivileged
account before the event loop - the point past which the process is
actually parsing bytes a client sent - ever runs.

## Status

- **~~Phase 1 (the `user` directive)~~ - superseded by v4.** Was
  done and verified (all four accept/reject combinations, full
  regression suite green) against the pre-v4 parser; that commit
  stays on this branch's history for reference but this plan no
  longer builds on it directly - see "Execution order" above. The
  validation logic itself is carried into `V4-PLAN.md` Phase 4.
- **Phase 1 (the drop itself, was Phase 2) - done.**
  `dropPrivileges()` (`inc/Privileges.hpp`,
  `src/Server/Privileges.cpp`), called from `main()` between the
  pidfile write and `server.start()`. `initgroups()` ->
  `setgid()` -> `setuid()`, paranoid `setuid(0)` probe afterward.
  No-op verified (whole test suite unchanged), soft-fail path
  verified live (non-root run with `user` set warns and keeps
  running). The actual root-drop path needs a real root process -
  that's Phase 3's container-verified testing, not this dev machine.
  `webserv_tests` 45/45, `tests/run_tests.sh` 33/33.
- **Phase 2 (install.sh, was Phase 3) - done.** Creates the
  `webserv` system account (idempotent), scoped `chown` of
  `/var/log/webserv` and `/var/www/webserv/uploads` only -
  config/binary/site content stay root-owned.
  `conf/webserv.conf.install` ships `user webserv webserv` by
  default. `uninstall.sh` deliberately untouched (the account isn't
  removed on uninstall, matching its existing survive-unless-asked
  philosophy). No passwordless root on this dev machine to actually
  execute the script, so this is shell-syntax-checked only; real
  execution is Phase 3's container-verified job. Whole test suite
  unaffected (`webserv_tests` 45/45, `tests/run_tests.sh` 33/33).
- **Phase 3 (testing, docs, was Phase 4) - done.**
  Container-verified end to end as real root (Ubuntu 24.04 Docker
  image, install.sh run for real): 14/14 checks - the account is
  created, ownership is scoped correctly, real/effective/saved
  uid/gid all match the unprivileged account (not just effective
  uid), CGI inherits the drop, upload and SIGHUP log-reopen both
  still work post-drop, a nonexistent account is a config error
  before any socket binds. `tests/run_tests.sh` gained the no-root
  half of that last check (34/34 total). Docs updated: main-context
  directives table, "Production deployment", "Known limitations",
  "Possible future work".
- **All three phases done. Ready to merge to `main`.**

## Rule for this effort

Same model as v2/v3: one dedicated branch (`v5`), phases land as their
own commits, no merge to `main` until the whole story - the directive,
the drop itself, the install.sh changes it requires, and container
verification of the actual dropped credentials - works end to end.

## Decisions made

- **Target account: a dedicated `webserv` system user**, created by
  `install.sh` if it doesn't already exist
  (`useradd --system --no-create-home --shell /usr/sbin/nologin
  webserv`). Rejected `www-data` (not portable outside Debian/Ubuntu,
  and shares the account with any other web-facing service on the
  box) and `nobody` (universally present with zero setup, but the
  weakest isolation - a compromise of this service could potentially
  touch files or processes belonging to any *other* service also
  running as `nobody`). One more moving part in the installer, in
  exchange for the account only ever meaning "webserv."
- **On by default for new installs.** `conf/webserv.conf.install`
  ships with `user webserv webserv;` already in it, not commented
  out. Existing v3 installs are unaffected either way -
  `install.sh`'s non-clobber rule means an already-installed
  `/etc/webserv/webserv.conf` is never touched - so this only changes
  what a brand-new install looks like, and it looks like the secure
  option by default rather than requiring an admin to know to ask for
  it.
- **Soft-fail if not already root, hard-fail if root and the drop
  itself fails.** Matches nginx's own behavior: if `webserv` is
  launched as a regular user with `user` set in the config (a normal
  dev/test scenario, not a misconfiguration), `setuid()`/`setgid()`
  would just fail with `EPERM` anyway - log a warning and keep running
  as whoever launched it, since that's already no less privileged than
  the alternative. But if the process *is* root and the drop fails for
  any other reason (target user vanished between config validation and
  startup, a syscall error), that has to refuse to start - silently
  continuing to run as root after the admin explicitly asked not to
  would be a real regression of exactly the thing this plan exists to
  fix.
- **Directory ownership is scoped, not blanket.** `install.sh` `chown`s
  only what the dropped-to process actually needs to write at runtime
  - `/var/log/webserv/` (so a `SIGHUP` log-reopen and fresh log files
  keep working) and `/var/www/webserv/uploads/` (+ `uploads/cgi/`, the
  CGI body-staging directory). Everything else - the config
  (`/etc/webserv/`), the binary, and the served site content itself
  (`index.html`, `cgi-bin/*.py`, `err/*.html`) - stays root-owned and
  read-only to the running process. This is deliberate defense in
  depth: a compromised CGI script or a bug in request handling
  shouldn't be able to rewrite the site it's serving or the config
  that controls it, only write into the specific directories that are
  supposed to be writable.
- **CGI inherits the drop for free.** `fork()` copies the parent's
  credentials at the time of the call; since CGI only ever forks from
  inside the event loop (after the drop has already happened), CGI
  scripts start running as the unprivileged user too, with no CGI-
  specific code needed. Worth stating plainly: this actually *fixes* a
  real gap in the current (v3 and earlier) behavior, where a
  root-run server executes every CGI script as root - the same
  hazard just further downstream.

## Phases

### Phase 1 - the drop itself

By the time this phase starts, `user <name> [group];` is already a
real, validated main-context directive - implemented in
`V4-PLAN.md` Phase 4, not here (see "Execution order" above). This
phase assumes `dropUser`/`dropGroup` (or whatever the v4 parser ends
up calling its equivalent globals - naming confirmed once v4 actually
lands) already hold a config-validated value by the time it starts.

- New `src/Server/Privileges.cpp` / `inc/Privileges.hpp` (mirrors how
  TLS got its own focused file in v2) - `dropPrivileges()`, called
  from `main.cpp` at the same point `pid`/`error_log` already hook in:
  after every socket is bound (`setupSocket()`), after TLS certs are
  loaded (`setupSsl()` - reading a root-only-permissioned private key
  has to happen before the drop), after the pidfile is written and
  logs are opened (all of those need root or the directory
  permissions root has), and strictly before `server.start()` - the
  line past which untrusted request bytes get parsed.
- Correct order, in one function: `initgroups()` (clears the
  supplementary group list inherited from root - easy to miss, and a
  real privilege-escalation vector if skipped, since `setuid()`/
  `setgid()` alone don't touch supplementary groups) → `setgid()` →
  `setuid()`. Group before user, always - once the uid is dropped,
  changing gid may no longer be permitted.
- A paranoid verification step after the drop: attempt `setuid(0)`
  and confirm it fails. A process that dropped privileges correctly
  can never regain root; if this check doesn't fail, the drop didn't
  fully take (the classic case being a leftover *saved* uid still 0,
  which lets a process re-escalate via `setuid()` even without
  `CAP_SETUID`) - treated as a hard startup failure, not a warning.
- Soft-fail/hard-fail semantics per the decision above, implemented
  here.

### Phase 2 - install.sh

- Create the `webserv` system user/group if missing (idempotent, same
  non-clobber spirit as every other `install.sh` step - safe to
  re-run).
- `chown` `/var/log/webserv/` and `/var/www/webserv/uploads/`
  (+ `uploads/cgi/`) to `webserv:webserv`. Everything else installed
  stays root-owned.
- `conf/webserv.conf.install` gains `user webserv webserv;` as a
  shipped default (per the decision above) - existing installs
  untouched, per `install.sh`'s non-clobber rule.
- `uninstall.sh`: no change to its existing prompts, but worth a
  decision recorded here rather than guessed at implementation time -
  the `webserv` system user itself is *not* removed by `uninstall.sh`
  (matching the existing "config and logs survive uninstall unless
  asked" philosophy; a leftover unprivileged system account with no
  login shell and no home directory is inert, not a cleanup
  obligation).

### Phase 3 - testing and docs

- Container-verified (needs root, real system paths - same reasoning
  as v3's `install.sh` testing, not appropriate for the regular
  `tests/run_tests.sh` suite):
  - The running process's real/effective/*and saved* uid and gid
    (`/proc/<pid>/status`) all match the target unprivileged account,
    not root - checking all three, not just effective uid, is the
    actual proof of a full drop (a leftover saved-uid of 0 is exactly
    the silent-failure case the paranoid `setuid(0)` check above
    exists to catch).
  - The server still successfully binds a privileged port (`80`)
    *before* dropping, and serves requests normally afterward.
  - A CGI script's own process (`ps` from inside the container) also
    shows the unprivileged uid, confirming the "CGI inherits the
    drop" side effect.
  - Log rotation (`SIGHUP` + reopen) and a file upload both still
    succeed post-drop, proving the `install.sh` `chown` scoping is
    sufficient - and that it isn't *more* than sufficient (the config
    and static site content stay unwritable to the dropped-to user).
  - `user` pointed at a nonexistent account is a config error at
    startup, before any socket binds.
  - Started as a non-root user with `user` set: warns and keeps
    running as the launching account rather than refusing to start.
- `tests/run_tests.sh`: the config-validation case (nonexistent user
  → config error) needs no root and fits the regular suite the same
  way v3's `error_log`-level-validation test did; the rest stays
  container-only.
- Docs: `user`/`group` added to the "Main-context directives" table;
  "Production deployment" section's path table gains the `webserv`
  system user/group; "Known limitations" loses the "runs as root"
  bullet (replaced with whatever residual scope this phase doesn't
  cover, if anything turns out to still need it); "Possible future
  work"'s privilege-drop entry is removed, since this plan is what
  it was waiting on.

## Testing

Same discipline as v2 and v3: build warning-free, run the full
`tests/run_tests.sh` suite (must stay green throughout - nothing in
this plan touches the non-root dev/test path), and verify manually
against a real running server before moving to the next phase. Phase
3's container verification is the one that actually matters for this
plan's core claim (the process is genuinely unprivileged, not just
configured to look that way) - it doesn't get skipped or deferred the
way v3's `systemctl start/stop` coverage was explicitly left
unverified (that gap was about tooling `install.sh` didn't need to
prove anything security-relevant; this one is the entire point of the
plan).

## Explicitly not in scope

- **Capabilities-based partial privilege** (e.g. `CAP_NET_BIND_SERVICE`
  via `setcap` on the binary instead of a root-then-drop dance). A
  real, arguably cleaner alternative for the specific "bind a
  privileged port" problem, but it's a packaging-level choice
  (`setcap` has to be reapplied on every binary rebuild/upgrade,
  which `install.sh` would need to own) that changes the deployment
  model rather than fixing a gap in this one - worth its own future
  discussion, not bundled here.
- **Per-location or per-CGI-script user isolation** (running different
  CGI scripts as different unprivileged users, closer to how PHP-FPM
  pools work). This plan gets the whole server off root; finer-grained
  isolation between locations/scripts is a materially bigger feature,
  not a natural extension of it.
- **Anything from v3's own "not in scope" list** (SNI, multi-worker
  scaling, HTTP pipelining) - unrelated to privilege drop, still not
  part of this plan either.
