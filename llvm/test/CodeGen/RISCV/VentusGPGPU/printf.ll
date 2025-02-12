; RUN: opt -mtriple=riscv32 -ventus-printf-runtime-binding -mcpu=ventus-gpgpu -S < %s | FileCheck --check-prefix=VENTUS %s
; RUN: opt -mtriple=riscv32 -passes=ventus-printf-runtime-binding -mcpu=ventus-gpgpu -S < %s | FileCheck --check-prefix=VENTUS %s

; VENTUS-LABEL: @test_kernel(
; VENTUS-LABEL: entry
; VENTUS: call ptr addrspace(1) @__builtin_riscv_printf_alloc
; VENTUS-LABEL: entry.split
; VENTUS: icmp ne ptr addrspace(1) %printf_alloc_fn, null
; VENTUS: %PrintBuffID = getelementptr i8, ptr addrspace(1) %printf_alloc_fn, i32 0
; VENTUS: %PrintBuffIdCast = bitcast ptr addrspace(1) %PrintBuffID to ptr addrspace(1)
; VENTUS: store i32 1, ptr addrspace(1) %PrintBuffIdCast
; VENTUS: %PrintBuffGep = getelementptr i8, ptr addrspace(1) %printf_alloc_fn, i32 4
; VENTUS: %PrintBuffPtrCast = bitcast ptr addrspace(1) %PrintBuffGep to ptr addrspace(1)
; VENTUS: store i32 10, ptr addrspace(1) %PrintBuffPtrCast

@.str = private unnamed_addr addrspace(4) constant [5 x i8] c"%5d\0A\00", align 1

define ventus_kernel void @test_kernel() {
entry:
  %call = call i32 (ptr addrspace(4), ...) @printf(ptr addrspace(4) @.str, i32 noundef 10)
  ret void
}

declare i32 @printf(ptr addrspace(4), ...)
