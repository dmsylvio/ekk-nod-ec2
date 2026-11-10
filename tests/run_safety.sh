#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
build_dir=$(mktemp -d /tmp/ekk-safety.XXXXXX)
trap 'rm -rf "$build_dir"' EXIT
for unit in memory_safety parser_regressions cs2_safety math_regressions cs2_feature_safety linux_io_safety ksj_socket_safety; do
    g++ -std=c++11 -pthread -g -O1 -fsanitize=address,undefined \
        -fno-omit-frame-pointer -ffunction-sections -Wl,--gc-sections \
        "tests/$unit.cpp" -o "$build_dir/$unit"
    "$build_dir/$unit"
done
g++ -std=c++11 -D_KERNEL_MODE -fsanitize=address,undefined \
    tests/math_regressions.cpp -o "$build_dir/math_kernel"
"$build_dir/math_kernel"
python3 tests/kernel_copy_contract.py
python3 tests/um_process_enumeration.py
for source in cs2/shared/cs2.cpp cs2/shared/cs2features.cpp apex/shared/apexfeatures.cpp \
    library/vm/linux/vm.cpp library/vm/proton/vm.cpp library/vm/ksj/src/vm.cpp; do
    g++ -std=c++11 -fsyntax-only "$source"
done
printf '%s\n' 'Safety regression suite passed.'
