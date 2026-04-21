# RUN: rm -rf %t && split-file %s %t
# RUN: llvm-mc -filetype=obj -triple=riscv32 %t/kernel.s -o %t/kernel.o
# RUN: ld.lld -m elf32lriscv -T %t/linker.ld -e kernel %t/kernel.o -o %t/out
# RUN: llvm-readobj -S %t/out | FileCheck %s --check-prefix=SECTIONS
# RUN: llvm-objdump -s --section=.pack %t/out | FileCheck %s --check-prefix=DATA

# SECTIONS: Name: .pack
# SECTIONS-NOT: .ventus.resource.kernel
# DATA:      Contents of section .pack:
# DATA-NEXT: {{[0-9a-f]+}} 03000000 00000000 03000000 00000000
# DATA-NEXT: {{[0-9a-f]+}} 05000000 00000000 00000000 00000000
# DATA-NEXT: {{[0-9a-f]+}} 00000000 00000000 10000000 00000000
# DATA-NEXT: {{[0-9a-f]+}} 0c000000 00000000

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

#--- linker.ld
SECTIONS {
  .pack : {
    before = .;
    *(.ventus.resource.*)
    after = .;
  }
}
