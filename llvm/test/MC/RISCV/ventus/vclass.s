# RUN: llvm-mc -triple=riscv32 -show-encoding --mattr=+v %s \
# RUN:  | FileCheck %s --check-prefixes=CHECK-ENCODING,CHECK-INST
# RUN: llvm-mc -triple=riscv32 -filetype=obj --mattr=+v %s \
# RUN:   | llvm-objdump -d --mattr=+v - \
# RUN:   | FileCheck %s --check-prefix=CHECK-INST


vfclass.v v4, v2
# CHECK-INST: vfclass.v v4, v2
# CHECK-ENCODING: [0x57,0x12,0x28,0x4e]





