; RUN: opt -S -mtriple=riscv32 -passes=ventus-always-inline %s | FileCheck %s

@internal_alias = internal alias i32 (i32), ptr @original_function
@public_alias = alias i32 (i32), ptr @original_function

define i32 @original_function(i32 %x) {
  %result = add i32 %x, 42
  ret i32 %result
}

define i32 @call_public() {
  %res = call i32 @public_alias(i32 7)
  ret i32 %res
}

; CHECK: define i32 @original_function
; CHECK-NOT: @internal_alias
; CHECK: @public_alias
