# RUN: llvm-mc -triple=riscv32 -mcpu=ventus-gpgpu -mattr=+v -show-encoding %s | FileCheck %s

vt.rt.traverse v8, v4
# CHECK: vt.rt.traverse v8, v4
# CHECK: encoding: [0x0a,0x04,0x40,0xe2]

vt.rt.traverse v8, v4, v6
# CHECK: vt.rt.traverse v8, v4, v6
# CHECK: encoding: [0x0a,0x24,0x43,0xe2]

vt.rt.release v4
# CHECK: vt.rt.release v4
# CHECK: encoding: [0x0a,0x10,0x40,0xe2]
