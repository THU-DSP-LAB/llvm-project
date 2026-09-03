; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -verify-machineinstrs < %s | FileCheck %s

declare i32 @llvm.riscv.ventus.rt.traverse.i32(i32)
declare void @llvm.riscv.ventus.rt.release.i32(i32)
declare void @llvm.riscv.ventus.rt.enqueue.i32(i32)
declare i32 @llvm.riscv.ventus.rt.local.load.i32(i32)
declare void @llvm.riscv.ventus.rt.local.store.i32(i32, i32)
declare i32 @llvm.riscv.ventus.rt.worker.id()
declare i32 @llvm.riscv.ventus.rt.warp.first.tid()
declare i32 @llvm.riscv.ventus.kernel.metadata()

define i32 @test_kernel_metadata() {
; CHECK-LABEL: test_kernel_metadata:
; CHECK: csrr {{[a-z0-9]+}}, CSR_KNL
  %metadata = call i32 @llvm.riscv.ventus.kernel.metadata()
  ret i32 %metadata
}

define i32 @test_rt_warp_first_tid() {
; CHECK-LABEL: test_rt_warp_first_tid:
; CHECK: csrr.v v{{[0-9]+}}, CSR_TID
  %tid = call i32 @llvm.riscv.ventus.rt.warp.first.tid()
  ret i32 %tid
}

define i32 @test_rt_traverse(i32 %warp_first_tid) {
; CHECK-LABEL: test_rt_traverse:
; CHECK: vt.rt.traverse v{{[0-9]+}}, v{{[0-9]+}}
  %status = call i32 @llvm.riscv.ventus.rt.traverse.i32(i32 %warp_first_tid)
  ret i32 %status
}

define i32 @test_rt_traverse_with_first_tid() {
; CHECK-LABEL: test_rt_traverse_with_first_tid:
; CHECK: csrr.v v[[FIRST_TID:[0-9]+]], CSR_TID
; CHECK: vt.rt.traverse v{{[0-9]+}}, v[[FIRST_TID]]
  %warp_first_tid = call i32 @llvm.riscv.ventus.rt.warp.first.tid()
  %status = call i32 @llvm.riscv.ventus.rt.traverse.i32(i32 %warp_first_tid)
  ret i32 %status
}

define void @test_rt_release(i32 %warp_first_tid) {
; CHECK-LABEL: test_rt_release:
; CHECK: vt.rt.release v{{[0-9]+}}
  call void @llvm.riscv.ventus.rt.release.i32(i32 %warp_first_tid)
  ret void
}

define void @test_rt_enqueue(i32 %mailbox) {
; CHECK-LABEL: test_rt_enqueue:
; CHECK: vt.rt.enqueue v{{[0-9]+}}
  call void @llvm.riscv.ventus.rt.enqueue.i32(i32 %mailbox)
  ret void
}

define i32 @test_rt_local_load() {
; CHECK-LABEL: test_rt_local_load:
; CHECK: vlrt.w v{{[0-9]+}}, 36
  %value = call i32 @llvm.riscv.ventus.rt.local.load.i32(i32 36)
  ret i32 %value
}

define void @test_rt_local_store(i32 %value) {
; CHECK-LABEL: test_rt_local_store:
; CHECK: vsrt.w v{{[0-9]+}}, 36
  call void @llvm.riscv.ventus.rt.local.store.i32(i32 %value, i32 36)
  ret void
}

define i32 @test_rt_worker_id() {
; CHECK-LABEL: test_rt_worker_id:
; CHECK: csrr.v v{{[0-9]+}}, mhartid
  %worker = call i32 @llvm.riscv.ventus.rt.worker.id()
  ret i32 %worker
}

define void @test_rt_order(ptr addrspace(3) %scratch, i32 %slot, i32 %value) {
; CHECK-LABEL: test_rt_order:
; CHECK: vsw{{[0-9]*}}.v
; CHECK-NEXT: vt.rt.traverse
; CHECK: vsw{{[0-9]*}}.v
; CHECK-NEXT: vt.rt.release
  store i32 %value, ptr addrspace(3) %scratch, align 4
  %status = call i32 @llvm.riscv.ventus.rt.traverse.i32(i32 %slot)
  store i32 %status, ptr addrspace(3) %scratch, align 4
  call void @llvm.riscv.ventus.rt.release.i32(i32 %slot)
  ret void
}
