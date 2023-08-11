# RUN: llvm-mc -triple=riscv32 -show-encoding --mattr=+v %s \
# RUN:  | FileCheck %s --check-prefixes=CHECK-ENCODING,CHECK-INST
# RUN: llvm-mc -triple=riscv32 -filetype=obj --mattr=+v %s \
# RUN:   | llvm-objdump -d --mattr=+v - \
# RUN:   | FileCheck %s --check-prefix=CHECK-INST


vand.vv v6, v4, v3
# CHECK-INST: vand.vv v6, v4, v3
# CHECK-ENCODING: [0x57,0x83,0x41,0x26]

vand.vi v6, v4, 3
# CHECK-INST: vand.vi v6, v4, 3
# CHECK-ENCODING: [0x57,0xb3,0x41,0x26]

vand.vx v6, v4, gp
# CHECK-INST: vand.vx v6, v4, gp
# CHECK-ENCODING: [0x57,0xc3,0x41,0x26]


vor.vv v6, v4, v3
# CHECK-INST: vor.vv v6, v4, v3
# CHECK-ENCODING: [0x57,0x83,0x41,0x2a]

vor.vi v6, v4, 3
# CHECK-INST: vor.vi v6, v4, 3
# CHECK-ENCODING: [0x57,0xb3,0x41,0x2a]

vor.vx v6, v4, gp
# CHECK-INST: vor.vx v6, v4, gp
# CHECK-ENCODING: [0x57,0xc3,0x41,0x2a]


vxor.vv v6, v4, v3
# CHECK-INST: vxor.vv v6, v4, v3
# CHECK-ENCODING: [0x57,0x83,0x41,0x2e]

vxor.vi v6, v4, 3
# CHECK-INST: vxor.vi v6, v4, 3
# CHECK-ENCODING: [0x57,0xb3,0x41,0x2e]

vxor.vx v6, v4, gp
# CHECK-INST: vxor.vx v6, v4, gp
# CHECK-ENCODING: [0x57,0xc3,0x41,0x2e]




