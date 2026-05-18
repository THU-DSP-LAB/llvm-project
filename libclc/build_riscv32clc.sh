#!/usr/bin/bash
set -euo pipefail

LIBCLC_DIR=$1
LIBCLC_BUILD_DIR=$2
BINARY_DIR=$3
DISABLE_PROBLEM_MATH=${VENTUS_LIBCLC_DISABLE_PROBLEM_MATH:-OFF}

BUILTINS_BC=${LIBCLC_BUILD_DIR}/builtins.link.riscv32--.bc
BUILTINS_DIR=${LIBCLC_BUILD_DIR}/CMakeFiles/builtins.link.riscv32--.dir
OUTPUT_OBJ=${LIBCLC_BUILD_DIR}/riscv32clc.o
PROBLEM_MATH_BCS=(
    "generic/lib/math/acosh.bc"
    "generic/lib/math/asinh.bc"
    "generic/lib/math/clc_tan.bc"
    "generic/lib/math/cos.bc"
    "generic/lib/math/sin.bc"
)

is_problem_math_bc() {
    local item=$1
    local rel

    rel=$(realpath --relative-to="${BUILTINS_DIR}" "${item}")
    for problem in "${PROBLEM_MATH_BCS[@]}"; do
        if [ "${rel}" = "${problem}" ]; then
            return 0
        fi
    done
    return 1
}

compile_partial_libclc_object() {
    local object_file_list=()
    local item
    local obj

    if [ ! -d "${BUILTINS_DIR}" ]; then
        echo "missing libclc per-source bitcode directory: ${BUILTINS_DIR}" >&2
        exit 1
    fi

    echo "WARNING: VENTUS_LIBCLC_DISABLE_PROBLEM_MATH=ON is a temporary debug-only build mode." >&2
    echo "WARNING: generated riscv32clc.o is incomplete; skipped acosh/asinh/clc_tan/cos/sin inputs." >&2

    while IFS= read -r item; do
        if is_problem_math_bc "${item}"; then
            echo "skipping problematic libclc bitcode: ${item}" >&2
            continue
        fi

        obj="${item}.o"
        "${BINARY_DIR}/bin/clang" -target riscv32 -mcpu=ventus-gpgpu \
            -cl-std=CL2.0 \
            -Wno-override-module \
            -ffunction-sections -fdata-sections \
            -c "${item}" \
            -o "${obj}"
        object_file_list+=("${obj}")
    done < <(find "${BUILTINS_DIR}" -name "*.bc" | sort)

    "${BINARY_DIR}/bin/ld.lld" --relocatable "${object_file_list[@]}" \
        --allow-multiple-definition \
        -o "${OUTPUT_OBJ}"
}

if [ ! -f "${BUILTINS_BC}" ]; then
    echo "missing linked libclc bitcode: ${BUILTINS_BC}" >&2
    exit 1
fi

if [ "${DISABLE_PROBLEM_MATH}" = "ON" ]; then
    compile_partial_libclc_object
elif [ "${DISABLE_PROBLEM_MATH}" = "OFF" ]; then
    "${BINARY_DIR}/bin/clang" -target riscv32 -mcpu=ventus-gpgpu \
        -cl-std=CL2.0 \
        -Wno-override-module \
        -ffunction-sections -fdata-sections \
        -c "${BUILTINS_BC}" \
        -o "${OUTPUT_OBJ}"
else
    echo "unsupported VENTUS_LIBCLC_DISABLE_PROBLEM_MATH: ${DISABLE_PROBLEM_MATH}" >&2
    exit 1
fi

cp "${OUTPUT_OBJ}" "${BINARY_DIR}/lib/riscv32clc.o"
