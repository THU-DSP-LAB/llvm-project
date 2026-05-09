; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -stop-after=ventus-sgpr-keepalive < %s -o - | FileCheck %s --check-prefix=DISABLED
; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -ventus-use-legacy-sgpr-keepalive -stop-after=ventus-sgpr-keepalive < %s -o - | FileCheck %s --check-prefixes=MIR,MIR-NODIV,MIR-LOOP
; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -verify-machineinstrs < %s -o /dev/null

target datalayout = "e-m:e-p:32:32-i64:64-n32-S128"
target triple = "riscv32-unknown-unknown"

declare i32 @_Z13get_global_idj(i32 noundef)

; DISABLED-NOT: PseudoSGPRKeepAlive

define dso_local ventus_kernel void @keepalive_switch(ptr addrspace(1) %out) {
; MIR-LABEL: name: keepalive_switch
; MIR: [[KEEP:%[0-9]+]]:gpr = LUI 260096
; MIR: bb.{{[0-9]+}}.case0:
; MIR: %{{[0-9]+}}:vgpr = COPY [[KEEP]]
; MIR: bb.{{[0-9]+}}.join:
; MIR-NEXT:   PseudoSGPRKeepAlive [[KEEP]]
; MIR-NEXT:   PseudoRET
entry:
  %gid = call i32 @_Z13get_global_idj(i32 0)
  %slot = getelementptr inbounds i32, ptr addrspace(1) %out, i32 %gid
  %idx = and i32 %gid, 3
  %keep = add i32 1065353216, 0
  switch i32 %idx, label %default [
    i32 0, label %case0
    i32 1, label %case1
    i32 2, label %case2
  ]

case0:
  store i32 %keep, ptr addrspace(1) %slot, align 4
  br label %join

case1:
  store i32 1061158912, ptr addrspace(1) %slot, align 4
  br label %join

case2:
  store i32 1069547520, ptr addrspace(1) %slot, align 4
  br label %join

default:
  store i32 1056964608, ptr addrspace(1) %slot, align 4
  br label %join

join:
  ret void
}

define dso_local ventus_kernel void @no_divergence_two_returns(i32 %x) {
; MIR-NODIV-LABEL: name: no_divergence_two_returns
; MIR-NODIV: bb.{{[0-9]+}}.ret0:
; MIR-NODIV-NEXT:   PseudoRET
; MIR-NODIV: bb.{{[0-9]+}}.ret1:
; MIR-NODIV-NEXT:   PseudoRET
entry:
  %c = icmp eq i32 %x, 0
  br i1 %c, label %ret0, label %ret1

ret0:
  ret void

ret1:
  ret void
}

define dso_local ventus_kernel void @loop_header_use(ptr addrspace(1) %out) {
; MIR-LOOP-LABEL: name: loop_header_use
; MIR-LOOP: [[KEEP:%[0-9]+]]:gpr = LUI 260096
; MIR-LOOP: bb.{{[0-9]+}}.loop:
; MIR-LOOP: %{{[0-9]+}}:vgpr = COPY [[KEEP]]
; MIR-LOOP: bb.{{[0-9]+}}.exit:
; MIR-LOOP-NOT:   PseudoSGPRKeepAlive [[KEEP]]
; MIR-LOOP:   PseudoRET
entry:
  %gid = call i32 @_Z13get_global_idj(i32 0)
  %slot0 = getelementptr inbounds i32, ptr addrspace(1) %out, i32 %gid
  %slot1 = getelementptr inbounds i32, ptr addrspace(1) %slot0, i32 1
  %keep = add i32 1065353216, 0
  br label %loop

loop:
  %i = phi i32 [ 0, %entry ], [ 1, %body ]
  store i32 %keep, ptr addrspace(1) %slot1, align 4
  %c = icmp eq i32 %gid, 0
  br i1 %c, label %body, label %exit

body:
  store i32 %i, ptr addrspace(1) %slot0, align 4
  br label %loop

exit:
  ret void
}
