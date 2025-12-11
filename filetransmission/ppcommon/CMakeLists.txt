cmake_minimum_required(VERSION 3.20)

find_package(Boost 1.89.0 REQUIRED
    COMPONENTS headers
    CONFIG
)

find_package(Threads REQUIRED)

project(ppcommon
    VERSION 0.1.0
    LANGUAGES CXX
)

add_library(${PROJECT_NAME} INTERFACE)

target_include_directories(${PROJECT_NAME} INTERFACE
    include)

target_link_libraries(${PROJECT_NAME}
    INTERFACE net_common
    INTERFACE logger
)
