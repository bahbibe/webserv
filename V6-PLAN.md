# webserv v6 - a Docker Hub image

## Context

`scripts/install.sh` (v3) covers "I have a Linux box and want this
running as a system service." A Docker image covers the other common
path: "let me try this in one command, or run it as part of something
else without touching the host at all." Straightforward compared to
v4/v5 - this is packaging, not a change to the server itself.

## Rule for this effort

Same model as v2/v3/v4/v5: one dedicated branch (`v6`), phases land as
their own commits, no merge to `main` until verified end to end. The
image itself is never pushed to Docker Hub without an explicit
go-ahead in the moment - that's a public, named publish under the
user's account, not a repo-internal step like every merge/tag before
it.

## Decisions made

- **`debian-slim` for the runtime stage**, not `alpine`. This project
  is built and verified against glibc (Ubuntu 24.04 throughout this
  session, including every container test in v3). Alpine's musl libc
  is a real compatibility variable for a C++ binary that has never
  been built or run against it - not just an image-size trade, new
  surface to verify. Multi-stage build regardless, so the *build*
  stage's extra weight (cmake, g++, libssl-dev, the spdlog
  `FetchContent` source) never ships - only the final runtime stage's
  size is the debian-vs-alpine trade, and that gap matters less than
  the compatibility risk here.
- **Manual `docker push` when ready, not GitHub Actions automation.**
  Keeps a Docker Hub access token out of GitHub Actions secrets
  entirely. Revisit CI automation later if manual pushes become
  actual friction - not a default to reach for up front.

## Phases

### Phase 1 - Dockerfile

- Multi-stage build:
  - **Build stage**: a glibc-based image with the build toolchain
    (cmake, g++, libssl-dev), builds `webserv` exactly as
    `tests/run_tests.sh` already does (`cmake -S . -B build && cmake
    --build build -j`) - proving the Dockerfile's build step matches
    the project's own tested build path, not a separate one that
    could silently drift from it.
  - **Runtime stage**: `debian-slim`, only what's needed to *run* the
    binary - the built `webserv` executable, `libssl3`/`libcrypto`
    (OpenSSL runtime, not the `-dev` headers), `libstdc++`. No
    compiler, no build tools, no spdlog source tree.
- Config/site baked in: `conf/default.conf` (adjusted for a
  container - see Phase 2) and `WWW/` copied into the image, so
  `docker run` works with zero required flags - matches this
  project's own "it should just work" bar from the showcase pass
  (step 2 of that work: run it as a new user would, don't ship
  something that needs undocumented setup first).
- `EXPOSE 8090` (matching the example config's port) documented in
  the Dockerfile; `EXPOSE 443`/TLS is a config-driven addition, not a
  Dockerfile default, since it needs a real certificate mounted in.

### Phase 2 - container-aware config

- `conf/default.conf`'s `host 127.0.0.1` doesn't work inside a
  container - the server needs to bind `0.0.0.0` (all interfaces) for
  Docker's own port mapping (`-p 8090:8090`) to reach it at all. A
  Docker-specific config variant (`conf/webserv.conf.docker`, mirroring
  the `webserv.conf.install` precedent from v3 - a config template for
  a specific deployment shape, not a change to the dev-checkout
  default) rather than changing `conf/default.conf` itself and
  breaking the plain "clone and run" workflow that config is for.
- Volume mount points documented (not required, but supported) for
  anyone who wants their own site content or TLS certs without
  rebuilding the image: e.g. `-v ./mysite:/app/WWW`,
  `-v ./certs:/app/certs`.

### Phase 3 - build and run verification

- `docker build` succeeds; `docker run` serves the baked-in example
  site correctly (static/CGI/upload/404 - the same matrix v3's
  container testing already used) reachable from the host via the
  mapped port.
- Image size checked and reported (the actual payoff of the
  multi-stage build - worth a concrete number, not just "should be
  smaller").
- A volume-mounted custom site/config tested too, confirming the
  documented override path in Phase 2 actually works, not just the
  baked-in default.

### Phase 4 - docs, then the actual push

- README gets a short "Docker" section: `docker build`/`docker run`
  commands, the port-mapping/volume-mount notes from Phase 2.
- Once everything above is verified and the user explicitly says so
  in the moment: `docker login` + `docker push` to a Docker Hub
  repository under their account (name TBD - presumably
  `bahbibe/webserv` or similar, confirmed with the user before the
  first push since it's a public, named artifact).

## Testing

Same discipline as every prior `vN-PLAN.md`: build and run for real,
verify against the actual container (not just "the Dockerfile looks
right"), before considering any phase done. Nothing here needs root
on the host the way v3/v4's container testing did - Docker itself
already provides the isolation - so this is more straightforward to
verify than v3 or v4 was.

## Explicitly not in scope

- **CI automation** (auto-build/push on release) - per the decision
  above, not now.
- **A `docker-compose.yml`** - worth adding if/when there's a second
  container in the picture (e.g. a reverse proxy in front of it, or a
  TLS cert-renewal sidecar); premature for a single-container image.
- **Multi-arch builds** (arm64, etc.) - this project has only ever
  been built/tested on x86_64 this session; a real multi-arch build
  needs its own verification pass, not assumed to work.
