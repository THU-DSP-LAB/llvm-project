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
 * kernel metadata buffer:	
 * +-------4---------+----------4----------+-----4----+-------4-------+-------4-------
 * | kernel func ptr | kernel arg base ptr | work_dim | global_size_x | global_size_y 
 * +-------4-------+------4-------+------4-------+------4-------+-------4---------
 * | global_size_z | local_size_x | local_size_y | local_size_z | global_offset_x 
 * +--------4--------+--------4--------+----
 * | global_offset_y | global_offset_z | ...
 *
 *
 * kernel arg buffer:
 * +-------+-------+--------+-----
 * | arg_0 | arg_1 |  arg_2 | ...
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

  # allocate warp and per-thread level stack pointers, both
  # stacks grows upwards
  csrr sp, CSR_LDS      # sp points to baseaddr of local memory of each SM
  addi tp, zero, 0      # tp points to the lower bound of private memory(1K) of each thread

  # clear BSS segment
  la      a0, _edata
  la      a2, _end
  beq     a0, a2, 2f
1:
  sw      zero, (a0)
  addi    a0, a0, 4
  bltu    a0, a2, 1b

2:
  csrr t0, CSR_KNL	  # get addr of kernel metadata
  lw t1, 0(t0)            # get kernel program address
  lw a0, 4(t0)            # get kernel arg buffer base address
  jalr t1                 # call kernel program

  # call exit routine
  # tail    exit

  # End of warp execution
  endprg
  .size  _start, .-_start
