# RUN: llvm-mc -triple=riscv32 -show-encoding --mattr=+v %s \
# RUN:  | FileCheck %s --check-prefixes=CHECK-ENCODING,CHECK-INST
# RUN: llvm-mc -triple=riscv32 -filetype=obj --mattr=+v %s \
# RUN:   | llvm-objdump -d --mattr=+v - \
# RUN:   | FileCheck %s --check-prefix=CHECK-INST


vfmadd.vv v4, v2, v1
# CHECK-INST: vfmadd.vv v4, v2, v1
# CHECK-ENCODING: [0x57,0x12,0x11,0xa2]

vfmadd.vf v4, ra, v2
# CHECK-INST: vfmadd.vf v4, ra, v2
# CHECK-ENCODING: [0x57,0xd2,0x20,0xa2]


vfnmadd.vv v4, v2, v1
# CHECK-INST: vfnmadd.vv v4, v2, v1
# CHECK-ENCODING: [0x57,0x12,0x11,0xa6]

vfnmadd.vf v4, ra, v2
# CHECK-INST: vfnmadd.vf v4, ra, v2
# CHECK-ENCODING: [0x57,0xd2,0x20,0xa6]
