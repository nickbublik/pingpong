cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Boost 1.89.0 REQUIRED
    COMPONENTS headers
    CONFIG
)

project(ppserver
    VERSION 0.1.0
    LANGUAGES CXX
)

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

add_executable(${PROJECT_NAME}
    src/server.cpp
    src/discovery_server.cpp
)

set(CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

target_link_libraries(${PROJECT_NAME}
    PRIVATE Boost::headers           # or Boost::boost on older CMake
    PRIVATE Threads::Threads
    PRIVATE OpenSSL::Crypto
    PRIVATE net_common
    PRIVATE ppcommon
)
