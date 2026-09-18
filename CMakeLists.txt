cmake_minimum_required(VERSION 3.16)
project(webserv CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Werror)

# Off by default - doubles memory use and slows every syscall, not
# something a normal dev loop or the Docker image wants. CI turns it
# on in a dedicated job; locally: cmake -S . -B build-asan
# -DWEBSERV_SANITIZE=ON.
option(WEBSERV_SANITIZE "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)
if(WEBSERV_SANITIZE)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

# Pin explicitly to Debug (-g, no -O flag) before fetching any
# dependency below. Leaving CMAKE_BUILD_TYPE unset was the Phase 0
# choice so nothing added -O2 - but spdlog's own CMakeLists.txt
# defaults an *unset* CMAKE_BUILD_TYPE to Release for the whole build
# tree (it's a CACHE variable, shared across the top-level project and
# every FetchContent'd dependency), which reintroduces the same
# glibc _FORTIFY_SOURCE/-Werror=unused-result trap on freopen()/
# chdir() that Phase 0 already worked around once. Setting it here,
# before FetchContent, makes spdlog's "if(NOT CMAKE_BUILD_TYPE)" guard
# see it as already chosen and leave it alone.
set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)

include(FetchContent)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.14.1
)
FetchContent_MakeAvailable(spdlog)

find_package(OpenSSL REQUIRED)

# Everything except main.cpp - shared between the real webserv binary
# and the unit test binary below (webserv_tests), so a test can link
# the actual parsing/server code without pulling in main()'s own
# int main() (a program can only have one).
set(LIB_SOURCES
    src/Globals.cpp
    src/Server/Server.cpp
    src/Server/Location.cpp
    src/Server/Webserver.cpp
    src/Server/ConfigUtils.cpp
    src/Server/Config.cpp
    src/Server/Lexer.cpp
    src/Server/ConfigParser.cpp
    src/Server/Privileges.cpp
    src/Server/Tls.cpp
    src/Request/Request.cpp
    src/Request/Helpers.cpp
    src/Request/Boundaries.cpp
    src/Request/Chunks.cpp
    src/Response/Response.cpp
    src/Response/Cgi.cpp
    src/Response/DefaultPages.cpp
)

add_executable(webserv main.cpp ${LIB_SOURCES})
target_include_directories(webserv PRIVATE inc)
target_link_libraries(webserv PRIVATE spdlog::spdlog_header_only OpenSSL::SSL OpenSSL::Crypto)

# Keep the binary at the repo root (./webserv), matching every existing
# script and manual invocation - purely a build-system swap in Phase 0,
# nothing downstream should need to change.
set_target_properties(webserv PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# --- unit tests (v4: config parser hardening) ---
#
# Scoped narrowly to parser-level logic, not HTTP behavior - the
# existing curl-driven tests/run_tests.sh stays authoritative for
# that. doctest is a dev-only dependency: it's linked into
# webserv_tests, never into the webserv binary itself above.
option(WEBSERV_BUILD_TESTS "Build the doctest-based unit test suite" ON)
if(WEBSERV_BUILD_TESTS)
    FetchContent_Declare(
        doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG v2.4.11
    )
    FetchContent_MakeAvailable(doctest)

    file(GLOB TEST_SOURCES tests/unit/*.cpp)
    add_executable(webserv_tests ${TEST_SOURCES} ${LIB_SOURCES})
    target_include_directories(webserv_tests PRIVATE inc)
    target_link_libraries(webserv_tests PRIVATE
        spdlog::spdlog_header_only OpenSSL::SSL OpenSSL::Crypto doctest::doctest
    )
    set_target_properties(webserv_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build
    )

    include(CTest)
    include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
    # WORKING_DIRECTORY: the parser tests load a real conf/mime.types
    # by a path relative to the repo root - ctest otherwise runs each
    # test with the build directory as cwd (whether invoked as
    # `ctest --test-dir build` or `cd build && ctest`), which breaks
    # that lookup even though running the webserv_tests binary
    # directly from the repo root works fine.
    doctest_discover_tests(webserv_tests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

# --- fuzzing (libFuzzer, Clang only - libFuzzer ships in compiler-rt,
# GCC has no equivalent driver) ---
#
# Chunks.cpp is a hand-written byte-level state machine parsing
# untrusted client input directly - the highest-risk parsing surface
# in the request path, and the one least exercised by hand-written
# e2e cases. Off by default: needs Clang and isn't part of a normal
# build or the doctest/e2e suites.
option(WEBSERV_FUZZ "Build libFuzzer harnesses (requires Clang)" OFF)
if(WEBSERV_FUZZ)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        message(FATAL_ERROR "WEBSERV_FUZZ requires Clang: cmake -DCMAKE_CXX_COMPILER=clang++ -DWEBSERV_FUZZ=ON")
    endif()
    add_executable(fuzz_chunks tests/fuzz/fuzz_chunks.cpp src/Request/Chunks.cpp)
    target_include_directories(fuzz_chunks PRIVATE inc)
    # webserv.hpp (pulled in via Chunks.hpp) includes spdlog/spdlog.h
    # unconditionally - header-only, so linking it just adds the
    # include path, no extra library code pulled into the fuzz binary.
    target_link_libraries(fuzz_chunks PRIVATE spdlog::spdlog_header_only)
    target_compile_options(fuzz_chunks PRIVATE -fsanitize=fuzzer,address,undefined -g -O1)
    target_link_options(fuzz_chunks PRIVATE -fsanitize=fuzzer,address,undefined)
    set_target_properties(fuzz_chunks PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build
    )

    # Same class of parser as Chunks (hand-written state machine over
    # untrusted input), for multipart/form-data instead of chunked
    # transfer-encoding. No discard mode here - it writes real (small)
    # scratch files, see the harness's own header comment.
    add_executable(fuzz_boundaries tests/fuzz/fuzz_boundaries.cpp src/Request/Boundaries.cpp)
    target_include_directories(fuzz_boundaries PRIVATE inc)
    target_link_libraries(fuzz_boundaries PRIVATE spdlog::spdlog_header_only)
    target_compile_options(fuzz_boundaries PRIVATE -fsanitize=fuzzer,address,undefined -g -O1)
    target_link_options(fuzz_boundaries PRIVATE -fsanitize=fuzzer,address,undefined)
    set_target_properties(fuzz_boundaries PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build
    )
endif()
