cmake_minimum_required(VERSION 3.20)

project(net_common
    VERSION 0.1.0
    LANGUAGES CXX
)

# Use CONFIG / NO_MODULE so CMP0167 and FindBoost are out of the picture
find_package(Boost 1.89.0 REQUIRED
    COMPONENTS headers
    CONFIG
)

find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(${PROJECT_NAME} INTERFACE)

target_include_directories(${PROJECT_NAME} INTERFACE
    include)

# Asio is header-only: depend on Boost headers
target_link_libraries(${PROJECT_NAME}
    INTERFACE Boost::headers           # or Boost::boost on older CMake
    INTERFACE Threads::Threads
    INTERFACE tsqueue
)
