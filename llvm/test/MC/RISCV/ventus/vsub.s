# RUN: llvm-mc -triple=riscv32 -show-encoding --mattr=+v %s \
# RUN:  | FileCheck %s --check-prefixes=CHECK-ENCODING,CHECK-INST
# RUN: llvm-mc -triple=riscv32 -filetype=obj --mattr=+v %s \
# RUN:   | llvm-objdump -d --mattr=+v - \
# RUN:   | FileCheck %s --check-prefix=CHECK-INST


vsub.vv v6, v4, v3
# CHECK-INST: vsub.vv v6, v4, v3
# CHECK-ENCODING: [0x57,0x83,0x41,0x0a]

vfsub.vv v6, v4, v3
# CHECK-INST: vfsub.vv v6, v4, v3
# CHECK-ENCODING: [0x57,0x93,0x41,0x0a]

vsub.vx v6, v4, gp
# CHECK-INST: vsub.vx v6, v4, gp
# CHECK-ENCODING: [0x57,0xc3,0x41,0x0a]

vfsub.vf v6, v4, gp
# CHECK-INST: vfsub.vf v6, v4, gp
# CHECK-ENCODING: [0x57,0xd3,0x41,0x0a]

vrsub.vi v6, v4, 3
# CHECK-INST: vrsub.vi v6, v4, 3
# CHECK-ENCODING: [0x57,0xb3,0x41,0x0e]

vrsub.vx v6, v4, gp
# CHECK-INST: vrsub.vx v6, v4, gp
# CHECK-ENCODING: [0x57,0xc3,0x41,0x0e]

vfrsub.vf v4, v2, ra
# CHECK-INST: vfrsub.vf v4, v2, ra
# CHECK-ENCODING: [0x57,0xd2,0x20,0x9e]
