// RUN: %clang_cc1 -triple riscv32 -target-feature +zbb -target-feature +zbc -target-feature +zbkx -target-feature +zkne -target-feature +zknh -emit-llvm %s -o - | FileCheck %s --check-prefixes=CHECK,RV32
// RUN: %clang_cc1 -triple riscv64 -target-feature +zbb -target-feature +zbc -target-feature +zbkx -target-feature +zkne -target-feature +zknh -emit-llvm %s -o - | FileCheck %s --check-prefixes=CHECK,RV64

// CHECK-LABEL: @orc_b_32(
// CHECK: call i32 @llvm.riscv.orc.b.i32(
unsigned orc_b_32(unsigned a) {
  return __builtin_riscv_orc_b_32(a);
}

// CHECK-LABEL: @clz_32(
// CHECK: call i32 @llvm.ctlz.i32(
unsigned clz_32(unsigned a) {
  return __builtin_riscv_clz_32(a);
}

// CHECK-LABEL: @ctz_32(
// CHECK: call i32 @llvm.cttz.i32(
unsigned ctz_32(unsigned a) {
  return __builtin_riscv_ctz_32(a);
}

// CHECK-LABEL: @clmul_builtin(
// RV32: call i32 @llvm.riscv.clmul.i32(
// RV64: call i64 @llvm.riscv.clmul.i64(
unsigned long clmul_builtin(unsigned long a, unsigned long b) {
  return __builtin_riscv_clmul(a, b);
}

// CHECK-LABEL: @xperm8_builtin(
// RV32: call i32 @llvm.riscv.xperm8.i32(
// RV64: call i64 @llvm.riscv.xperm8.i64(
unsigned long xperm8_builtin(unsigned long a, unsigned long b) {
  return __builtin_riscv_xperm8(a, b);
}

#if __riscv_xlen == 32
// RV32-LABEL: @aes32esi_builtin(
// RV32: call i32 @llvm.riscv.aes32esi(
unsigned aes32esi_builtin(unsigned a, unsigned b) {
  return __builtin_riscv_aes32esi_32(a, b, 3);
}
#endif

// CHECK-LABEL: @sha256sig0_builtin(
// RV32: call i32 @llvm.riscv.sha256sig0.i32(
// RV64: call i64 @llvm.riscv.sha256sig0.i64(
unsigned sha256sig0_builtin(unsigned a) {
  return __builtin_riscv_sha256sig0(a);
}

#if __riscv_xlen == 64
// RV64-LABEL: @orc_b_64(
// RV64: call i64 @llvm.riscv.orc.b.i64(
unsigned long orc_b_64(unsigned long a) {
  return __builtin_riscv_orc_b_64(a);
}

// RV64-LABEL: @aes64es_builtin(
// RV64: call i64 @llvm.riscv.aes64es(
unsigned long aes64es_builtin(unsigned long a, unsigned long b) {
  return __builtin_riscv_aes64es_64(a, b);
}

// RV64-LABEL: @sha512sig0_builtin(
// RV64: call i64 @llvm.riscv.sha512sig0(
unsigned long sha512sig0_builtin(unsigned long a) {
  return __builtin_riscv_sha512sig0_64(a);
}
#endif
