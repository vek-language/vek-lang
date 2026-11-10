cmake_minimum_required(VERSION 3.20)
project(vek LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(vek_core STATIC
    # Add .cpp's after implementation.
    src/parser.cpp
)

target_include_directories(vek_core PUBLIC headers)
target_compile_features(vek_core PUBLIC cxx_std_20)

# VEK DK
add_executable(vek vek.cpp)
target_link_libraries(vek PRIVATE vek_core)

# VM
add_executable(vekvm vekvm.cpp)
target_link_libraries(vekvm PRIVATE vek_core)