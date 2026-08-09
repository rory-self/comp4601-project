cmake_minimum_required(VERSION 3.16)
project(arith CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Threads REQUIRED)

file(GLOB SOURCES ${CMAKE_SOURCE_DIR}/src/*.cpp)

add_executable(arith ${SOURCES})
target_include_directories(arith PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(arith PRIVATE Threads::Threads)