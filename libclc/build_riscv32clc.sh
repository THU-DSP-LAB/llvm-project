#!/usr/bin/bash

# Enhanced script to build libclc as bitcode library
# Usage: ./build_riscv32clc.sh <LIBCLC_DIR> <LIBCLC_BUILD_DIR> <BINARY_DIR>

set -e  # Exit on any error

LIBCLC_DIR=$1
LIBCLC_BUILD_DIR=$2
BINARY_DIR=$3

# Validate input parameters
if [ -z "$LIBCLC_DIR" ] || [ -z "$LIBCLC_BUILD_DIR" ] || [ -z "$BINARY_DIR" ]; then
    echo "Usage: $0 <LIBCLC_DIR> <LIBCLC_BUILD_DIR> <BINARY_DIR>"
    echo "Example: $0 /path/to/libclc /path/to/build /path/to/llvm/bin"
    exit 1
fi

# Check if required tools exist
if [ ! -f "${BINARY_DIR}/bin/llvm-link" ]; then
    echo "Error: llvm-link not found at ${BINARY_DIR}/bin/llvm-link"
    exit 1
fi

if [ ! -f "${BINARY_DIR}/bin/opt" ]; then
    echo "Error: opt not found at ${BINARY_DIR}/bin/opt"
    exit 1
fi

echo "Building libclc bitcode library..."
echo "LIBCLC_DIR: $LIBCLC_DIR"
echo "LIBCLC_BUILD_DIR: $LIBCLC_BUILD_DIR"
echo "BINARY_DIR: $BINARY_DIR"

# Create output directory if it doesn't exist
mkdir -p "${BINARY_DIR}/lib"

# Look for the optimized bitcode file from CMake build
echo "Searching for optimized bitcode files..."
opt_bc_file=$(find ${LIBCLC_BUILD_DIR} -name "builtins.opt.riscv32--.bc" -type f | head -1)
if [ -n "$opt_bc_file" ] && [ -f "$opt_bc_file" ]; then
    echo "  Found optimized bitcode file: $opt_bc_file"
    # Use the optimized file directly
    cp "$opt_bc_file" ${LIBCLC_BUILD_DIR}/riscv32clc_opt.bc
    echo "Using pre-optimized bitcode file"
else
    # Look for the main linked bitcode file
    main_bc_file=$(find ${LIBCLC_BUILD_DIR} -name "builtins.link.riscv32--.bc" -type f | head -1)
    if [ -n "$main_bc_file" ] && [ -f "$main_bc_file" ]; then
        echo "  Found main bitcode file: $main_bc_file"
        # Optimize the main file
        echo "Optimizing bitcode library..."
        ${BINARY_DIR}/bin/opt -O3 "$main_bc_file" \
            -o ${LIBCLC_BUILD_DIR}/riscv32clc_opt.bc
    else
        echo "Error: No suitable bitcode files found in ${LIBCLC_BUILD_DIR}"
        echo "Please ensure libclc has been built with CMake first"
        exit 1
    fi
fi

# Copy the optimized bitcode library to the binary directory
echo "Installing bitcode library..."
cp ${LIBCLC_BUILD_DIR}/riscv32clc_opt.bc ${BINARY_DIR}/lib/riscv32clc.bc

# Verify the output
if [ -f "${BINARY_DIR}/lib/riscv32clc.bc" ]; then
    echo "Successfully created bitcode library: ${BINARY_DIR}/lib/riscv32clc.bc"
    ls -lh "${BINARY_DIR}/lib/riscv32clc.bc"
else
    echo "Error: Failed to create bitcode library"
    exit 1
fi

echo "libclc bitcode library build completed successfully!"
