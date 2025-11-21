cmake_minimum_required(VERSION 3.20)

add_subdirectory(${CMAKE_SOURCE_DIR}/tsqueue)
add_subdirectory(${CMAKE_SOURCE_DIR}/net_common)
add_subdirectory(${CMAKE_SOURCE_DIR}/server)
add_subdirectory(${CMAKE_SOURCE_DIR}/client)

project(asio_draft
    VERSION 0.1.0
    LANGUAGES CXX
)

# Use CONFIG / NO_MODULE so CMP0167 and FindBoost are out of the picture
find_package(Boost 1.89.0 REQUIRED
    COMPONENTS headers
    OPTIONAL_COMPONENTS system
    CONFIG
)

find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(${PROJECT_NAME}
    src/main.cpp
)

set(CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Asio is header-only: depend on Boost headers
target_link_libraries(${PROJECT_NAME}
    PRIVATE Boost::headers           # or Boost::boost on older CMake
    PRIVATE Threads::Threads
    PRIVATE net_common
)

# If Boost::system target exists (e.g. on some distros / older Boost),
# link it too. On Arch 1.89 it simply won't exist, and that's OK.
if(TARGET Boost::system)
    target_link_libraries(${PROJECT_NAME} PRIVATE Boost::system)
endif()
