cmake_minimum_required(VERSION 3.10)
project(HyperMonolithicKernel VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

file(GLOB_RECURSE SOURCES "src/kernel/*.cpp")

add_library(kernel_core ${SOURCES})

# Example executable to demonstrate usage (optional, but good for verification)
add_executable(kernel_main src/main.cpp)
if(UNIX)
    target_link_libraries(kernel_main PRIVATE -Wl,--whole-archive kernel_core -Wl,--no-whole-archive)
else()
    target_link_libraries(kernel_main PRIVATE kernel_core)
endif()
