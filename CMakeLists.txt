cmake_minimum_required(VERSION 3.5)
project(riscv_pipeline CXX)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -O2 -Wall -Wextra -Wpedantic")
add_executable(code main.cpp)
