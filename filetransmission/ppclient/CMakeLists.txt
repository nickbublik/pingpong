cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Use CONFIG / NO_MODULE so CMP0167 and FindBoost are out of the picture
find_package(Boost 1.89.0 REQUIRED
    COMPONENTS headers program_options
    CONFIG
)

find_package(Threads REQUIRED)

project(ppclient
    VERSION 0.1.0
    LANGUAGES CXX
)

add_executable(${PROJECT_NAME}
    src/receiver.cpp
    src/sender.cpp
    src/main.cpp
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE Threads::Threads
    PRIVATE Boost::headers
    PRIVATE Boost::program_options
    PRIVATE net_common
    PRIVATE ppcommon
)
