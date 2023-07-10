# RUN: llvm-mc -triple=riscv32 -show-encoding --mattr=+v %s \
# RUN:  | FileCheck %s --check-prefixes=CHECK-ENCODING,CHECK-INST
# RUN: llvm-mc -triple=riscv32 -filetype=obj --mattr=+v %s \
# RUN:   | llvm-objdump -d --mattr=+v - \
# RUN:   | FileCheck %s --check-prefix=CHECK-INST

vsw12.v v6, 4(v1)
# CHECK-INST: vsw12.v v6, 4(v1)
# CHECK-ENCODING: [0x7b,0xe2,0x60,0x00]

vsh12.v v6, 4(v1)
# CHECK-INST: vsh12.v v6, 4(v1)
# CHECK-ENCODING: [0x7b,0xb2,0x60,0x00]

vsb12.v v6, 4(v1)
# CHECK-INST: vsb12.v v6, 4(v1)
# CHECK-ENCODING: [0x7b,0xf2,0x60,0x00]

vsw.v v6, 4(v1)
# CHECK-INST: vsw.v v6, 4(v1)
# CHECK-ENCODING: [0x2b,0xa2,0x60,0x80]

vsh.v v6, 4(v1)
# CHECK-INST: vsh.v v6, 4(v1)
# CHECK-ENCODING: [0x2b,0x92,0x60,0x80]

vsb.v v6, 4(v1)
# CHECK-INST: vsb.v v6, 4(v1)
# CHECK-ENCODING: [0x2b,0x82,0x60,0x80]
