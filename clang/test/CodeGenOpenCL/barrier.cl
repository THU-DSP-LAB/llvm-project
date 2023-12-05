// RUN: clang -no-opaque-pointers -triple riscv32-unknown-unknown -S -emit-llvm -o - %s | FileCheck %s

void test() {
    // CHECK: call void @llvm.riscv.ventus.barrier(i32 1)
    // CHECK-NEXT: call void @llvm.riscv.ventus.barrier.with.scope(i32 1, i32 2)
    // CHECK-NEXT: call void @llvm.riscv.ventus.barrier(i32 1)
    // CHECK-NEXT: call void @llvm.riscv.ventus.barrier.with.scope(i32 1, i32 2)
    barrier(1);
    barrier(1, 2);
    work_group_barrier(1);
    work_group_barrier(1, 2);
}