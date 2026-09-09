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

set(SOURCES
    main.cpp
    src/Server/Server.cpp
    src/Server/Location.cpp
    src/Server/Webserver.cpp
    src/Server/ConfigUtils.cpp
    src/Server/Config.cpp
    src/Server/Tls.cpp
    src/Request/Request.cpp
    src/Request/Helpers.cpp
    src/Request/Boundaries.cpp
    src/Request/Chunks.cpp
    src/Response/Response.cpp
    src/Response/Cgi.cpp
    src/Response/DefaultPages.cpp
)

add_executable(webserv ${SOURCES})
target_include_directories(webserv PRIVATE inc)
target_link_libraries(webserv PRIVATE spdlog::spdlog_header_only OpenSSL::SSL OpenSSL::Crypto)

# Keep the binary at the repo root (./webserv), matching every existing
# script and manual invocation - purely a build-system swap in Phase 0,
# nothing downstream should need to change.
set_target_properties(webserv PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}
)
