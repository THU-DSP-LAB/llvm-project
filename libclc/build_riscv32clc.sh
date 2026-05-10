#!/usr/bin/bash

LIBCLC_DIR=$1
LIBCLC_BUILD_DIR=$2
BINARY_DIR=$3

BUILTINS_BC=${LIBCLC_BUILD_DIR}/builtins.link.riscv32--.bc
OUTPUT_OBJ=${LIBCLC_BUILD_DIR}/riscv32clc.o

if [ ! -f "${BUILTINS_BC}" ]; then
    echo "missing linked libclc bitcode: ${BUILTINS_BC}" >&2
    exit 1
fi

${BINARY_DIR}/bin/clang -target riscv32 -mcpu=ventus-gpgpu \
    -cl-std=CL2.0 \
    -Wno-override-module \
    -ffunction-sections -fdata-sections \
    -c "${BUILTINS_BC}" \
    -o "${OUTPUT_OBJ}"

cp "${OUTPUT_OBJ}"	"${BINARY_DIR}/lib/riscv32clc.o"
