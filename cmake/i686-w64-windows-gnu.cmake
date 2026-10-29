# Toolchain for the 32-bit, no-CRT Windows SpeedTree geometry dumper.
#
# The dumper must be a PE32/i386 executable because the original SpeedTreeRT.dll
# is 32-bit. It is built with clang + lld and imports only kernel32; there is no
# dependency on the C/C++ runtime or on a MinGW sysroot.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR i686)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_C_COMPILER_TARGET i686-w64-windows-gnu)
set(CMAKE_CXX_COMPILER_TARGET i686-w64-windows-gnu)

# Compiler checks cannot link (no CRT yet).
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# CMake otherwise injects the MinGW default import libraries, which are not
# installed in the Wine-only build environment.
set(CMAKE_C_STANDARD_LIBRARIES "" CACHE STRING "" FORCE)
set(CMAKE_CXX_STANDARD_LIBRARIES "" CACHE STRING "" FORCE)
