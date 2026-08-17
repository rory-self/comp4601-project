cmake_minimum_required(VERSION 3.30)
project(arithmetic_coder)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Threads REQUIRED)

set(INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)

add_library(coderlib
    src/buffer.cpp
    src/input_buffer.cpp
    src/encoder.cpp
    src/frequency_table.cpp
    src/arith.cpp
    src/decoder.cpp
    src/replicate.cpp
)
target_include_directories(coderlib PUBLIC ${INCLUDE_DIR})
target_compile_options(coderlib PRIVATE -Wall)
target_link_libraries(coderlib PUBLIC Threads::Threads)

add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE coderlib)
target_compile_options(${PROJECT_NAME} PRIVATE -Wall)