
; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -print-after-isel %s >& /tmp/reg.txt -o /dev/null
; RUN: FileCheck %s < /tmp/reg.txt

define dso_local i32 @regexti(i32 noundef %a) local_unnamed_addr #0 {
entry:
  ; CHECK: %1:vgpr = PseudoVXOR_VI_IMM11 %0:vgpr, 257
  %add1 = or i32 %a, 257
  ret i32 %add1
}
