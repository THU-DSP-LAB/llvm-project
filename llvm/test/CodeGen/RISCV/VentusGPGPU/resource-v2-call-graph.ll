; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -verify-machineinstrs < %s \
; RUN:   | FileCheck -check-prefix=VENTUS %s

@helper.tmp = internal addrspace(3) global [10 x i32] undef, align 4
@private.tmp = internal addrspace(5) global i32 undef, align 4

; VENTUS: .section	.ventus.resource.kernel,"w",@progbits
; VENTUS-NEXT: .p2align	3
; VENTUS-NEXT: .word	3
; VENTUS-NEXT: .word	0
; VENTUS-NEXT: .quad	0
; VENTUS-NEXT: .quad	9
; VENTUS-NEXT: .quad	40
; VENTUS-NEXT: .quad	0
; VENTUS-NEXT: .quad	4
; VENTUS-NEXT: .quad	0

; VENTUS: .section	.ventus.resource.kernel_unknown,"w",@progbits
; VENTUS-NEXT: .p2align	3
; VENTUS-NEXT: .word	3
; VENTUS-NEXT: .word	56
; VENTUS-NEXT: .quad	0
; VENTUS-NEXT: .quad	5
; VENTUS-NEXT: .quad	0
; VENTUS-NEXT: .quad	0
; VENTUS-NEXT: .quad	-1
; VENTUS-NEXT: .quad	-1

; VENTUS: .section	.ventus.resource.kernel_unknown_static,"w",@progbits
; VENTUS-NEXT: .p2align	3
; VENTUS-NEXT: .word	3
; VENTUS-NEXT: .word	56
; VENTUS-NEXT: .quad	2
; VENTUS-NEXT: .quad	7
; VENTUS-NEXT: .quad	0
; VENTUS-NEXT: .quad	4
; VENTUS-NEXT: .quad	-1
; VENTUS-NEXT: .quad	-1

; VENTUS: .section	.ventus.resource.kernel_unknown_local,"w",@progbits
; VENTUS-NEXT: .p2align	3
; VENTUS-NEXT: .word	3
; VENTUS-NEXT: .word	56
; VENTUS-NEXT: .quad	0
; VENTUS-NEXT: .quad	9
; VENTUS-NEXT: .quad	40
; VENTUS-NEXT: .quad	0
; VENTUS-NEXT: .quad	-1
; VENTUS-NEXT: .quad	-1

; VENTUS: .section	.ventus.resource.kernel_alias,"w",@progbits
; VENTUS-NEXT: .p2align	3
; VENTUS-NEXT: .word	3
; VENTUS-NEXT: .word	0
; VENTUS-NEXT: .quad	0
; VENTUS-NEXT: .quad	5
; VENTUS-NEXT: .quad	0
; VENTUS-NEXT: .quad	0
; VENTUS-NEXT: .quad	4
; VENTUS-NEXT: .quad	0

declare void @ext()

define dso_local void @helper() {
entry:
  store i32 1, ptr addrspace(3) @helper.tmp, align 4
  store i32 2, ptr addrspace(3) getelementptr inbounds ([10 x i32], ptr addrspace(3) @helper.tmp, i32 0, i32 9), align 4
  ret void
}

define dso_local ventus_kernel void @kernel() {
entry:
  call void @helper()
  ret void
}

define dso_local ventus_kernel void @kernel_unknown() {
entry:
  call void @ext()
  ret void
}

define dso_local ventus_kernel void @kernel_unknown_static() {
entry:
  store i32 3, ptr addrspace(5) @private.tmp, align 4
  call void @ext()
  ret void
}

define dso_local ventus_kernel void @kernel_unknown_local() {
entry:
  store i32 3, ptr addrspace(3) @helper.tmp, align 4
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
