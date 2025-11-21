cmake_minimum_required(VERSION 3.20)

project(server
    VERSION 0.1.0
    LANGUAGES CXX
)

find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(${PROJECT_NAME}
    src/server.cpp
)

set(CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

target_link_libraries(${PROJECT_NAME}
    PRIVATE Threads::Threads
    PRIVATE net_common
)
