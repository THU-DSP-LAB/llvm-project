# RUN: rm -rf %t && split-file %s %t
# RUN: llvm-mc -filetype=obj -triple=riscv32 %t/new.s -o %t/new.o
# RUN: llvm-mc -filetype=obj -triple=riscv32 %t/old.s -o %t/old.o
# RUN: not ld.lld -m elf32lriscv -e kernel %t/new.o %t/old.o -o %t/out 2>&1 \
# RUN:   | FileCheck %s --check-prefix=ERR

# ERR: old.o: mixes legacy .ventus.resource.* input with link-time finalization

#--- new.s
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
  .long 0
  .long 0
  .long 36
  .long 100
  .long 100

  .long kernel
  .long 1
  .quad 3
  .quad 5
  .quad 16
  .quad 8
  .quad 4
  .long 0
  .long 0
  .long 0
  .long 0

#--- old.s
  .section .ventus.resource.legacy,"w",@progbits
  .balign 8
  .long 2
  .long 0
  .quad 1
  .quad 1
  .quad 0
  .quad 0
  .quad 0
  .quad 0
