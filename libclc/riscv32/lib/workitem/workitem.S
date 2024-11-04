/**
 * Copyright (c) 2023 Terapines Technology (Wuhan) Co., Ltd
 *
 * Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://llvm.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */
/**
 * See ventus.h for kernel metadata buffer detailed layout.
 *
 * workitem_id:
 * 1 dim:
 * get_local_id(0) = CSR_TID + vid.v
 *
 * 2 dims:
 * get_local_id(0) = (CSR_TID + vid.v) % local_size_x
 * get_local_id(1) = (CSR_TID + vid.v) / local_size_x
 *
 * 3 dims:
 * get_local_id(0) = (CSR_TID + vid.v) % (local_size_x)
 * get_local_id(1) = (CSR_TID + vid.v) %(local_size_x * local_size_y) / local_size_x
 * get_local_id(2) = (CSR_TID + vid.v) / (local_size_x * local_size_y)
 *
 *
 * global_id (uniform methods in 1/2/3 dims):
 * get_global_id(0) = _global_offset_x + CSR_GID_X * local_size_x + local_id_x
 * get_global_id(1) = _global_offset_y + CSR_GID_Y * local_size_y + local_id_y
 * get_global_id(2) = _global_offset_z + CSR_GID_Z * local_size_z + local_id_z
 *
 *
 * global_linear_id:
 * 1 dim:
 * global_linear_id1 = get_global_id(0) - global_offset_x
 *
 * 2 dims:
 * global_linear_id2 = (get_global_id(1) - global_offset_y) * global_size_x + global_linear_id1
 *
 * 3 dims:
 * (get_global_id(2) - global_offset_z) * global_size_x * global_size_y + global_linear_id2
 *
 */

#include "ventus.h"

   // Workaround for pocl driver
  .type  _local_id_x, @object
  .section .sdata,"aw",@progbits
  .globl _local_id_x
  .p2align 2
_local_id_x:
  .word 0

  .type  _local_id_y, @object
  .section .sdata,"aw",@progbits
  .globl _local_id_y
  .p2align 2
_local_id_y:
  .word 0

  .type  _local_id_z, @object
  .section .sdata,"aw",@progbits
  .globl _local_id_z
  .p2align 2
_local_id_z:
  .word 0
  // End workaround for pocl driver

  .section	.text.__builtin_riscv_global_linear_id,"ax",@progbits
  .global __builtin_riscv_global_linear_id
  .type __builtin_riscv_global_linear_id, @function
__builtin_riscv_global_linear_id:
  csrr.v v0, CSR_GLL_ID      # Read global_linear_id
  ret


  .section	.text.__builtin_riscv_workgroup_id_x,"ax",@progbits
  .global __builtin_riscv_workgroup_id_x
  .type __builtin_riscv_workgroup_id_x, @function
__builtin_riscv_workgroup_id_x:
  csrr a0, CSR_GID_X      # Read group_id_x
  vmv.v.x v0, a0
  ret


  .section	.text.__builtin_riscv_workgroup_id_y,"ax",@progbits
  .global __builtin_riscv_workgroup_id_y
  .type __builtin_riscv_workgroup_id_y, @function
__builtin_riscv_workgroup_id_y:
  csrr a0, CSR_GID_Y      # Read group_id_y
  vmv.v.x v0, a0
  ret


  .section	.text.__builtin_riscv_workgroup_id_z,"ax",@progbits
  .global __builtin_riscv_workgroup_id_z
  .type __builtin_riscv_workgroup_id_z, @function
__builtin_riscv_workgroup_id_z:
  csrr a0, CSR_GID_Z      # Read group_id_z
  vmv.v.x v0, a0
  ret


  .section	.text.__builtin_riscv_workitem_id_x,"ax",@progbits
  .global __builtin_riscv_workitem_id_x
  .type __builtin_riscv_workitem_id_x, @function
__builtin_riscv_workitem_id_x:
  csrr.v v0, CSR_LC_ID_X      # Read local_id_x
  ret


  .section	.text.__builtin_riscv_workitem_id_y,"ax",@progbits
  .global __builtin_riscv_workitem_id_y
  .type __builtin_riscv_workitem_id_y, @function
__builtin_riscv_workitem_id_y:
  csrr.v v0, CSR_LC_ID_Y      # Read local_id_y
  ret


  .section	.text.__builtin_riscv_workitem_id_z,"ax",@progbits
  .global __builtin_riscv_workitem_id_z
  .type __builtin_riscv_workitem_id_z, @function
__builtin_riscv_workitem_id_z:
  csrr.v v0, CSR_LC_ID_Z      # Read local_id_z
  ret


  .section	.text.__builtin_riscv_global_id_x,"ax",@progbits
  .global __builtin_riscv_global_id_x
  .type __builtin_riscv_global_id_x, @function
__builtin_riscv_global_id_x:
  csrr.v v0, CSR_GL_ID_X      # Read global_id_x
  ret


  .section	.text.__builtin_riscv_global_id_y,"ax",@progbits
  .global __builtin_riscv_global_id_y
  .type __builtin_riscv_global_id_y, @function
__builtin_riscv_global_id_y:
  csrr.v v0, CSR_GL_ID_Y      # Read global_id_y
  ret


  .section	.text.__builtin_riscv_global_id_z,"ax",@progbits
  .global __builtin_riscv_global_id_z
  .type __builtin_riscv_global_id_z, @function
__builtin_riscv_global_id_z:
  csrr.v v0, CSR_GL_ID_Z      # Read global_id_z
  ret


  .section	.text.__builtin_riscv_local_size_x,"ax",@progbits
  .global __builtin_riscv_local_size_x
  .type __builtin_riscv_local_size_x, @function
