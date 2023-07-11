# RUN: llvm-mc -triple=riscv32 -show-encoding --mattr=+v %s \
# RUN:  | FileCheck %s --check-prefixes=CHECK-ENCODING,CHECK-INST
# RUN: llvm-mc -triple=riscv32 -filetype=obj --mattr=+v %s \
# RUN:   | llvm-objdump -d --mattr=+v - \
# RUN:   | FileCheck %s --check-prefix=CHECK-INST

join v0, v0, 0
# CHECK-INST: join v0, v0, 0
# CHECK-ENCODING: [0x5b,0x20,0x00,0x00]

setrpc zero, x4, 256
# CHECK-INST: setrpc zero, tp, 256
# CHECK-ENCODING: [0x5b,0x30,0x02,0x10]

regext x0, x0, 8
# CHECK-INST: regext zero, zero, 8
# CHECK-ENCODING: [0x0b,0x20,0x80,0x00]

regexti x0, x0, 8
# CHECK-INST: regexti zero, zero, 8
# CHECK-ENCODING: [0x0b,0x30,0x80,0x00]

endprg x0, x0, x0
# CHECK-INST: endprg x0, x0, x0
# CHECK-ENCODING: [0x0b,0x40,0x00,0x00]

barrier x0, x0, 4
# CHECK-INST: barrier x0, x0, 4
# CHECK-ENCODING: [0x0b,0x40,0x02,0x04]

barriersub x0, x0, 4
# CHECK-INST: barriersub x0, x0, 4
# CHECK-ENCODING: [0x0b,0x40,0x02,0x06]

vadd12.vi v1, v0, 4
# CHECK-INST: vadd12.vi v1, v0, 4
# CHECK-ENCODING: [0x8b,0x00,0x40,0x00]
