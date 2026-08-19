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

define dso_local ventus_kernel void @private_frame_offset_neg8004(i32 %x) #0 {
; CHECK-LABEL: private_frame_offset_neg8004:
; CHECK:       sub [[LARGE_SREG:[a-z0-9]+]], tp, [[LARGE_SREG]]
; CHECK-NEXT:  vmv.v.x [[LARGE_VBASE:v[0-9]+]], [[LARGE_SREG]]
; CHECK-NEXT:  vsw.v {{v[0-9]+}}, {{[0-9]+}}([[LARGE_VBASE]])
entry:
  %buf = alloca [2000 x i32], align 4, addrspace(5)
  store volatile i32 %x, ptr addrspace(5) %buf, align 4
  call void @escape(ptr addrspace(5) %buf)
  ret void
}

; A megakernel RT ABI prefix is below the private stack.  The stack grows TP
; upward before using negative frame offsets, so the 12-byte AS5 frame starts
; at logical PDS offset 308 rather than overlapping the reserved prefix.
define dso_local ventus_kernel void @rt_private_prefix(i32 %x) #1 {
; CHECK-LABEL: rt_private_prefix:
; CHECK:       addi tp, tp, 320
; CHECK:       vsw.v {{v[0-9]+}}, -12(v32)
entry:
  %slot = alloca i32, align 4, addrspace(5)
  store volatile i32 %x, ptr addrspace(5) %slot, align 4
  call void @escape(ptr addrspace(5) %slot)
  ret void
}

declare void @escape(ptr addrspace(5))
declare void @llvm.lifetime.start.p5(i64 immarg, ptr addrspace(5) nocapture)
declare void @llvm.lifetime.end.p5(i64 immarg, ptr addrspace(5) nocapture)

attributes #0 = { "target-cpu"="ventus-gpgpu" }
attributes #1 = { "target-cpu"="ventus-gpgpu" "ventus-rt-private-prefix"="308" }
