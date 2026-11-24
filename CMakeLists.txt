cmake_minimum_required(VERSION 3.20)
project(vek LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Shared core library
add_library(vek_core INTERFACE)

target_include_directories(vek_core INTERFACE
    shared
    shared/headers
    shared/natives
)
target_compile_features(vek_core INTERFACE cxx_std_20)

# Windows icon resources
if(WIN32)
    set(VKC_ICON_RC "${CMAKE_CURRENT_SOURCE_DIR}/assets/filedata/vkc.rc")
    set(VEK_ICON_RC "${CMAKE_CURRENT_SOURCE_DIR}/assets/filedata/vek.rc")
endif()

# VEK Compiler & DK
add_executable(vek
    vek/main.cpp
    vek/src/paths.cpp
    ${VEK_ICON_RC}
)
target_include_directories(vek PRIVATE vek/headers)
target_link_libraries(vek PRIVATE vek_core)

# VEK Virtual Machine
add_executable(vekvm
    vekvm/main.cpp
    ${VKC_ICON_RC}
)
target_include_directories(vekvm PRIVATE vekvm/headers)
target_link_libraries(vekvm PRIVATE vek_core)

# VEK Package Manager
add_executable(vekp
    vekp/main.cpp
    external/miniz/miniz.c
    ${VEK_ICON_RC}
)
target_include_directories(vekp PRIVATE
    vekp/headers
    external
    external/miniz
)
target_link_libraries(vekp PRIVATE vek_core)

if(WIN32)
    target_link_libraries(vekp PRIVATE winhttp)
else()
    find_package(CURL REQUIRED)
    target_link_libraries(vekp PRIVATE CURL::libcurl)
endif()

# Standalone static linking on MinGW
if(MINGW)
    target_link_options(vek PRIVATE -s -static -static-libgcc -static-libstdc++)
    target_link_options(vekvm PRIVATE -s -static -static-libgcc -static-libstdc++)
    target_link_options(vekp PRIVATE -s -static -static-libgcc -static-libstdc++)
endif()