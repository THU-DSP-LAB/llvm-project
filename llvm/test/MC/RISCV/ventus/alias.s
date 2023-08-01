# RUN: llvm-mc -triple=riscv32 -show-encoding --mattr=+v %s \
# RUN:  | FileCheck %s --check-prefixes=CHECK-ENCODING,CHECK-INST
# RUN: llvm-mc -triple=riscv32 -filetype=obj --mattr=+v %s \
# RUN:   | llvm-objdump -d --mattr=+v - \
# RUN:   | FileCheck %s --check-prefix=CHECK-INST

vmslt.vi	v2, v3, 5
# CHECK-INST: vmsle.vi v2, v3, 4
# CHECK-ENCODING: [0x57,0x31,0x32,0x76]

vmsltu.vi	v2, v3, 5
# CHECK-INST: vmsleu.vi v2, v3, 4
# CHECK-ENCODING: [0x57,0x31,0x32,0x72]

vmsge.vi	v2, v7, 8
# CHECK-INST: vmsgt.vi v2, v7, 7
# CHECK-ENCODING: [0x57,0xb1,0x73,0x7e]

vmsgeu.vi	v2, v7, 8
# CHECK-INST: vmsgtu.vi v2, v7, 7
# CHECK-ENCODING: [0x57,0xb1,0x73,0x7a]
