// RUN: %clang_cc1 -no-opaque-pointers -triple riscv32-unknown-unknown -target-cpu ventus-gpgpu -cl-std=CL2.0 -S -emit-llvm -o - %s | FileCheck %s

typedef unsigned int uint;

uint test_shuffle_idx(uint a0) {
  // CHECK-LABEL: @test_shuffle_idx
  // CHECK: call i32 @llvm.riscv.ventus.shuffle.idx.i32(i32 %{{.*}}, i32 3)
  return __builtin_riscv_ventus_shuffle_idx_i32(a0, 3);
}

uint test_shuffle_up(uint a0) {
  // CHECK-LABEL: @test_shuffle_up
  // CHECK: call i32 @llvm.riscv.ventus.shuffle.up.i32(i32 %{{.*}}, i32 1)
  return __builtin_riscv_ventus_shuffle_up_i32(a0, 1);
}

uint test_shuffle_down(uint a0) {
  // CHECK-LABEL: @test_shuffle_down
  // CHECK: call i32 @llvm.riscv.ventus.shuffle.down.i32(i32 %{{.*}}, i32 2)
  return __builtin_riscv_ventus_shuffle_down_i32(a0, 2);
}

uint test_shuffle_bfly(uint a0) {
  // CHECK-LABEL: @test_shuffle_bfly
  // CHECK: call i32 @llvm.riscv.ventus.shuffle.bfly.i32(i32 %{{.*}}, i32 4)
  return __builtin_riscv_ventus_shuffle_bfly_i32(a0, 4);
}
