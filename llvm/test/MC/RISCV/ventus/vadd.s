# RUN: llvm-mc -triple=riscv32 -show-encoding --mattr=+v %s \
# RUN:  | FileCheck %s --check-prefixes=CHECK-ENCODING,CHECK-INST
# RUN: llvm-mc -triple=riscv32 -filetype=obj --mattr=+v %s \
# RUN:   | llvm-objdump -d --mattr=+v - \
# RUN:   | FileCheck %s --check-prefix=CHECK-INST


vadd.vv v6, v4, v3
# CHECK-INST: vadd.vv v6, v4, v3
# CHECK-ENCODING: [0x57,0x83,0x41,0x02]

vfadd.vv v6, v4, v3
# CHECK-INST: vfadd.vv v6, v4, v3
# CHECK-ENCODING: [0x57,0x93,0x41,0x02]

vadd.vi v6, v4, 3
# CHECK-INST: vadd.vi v6, v4, 3
# CHECK-ENCODING: [0x57,0xb3,0x41,0x02]

vadd.vx v6, v4, gp
# CHECK-INST: vadd.vx v6, v4, gp
# CHECK-ENCODING: [0x57,0xc3,0x41,0x02]

vfadd.vf v6, v4, gp
# CHECK-INST: vfadd.vf v6, v4, gp
# CHECK-ENCODING: [0x57,0xd3,0x41,0x02]

