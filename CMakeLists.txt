cmake_minimum_required(VERSION 3.16)
project(webserv CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Werror)

set(SOURCES
    main.cpp
    src/Server/Server.cpp
    src/Server/Location.cpp
    src/Server/Webserver.cpp
    src/Server/ConfigUtils.cpp
    src/Server/Config.cpp
    src/Request/Request.cpp
    src/Request/Helpers.cpp
    src/Request/Boundaries.cpp
    src/Request/Chunks.cpp
    src/Response/Response.cpp
    src/Response/Cgi.cpp
)

add_executable(webserv ${SOURCES})
target_include_directories(webserv PRIVATE inc)

# Keep the binary at the repo root (./webserv), matching every existing
# script and manual invocation - purely a build-system swap in Phase 0,
# nothing downstream should need to change.
set_target_properties(webserv PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}
)
