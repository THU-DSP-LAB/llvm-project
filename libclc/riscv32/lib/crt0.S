/**
 * Copyright (c) 2023 Terapines Technology (Wuhan) Co., Ltd
 *
 * Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://llvm.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

/**
 * crt0.S : Entry point for Ventus OpenCL C kernel programs
 *
 * kernel arg buffer:
 * +-------+-------+--------+-----
 * | arg_0 | arg_1 |  arg_2 | ...
 */

#include "ventus.h"

  .text
  .global _start
  .type   _start, @function
_start:
  # set global pointer register
  .option push
  .option norelax
  la gp, __global_pointer$
  .option pop

  # allocate warp and per-thread level stack pointers, both
  # stacks grows upwards
  li t4,32
  vsetvli t4,t4,e32,m1,ta,ma
  li t4,0x2000
  csrrs t4,mstatus,t4
  li t4,0
  csrr sp, CSR_LDS      # sp points to baseaddr of local memory of each SM
  addi tp, x0, 0        # tp points to baseaddr for lower bound of private memory(1K) of each thread

  # clear BSS segment
  la      a0, _edata
  la      a2, _end
  beq     a0, a2, 2f
1:
  sw      zero, (a0)
  addi    a0, a0, 4
  bltu    a0, a2, 1b

2:
  csrr t0, CSR_KNL	      # get addr of kernel metadata
  lw t1, KNL_ENTRY(t0)    # get kernel program address
  lw a0, KNL_ARG_BASE(t0) # get kernel arg buffer base address
  jalr t1                 # call kernel program

  # call exit routine
  # tail    exit

  # End of warp execution
  endprg x0, x0, x0
  j spike_end
  .size  _start, .-_start


  .section ".tohost","aw",@progbits
  .align 6
  .globl tohost
  tohost: .dword 0
  .align 6
  .globl fromhost
  fromhost: .dword 0

  .global spike_end
  .type spike_end,function
spike_end:
  li t1,1
  la t0,tohost
  sw t1,0(t0)
