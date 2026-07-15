; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -verify-machineinstrs < %s -o /dev/null

; Lowering this bitcast can feed COPY_TO_REGCLASS with a physical-register
; SelectionDAG leaf.  InstrEmitter must use that physical register directly
; instead of looking it up in the virtual-register map.

define ventus_kernel void @sqrt_bitcast_private_store() {
entry:
  %sqrt = call float @llvm.sqrt.f32(float 0.000000e+00)
  %bits = bitcast float %sqrt to i32
  store i32 %bits, ptr addrspace(5) null, align 4
  ret void
}

declare float @llvm.sqrt.f32(float)
