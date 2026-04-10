; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -stop-after=ventus-sgpr-keepalive < %s -o - | FileCheck %s --check-prefixes=MIR-SIDE,MIR-SYNTH
; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -stop-after=ventus-remove-sgpr-keepalive < %s -o - | FileCheck %s --check-prefixes=REMOVE-SIDE,REMOVE-SYNTH
; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -verify-machineinstrs < %s -o /dev/null

target datalayout = "e-m:e-p:32:32-i64:64-n32-S128"
target triple = "riscv32-unknown-unknown"

declare i32 @_Z13get_global_idj(i32 noundef)

define dso_local ventus_kernel void @side_entry_join(ptr addrspace(1) %out, i32 %flag) {
; MIR-SIDE-LABEL: name: side_entry_join
; MIR-SIDE: bb.1.branch:
; MIR-SIDE: [[KEEP:%[0-9]+]]:gpr = LUI 260096
; MIR-SIDE: bb.{{[0-9]+}}:
; MIR-SIDE-NEXT: successors: %bb.4
; MIR-SIDE: PseudoSGPRKeepAlive [[KEEP]]
; MIR-SIDE: bb.{{[0-9]+}}:
; MIR-SIDE-NEXT: successors: %bb.4
; MIR-SIDE: PseudoSGPRKeepAlive [[KEEP]]
; MIR-SIDE: bb.4.join:
; MIR-SIDE-NOT: PHI
; MIR-SIDE-NOT: IMPLICIT_DEF
; MIR-SIDE: PseudoRET
; REMOVE-SIDE-LABEL: name: side_entry_join
; REMOVE-SIDE-NOT: PseudoSGPRKeepAlive
; REMOVE-SIDE-NOT: PseudoSGPRKeepAliveBlock
; REMOVE-SIDE: bb.2.then:
; REMOVE-SIDE: PseudoBR %bb.{{[0-9]+}}
; REMOVE-SIDE: bb.4.else:
; REMOVE-SIDE: bb.{{[0-9]+}}.join:
entry:
  %gid = call i32 @_Z13get_global_idj(i32 0)
  %slot = getelementptr inbounds i32, ptr addrspace(1) %out, i32 %gid
  %go_side = icmp eq i32 %flag, 0
  br i1 %go_side, label %side, label %branch

side:
  br label %join

branch:
  %keep = add i32 1065353216, 0
  %c = icmp eq i32 %gid, 0
  br i1 %c, label %then, label %else

then:
  store i32 %keep, ptr addrspace(1) %slot, align 4
  br label %join

else:
  store i32 1061158912, ptr addrspace(1) %slot, align 4
  br label %join

join:
  ret void
}

define dso_local ventus_kernel void @synthetic_return_side_entry(ptr addrspace(1) %out) {
; MIR-SYNTH-LABEL: name: synthetic_return_side_entry
; MIR-SYNTH: bb.2.setup:
; MIR-SYNTH: [[KEEP:%[0-9]+]]:gpr = LUI 260096
; MIR-SYNTH: bb.{{[0-9]+}}:
; MIR-SYNTH-NEXT: successors: %bb.5
; MIR-SYNTH: PseudoSGPRKeepAlive [[KEEP]]
; MIR-SYNTH: bb.{{[0-9]+}}:
; MIR-SYNTH-NEXT: successors: %bb.5
; MIR-SYNTH: PseudoSGPRKeepAlive [[KEEP]]
; MIR-SYNTH: bb.5:
; MIR-SYNTH-NOT: PHI
; MIR-SYNTH-NOT: IMPLICIT_DEF
; MIR-SYNTH: PseudoRET
; REMOVE-SYNTH-LABEL: name: synthetic_return_side_entry
; REMOVE-SYNTH-NOT: PseudoSGPRKeepAlive
; REMOVE-SYNTH-NOT: PseudoSGPRKeepAliveBlock
; REMOVE-SYNTH: bb.3.then:
; REMOVE-SYNTH: PseudoBR %bb.{{[0-9]+}}
; REMOVE-SYNTH: bb.{{[0-9]+}}:
entry:
  %gid = call i32 @_Z13get_global_idj(i32 0)
  %slot = getelementptr inbounds i32, ptr addrspace(1) %out, i32 %gid
  %c0 = icmp eq i32 %gid, 0
  br i1 %c0, label %earlyret, label %setup

earlyret:
  ret void

setup:
  %keep = add i32 1065353216, 0
  %c1 = icmp eq i32 %gid, 1
  br i1 %c1, label %then, label %elseret

then:
  store i32 %keep, ptr addrspace(1) %slot, align 4
  ret void

elseret:
  ret void
}
