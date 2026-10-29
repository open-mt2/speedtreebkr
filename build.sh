#!/usr/bin/env bash
# Build the 32-bit Windows SpeedTree geometry dumper.
#
# The output is build/speedtree_dumper.exe, a PE32 executable with no CRT that
# calls the original SpeedTreeRT.dll through its exported C++ symbols. It runs
# under Wine (see bake.py) or natively on 32-bit-capable Windows.
#
# Usage: ./build.sh [--clean] [--test]
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
build_dir="${BUILD_DIR:-${here}/build}"
generator="${CMAKE_GENERATOR:-Ninja}"
wine_libs="${WINE_LIBS:-/usr/lib/wine/i386-windows}"

clean=0
run_tests=0
for argument in "$@"; do
    case "${argument}" in
        --clean) clean=1 ;;
        --test) run_tests=1 ;;
        -h|--help)
            echo "usage: $(basename "$0") [--clean] [--test]"
            exit 0
            ;;
        *)
            echo "unknown option: ${argument}" >&2
            exit 2
            ;;
    esac
done

if ((clean)); then
    rm -rf "${build_dir}"
fi

cmake -S "${here}" -B "${build_dir}" \
    -G "${generator}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="${here}/cmake/i686-w64-windows-gnu.cmake" \
    -DWINE_LIBS="${wine_libs}"

cmake --build "${build_dir}" --parallel "$(nproc 2>/dev/null || echo 4)"

echo "built ${build_dir}/speedtree_dumper.exe"

if ((run_tests)); then
    BUILD_DIR="${build_dir}" "${here}/tests/run.sh"
fi
