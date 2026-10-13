# RUN: rm -rf %t && split-file %s %t
# RUN: llvm-mc -filetype=obj -triple=riscv32 %t/kernel.s -o %t/kernel.o
# RUN: llvm-mc -filetype=obj -triple=riscv32 %t/helper.s -o %t/helper.o
# RUN: llvm-mc -filetype=obj -triple=riscv32 %t/alias.s -o %t/alias.o
# RUN: ld.lld -m elf32lriscv -e kernel %t/kernel.o %t/helper.o -o %t/out
# RUN: llvm-objdump -s --section=.ventus.resource.kernel %t/out \
# RUN:   | FileCheck %s --check-prefix=DATA
# RUN: llvm-objdump -d %t/out | FileCheck %s --check-prefix=DISASM
# RUN: ld.lld -m elf32lriscv -e alias_kernel %t/alias.o -o %t/alias-out
# RUN: llvm-objdump -s --section=.ventus.resource.alias_kernel %t/alias-out \
# RUN:   | FileCheck %s --check-prefix=ALIAS-DATA
# RUN: llvm-objdump -d %t/alias-out | FileCheck %s --check-prefix=ALIAS-DISASM
# RUN: ld.lld -m elf32lriscv -r %t/kernel.o %t/helper.o -o %t/merged.o
# RUN: ld.lld -m elf32lriscv -e kernel %t/merged.o -o %t/out-r
# RUN: llvm-objdump -s --section=.ventus.resource.kernel %t/out-r \
# RUN:   | FileCheck %s --check-prefix=DATA
# RUN: llvm-objdump -d %t/out-r | FileCheck %s --check-prefix=DISASM
# RUN: llvm-mc -filetype=obj -triple=riscv32 %t/bad-align.s -o %t/bad-align.o
# RUN: not ld.lld -m elf32lriscv -e bad_align_kernel %t/bad-align.o -o /dev/null 2>&1 \
# RUN:   | FileCheck %s --check-prefix=BAD-ALIGN
# RUN: llvm-mc -filetype=obj -triple=riscv32 %t/z-local.s -o %t/z-local.o
# RUN: llvm-mc -filetype=obj -triple=riscv32 %t/a-local.s -o %t/a-local.o
# RUN: ld.lld -m elf32lriscv -e same_name_kernel %t/z-local.o %t/a-local.o -o %t/local-same
# RUN: llvm-objdump -d %t/local-same | FileCheck %s --check-prefix=LOCAL-SAME-DISASM

# DATA:      Contents of section .ventus.resource.kernel:
# DATA-NEXT: {{[0-9a-f]+}} 03000000 00000000 07000000 00000000
# DATA-NEXT: {{[0-9a-f]+}} 09000000 00000000 30000000 00000000
# DATA-NEXT: {{[0-9a-f]+}} 00000000 00000000 00000000 00000000
# DATA-NEXT: {{[0-9a-f]+}} 00000000 00000000

# DISASM-LABEL: <kernel>:
# DISASM:       lui a0, 0
# DISASM-NEXT:  sw zero, 32(a0)
# DISASM-NEXT:  addi a0, a0, 32
# DISASM-LABEL: <helper>:
# DISASM:       lui a0, 0
# DISASM-NEXT:  sw zero, 0(a0)
# DISASM-NEXT:  mv a0, a0

# ALIAS-DATA:      Contents of section .ventus.resource.alias_kernel:
# ALIAS-DATA-NEXT: {{[0-9a-f]+}} 03000000 00000000 03000000 00000000
# ALIAS-DATA-NEXT: {{[0-9a-f]+}} 05000000 00000000 10000000 00000000
# ALIAS-DATA-NEXT: {{[0-9a-f]+}} 00000000 00000000 00000000 00000000
# ALIAS-DATA-NEXT: {{[0-9a-f]+}} 00000000 00000000

# ALIAS-DISASM-LABEL: <alias_kernel>:
# ALIAS-DISASM:       lui a0, 0
# ALIAS-DISASM-NEXT:  sw zero, 0(a0)
# ALIAS-DISASM-NEXT:  mv a0, a0
# ALIAS-DISASM-NEXT:  lui a0, 0
# ALIAS-DISASM-NEXT:  sw zero, 0(a0)
# ALIAS-DISASM-NEXT:  mv a0, a0

# BAD-ALIGN: error: invalid Ventus LDS static alignment for symbol bad_align_lds: 6

# LOCAL-SAME-DISASM-LABEL: <same_name_kernel>:
# LOCAL-SAME-DISASM:       lui a0, 0
# LOCAL-SAME-DISASM-NEXT:  mv a0, a0
# LOCAL-SAME-DISASM-LABEL: <same_name_helper>:
# LOCAL-SAME-DISASM:       lui a0, 0
# LOCAL-SAME-DISASM-NEXT:  addi a0, a0, 8

#--- kernel.s
  .text
  .globl kernel
