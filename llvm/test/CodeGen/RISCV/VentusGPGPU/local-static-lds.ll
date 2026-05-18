; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -verify-machineinstrs \
; RUN:   -asm-verbose < %s | FileCheck %s
; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -verify-machineinstrs \
; RUN:   -filetype=obj < %s -o %t.o
; RUN: llvm-readobj -r %t.o | FileCheck %s --check-prefix=RELOC

@a = hidden addrspace(3) global [32 x i32] undef, align 4
@b = hidden addrspace(3) global [1 x i32] undef, align 4
@c = hidden addrspace(3) global [1 x i32] undef, align 64

define ptr addrspace(3) @get_a() {
; CHECK-LABEL: get_a:
; CHECK-NOT: addi s0, s0
; CHECK: lui [[OFF:[a-z0-9]+]], %ventus_lds_hi(a)
; CHECK-NEXT: csrr [[LDS:[a-z0-9]+]], CSR_LDS
; CHECK-NEXT: add [[ADDR:[a-z0-9]+]], [[LDS]], [[OFF]]
; CHECK-NEXT: addi [[ADDR]], [[ADDR]], %ventus_lds_lo(a)
; CHECK-NEXT: vmv.v.x v0, [[ADDR]]
; CHECK-NEXT: ret
entry:
  ret ptr addrspace(3) @a
}

define ptr addrspace(3) @get_b() {
; CHECK-LABEL: get_b:
; CHECK-NOT: addi s0, s0
; CHECK: lui [[OFF:[a-z0-9]+]], %ventus_lds_hi(b)
; CHECK-NEXT: csrr [[LDS:[a-z0-9]+]], CSR_LDS
; CHECK-NEXT: add [[ADDR:[a-z0-9]+]], [[LDS]], [[OFF]]
; CHECK-NEXT: addi [[ADDR]], [[ADDR]], %ventus_lds_lo(b)
; CHECK-NEXT: vmv.v.x v0, [[ADDR]]
; CHECK-NEXT: ret
entry:
  ret ptr addrspace(3) @b
}

define ptr addrspace(3) @get_c() {
; CHECK-LABEL: get_c:
; CHECK: lui [[OFF:[a-z0-9]+]], %ventus_lds_hi(c)
; CHECK-NEXT: csrr [[LDS:[a-z0-9]+]], CSR_LDS
; CHECK-NEXT: add [[ADDR:[a-z0-9]+]], [[LDS]], [[OFF]]
; CHECK-NEXT: addi [[ADDR]], [[ADDR]], %ventus_lds_lo(c)
; CHECK-NEXT: vmv.v.x v0, [[ADDR]]
; CHECK-NEXT: ret
entry:
  ret ptr addrspace(3) @c
}

define void @static_and_stack() {
; CHECK-LABEL: static_and_stack:
; CHECK: addi tp, tp, 16
; CHECK: lui [[OFF:[a-z0-9]+]], %ventus_lds_hi(b)
; CHECK-NEXT: csrr [[LDS:[a-z0-9]+]], CSR_LDS
; CHECK-NEXT: add [[ADDR:[a-z0-9]+]], [[LDS]], [[OFF]]
; CHECK: sw {{[a-z0-9]+}}, %ventus_lds_lo(b)([[ADDR]])
; CHECK: addi t0, tp, -16
; CHECK: addi tp, tp, -16
entry:
  %tmp = alloca [4 x i32], align 4, addrspace(3)
  store i32 1, ptr addrspace(3) @b, align 4
  call void @use_local(ptr addrspace(3) %tmp)
  ret void
}

define ventus_kernel void @kernel(ptr addrspace(1) %out) {
entry:
  %p = call ptr addrspace(3) @get_b()
  %v = ptrtoint ptr addrspace(3) %p to i32
  store i32 %v, ptr addrspace(1) %out, align 4
  %q = call ptr addrspace(3) @get_c()
  %w = ptrtoint ptr addrspace(3) %q to i32
  %out1 = getelementptr i32, ptr addrspace(1) %out, i32 1
  store i32 %w, ptr addrspace(1) %out1, align 4
  ret void
}

define ventus_kernel void @kernel_local_arg_with_static(ptr addrspace(3) %local_arg, ptr addrspace(1) %out) {
; CHECK-LABEL: kernel_local_arg_with_static:
; CHECK:       lw [[ARG_OFFSET:[a-z0-9]+]], 0(a0)
; CHECK-NEXT:  csrr [[LDS:[a-z0-9]+]], CSR_LDS
; CHECK-NEXT:  add [[LOCAL_ARG:[a-z0-9]+]], [[LDS]], [[ARG_OFFSET]]
; CHECK:       lui [[OFF:[a-z0-9]+]], %ventus_lds_hi(c)
; CHECK-NEXT:  add [[STATIC_ADDR:[a-z0-9]+]], [[LDS]], [[OFF]]
; CHECK:       sw {{[a-z0-9]+}}, %ventus_lds_lo(c)([[STATIC_ADDR]])
; CHECK:       sw [[LOCAL_ARG]], 0({{[a-z0-9]+}})
; CHECK:       ret
entry:
  store i32 7, ptr addrspace(3) @c, align 4
  %v = ptrtoint ptr addrspace(3) %local_arg to i32
  store i32 %v, ptr addrspace(1) %out, align 4
  ret void
}

declare void @use_local(ptr addrspace(3))

; CHECK: .section .ventus.resource.kernel,"w",@progbits
; CHECK-NEXT: .p2align 3
; CHECK-NEXT: .word 3
; CHECK-NEXT: .word 0
; CHECK-NEXT: .quad 34
; CHECK-NEXT: .quad 11
; CHECK-NEXT: .quad 196
; CHECK-NEXT: .quad 0
; CHECK-NEXT: .quad 8
; CHECK-NEXT: .quad 4

; RELOC:      Section ({{.*}}) .rela.text {
; RELOC-NEXT:   0x0 R_RISCV_VENTUS_LDS_HI20 a 0x0
; RELOC-NEXT:   0xC R_RISCV_VENTUS_LDS_LO12_I a 0x0
; RELOC-NEXT:   0x18 R_RISCV_VENTUS_LDS_HI20 b 0x0
; RELOC-NEXT:   0x24 R_RISCV_VENTUS_LDS_LO12_I b 0x0
; RELOC:        0x6C R_RISCV_VENTUS_LDS_LO12_S b 0x0
; RELOC:        0x124 R_RISCV_VENTUS_LDS_LO12_S c 0x0
