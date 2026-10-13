# RUN: llvm-mc -triple=riscv32 -mcpu=ventus-gpgpu -mattr=+v -show-encoding %s | FileCheck %s

shuffle.idx v8, v4, 3
# CHECK: shuffle.idx v8, v4, 3
# CHECK: encoding: [0x42,0x94,0x41,0x26]
