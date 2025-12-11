cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENABLE_DEBUG_LOG "Enable debug logging" OFF)

IF(ENABLE_DEBUG_LOG)
    message(STATUS "ENABLE_DEBUG_LOG is on")
    add_compile_definitions(ENABLE_DEBUG_LOG=1)
ELSE()
    message(STATUS "ENABLE_DEBUG_LOG is off")
    add_compile_definitions(ENABLE_DEBUG_LOG=0)
ENDIF()

add_subdirectory(${CMAKE_SOURCE_DIR}/logger)
add_subdirectory(${CMAKE_SOURCE_DIR}/tsqueue)
add_subdirectory(${CMAKE_SOURCE_DIR}/net_common)
add_subdirectory(${CMAKE_SOURCE_DIR}/filetransmission)

