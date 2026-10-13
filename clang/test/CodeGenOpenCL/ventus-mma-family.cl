// RUN: %clang_cc1 -no-opaque-pointers -triple riscv32-unknown-unknown -target-cpu ventus-gpgpu -cl-std=CL2.0 -fsyntax-only %s

typedef unsigned int uint;
typedef uint uint2 __attribute__((ext_vector_type(2)));
typedef uint uint4 __attribute__((ext_vector_type(4)));
typedef uint uint8 __attribute__((ext_vector_type(8)));
typedef float float2 __attribute__((ext_vector_type(2)));
typedef float float4 __attribute__((ext_vector_type(4)));
typedef float float8 __attribute__((ext_vector_type(8)));

float2 test_m8n8k16_row_row_f32_f16_f16_f32(uint2 a, uint2 b, float2 c) {
  return __builtin_riscv_ventus_mma_m8n8k16_row_row_f32_f16_f16_f32(a, b, c);
}

uint test_m8n8k16_row_row_f16_f16_f16_f16(uint2 a, uint2 b, uint c) {
  return __builtin_riscv_ventus_mma_m8n8k16_row_row_f16_f16_f16_f16(a, b, c);
}

float2 test_m8n8k16_row_row_f32_bf16_bf16_f32(uint2 a, uint2 b, float2 c) {
  return __builtin_riscv_ventus_mma_m8n8k16_row_row_f32_bf16_bf16_f32(a, b, c);
}

float2 test_m8n8k16_row_col_f32_f16_f16_f32(uint2 a, uint2 b, float2 c) {
  return __builtin_riscv_ventus_mma_m8n8k16_row_col_f32_f16_f16_f32(a, b, c);
}

uint test_m8n8k16_row_col_f16_f16_f16_f16(uint2 a, uint2 b, uint c) {
  return __builtin_riscv_ventus_mma_m8n8k16_row_col_f16_f16_f16_f16(a, b, c);
}

float2 test_m8n8k16_row_col_f32_bf16_bf16_f32(uint2 a, uint2 b, float2 c) {
  return __builtin_riscv_ventus_mma_m8n8k16_row_col_f32_bf16_bf16_f32(a, b, c);
}

float2 test_m8n8k16_col_row_f32_f16_f16_f32(uint2 a, uint2 b, float2 c) {
  return __builtin_riscv_ventus_mma_m8n8k16_col_row_f32_f16_f16_f32(a, b, c);
}

uint test_m8n8k16_col_row_f16_f16_f16_f16(uint2 a, uint2 b, uint c) {
  return __builtin_riscv_ventus_mma_m8n8k16_col_row_f16_f16_f16_f16(a, b, c);
}

float2 test_m8n8k16_col_row_f32_bf16_bf16_f32(uint2 a, uint2 b, float2 c) {
  return __builtin_riscv_ventus_mma_m8n8k16_col_row_f32_bf16_bf16_f32(a, b, c);
}

float2 test_m8n8k16_col_col_f32_f16_f16_f32(uint2 a, uint2 b, float2 c) {
  return __builtin_riscv_ventus_mma_m8n8k16_col_col_f32_f16_f16_f32(a, b, c);
}

uint test_m8n8k16_col_col_f16_f16_f16_f16(uint2 a, uint2 b, uint c) {
  return __builtin_riscv_ventus_mma_m8n8k16_col_col_f16_f16_f16_f16(a, b, c);
}

float2 test_m8n8k16_col_col_f32_bf16_bf16_f32(uint2 a, uint2 b, float2 c) {
  return __builtin_riscv_ventus_mma_m8n8k16_col_col_f32_bf16_bf16_f32(a, b, c);
}

float4 test_m16n8k16_row_row_f32_f16_f16_f32(uint4 a, uint2 b, float4 c) {
  return __builtin_riscv_ventus_mma_m16n8k16_row_row_f32_f16_f16_f32(a, b, c);
}

uint2 test_m16n8k16_row_row_f16_f16_f16_f16(uint4 a, uint2 b, uint2 c) {
  return __builtin_riscv_ventus_mma_m16n8k16_row_row_f16_f16_f16_f16(a, b, c);
}

