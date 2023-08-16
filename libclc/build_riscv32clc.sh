#!/usr/bin/bash

LIBCLC_DIR=$1
LIBCLC_BUILD_DIR=$2
BINARY_DIR=$3

# Collect all the object files in build directory
OBJECT_FILE_LIST=""
for item in $(find ${LIBCLC_BUILD_DIR} -name "*.bc.o" -o -name "*.c.o")
do
    OBJECT_FILE_LIST="${OBJECT_FILE_LIST} ${item}"
done

# Compile other left IR files
for item in $(ls ${LIBCLC_DIR}/generic/lib | grep  ll)
do
    ${BINARY_DIR}/bin/clang -target riscv32 -mcpu=ventus-gpgpu \
        -cl-std=CL2.0 \
        -Wno-override-module \
        -ffunction-sections -fdata-sections \
        -c ${LIBCLC_DIR}/generic/lib/${item} \
        -o ${LIBCLC_BUILD_DIR}/${item}.o
    OBJECT_FILE_LIST="${OBJECT_FILE_LIST} ${LIBCLC_BUILD_DIR}/${item}.o"
done

${BINARY_DIR}/bin/ld.lld --relocatable ${OBJECT_FILE_LIST} \
            --allow-multiple-definition \
             -o ${LIBCLC_BUILD_DIR}/riscv32clc.o
cp ${LIBCLC_BUILD_DIR}/riscv32clc.o	${BINARY_DIR}/lib
