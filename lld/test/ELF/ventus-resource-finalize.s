# RUN: rm -rf %t && split-file %s %t
# RUN: llvm-mc -filetype=obj -triple=riscv32 %t/kernel.s -o %t/kernel.o
# RUN: llvm-mc -filetype=obj -triple=riscv32 %t/helper.s -o %t/helper.o
# RUN: ld.lld -m elf32lriscv -e kernel %t/kernel.o %t/helper.o -o %t/out
# RUN: llvm-readobj -S %t/out | FileCheck %s --check-prefix=SECTIONS
# RUN: llvm-objdump -s --section=.ventus.resource.kernel %t/out | FileCheck %s --check-prefix=DATA

# SECTIONS: Name: .ventus.resource.kernel
# SECTIONS-NOT: .ventus.resobj

# DATA:      Contents of section .ventus.resource.kernel:
# DATA-NEXT: {{[0-9a-f]+}} 02000000 00000000 07000000 00000000
# DATA-NEXT: {{[0-9a-f]+}} 09000000 00000000 20000000 00000000
# DATA-NEXT: {{[0-9a-f]+}} 14000000 00000000 28000000 00000000
# DATA-NEXT: {{[0-9a-f]+}} 20000000 00000000

#--- kernel.s
  .text
  .globl kernel
kernel:
  ret
  .size kernel, .-kernel

  .data
  .globl lds_obj
lds_obj:
  .space 32
  .size lds_obj, .-lds_obj

  .section .ventus.resource.kernel,"w",@progbits
  .long 0xdeadbeef
  .long 0xcafebabe

  .section .ventus.resobj,"",@progbits
  .balign 8
  .long 0x5652534f
  .long 1
  .long 0
  .long 1
  .long 1
  .long 1
  .long 36
  .long 100
  .long 108

  .long kernel
  .long 1
  .quad 3
  .quad 5
  .quad 16
  .quad 8
  .quad 4
  .long 0
  .long 1
  .long 0
  .long 1

  .long helper
  .long 0

  .long lds_obj
  .long 1
  .quad 32

#--- helper.s
  .text
  .globl helper
helper:
  ret
  .size helper, .-helper

  .data
  .globl pds_obj
pds_obj:
  .space 20
  .size pds_obj, .-pds_obj

  .section .ventus.resobj,"",@progbits
  .balign 8
  .long 0x5652534f
  .long 1
  .long 0
  .long 1
  .long 0
  .long 1
  .long 36
  .long 100
  .long 100

  .long helper
  .long 0
  .quad 7
  .quad 9
  .quad 24
  .quad 12
  .quad 8
  .long 0
  .long 0
  .long 0
  .long 1

  .long pds_obj
  .long 2
  .quad 20
