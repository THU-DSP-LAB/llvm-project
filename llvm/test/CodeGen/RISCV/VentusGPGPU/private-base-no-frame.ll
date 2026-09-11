; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -verify-machineinstrs < %s \
; RUN:   | FileCheck %s

target datalayout = "e-m:e-p:32:32-i64:64-n32-S128-A5-G1"
target triple = "riscv32"

; Constant AS5 accesses use the reserved vector base even without an alloca
; or spill frame. It must be initialized from the runtime's TP first.
define dso_local ventus_kernel void @private_base_no_frame(i32 %value) {
; CHECK-LABEL: private_base_no_frame:
; CHECK:       vmv.v.x v32, tp
; CHECK:       vsw.v {{v[0-9]+}}, 64(v32)
entry:
  store volatile i32 %value, ptr addrspace(5) inttoptr (i32 64 to ptr addrspace(5)), align 4
  ret void
}
