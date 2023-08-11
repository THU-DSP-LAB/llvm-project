# RUN: llvm-mc -triple=riscv32 -show-encoding --mattr=+v %s \
# RUN:  | FileCheck %s --check-prefixes=CHECK-ENCODING,CHECK-INST
# RUN: llvm-mc -triple=riscv32 -filetype=obj --mattr=+v %s \
# RUN:   | llvm-objdump -d --mattr=+v - \
# RUN:   | FileCheck %s --check-prefix=CHECK-INST



vfmsac.vv v4, v2, v1
# CHECK-INST: vfmsac.vv v4, v2, v1
# CHECK-ENCODING: [0x57,0x92,0x20,0xba]

vfmsac.vf v4, v2, ra
# CHECK-INST: vfmsac.vf v4, v2, ra
# CHECK-ENCODING: [0x57,0xd2,0x20,0xba]


vfnmsac.vv v4, v2, v1
# CHECK-INST: vfnmsac.vv v4, v2, v1
# CHECK-ENCODING: [0x57,0x92,0x20,0xbe]

vfnmsac.vf v4, v2, ra
# CHECK-INST: vfnmsac.vf v4, v2, ra
# CHECK-ENCODING: [0x57,0xd2,0x20,0xbe]