# RUN: not llvm-mc -triple riscv32  -mattr=+v %s 2>&1 | FileCheck %s

# Invalid operands
vsw12.v v3, 4(t1) # CHECK: :[[@LINE]]:15: error: invalid operand for instruction

vsw.v v3, 4(t1) # CHECK: :[[@LINE]]:13: error: invalid operand for instruction

vadd.vi v2, v1, 16 #CHECK: :[[@LINE]]:17: error: immediate must be an integer in the range [-16, 15]

vrsub.vi v2, v1, -17 #CHECK: :[[@LINE]]:18: error: immediate must be an integer in the range [-16, 15]

vsll.vi v2, v1, -1 #CHECK: :[[@LINE]]:17: error: immediate must be an integer in the range [0, 31]

vsra.vi v2, v1, 32 #CHECK: :[[@LINE]]:17: error: immediate must be an integer in the range [0, 31]

vbeq v3, v2, 0x2048 #CHECK: :[[@LINE]]:14: error: immediate must be a multiple of 2 bytes in the range [-4096, 4094]

fadd.s a1, a3, v2 #CHECK: :[[@LINE]]:16: error: invalid operand for instruction