float4 test_m16n8k16_row_row_f32_bf16_bf16_f32(uint4 a, uint2 b, float4 c) {
  return __builtin_riscv_ventus_mma_m16n8k16_row_row_f32_bf16_bf16_f32(a, b, c);
}

float4 test_m16n8k16_row_col_f32_f16_f16_f32(uint4 a, uint2 b, float4 c) {
  return __builtin_riscv_ventus_mma_m16n8k16_row_col_f32_f16_f16_f32(a, b, c);
}

uint2 test_m16n8k16_row_col_f16_f16_f16_f16(uint4 a, uint2 b, uint2 c) {
  return __builtin_riscv_ventus_mma_m16n8k16_row_col_f16_f16_f16_f16(a, b, c);
}

float4 test_m16n8k16_row_col_f32_bf16_bf16_f32(uint4 a, uint2 b, float4 c) {
  return __builtin_riscv_ventus_mma_m16n8k16_row_col_f32_bf16_bf16_f32(a, b, c);
}

float4 test_m16n8k16_col_row_f32_f16_f16_f32(uint4 a, uint2 b, float4 c) {
  return __builtin_riscv_ventus_mma_m16n8k16_col_row_f32_f16_f16_f32(a, b, c);
}

uint2 test_m16n8k16_col_row_f16_f16_f16_f16(uint4 a, uint2 b, uint2 c) {
  return __builtin_riscv_ventus_mma_m16n8k16_col_row_f16_f16_f16_f16(a, b, c);
}

float4 test_m16n8k16_col_row_f32_bf16_bf16_f32(uint4 a, uint2 b, float4 c) {
  return __builtin_riscv_ventus_mma_m16n8k16_col_row_f32_bf16_bf16_f32(a, b, c);
}

float4 test_m16n8k16_col_col_f32_f16_f16_f32(uint4 a, uint2 b, float4 c) {
  return __builtin_riscv_ventus_mma_m16n8k16_col_col_f32_f16_f16_f32(a, b, c);
}

uint2 test_m16n8k16_col_col_f16_f16_f16_f16(uint4 a, uint2 b, uint2 c) {
  return __builtin_riscv_ventus_mma_m16n8k16_col_col_f16_f16_f16_f16(a, b, c);
}

float4 test_m16n8k16_col_col_f32_bf16_bf16_f32(uint4 a, uint2 b, float4 c) {
  return __builtin_riscv_ventus_mma_m16n8k16_col_col_f32_bf16_bf16_f32(a, b, c);
}

float4 test_m8n16k16_row_row_f32_f16_f16_f32(uint2 a, uint4 b, float4 c) {
  return __builtin_riscv_ventus_mma_m8n16k16_row_row_f32_f16_f16_f32(a, b, c);
}

uint2 test_m8n16k16_row_row_f16_f16_f16_f16(uint2 a, uint4 b, uint2 c) {
  return __builtin_riscv_ventus_mma_m8n16k16_row_row_f16_f16_f16_f16(a, b, c);
}

float4 test_m8n16k16_row_row_f32_bf16_bf16_f32(uint2 a, uint4 b, float4 c) {
  return __builtin_riscv_ventus_mma_m8n16k16_row_row_f32_bf16_bf16_f32(a, b, c);
}

float4 test_m8n16k16_row_col_f32_f16_f16_f32(uint2 a, uint4 b, float4 c) {
  return __builtin_riscv_ventus_mma_m8n16k16_row_col_f32_f16_f16_f32(a, b, c);
}

uint2 test_m8n16k16_row_col_f16_f16_f16_f16(uint2 a, uint4 b, uint2 c) {
  return __builtin_riscv_ventus_mma_m8n16k16_row_col_f16_f16_f16_f16(a, b, c);
}

float4 test_m8n16k16_row_col_f32_bf16_bf16_f32(uint2 a, uint4 b, float4 c) {
  return __builtin_riscv_ventus_mma_m8n16k16_row_col_f32_bf16_bf16_f32(a, b, c);
}

float4 test_m8n16k16_col_row_f32_f16_f16_f32(uint2 a, uint4 b, float4 c) {
  return __builtin_riscv_ventus_mma_m8n16k16_col_row_f32_f16_f16_f32(a, b, c);
}

