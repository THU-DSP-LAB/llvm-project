# RUN: llvm-mc -triple=riscv32 -mcpu=ventus-gpgpu -mattr=+v -show-encoding %s | FileCheck %s

vt.rt.traverse v8, v4
# CHECK: vt.rt.traverse v8, v4
# CHECK: encoding:

vt.rt.release v4
# CHECK: vt.rt.release v4
# CHECK: encoding:
