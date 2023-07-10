# RUN: llvm-mc -triple=riscv32 -show-encoding --mattr=+v %s \
# RUN:  | FileCheck %s --check-prefixes=CHECK-ENCODING,CHECK-INST
# RUN: llvm-mc -triple=riscv32 -filetype=obj --mattr=+v %s \
# RUN:   | llvm-objdump -d --mattr=+v - \
# RUN:   | FileCheck %s --check-prefix=CHECK-INST


vlw12.v v6, 4(v1)
# CHECK-INST: vlw12.v v6, 4(v1)
# CHECK-ENCODING: [0x7b,0xa3,0x40,0x00]

vlh12.v v6, 4(v1)
# CHECK-INST: vlh12.v v6, 4(v1)
# CHECK-ENCODING: [0x7b,0x93,0x40,0x00]

vlb12.v v6, 4(v1)
# CHECK-INST: vlb12.v v6, 4(v1)
# CHECK-ENCODING: [0x7b,0x83,0x40,0x00]

vlhu12.v v6, 8(v1)
# CHECK-INST: vlhu12.v v6, 8(v1)
# CHECK-ENCODING: [0x7b,0xd3,0x80,0x00]

vlbu12.v v6, 12(v1)
# CHECK-INST: vlbu12.v v6, 12(v1)
# CHECK-ENCODING: [0x7b,0xc3,0xc0,0x00]

vlw.v v6, 12(v1)
# CHECK-INST: vlw.v v6, 12(v1)
# CHECK-ENCODING: [0x2b,0xa3,0xc0,0x00]

vlh.v v6, 12(v1)
# CHECK-INST: vlh.v v6, 12(v1)
# CHECK-ENCODING: [0x2b,0x93,0xc0,0x00]

vlb.v v6, 12(v1)
# CHECK-INST: vlb.v v6, 12(v1)
# CHECK-ENCODING: [0x2b,0x83,0xc0,0x00]

vlhu.v v6, 12(v1)
# CHECK-INST: vlhu.v v6, 12(v1)
# CHECK-ENCODING: [0x2b,0xd3,0xc0,0x00]

vlbu.v v6, 12(v1)
# CHECK-INST: vlbu.v v6, 12(v1)
# CHECK-ENCODING: [0x2b,0xc3,0xc0,0x00]
