# RUN: llvm-mc -triple=riscv32 -show-encoding --mattr=+v %s \
# RUN:  | FileCheck %s --check-prefixes=CHECK-ENCODING,CHECK-INST
# RUN: llvm-mc -triple=riscv32 -filetype=obj --mattr=+v %s \
# RUN:   | llvm-objdump -d --mattr=+v - \
# RUN:   | FileCheck %s --check-prefix=CHECK-INST


vfsgnj.vv v4, v2, v1
# CHECK-INST: vfsgnj.vv v4, v2, v1
# CHECK-ENCODING: [0x57,0x92,0x20,0x22]

vfsgnj.vf v4, v2, ra
# CHECK-INST: vfsgnj.vf v4, v2, ra
# CHECK-ENCODING: [0x57,0xd2,0x20,0x22]


vfsgnjn.vv v4, v2, v1
# CHECK-INST: vfsgnjn.vv v4, v2, v1
# CHECK-ENCODING: [0x57,0x92,0x20,0x26]

vfsgnjn.vf v4, v2, ra
# CHECK-INST: vfsgnjn.vf v4, v2, ra
# CHECK-ENCODING: [0x57,0xd2,0x20,0x26]


vfsgnjx.vv v4, v2, v1
# CHECK-INST: vfsgnjx.vv v4, v2, v1
# CHECK-ENCODING: [0x57,0x92,0x20,0x2a]

vfsgnjx.vf v4, v2, ra
# CHECK-INST: vfsgnjx.vf v4, v2, ra
# CHECK-ENCODING: [0x57,0xd2,0x20,0x2a]




