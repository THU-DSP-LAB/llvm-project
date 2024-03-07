# RUN: llvm-mc -triple=riscv32 -show-encoding --mattr=+v %s \
# RUN:  | FileCheck %s --check-prefixes=CHECK-ENCODING,CHECK-INST
# RUN: llvm-mc -triple=riscv32 -filetype=obj --mattr=+v %s \
# RUN:   | llvm-objdump -d --mattr=+v - \
# RUN:   | FileCheck %s --check-prefix=CHECK-INST

# SKIP VMV_S_X

# SKIP VMERGE_VVM
# SKIP VMV_V_V
# SKIP VMERGE_VIM
# SKIP VMV_V_I
# SKIP VMERGE_VXM

vmv.v.x v6, s0
# CHECK-INST: vmv.v.x v6, s0
# CHECK-ENCODING: [0x57,0x43,0x04,0x5e]
