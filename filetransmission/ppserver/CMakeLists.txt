cmake_minimum_required(VERSION 3.20)

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

target_link_libraries(${PROJECT_NAME}
    PRIVATE Boost::headers
    PRIVATE Threads::Threads
    PRIVATE OpenSSL::Crypto
    PRIVATE net_common
    PRIVATE ppcommon
    PRIVATE logger
)
