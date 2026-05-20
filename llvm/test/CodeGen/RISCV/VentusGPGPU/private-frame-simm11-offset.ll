; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -verify-machineinstrs < %s \
; RUN:   | FileCheck %s

target datalayout = "e-m:e-p:32:32-i64:64-n32-S128-A5-G1"
target triple = "riscv32"

define dso_local ventus_kernel void @private_frame_offset_neg1028(i32 %x) #0 {
; CHECK-LABEL: private_frame_offset_neg1028:
; CHECK:       vsub12.vi {{v[0-9]+}}, v32, 1024
; CHECK-NEXT:  vsw.v {{v[0-9]+}}, -12({{v[0-9]+}})
entry:
  %buf = alloca [257 x i32], align 4, addrspace(5)
  call void @llvm.lifetime.start.p5(i64 1028, ptr addrspace(5) %buf)
  store volatile i32 %x, ptr addrspace(5) %buf, align 4
  call void @escape(ptr addrspace(5) %buf)
  call void @llvm.lifetime.end.p5(i64 1028, ptr addrspace(5) %buf)
  ret void
}

declare void @escape(ptr addrspace(5))
declare void @llvm.lifetime.start.p5(i64 immarg, ptr addrspace(5) nocapture)
declare void @llvm.lifetime.end.p5(i64 immarg, ptr addrspace(5) nocapture)

attributes #0 = { "target-cpu"="ventus-gpgpu" }
