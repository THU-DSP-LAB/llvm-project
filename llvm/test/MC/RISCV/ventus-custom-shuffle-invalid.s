# RUN: not llvm-mc -triple=riscv32 -mcpu=ventus-gpgpu -mattr=+v %s 2>&1 | FileCheck %s

shuffle.idx v8, v4, 3, v0.t
# CHECK: error: invalid operand for instruction
