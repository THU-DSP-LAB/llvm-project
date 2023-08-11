# RUN: llvm-mc -triple=riscv32 -show-encoding --mattr=+v %s \
# RUN:  | FileCheck %s --check-prefixes=CHECK-ENCODING,CHECK-INST
# RUN: llvm-mc -triple=riscv32 -filetype=obj --mattr=+v %s \
# RUN:   | llvm-objdump -d --mattr=+v - \
# RUN:   | FileCheck %s --check-prefix=CHECK-INST


vmseq.vv v4, v2, v1
# CHECK-INST: vmseq.vv v4, v2, v1
# CHECK-ENCODING: [0x57,0x82,0x20,0x62]

vmfeq.vv v4, v2, v1
# CHECK-INST: vmfeq.vv v4, v2, v1
# CHECK-ENCODING: [0x57,0x92,0x20,0x62]

# SKIP VMANDNOT_MM

vmseq.vi v4, v2, 1
# CHECK-INST: vmseq.vi v4, v2, 1
# CHECK-ENCODING: [0x57,0xb2,0x20,0x62]

vmseq.vx v4, v2, ra
# CHECK-INST: vmseq.vx v4, v2, ra
# CHECK-ENCODING: [0x57,0xc2,0x20,0x62]

vmfeq.vf v4, v2, ra
# CHECK-INST: vmfeq.vf v4, v2, ra
# CHECK-ENCODING: [0x57,0xd2,0x20,0x62]


vmsne.vv v4, v2, v1
# CHECK-INST: vmsne.vv v4, v2, v1
# CHECK-ENCODING: [0x57,0x82,0x20,0x66]

vmfle.vv v4, v2, v1
# CHECK-INST: vmfle.vv v4, v2, v1
# CHECK-ENCODING: [0x57,0x92,0x20,0x66]

# SKIP VMAND_MM

vmsne.vi v4, v2, 1
# CHECK-INST: vmsne.vi v4, v2, 1
# CHECK-ENCODING: [0x57,0xb2,0x20,0x66]

vmsne.vx v4, v2, ra
# CHECK-INST: vmsne.vx v4, v2, ra
# CHECK-ENCODING: [0x57,0xc2,0x20,0x66]

vmfle.vf v4, v2, ra
# CHECK-INST: vmfle.vf v4, v2, ra
# CHECK-ENCODING: [0x57,0xd2,0x20,0x66]


vmsltu.vv v4, v2, v1
# CHECK-INST: vmsltu.vv v4, v2, v1
# CHECK-ENCODING: [0x57,0x82,0x20,0x6a]

# SKIP VMOR_MM

vmsltu.vx v4, v2, ra
# CHECK-INST: vmsltu.vx v4, v2, ra
# CHECK-ENCODING: [0x57,0xc2,0x20,0x6a]


vmslt.vv v4, v2, v1
# CHECK-INST: vmslt.vv v4, v2, v1
# CHECK-ENCODING: [0x57,0x82,0x20,0x6e]

vmflt.vv v4, v2, v1
# CHECK-INST: vmflt.vv v4, v2, v1
# CHECK-ENCODING: [0x57,0x92,0x20,0x6e]

# SKIP VMXOR_MM

vmslt.vx v4, v2, ra
# CHECK-INST: vmslt.vx v4, v2, ra
# CHECK-ENCODING: [0x57,0xc2,0x20,0x6e]

vmflt.vf v4, v2, ra
# CHECK-INST: vmflt.vf v4, v2, ra
# CHECK-ENCODING: [0x57,0xd2,0x20,0x6e]


vmsleu.vv v4, v2, v1
# CHECK-INST: vmsleu.vv v4, v2, v1
# CHECK-ENCODING: [0x57,0x82,0x20,0x72]

vmfne.vv v4, v2, v1
# CHECK-INST: vmfne.vv v4, v2, v1
# CHECK-ENCODING: [0x57,0x92,0x20,0x72]

# SKIP VMORNOT_MM

vmsleu.vi v4, v2, 1
# CHECK-INST: vmsleu.vi v4, v2, 1
# CHECK-ENCODING: [0x57,0xb2,0x20,0x72]

vmsleu.vx v4, v2, ra
# CHECK-INST: vmsleu.vx v4, v2, ra
# CHECK-ENCODING: [0x57,0xc2,0x20,0x72]

vmfne.vf v4, v2, ra
# CHECK-INST: vmfne.vf v4, v2, ra
# CHECK-ENCODING: [0x57,0xd2,0x20,0x72]

vmsle.vv v4, v2, v1
# CHECK-INST: vmsle.vv v4, v2, v1
# CHECK-ENCODING: [0x57,0x82,0x20,0x76]

# SKIP VMNAND_MM

vmsle.vi v4, v2, 1
# CHECK-INST: vmsle.vi v4, v2, 1
# CHECK-ENCODING: [0x57,0xb2,0x20,0x76]

vmsle.vx v4, v2, ra
# CHECK-INST: vmsle.vx v4, v2, ra
# CHECK-ENCODING: [0x57,0xc2,0x20,0x76]

vmfgt.vf v4, v2, ra
# CHECK-INST: vmfgt.vf v4, v2, ra
# CHECK-ENCODING: [0x57,0xd2,0x20,0x76]


# SKIP VMNOR_MM

vmsgtu.vi v4, v2, 1
# CHECK-INST: vmsgtu.vi v4, v2, 1
# CHECK-ENCODING: [0x57,0xb2,0x20,0x7a]

vmsgtu.vx v4, v2, ra
# CHECK-INST: vmsgtu.vx v4, v2, ra
# CHECK-ENCODING: [0x57,0xc2,0x20,0x7a]


# SKIP VMXNOR_MM

vmsgt.vi v4, v2, 1
# CHECK-INST: vmsgt.vi v4, v2, 1
# CHECK-ENCODING: [0x57,0xb2,0x20,0x7e]

vmsgt.vx v4, v2, ra
# CHECK-INST: vmsgt.vx v4, v2, ra
# CHECK-ENCODING: [0x57,0xc2,0x20,0x7e]

vmfge.vf v4, v2, ra
# CHECK-INST: vmfge.vf v4, v2, ra
# CHECK-ENCODING: [0x57,0xd2,0x20,0x7e]

