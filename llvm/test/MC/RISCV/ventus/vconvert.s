# RUN: llvm-mc -triple=riscv32 -show-encoding --mattr=+v %s \
# RUN:  | FileCheck %s --check-prefixes=CHECK-ENCODING,CHECK-INST
# RUN: llvm-mc -triple=riscv32 -filetype=obj --mattr=+v %s \
# RUN:   | llvm-objdump -d --mattr=+v - \
# RUN:   | FileCheck %s --check-prefix=CHECK-INST


vfcvt.xu.f.v v4, v2
# CHECK-INST: vfcvt.xu.f.v v4, v2
# CHECK-ENCODING: [0x57,0x12,0x20,0x4a]

vfcvt.x.f.v v4, v2
# CHECK-INST: vfcvt.x.f.v v4, v2
# CHECK-ENCODING: [0x57,0x92,0x20,0x4a]

vfcvt.f.xu.v v4, v2
# CHECK-INST: vfcvt.f.xu.v v4, v2
# CHECK-ENCODING: [0x57,0x12,0x21,0x4a]

vfcvt.f.x.v v4, v2
# CHECK-INST: vfcvt.f.x.v v4, v2
# CHECK-ENCODING: [0x57,0x92,0x21,0x4a]

vfcvt.rtz.xu.f.v v4, v2
# CHECK-INST: vfcvt.rtz.xu.f.v v4, v2
# CHECK-ENCODING: [0x57,0x12,0x23,0x4a]

vfcvt.rtz.x.f.v v4, v2
# CHECK-INST: vfcvt.rtz.x.f.v v4, v2
# CHECK-ENCODING: [0x57,0x92,0x23,0x4a]







