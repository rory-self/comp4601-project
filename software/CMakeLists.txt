cmake_minimum_required(VERSION 3.30)
project(arithmetic_coder)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

set(INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)

add_library(coderlib
    src/buffer.cpp
    src/input_buffer.cpp
    src/encoder.cpp
    src/frequency_table.cpp
    src/arith.cpp
    src/decoder.cpp
)
target_include_directories(coderlib PRIVATE ${INCLUDE_DIR})
target_compile_options(coderlib PRIVATE -Wall)

add_executable(${PROJECT_NAME} src/main.cpp)
target_include_directories(${PROJECT_NAME} PRIVATE ${INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE coderlib)
target_compile_options(${PROJECT_NAME} PRIVATE -Wall)

