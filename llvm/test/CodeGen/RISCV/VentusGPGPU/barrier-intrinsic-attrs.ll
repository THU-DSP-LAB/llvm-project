; RUN: llvm-as < %s | llvm-dis | FileCheck %s

; CHECK: Function Attrs: convergent nounwind
; CHECK-NEXT: declare void @llvm.riscv.ventus.barrier(i32 immarg)
; CHECK: Function Attrs: convergent nounwind
; CHECK-NEXT: declare void @llvm.riscv.ventus.barrier.with.scope(i32 immarg, i32 immarg)
; CHECK: Function Attrs: convergent nounwind
; CHECK-NEXT: declare void @llvm.riscv.ventus.subgroup.barrier(i32 immarg)
; CHECK: Function Attrs: convergent nounwind
; CHECK-NEXT: declare void @llvm.riscv.ventus.subgroup.barrier.with.scope(i32 immarg, i32 immarg)

declare void @llvm.riscv.ventus.barrier(i32 immarg)
declare void @llvm.riscv.ventus.barrier.with.scope(i32 immarg, i32 immarg)
declare void @llvm.riscv.ventus.subgroup.barrier(i32 immarg)
declare void @llvm.riscv.ventus.subgroup.barrier.with.scope(i32 immarg, i32 immarg)
