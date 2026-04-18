# RUN: rm -rf %t && split-file %s %t
# RUN: llvm-mc -filetype=obj -triple=riscv32 %t/kernel.s -o %t/kernel.o
# RUN: llvm-mc -filetype=obj -triple=riscv32 %t/helper.s -o %t/helper.o
# RUN: ld.lld -m elf32lriscv -e kernel %t/kernel.o %t/helper.o -o %t/out
# RUN: llvm-readobj -S %t/out | FileCheck %s --check-prefix=SECTIONS
# RUN: llvm-objdump -s --section=.ventus.resource.kernel %t/out | FileCheck %s --check-prefix=DATA

# SECTIONS: Name: .ventus.resource.kernel
# SECTIONS-NOT: .ventus.resobj

# DATA:      Contents of section .ventus.resource.kernel:
# DATA-NEXT: {{[0-9a-f]+}} 02000000 30000000 03000000 00000000
# DATA-NEXT: {{[0-9a-f]+}} 05000000 00000000 00000000 00000000
# DATA-NEXT: {{[0-9a-f]+}} 00000000 00000000 ffffffff ffffffff
# DATA-NEXT: {{[0-9a-f]+}} ffffffff ffffffff

#--- kernel.s
  .text
  .globl kernel
kernel:
  ret
  .size kernel, .-kernel

  .section .ventus.resobj,"",@progbits
  .balign 8
  .long 0x5652534f
  .long 1
  .long 0
  .long 1
  .long 1
  .long 0
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
  .long 0

  .long helper
  .long 0

#--- helper.s
  .text
  .globl helper
helper:
  ret
  .size helper, .-helper
