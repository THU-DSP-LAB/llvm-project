; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -verify-machineinstrs < %s \
; RUN:   | FileCheck -check-prefix=VENTUS %s

; Function Attrs: nofree norecurse nosync nounwind memory(none) vscale_range(1,2048)
define dso_local i32 @loop(i32 noundef %x) local_unnamed_addr {
; VENTUS:    vbeq	v1, v0, .LBB0_4
; VENTUS:    vadd.vi	v0, v0, 2
; VENTUS:    vbne	v0, v2, .LBB0_1
entry:
  br label %for.body

for.body:                                         ; preds = %entry, %for.inc
  %i.05 = phi i32 [ 0, %entry ], [ %add, %for.inc ]
  %cmp1 = icmp eq i32 %i.05, %x
  br i1 %cmp1, label %cleanup, label %for.inc

for.inc:                                          ; preds = %for.body
  %add = add nuw nsw i32 %i.05, 2
  %cmp.not = icmp eq i32 %add, 64
  br i1 %cmp.not, label %cleanup, label %for.body

cleanup:                                          ; preds = %for.body, %for.inc
  %spec.select = phi i32 [ %i.05, %for.body ], [ 0, %for.inc ]
  ret i32 %spec.select
}