// RUN: %clang_cc1 -no-opaque-pointers -triple riscv32-unknown-unknown -target-cpu ventus-gpgpu -cl-std=CL2.0 -S -emit-llvm -o - %s | FileCheck %s

typedef unsigned int uint;
typedef uint uint2 __attribute__((ext_vector_type(2)));
typedef uint uint4 __attribute__((ext_vector_type(4)));
typedef float float4 __attribute__((ext_vector_type(4)));

float4 test(uint4 a, uint2 b, float4 c) {
  // CHECK-LABEL: @test
  // CHECK: extractelement <4 x i32> %{{.*}}, i64 0
  // CHECK: call { i32, i32, i32, i32 } @llvm.riscv.ventus.mma.m16n8k16.row.col.f32.f16.f16.f32
  // CHECK: extractvalue { i32, i32, i32, i32 } %{{.*}}, 0
  return __builtin_riscv_ventus_mma_m16n8k16_row_col_f32_f16_f16_f32(a, b, c);
}
