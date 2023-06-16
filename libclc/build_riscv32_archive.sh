#!/usr/bin/bash

BIN_DIR=$1
BITCODE_FILE=$2
BUILD_DIR=$3
ARCHIVE_FILE_PREFIX=${BUILD_DIR}/riscv32--
COMPILER_FLAGS="-target riscv32  -mcpu=ventus-gpgpu"
LLC_FLAGS="-mtriple=riscv32  -mcpu=ventus-gpgpu"

if [ ! -d "${BIN_DIR}" ]; then
	echo "ERROR: LLVM binary directory does not exist!"
	exit 1
fi

if [ ! -f "${BITCODE_FILE}" ]; then
	echo "ERROR: Not a file: ${BITCODE_FILE}"
	exit 1
fi

LLVM_DIS=${BIN_DIR}/llvm-dis
LLVM_LLC=${BIN_DIR}/llc
LLVM_CLANG=${BIN_DIR}/clang
LLVM_AR=${BIN_DIR}/llvm-ar

${LLVM_DIS} ${BITCODE_FILE} -o ${ARCHIVE_FILE_PREFIX}.ll
${LLVM_LLC} ${LLC_FLAGS} ${ARCHIVE_FILE_PREFIX}.ll -o ${ARCHIVE_FILE_PREFIX}.s
${LLVM_CLANG} ${COMPILER_FLAGS} -c ${ARCHIVE_FILE_PREFIX}.s -o ${ARCHIVE_FILE_PREFIX}.o
${LLVM_AR} rcs ${ARCHIVE_FILE_PREFIX}.a ${ARCHIVE_FILE_PREFIX}.o


# LIB_DIR= $(cd "$(dirname "${BITCODE_FILE}")"| pwd)
# echo "*******"
# echo $LIB_DIR
# echo "*******"
# mv ${ARCHIVE_FILE_PREFIX}.a ${LIB_DIR}/lib