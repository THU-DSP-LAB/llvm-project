/**
 * Copyright (c) 2023 Terapines Technology (Wuhan) Co., Ltd
 *
 * Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://llvm.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

/**
 * crt0.S : Entry point for Ventus OpenCL C kernel programs
 */

  .text
  .global _start
  .type   _start, @function
_start:
  # set global pointer register
  .option push
  .option norelax
  la gp, __global_pointer
  .option pop

  # allocate warp and per-thread level stack pointers
  csrr t0, CSR_LDS       # get baseaddr of local memory
  li t1, 131072
  add sp, t0, t1         # sp points to local memory + 128K
  li tp, 1024            # tp points to upper bound of private memory(1K) of each thread

  # clear BSS segment
  la      a0, _edata
  la      a2, _end
  beq     a0, a2, 2f
1:
  sw      zero, (a0)
  addi    a0, a0, 4
  bltu    a0, a2, 1b

2:
  csrr a3, CSR_KNL	  # get addr of kernel metadata
  lw a2, 0(a3)            # get kernel program address
  lw a0, 4(a3)            # get kernel 1st arg address
  jalr a2                 # call kernel program

  # call exit routine
  # tail    exit
.size  _start, .-_start
