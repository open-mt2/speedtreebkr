cmake_minimum_required(VERSION 3.20)
project(SpeedTreeBaker VERSION 1.0.0 LANGUAGES CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows" OR
   NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(i[3-6]86|x86)$")
    message(FATAL_ERROR
        "speedtree_baker targets 32-bit Windows (SpeedTreeRT.dll is PE32).\n"
        "Configure with: cmake -B build -G Ninja "
        "-DCMAKE_TOOLCHAIN_FILE=cmake/i686-w64-windows-gnu.cmake")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SPEEDTREELIB_INCLUDE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../../src/SpeedTreeLib/include"
    CACHE PATH "Directory containing SpeedTreeLib/TreeCacheFormat.h")
if(NOT EXISTS "${SPEEDTREELIB_INCLUDE_DIR}/SpeedTreeLib/TreeCacheFormat.h")
    message(FATAL_ERROR
        "TreeCacheFormat.h not found under SPEEDTREELIB_INCLUDE_DIR.\n"
        "Set -DSPEEDTREELIB_INCLUDE_DIR=<client>/src/SpeedTreeLib/include")
endif()

# The dumper imports kernel32 only. Wine ships an import library that ld.lld
# can consume without a MinGW sysroot.
set(WINE_LIBS "/usr/lib/wine/i386-windows"
    CACHE PATH "Directory containing Wine's i386 Windows import libraries")
find_library(KERNEL32_LIBRARY
    NAMES kernel32 libkernel32.a
    HINTS "${WINE_LIBS}"
    DOC "Wine's 32-bit kernel32 import library")
if(NOT KERNEL32_LIBRARY)
    message(FATAL_ERROR
        "kernel32 import library not found. Install Wine's i386 support or set "
        "-DWINE_LIBS=<dir> (expected libkernel32.a).")
endif()

set(SPEEDTREE_BAKER_COMPILE_OPTIONS
    -fno-exceptions
    -fno-rtti
    -fno-builtin
    -fdeclspec
    -Wall
    -Wextra)

add_library(speedtree_baker_core STATIC
    src/dumper.cpp
    src/platform.cpp
    src/speedtree_api.cpp
    src/tree_cache_writer.cpp)

target_include_directories(speedtree_baker_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${SPEEDTREELIB_INCLUDE_DIR}")

target_compile_options(speedtree_baker_core PRIVATE ${SPEEDTREE_BAKER_COMPILE_OPTIONS})
target_link_libraries(speedtree_baker_core INTERFACE "${KERNEL32_LIBRARY}")

add_executable(speedtree_dumper src/main.cpp)
target_link_libraries(speedtree_dumper PRIVATE speedtree_baker_core)
target_compile_options(speedtree_dumper PRIVATE ${SPEEDTREE_BAKER_COMPILE_OPTIONS})
target_link_options(speedtree_dumper PRIVATE
    -nostdlib
    -Wl,--entry,_mainCRTStartup@0
    -Wl,--subsystem,console)
set_target_properties(speedtree_dumper PROPERTIES
    OUTPUT_NAME speedtree_dumper
    SUFFIX ".exe")

install(TARGETS speedtree_dumper RUNTIME DESTINATION bin)
