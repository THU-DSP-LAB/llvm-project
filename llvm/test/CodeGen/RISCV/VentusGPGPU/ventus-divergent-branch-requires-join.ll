; RUN: not --crash llc -mtriple=riscv32 -mcpu=ventus-gpgpu -stop-after=ventus-sgpr-keepalive < %s -o /dev/null 2>&1 | FileCheck %s

target datalayout = "e-m:e-p:32:32-i64:64-n32-S128"
target triple = "riscv32-unknown-unknown"

declare i32 @_Z13get_global_idj(i32 noundef)

define dso_local ventus_kernel void @no_join_forever() {
; CHECK: LLVM ERROR: Ventus Insert SGPR KeepAlive requires every divergent branch to converge; missing immediate post-dominator in function 'no_join_forever'
entry:
  %gid = call i32 @_Z13get_global_idj(i32 0)
  %c = icmp eq i32 %gid, 0
  br i1 %c, label %loop, label %ret

loop:
  br label %loop

ret:
  ret void
}
