; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -verify-machineinstrs < %s \
; RUN:   | FileCheck -check-prefix=RESOBJ %s

@helper.tmp = internal addrspace(3) global [10 x i32] undef, align 4
@private.tmp = internal addrspace(5) global i32 undef, align 4

; RESOBJ: .section	.ventus.resobj,"",@progbits
; RESOBJ-NEXT: .p2align	3
; RESOBJ-NEXT: .word	1448235855
; RESOBJ-NEXT: .word	1
; RESOBJ-NEXT: .word	0
; RESOBJ-NEXT: .word	5
; RESOBJ-NEXT: .word	3
; RESOBJ-NEXT: .word	2
; RESOBJ-NEXT: .word	36
; RESOBJ-NEXT: .word	356
; RESOBJ-NEXT: .word	380
; RESOBJ-NEXT: .word	helper
; RESOBJ-NEXT: .word	0
; RESOBJ-NEXT: .quad	0
; RESOBJ-NEXT: .quad	9
; RESOBJ-NEXT: .quad	0
; RESOBJ-NEXT: .quad	0
; RESOBJ-NEXT: .quad	0
; RESOBJ-NEXT: .word	0
; RESOBJ-NEXT: .word	0
; RESOBJ-NEXT: .word	0
; RESOBJ-NEXT: .word	1
; RESOBJ-NEXT: .word	kernel
; RESOBJ-NEXT: .word	1
; RESOBJ-NEXT: .quad	0
; RESOBJ-NEXT: .quad	5
; RESOBJ-NEXT: .quad	4
; RESOBJ-NEXT: .quad	0
; RESOBJ-NEXT: .quad	0
; RESOBJ-NEXT: .word	0
; RESOBJ-NEXT: .word	1
; RESOBJ-NEXT: .word	1
; RESOBJ-NEXT: .word	0
; RESOBJ-NEXT: .word	kernel_unknown_static
; RESOBJ-NEXT: .word	1
; RESOBJ-NEXT: .quad	2
; RESOBJ-NEXT: .quad	7
; RESOBJ-NEXT: .quad	4
; RESOBJ-NEXT: .quad	0
; RESOBJ-NEXT: .quad	0
; RESOBJ-NEXT: .word	1
; RESOBJ-NEXT: .word	1
; RESOBJ-NEXT: .word	1
; RESOBJ-NEXT: .word	1
; RESOBJ-NEXT: .word	helper_alias_target
; RESOBJ-NEXT: .word	0
; RESOBJ-NEXT: .quad	0
; RESOBJ-NEXT: .quad	2
; RESOBJ-NEXT: .quad	0
; RESOBJ-NEXT: .quad	0
; RESOBJ-NEXT: .quad	0
; RESOBJ-NEXT: .word	2
; RESOBJ-NEXT: .word	0
; RESOBJ-NEXT: .word	2
; RESOBJ-NEXT: .word	0
; RESOBJ-NEXT: .word	kernel_alias
; RESOBJ-NEXT: .word	1
; RESOBJ-NEXT: .quad	0
; RESOBJ-NEXT: .quad	5
; RESOBJ-NEXT: .quad	4
; RESOBJ-NEXT: .quad	0
; RESOBJ-NEXT: .quad	0
; RESOBJ-NEXT: .word	2
; RESOBJ-NEXT: .word	1
; RESOBJ-NEXT: .word	2
; RESOBJ-NEXT: .word	0
; RESOBJ-NEXT: .word	helper
; RESOBJ-NEXT: .word	0
; RESOBJ-NEXT: .word	ext
; RESOBJ-NEXT: .word	0
; RESOBJ-NEXT: .word	helper_alias
; RESOBJ-NEXT: .word	0
; RESOBJ-NEXT: .word	helper.tmp
; RESOBJ-NEXT: .word	1
; RESOBJ-NEXT: .quad	40
; RESOBJ-NEXT: .word	private.tmp
; RESOBJ-NEXT: .word	2
; RESOBJ-NEXT: .quad	4

declare void @ext()

define dso_local void @helper() {
entry:
  store i32 1, ptr addrspace(3) @helper.tmp, align 4
  ret void
}

define dso_local ventus_kernel void @kernel() {
entry:
  call void @helper()
  ret void
}

define dso_local ventus_kernel void @kernel_unknown_static() {
entry:
  store i32 3, ptr addrspace(5) @private.tmp, align 4
  call void @ext()
  ret void
}

define dso_local void @helper_alias_target() {
entry:
  ret void
}

@helper_alias = alias void (), ptr @helper_alias_target

define dso_local ventus_kernel void @kernel_alias() {
entry:
  call void @helper_alias()
  ret void
}
