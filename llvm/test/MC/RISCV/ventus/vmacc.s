# RUN: llvm-mc -triple=riscv32 -show-encoding --mattr=+v %s \
# RUN:  | FileCheck %s --check-prefixes=CHECK-ENCODING,CHECK-INST
# RUN: llvm-mc -triple=riscv32 -filetype=obj --mattr=+v %s \
# RUN:   | llvm-objdump -d --mattr=+v - \
# RUN:   | FileCheck %s --check-prefix=CHECK-INST


vfmacc.vv v4, v2, v1
# CHECK-INST: vfmacc.vv v4, v2, v1
# CHECK-ENCODING: [0x57,0x12,0x11,0xb2]

vfmacc.vf v4, t1, v2
# CHECK-INST: vfmacc.vf v4, t1, v2
# CHECK-ENCODING: [0x57,0x52,0x23,0xb2]


vfnmacc.vv v4, v2, v1
# CHECK-INST: vfnmacc.vv v4, v2, v1
# CHECK-ENCODING: [0x57,0x12,0x11,0xb6]

vfnmacc.vf v4, ra, v2
# CHECK-INST: vfnmacc.vf v4, ra, v2
# CHECK-ENCODING: [0x57,0xd2,0x20,0xb6]
