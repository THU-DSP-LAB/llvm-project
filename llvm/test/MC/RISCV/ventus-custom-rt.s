# RUN: llvm-mc -triple=riscv32 -mcpu=ventus-gpgpu -mattr=+v -show-encoding %s | FileCheck %s

vt.rt.traverse v8, v4
# CHECK: vt.rt.traverse v8, v4
# CHECK: encoding:

vt.rt.release v4
# CHECK: vt.rt.release v4
# CHECK: encoding:

vt.rt.enqueue v2
# CHECK: vt.rt.enqueue v2
# CHECK: encoding: [0x0a,0x20,0x20,0xe2]

vlrt.w v8, 36
# CHECK: vlrt.w v8, 36
# CHECK: encoding: [0x0a,0x34,0x00,0x92]

vsrt.w v4, 36
# CHECK: vsrt.w v4, 36
# CHECK: encoding: [0x0a,0x40,0x40,0x92]
