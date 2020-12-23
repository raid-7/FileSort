cmake_minimum_required(VERSION 3.15)
project(FileSort)

set(CMAKE_CXX_STANDARD 20)

add_executable(fsort src/fsort_main.cpp src/mmap.cpp)
target_compile_options(fsort PRIVATE "-Wall" "-Wextra")

add_executable(fgen src/fgen_main.cpp src/mmap.cpp)
target_compile_options(fgen PRIVATE "-Wall" "-Wextra")
