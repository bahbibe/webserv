cmake_minimum_required(VERSION 3.16)
project(webserv CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Werror)

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

# --- unit tests (v4: config parser hardening, see V4-PLAN.md) ---
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
    doctest_discover_tests(webserv_tests)
endif()
