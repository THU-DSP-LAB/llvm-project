; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -verify-machineinstrs < %s \
; RUN:   | FileCheck %s

define dso_local i32 @regexti1(i32 noundef %a) {
entry:
  ; CHECK-LABEL: regexti1:
  ; CHECK: regexti	zero, zero, 384
  ; CHECK-NEXT: vor.vi	v0, v0, 15
  %res = or i32 %a, 399
  ret i32 %res
}

define dso_local i32 @regexti2(i32 noundef %a) {
entry:
  ; CHECK-LABEL: regexti2:
  ; CHECK: regexti	zero, zero, 1408
  ; CHECK-NEXT: vor.vi	v0, v0, 15
  %res = or i32 %a, -399
  ret i32 %res
}


define dso_local i32 @regexti3(i32 noundef %a) {
entry:
  ; CHECK-LABEL: regexti3:
  ; CHECK: regexti	zero, zero, 384
  ; CHECK-NEXT: vxor.vi	v0, v0, 15
  %res = xor i32 %a, 399
  ret i32 %res
}

define dso_local i32 @regexti4(i32 noundef %a) {
entry:
  ; CHECK-LABEL: regexti4:
  ; CHECK: regexti	zero, zero, 1408
  ; CHECK-NEXT: xor.vi	v0, v0, 15
  %res = xor i32 %a, -399
  ret i32 %res
}

define dso_local i32 @regexti5(i32 noundef %a) {
entry:
  ; CHECK-LABEL: regexti5:
  ; CHECK: regexti	zero, zero, 384
  ; CHECK-NEXT: vrsub.vi	v0, v0, 15
  %res = sub i32 399, %a
  ret i32 %res
}

define dso_local i32 @regexti6(i32 noundef %a) {
entry:
  ; CHECK-LABEL: regexti6:
  ; CHECK: regexti	zero, zero, 1408
  ; CHECK-NEXT: vrsub.vi	v0, v0, 15
  %res = sub i32 -399, %a
  ret i32 %res
}

define dso_local i32 @regexti7(i32 noundef %a) {
entry:
  ; CHECK-LABEL: regexti7:
  ; CHECK: regexti	zero, zero, 384
  ; CHECK-NEXT: vand.vi	v0, v0, 15
  %res = and i32 %a, 399
  ret i32 %res
}

define dso_local i32 @regexti8(i32 noundef %a) {
entry:
  ; CHECK-LABEL: regexti8:
  ; CHECK: regexti	zero, zero, 1408
  ; CHECK-NEXT: vand.vi	v0, v0, 15
  %res = and i32 %a, -399
  ret i32 %res
}

define dso_local i1 @regexti9(i32 noundef %a) {
entry:
  ; CHECK-LABEL: regexti9:
  ; CHECK: regexti	zero, zero, 384
  ; CHECK-NEXT: vmseq.vi	v0, v0, 15
  %res = icmp eq i32 %a, 399
  ret i1 %res
}

define dso_local i1 @regexti10(i32 noundef %a) {
entry:
  ; CHECK-LABEL: regexti10:
  ; CHECK: regexti	zero, zero, 1408
  ; CHECK-NEXT: vmseq.vi	v0, v0, 15
  %res = icmp eq i32 %a, -399
  ret i1 %res
}

define dso_local i1 @regexti11(i32 noundef %a) {
entry:
  ; CHECK-LABEL: regexti11:
  ; CHECK: regexti	zero, zero, 384
  ; CHECK-NEXT: vmsne.vi	v0, v0, 15
  %res = icmp ne i32 %a, 399
  ret i1 %res
}

define dso_local i1 @regexti12(i32 noundef %a) {
entry:
  ; CHECK-LABEL: regexti12:
  ; CHECK: regexti	zero, zero, 1408
  ; CHECK-NEXT: vmsne.vi	v0, v0, 15
  %res = icmp ne i32 %a, -399
  ret i1 %res
}

