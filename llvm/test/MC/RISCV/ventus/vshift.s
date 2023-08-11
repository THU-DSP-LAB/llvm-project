# RUN: llvm-mc -triple=riscv32 -show-encoding --mattr=+v %s \
# RUN:  | FileCheck %s --check-prefixes=CHECK-ENCODING,CHECK-INST
# RUN: llvm-mc -triple=riscv32 -filetype=obj --mattr=+v %s \
# RUN:   | llvm-objdump -d --mattr=+v - \
# RUN:   | FileCheck %s --check-prefix=CHECK-INST


vsll.vv v4, v2, v1
# CHECK-INST: vsll.vv v4, v2, v1
# CHECK-ENCODING: [0x57,0x82,0x20,0x96]

vsll.vi v4, v2, 1
# CHECK-INST: vsll.vi v4, v2, 1
# CHECK-ENCODING: [0x57,0xb2,0x20,0x96]

vsll.vx v4, v2, ra
# CHECK-INST: vsll.vx v4, v2, ra
# CHECK-ENCODING: [0x57,0xc2,0x20,0x96]


vsrl.vv v4, v2, v1
# CHECK-INST: vsrl.vv v4, v2, v1
# CHECK-ENCODING: [0x57,0x82,0x20,0xa2]

vsrl.vi v4, v2, 1
# CHECK-INST: vsrl.vi v4, v2, 1
# CHECK-ENCODING: [0x57,0xb2,0x20,0xa2]

vsrl.vx v4, v2, ra
# CHECK-INST: vsrl.vx v4, v2, ra
# CHECK-ENCODING: [0x57,0xc2,0x20,0xa2]


vsra.vv v4, v2, v1
# CHECK-INST: vsra.vv v4, v2, v1
# CHECK-ENCODING: [0x57,0x82,0x20,0xa6]

vsra.vi v4, v2, 1
# CHECK-INST: vsra.vi v4, v2, 1
# CHECK-ENCODING: [0x57,0xb2,0x20,0xa6]

vsra.vx v4, v2, ra
# CHECK-INST: vsra.vx v4, v2, ra
# CHECK-ENCODING: [0x57,0xc2,0x20,0xa6]



