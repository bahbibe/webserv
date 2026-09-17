# See V6-PLAN.md. Multi-stage on purpose: the build stage's toolchain
# (cmake, g++, libssl-dev, spdlog's FetchContent source tree) never
# ships - only the runtime stage's much smaller footprint does.

# ---- build stage ----
FROM debian:bookworm-slim AS build

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake git ca-certificates libssl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

# Same build command tests/run_tests.sh and every install.sh path
# already use - the Dockerfile's build step matches this project's
# own tested build path, not a separate one that could drift from it.
# Unit tests skipped here: WEBSERV_BUILD_TESTS pulls doctest via
# FetchContent, dev-only weight with nothing to verify inside a
# throwaway build-stage container.
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DWEBSERV_BUILD_TESTS=OFF && \
    cmake --build build -j4

# ---- runtime stage ----
FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    libssl3 libstdc++6 ca-certificates python3 \
    && rm -rf /var/lib/apt/lists/* && \
    useradd --system --no-create-home --shell /usr/sbin/nologin webserv

WORKDIR /app
COPY --from=build /src/webserv /app/webserv
COPY --from=build /src/conf /app/conf
COPY --from=build /src/WWW /app/WWW
# /app itself (not recursively) so the unprivileged user can create
# access.log there (it doesn't exist yet); uploads/ recursively since
# it needs to hold arbitrary uploaded files. Everything already
# baked in - the binary, conf/, the rest of WWW/ - stays root-owned,
# same scoped-ownership posture as v5's install.sh.
RUN chown webserv:webserv /app && chown -R webserv:webserv /app/WWW/uploads

# 8090 matches conf/webserv.conf.docker's listen port (see
# V6-PLAN.md Phase 2) - EXPOSE is documentation, docker run still
# needs its own -p to actually publish the port. TLS/443 isn't
# exposed by default: it's config-driven (needs a real certificate
# mounted in), not something a baked-in image should default to.
EXPOSE 8090

# No `user` directive in webserv.conf.docker: the container itself is
# the isolation boundary here, and this image never binds a
# privileged port (8090, not 80) - USER below is the equivalent of
# v5's privilege drop for the container deployment shape, done at the
# container level instead of inside the process.
USER webserv

ENTRYPOINT ["/app/webserv"]
CMD ["/app/conf/webserv.conf.docker"]
