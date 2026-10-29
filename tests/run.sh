#!/usr/bin/env bash
# Host-native unit test for the cache writer. Does not need Wine or SpeedTreeRT.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
tool_dir="$(cd "${here}/.." && pwd)"
out_dir="${BUILD_DIR:-${tool_dir}/build}"
cxx="${CXX:-c++}"

mkdir -p "${out_dir}"
"${cxx}" -std=c++17 -O2 -Wall -Wextra \
    -I"${tool_dir}/include" \
    -I"${tool_dir}/../../src/SpeedTreeLib/include" \
    "${here}/format_test.cpp" \
    "${tool_dir}/src/tree_cache_writer.cpp" \
    -o "${out_dir}/format_test"

"${out_dir}/format_test"
