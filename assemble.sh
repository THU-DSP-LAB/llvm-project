#!/usr/bin/bash

# This script will be used by pocl

echo "converting object file:"
echo $1
script_dir=$(cd "$(dirname "$0")" && pwd)
export PATH=$script_dir/install/bin:$PATH
llvm-objdump -d --mattr=+v $1.riscv > $1.dump
llvm-objcopy -O binary -j .text $1.riscv $1.temp
hexdump -e '1/4 "%08x" "\n"' $1.temp > $1.vmem
echo "finish converting to vmem file!"