uint2 test_m8n16k16_col_row_f16_f16_f16_f16(uint2 a, uint4 b, uint2 c) {
  return __builtin_riscv_ventus_mma_m8n16k16_col_row_f16_f16_f16_f16(a, b, c);
}

float4 test_m8n16k16_col_row_f32_bf16_bf16_f32(uint2 a, uint4 b, float4 c) {
  return __builtin_riscv_ventus_mma_m8n16k16_col_row_f32_bf16_bf16_f32(a, b, c);
}

float4 test_m8n16k16_col_col_f32_f16_f16_f32(uint2 a, uint4 b, float4 c) {
  return __builtin_riscv_ventus_mma_m8n16k16_col_col_f32_f16_f16_f32(a, b, c);
}

uint2 test_m8n16k16_col_col_f16_f16_f16_f16(uint2 a, uint4 b, uint2 c) {
  return __builtin_riscv_ventus_mma_m8n16k16_col_col_f16_f16_f16_f16(a, b, c);
}

float4 test_m8n16k16_col_col_f32_bf16_bf16_f32(uint2 a, uint4 b, float4 c) {
  return __builtin_riscv_ventus_mma_m8n16k16_col_col_f32_bf16_bf16_f32(a, b, c);
}

float8 test_m16n16k16_row_row_f32_f16_f16_f32(uint4 a, uint4 b, float8 c) {
  return __builtin_riscv_ventus_mma_m16n16k16_row_row_f32_f16_f16_f32(a, b, c);
}

uint4 test_m16n16k16_row_row_f16_f16_f16_f16(uint4 a, uint4 b, uint4 c) {
  return __builtin_riscv_ventus_mma_m16n16k16_row_row_f16_f16_f16_f16(a, b, c);
}

float8 test_m16n16k16_row_row_f32_bf16_bf16_f32(uint4 a, uint4 b, float8 c) {
  return __builtin_riscv_ventus_mma_m16n16k16_row_row_f32_bf16_bf16_f32(a, b, c);
}

float8 test_m16n16k16_row_col_f32_f16_f16_f32(uint4 a, uint4 b, float8 c) {
  return __builtin_riscv_ventus_mma_m16n16k16_row_col_f32_f16_f16_f32(a, b, c);
}

uint4 test_m16n16k16_row_col_f16_f16_f16_f16(uint4 a, uint4 b, uint4 c) {
  return __builtin_riscv_ventus_mma_m16n16k16_row_col_f16_f16_f16_f16(a, b, c);
}

float8 test_m16n16k16_row_col_f32_bf16_bf16_f32(uint4 a, uint4 b, float8 c) {
  return __builtin_riscv_ventus_mma_m16n16k16_row_col_f32_bf16_bf16_f32(a, b, c);
}

float8 test_m16n16k16_col_row_f32_f16_f16_f32(uint4 a, uint4 b, float8 c) {
  return __builtin_riscv_ventus_mma_m16n16k16_col_row_f32_f16_f16_f32(a, b, c);
}

uint4 test_m16n16k16_col_row_f16_f16_f16_f16(uint4 a, uint4 b, uint4 c) {
  return __builtin_riscv_ventus_mma_m16n16k16_col_row_f16_f16_f16_f16(a, b, c);
}

float8 test_m16n16k16_col_row_f32_bf16_bf16_f32(uint4 a, uint4 b, float8 c) {
  return __builtin_riscv_ventus_mma_m16n16k16_col_row_f32_bf16_bf16_f32(a, b, c);
}

float8 test_m16n16k16_col_col_f32_f16_f16_f32(uint4 a, uint4 b, float8 c) {
  return __builtin_riscv_ventus_mma_m16n16k16_col_col_f32_f16_f16_f32(a, b, c);
}

uint4 test_m16n16k16_col_col_f16_f16_f16_f16(uint4 a, uint4 b, uint4 c) {
  return __builtin_riscv_ventus_mma_m16n16k16_col_col_f16_f16_f16_f16(a, b, c);
}

float8 test_m16n16k16_col_col_f32_bf16_bf16_f32(uint4 a, uint4 b, float8 c) {
  return __builtin_riscv_ventus_mma_m16n16k16_col_col_f32_bf16_bf16_f32(a, b, c);
}