__builtin_riscv_local_size_x:
  csrr a0, CSR_KNL          # Get kernel metadata buffer
  lw t0, KNL_LC_SIZE_X(a0)  # Load local_size_x
  vmv.v.x v0, t0
  ret


  .section	.text.__builtin_riscv_local_size_y,"ax",@progbits
  .global __builtin_riscv_local_size_y
  .type __builtin_riscv_local_size_y, @function
__builtin_riscv_local_size_y:
  csrr a0, CSR_KNL          # Get kernel metadata buffer
  lw t0, KNL_LC_SIZE_Y(a0)  # Load local_size_y
  vmv.v.x v0, t0
  ret


  .section	.text.__builtin_riscv_local_size_z,"ax",@progbits
  .global __builtin_riscv_local_size_z
  .type __builtin_riscv_local_size_z, @function
__builtin_riscv_local_size_z:
  csrr a0, CSR_KNL          # Get kernel metadata buffer
  lw t0, KNL_LC_SIZE_Z(a0)  # Load local_size_z
  vmv.v.x v0, t0
  ret


  .section	.text.__builtin_riscv_global_size_x,"ax",@progbits
  .global __builtin_riscv_global_size_x
  .type __builtin_riscv_global_size_x, @function
__builtin_riscv_global_size_x:
  csrr a0, CSR_KNL          # Get kernel metadata buffer
  lw t0, KNL_GL_SIZE_X(a0)  # Get global_size_x
  vmv.v.x v0, t0
  ret


  .section	.text.__builtin_riscv_global_size_y,"ax",@progbits
  .global __builtin_riscv_global_size_y
  .type __builtin_riscv_global_size_y, @function
__builtin_riscv_global_size_y:
  csrr a0, CSR_KNL          # Get kernel metadata buffer
  lw t0, KNL_GL_SIZE_Y(a0)  # Get global_size_y
  vmv.v.x v0, t0
  ret


  .section	.text.__builtin_riscv_global_size_z,"ax",@progbits
  .global __builtin_riscv_global_size_z
  .type __builtin_riscv_global_size_z, @function
__builtin_riscv_global_size_z:
  csrr a0, CSR_KNL          # Get kernel metadata buffer
  lw t0, KNL_GL_SIZE_Z(a0)  # Get global_size_z
  vmv.v.x v0, t0
  ret


  .section	.text.__builtin_riscv_global_offset_x,"ax",@progbits
  .global __builtin_riscv_global_offset_x
  .type __builtin_riscv_global_offset_x, @function
__builtin_riscv_global_offset_x:
  csrr a0, CSR_KNL          # Get kernel metadata buffer
  lw t0, KNL_GL_OFFSET_X(a0)  # Get global_offset_x
  vmv.v.x v0, t0
  ret


  .section	.text.__builtin_riscv_global_offset_y,"ax",@progbits
  .global __builtin_riscv_global_offset_y
  .type __builtin_riscv_global_offset_y, @function
__builtin_riscv_global_offset_y:
  csrr a0, CSR_KNL          # Get kernel metadata buffer
  lw t0, KNL_GL_OFFSET_Y(a0)  # Get global_offset_y
  vmv.v.x v0, t0
  ret


  .section	.text.__builtin_riscv_global_offset_z,"ax",@progbits
  .global __builtin_riscv_global_offset_z
  .type __builtin_riscv_global_offset_z, @function
__builtin_riscv_global_offset_z:
  csrr a0, CSR_KNL          # Get kernel metadata buffer
  lw t0, KNL_GL_OFFSET_Z(a0)  # Get global_offset_z
  vmv.v.x v0, t0
  ret


  .section	.text.__builtin_riscv_num_groups_x,"ax",@progbits
  .global __builtin_riscv_num_groups_x
  .type __builtin_riscv_num_groups_x, @function
__builtin_riscv_num_groups_x:
  csrr a0, CSR_KNL          # Get kernel metadata buffer
  lw t1, KNL_GL_SIZE_X(a0)  # Get global_size_x
  lw t0, KNL_LC_SIZE_X(a0)  # Get local_size_x
  divu t1, t1, t0           # global_size_x / local_size_x
  vmv.v.x v0, t1
  ret


  .section	.text.__builtin_riscv_num_groups_y,"ax",@progbits
  .global __builtin_riscv_num_groups_y
  .type __builtin_riscv_num_groups_y, @function
__builtin_riscv_num_groups_y:
  csrr a0, CSR_KNL          # Get kernel metadata buffer
  lw t1, KNL_GL_SIZE_Y(a0)  # Get global_size_y
  lw t0, KNL_LC_SIZE_Y(a0)  # Get local_size_y
  divu t1, t1, t0           # global_size_y / local_size_y
  vmv.v.x v0, t1
  ret


  .section	.text.__builtin_riscv_num_groups_z,"ax",@progbits
  .global __builtin_riscv_num_groups_z
  .type __builtin_riscv_num_groups_z, @function
__builtin_riscv_num_groups_z:
  csrr a0, CSR_KNL          # Get kernel metadata buffer
  lw t1, KNL_GL_SIZE_Z(a0)  # Get global_size_z
  lw t2, KNL_LC_SIZE_Z(a0)  # Get local_size_z
  divu t1, t1, t2           # global_size_z / local_size_z
  vmv.v.x v0, t1
  ret


  .section	.text.__builtin_riscv_work_dim,"ax",@progbits
  .global __builtin_riscv_work_dim
  .type __builtin_riscv_work_dim, @function
__builtin_riscv_work_dim:
  csrr a0, CSR_KNL          # Get kernel metadata buffer
  lw t0, KNL_WORK_DIM(a0)   # Get work_dim
  vmv.v.x v0, t0
  ret
