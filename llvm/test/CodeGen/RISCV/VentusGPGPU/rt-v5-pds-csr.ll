; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -verify-machineinstrs < %s \
; RUN:   | FileCheck %s

define dso_local ventus_kernel void @read_v5_pds_base(ptr addrspace(1) %out) {
; CHECK-LABEL: read_v5_pds_base:
; CHECK:       csrr {{[a-z0-9]+}}, CSR_PDS
; CHECK:       csrr {{[a-z0-9]+}}, CSR_TID
entry:
  %pds = call i32 asm "csrr $0, CSR_PDS", "=r"()
  %tid = call i32 asm "csrr $0, CSR_TID", "=r"()
  store i32 %pds, ptr addrspace(1) %out, align 4
  %tid.out = getelementptr i32, ptr addrspace(1) %out, i32 1
  store i32 %tid, ptr addrspace(1) %tid.out, align 4
  ret void
}
