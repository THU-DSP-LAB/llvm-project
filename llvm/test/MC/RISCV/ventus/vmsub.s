# RUN: llvm-mc -triple=riscv32 -show-encoding --mattr=+v %s \
# RUN:  | FileCheck %s --check-prefixes=CHECK-ENCODING,CHECK-INST
# RUN: llvm-mc -triple=riscv32 -filetype=obj --mattr=+v %s \
# RUN:   | llvm-objdump -d --mattr=+v - \
# RUN:   | FileCheck %s --check-prefix=CHECK-INST


vfmsub.vv v4, v2, v1
# CHECK-INST: vfmsub.vv v4, v2, v1
# CHECK-ENCODING: [0x57,0x92,0x20,0xaa]

vfmsub.vf v4, v2, ra
# CHECK-INST: vfmsub.vf v4, v2, ra
# CHECK-ENCODING: [0x57,0xd2,0x20,0xaa]


vfnmsub.vv v4, v2, v1
# CHECK-INST: vfnmsub.vv v4, v2, v1
# CHECK-ENCODING: [0x57,0x92,0x20,0xae]

vfnmsub.vf v4, v2, ra
# CHECK-INST: vfnmsub.vf v4, v2, ra
# CHECK-ENCODING: [0x57,0xd2,0x20,0xae]








