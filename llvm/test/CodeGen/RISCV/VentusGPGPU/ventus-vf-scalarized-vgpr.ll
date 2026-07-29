; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -verify-machineinstrs < %s -o /dev/null
; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu \
; RUN:   -stop-after=ventus-VV-instructions-conversion < %s -o - | FileCheck %s

; A private-memory load is lane-private, but aggregate lowering can lose that
; divergence annotation before instruction selection.  VF selection may then
; transiently scalarize the VGPR input.  The VV legalization pass must run
; before the domain verifier and turn that use back into a legal VV operation.

define ventus_kernel void @scalarized_vf(ptr addrspace(5) %in,
                                         ptr addrspace(5) %out) {
; CHECK-LABEL: name: scalarized_vf
; CHECK-NOT: :gprf32 = COPY {{%[0-9]+}}:vgpr
; CHECK: VFADD_VV
; CHECK-NOT: :gprf32 = COPY {{%[0-9]+}}:vgpr
entry:
  %value = load i32, ptr addrspace(5) %in, align 4
  %as_float = uitofp i32 %value to float
  %sum = fadd float %as_float, 5.000000e-01
  %bits = bitcast float %sum to i32
  store i32 %bits, ptr addrspace(5) %out, align 4
  ret void
}
