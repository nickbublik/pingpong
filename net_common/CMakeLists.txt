cmake_minimum_required(VERSION 3.20)

project(net_common
    VERSION 0.1.0
    LANGUAGES CXX
)

find_package(Boost 1.89.0 REQUIRED
    COMPONENTS headers
    CONFIG
)

find_package(Threads REQUIRED)

add_library(${PROJECT_NAME} INTERFACE)

target_include_directories(${PROJECT_NAME} INTERFACE
    include)

target_link_libraries(${PROJECT_NAME}
    INTERFACE Boost::headers
    INTERFACE Threads::Threads
    INTERFACE logger
    INTERFACE tsqueue
)
