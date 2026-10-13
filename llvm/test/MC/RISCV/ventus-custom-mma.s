# RUN: llvm-mc -triple=riscv32 -mcpu=ventus-gpgpu -mattr=+v -show-encoding %s | FileCheck %s

mma.m8n8k16.row.row.f32.f16.f16.f32 v8, v4, v12
# CHECK: mma.m8n8k16.row.row.f32.f16.f16.f32 v8, v4, v12
# CHECK: encoding: [0x0a,0x34,0xc2,0x10]

mma.m16n16k16.col.col.f32.bf16.bf16.f32 v9, v5, v13
# CHECK: mma.m16n16k16.col.col.f32.bf16.bf16.f32 v9, v5, v13
# CHECK: encoding: [0x8a,0xd4,0xd2,0x26]
