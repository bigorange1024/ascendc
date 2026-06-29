# 根目录普通 C 演示（与 Makefile 等价，供 CMake Tools / IDE 使用）
cmake_minimum_required(VERSION 3.14)
project(ascendc_c_demo C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(app src/main.c src/demo.c)
target_include_directories(app PRIVATE ${CMAKE_SOURCE_DIR}/include)