kernel:
  lui a0, %ventus_lds_hi(kernel_lds)
  sw zero, %ventus_lds_lo(kernel_lds)(a0)
  addi a0, a0, %ventus_lds_lo(kernel_lds)
  ret
  .size kernel, .-kernel

  .data
  .globl kernel_lds
kernel_lds:
  .space 16
  .size kernel_lds, .-kernel_lds

  .section .ventus.resobj,"",@progbits
  .balign 8
  .long 0x5652534f
  .long 3
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
  .quad 0
  .quad 0
  .quad 0
  .long 0
  .long 1
  .long 0
  .long 1

  .long helper
  .long 0

  .long kernel_lds
  .long 1
  .quad 16
  .quad 16

#--- helper.s
  .text
  .globl helper
helper:
  lui a0, %ventus_lds_hi(helper_lds)
  sw zero, %ventus_lds_lo(helper_lds)(a0)
  addi a0, a0, %ventus_lds_lo(helper_lds)
  ret
  .size helper, .-helper

  .data
  .globl helper_lds
helper_lds:
  .space 32
  .size helper_lds, .-helper_lds

  .section .ventus.resobj,"",@progbits
  .balign 8
  .long 0x5652534f
  .long 3
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
  .quad 0
  .quad 0
  .quad 0
  .long 0
  .long 0
  .long 0
  .long 1

  .long helper_lds
  .long 1
  .quad 32
  .quad 32

#--- alias.s
  .text
  .globl alias_kernel
alias_kernel:
  lui a0, %ventus_lds_hi(alias_lds)
  sw zero, %ventus_lds_lo(alias_lds)(a0)
  addi a0, a0, %ventus_lds_lo(alias_lds)
  lui a0, %ventus_lds_hi(alias_lds_alias)
  sw zero, %ventus_lds_lo(alias_lds_alias)(a0)
  addi a0, a0, %ventus_lds_lo(alias_lds_alias)
  ret
  .size alias_kernel, .-alias_kernel

  .data
  .globl alias_lds
alias_lds:
alias_lds_alias:
  .space 16
  .size alias_lds, .-alias_lds
  .size alias_lds_alias, .-alias_lds_alias

  .section .ventus.resobj,"",@progbits
  .balign 8
  .long 0x5652534f
  .long 3
  .long 0
  .long 1
  .long 0
  .long 2
  .long 36
  .long 100
  .long 100

  .long alias_kernel
  .long 1
  .quad 3
  .quad 5
  .quad 0
  .quad 0
  .quad 0
  .long 0
  .long 0
  .long 0
  .long 2

  .long alias_lds
  .long 1
  .quad 16
  .quad 4

  .long alias_lds_alias
  .long 1
  .quad 16
  .quad 4

#--- bad-align.s
  .text
  .globl bad_align_kernel
bad_align_kernel:
  ret
  .size bad_align_kernel, .-bad_align_kernel

  .data
  .globl bad_align_lds
bad_align_lds:
  .space 4
  .size bad_align_lds, .-bad_align_lds

  .section .ventus.resobj,"",@progbits
  .balign 8
  .long 0x5652534f
  .long 3
  .long 0
  .long 1
  .long 0
  .long 1
  .long 36
  .long 100
  .long 100

  .long bad_align_kernel
  .long 1
  .quad 0
  .quad 0
  .quad 0
  .quad 0
  .quad 0
  .long 0
  .long 0
  .long 0
  .long 1

  .long bad_align_lds
  .long 1
  .quad 4
  .quad 6

#--- z-local.s
  .text
  .globl same_name_kernel
same_name_kernel:
  lui a0, %ventus_lds_hi(same_lds)
  addi a0, a0, %ventus_lds_lo(same_lds)
  ret
  .size same_name_kernel, .-same_name_kernel

  .data
same_lds:
  .space 8
  .size same_lds, .-same_lds

  .section .ventus.resobj,"",@progbits
  .balign 8
  .long 0x5652534f
  .long 3
  .long 0
  .long 1
  .long 1
  .long 1
  .long 36
  .long 100
  .long 108

  .long same_name_kernel
  .long 1
  .quad 0
  .quad 0
  .quad 0
  .quad 0
  .quad 0
  .long 0
  .long 1
  .long 0
  .long 1

  .long same_name_helper
  .long 0

  .long same_lds
  .long 1
  .quad 8
  .quad 4

#--- a-local.s
  .text
  .globl same_name_helper
same_name_helper:
  lui a0, %ventus_lds_hi(same_lds)
  addi a0, a0, %ventus_lds_lo(same_lds)
  ret
  .size same_name_helper, .-same_name_helper

  .data
same_lds:
  .space 8
  .size same_lds, .-same_lds

  .section .ventus.resobj,"",@progbits
  .balign 8
  .long 0x5652534f
  .long 3
  .long 0
  .long 1
  .long 0
  .long 1
  .long 36
  .long 100
  .long 100

  .long same_name_helper
  .long 0
  .quad 0
  .quad 0
  .quad 0
  .quad 0
  .quad 0
  .long 0
  .long 0
  .long 0
  .long 1

  .long same_lds
  .long 1
  .quad 8
  .quad 4
