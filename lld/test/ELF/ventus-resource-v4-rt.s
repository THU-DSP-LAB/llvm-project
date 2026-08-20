# RUN: rm -rf %t && split-file %s %t
# RUN: llvm-mc -filetype=obj -triple=riscv32 %t/valid.s -o %t/valid.o
# RUN: ld.lld -m elf32lriscv -e rt_kernel %t/valid.o -o %t/valid
# RUN: llvm-objdump -s --section=.ventus.resource.rt_kernel %t/valid | FileCheck %s --check-prefix=DATA
# RUN: llvm-mc -filetype=obj -triple=riscv32 %t/invalid.s -o %t/invalid.o
# RUN: not ld.lld -m elf32lriscv -e rt_kernel %t/invalid.o -o %t/invalid 2>&1 | FileCheck %s --check-prefix=INVALID

# DATA:      Contents of section .ventus.resource.rt_kernel:
# DATA-NEXT: {{[0-9a-f]+}} 04000000 00000000 03000000 00000000
# DATA-NEXT: {{[0-9a-f]+}} 05000000 00000000 00000000 00000000
# DATA-NEXT: {{[0-9a-f]+}} 00020000 00000000 00000000 00000000
# DATA-NEXT: {{[0-9a-f]+}} 00000000 00000000 03000000 05000000
# DATA-NEXT: {{[0-9a-f]+}} 05000000 00000000 80010000 80010000
# DATA-NEXT: {{[0-9a-f]+}} 80000000 60000000 00000000 00000000

# INVALID: invalid Ventus V4 RT resource for rt_kernel: PDS range exceeds linked PDS static allocation

#--- valid.s
  .text
  .globl rt_kernel
rt_kernel:
  ret
  .size rt_kernel, .-rt_kernel

  .data
  .globl pds_obj
pds_obj:
  .space 512
  .size pds_obj, .-pds_obj

  .section .ventus.resobj,"",@progbits
  .balign 8
  .long 0x5652534f
  .long 4
  .long 0
  .long 1
  .long 0
  .long 1
  .long 36
  .long 140
  .long 140

  .long rt_kernel
  .long 1
  .quad 3
  .quad 5
  .quad 0
  .quad 0
  .quad 0
  .long 0
  .long 0
  .long 0
  .long 1
  .long 3
  .long 5
  .long 5
  .long 0
  .long 384
  .long 384
  .long 128
  .long 96
  .long 0
  .long 0

  .long pds_obj
  .long 2
  .quad 512
  .quad 0

#--- invalid.s
  .text
  .globl rt_kernel
rt_kernel:
  ret
  .size rt_kernel, .-rt_kernel

  .data
  .globl pds_obj
pds_obj:
  .space 400
  .size pds_obj, .-pds_obj

  .section .ventus.resobj,"",@progbits
  .balign 8
  .long 0x5652534f
  .long 4
  .long 0
  .long 1
  .long 0
  .long 1
  .long 36
  .long 140
  .long 140

  .long rt_kernel
  .long 1
  .quad 3
  .quad 5
  .quad 0
  .quad 0
  .quad 0
  .long 0
  .long 0
  .long 0
  .long 1
  .long 3
  .long 5
  .long 5
  .long 0
  .long 384
  .long 384
  .long 128
  .long 96
  .long 0
  .long 0

  .long pds_obj
  .long 2
  .quad 400
  .quad 0
