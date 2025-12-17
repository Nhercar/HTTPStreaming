cmake_minimum_required(VERSION 3.16)
project(HTTPStreaming LANGUAGES CXX)

# Require C++20 for std::jthread and std::stop_token
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Sources for the minimal HTTP server
set(HTTP_SERVER_SOURCES
    src/HTTPServer/http_parser.cpp
    src/HTTPServer/http_response.cpp
    src/HTTPServer/logger.cpp
    src/HTTPServer/router.cpp
    src/HTTPServer/socket_server.cpp
    main.cpp
)

add_executable(HTTPStreaming ${HTTP_SERVER_SOURCES})

target_include_directories(HTTPStreaming PRIVATE src src/HTTPServer)

if (WIN32)
    target_link_libraries(HTTPStreaming PRIVATE ws2_32)
endif()

# Helpful warnings (optional)
if (MSVC)
    target_compile_options(HTTPStreaming PRIVATE /W4 /permissive-)
else()
    target_compile_options(HTTPStreaming PRIVATE -Wall -Wextra -Wpedantic)
endif()
