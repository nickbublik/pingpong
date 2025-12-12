cmake_minimum_required(VERSION 3.20)

find_package(Boost 1.89.0 REQUIRED
    COMPONENTS headers program_options
    CONFIG
)

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

project(ppclient
    VERSION 0.1.0
    LANGUAGES CXX
)

add_executable(${PROJECT_NAME}
    src/receiver.cpp
    src/sender.cpp
    src/main.cpp
    src/discovery_client.cpp
    src/unix_ip_utils.cpp
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE Threads::Threads
    PRIVATE Boost::headers
    PRIVATE Boost::program_options
    PRIVATE OpenSSL::Crypto
    PRIVATE net_common
    PRIVATE ppcommon
    PRIVATE ppgenerator
    PRIVATE logger
)
