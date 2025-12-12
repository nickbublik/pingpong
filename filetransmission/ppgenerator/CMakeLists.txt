cmake_minimum_required(VERSION 3.20)

project(ppgenerator
    VERSION 0.1.0
    LANGUAGES CXX
)

add_library(${PROJECT_NAME} STATIC
    src/phrase_generator.cpp
)

target_include_directories(${PROJECT_NAME}
    PUBLIC include
)
