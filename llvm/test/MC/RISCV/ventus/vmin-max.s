# RUN: llvm-mc -triple=riscv32 -show-encoding --mattr=+v %s \
# RUN:  | FileCheck %s --check-prefixes=CHECK-ENCODING,CHECK-INST
# RUN: llvm-mc -triple=riscv32 -filetype=obj --mattr=+v %s \
# RUN:   | llvm-objdump -d --mattr=+v - \
# RUN:   | FileCheck %s --check-prefix=CHECK-INST


vminu.vv v6, v4, v3
# CHECK-INST: vminu.vv v6, v4, v3
# CHECK-ENCODING: [0x57,0x83,0x41,0x12]

vfmin.vv v6, v4, v3
# CHECK-INST: vfmin.vv v6, v4, v3
# CHECK-ENCODING: [0x57,0x93,0x41,0x12]

vminu.vx v6, v4, gp
# CHECK-INST: vminu.vx v6, v4, gp
# CHECK-ENCODING: [0x57,0xc3,0x41,0x12]

vfmin.vf v6, v4, gp
# CHECK-INST: vfmin.vf v6, v4, gp
# CHECK-ENCODING: [0x57,0xd3,0x41,0x12]

vmin.vv v6, v4, v3
# CHECK-INST: vmin.vv v6, v4, v3
# CHECK-ENCODING: [0x57,0x83,0x41,0x16]

vmin.vx v6, v4, gp
# CHECK-INST: vmin.vx v6, v4, gp
# CHECK-ENCODING: [0x57,0xc3,0x41,0x16]

vmaxu.vv v6, v4, v3
# CHECK-INST: vmaxu.vv v6, v4, v3
# CHECK-ENCODING: [0x57,0x83,0x41,0x1a]

vfmax.vv v6, v4, v3
# CHECK-INST: vfmax.vv v6, v4, v3
# CHECK-ENCODING: [0x57,0x93,0x41,0x1a]

vmaxu.vx v6, v4, gp
# CHECK-INST: vmaxu.vx v6, v4, gp
# CHECK-ENCODING: [0x57,0xc3,0x41,0x1a]

vfmax.vf v6, v4, gp
# CHECK-INST: vfmax.vf v6, v4, gp
# CHECK-ENCODING: [0x57,0xd3,0x41,0x1a]

vmax.vv v6, v4, v3
# CHECK-INST: vmax.vv v6, v4, v3
# CHECK-ENCODING: [0x57,0x83,0x41,0x1e]

vmax.vx v6, v4, gp
# CHECK-INST: vmax.vx v6, v4, gp
# CHECK-ENCODING: [0x57,0xc3,0x41,0x1e]




