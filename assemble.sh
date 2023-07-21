#!/usr/bin/bash

# This script will be used by pocl

echo "converting object file:"
echo $1
script_dir=$(cd "$(dirname "$0")" && pwd)
export PATH=$script_dir/install/bin:$PATH
llvm-objdump -d --mattr=+v $1.riscv > $1.dump
awk 'function is_hex(s) { return match(s, /^[0-9a-fA-F]+$/) } /^ *[0-9a-fA-F]+:/ { for (i = 5; i >= 2; i--) { if (is_hex($i)) printf "%s", $i } print "" }' $1.dump > $1.vmem
echo "finish converting to vmem file!"
