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

  .text
  .global __builtin_riscv_global_linear_id
  .type __builtin_riscv_global_linear_id, @function
__builtin_riscv_global_linear_id:
  addi sp, sp, 4
  sw ra, -4(sp)
  csrr a3, CSR_KNL           # Get kernel metadata buffer
  lw t0, KNL_WORK_DIM(a3)    # Get work_dims
  call __builtin_riscv_global_id_x
  lw t4, KNL_GL_OFFSET_X(a3) # global_offset_x
  vsub.vx v5, v0, t4         # global_linear_id1
  li t5, 1
  beq t0, t5, .GLR        # Return global_linear_id for 1 dim
.GL_2DIM:
  call __builtin_riscv_global_id_y
  lw t6, KNL_GL_SIZE_X(a3) # global_size_x
  lw t5, KNL_GL_OFFSET_Y(a3) # global_offset_y
  vsub.vx v6, v0, t5      # tmp = global_id_y - global_offset_y
  vmul.vx v6, v6, t6      # tmp = tmp * global_size_x
  vadd.vv v5, v5, v6      # global_linear_id2 = tmp + global_linear_id1
  li t5, 2
  beq t0, t5, .GLR        # Return global_linear_id for 2 dim
.GL_3DIM:
  call __builtin_riscv_global_id_z
  lw t6, KNL_GL_SIZE_X(a3) # global_size_x
  lw t1, KNL_GL_SIZE_Y(a3) # global_size_y
  lw t5, KNL_GL_OFFSET_Z(a3)  # global_offset_z
  vsub.vx v6, v0, t5      # tmp = global_id_z - global_offset_z
  vmul.vx v6, v6, t6      # tmp = tmp * global_size_x
  vmul.vx v6, v6, t1      # tmp = tmp * global_size_y
  vadd.vv v5, v5, v6      # global_linear_id3 = tmp + global_linear_id2
.GLR:
  vadd.vx v0, v5, zero    # Return global_linear_id for 1/2/3 dims
  lw ra, -4(sp)
  addi sp, sp, -4
  ret


  .text
  .global __builtin_riscv_workgroup_id_x
  .type __builtin_riscv_workgroup_id_x, @function
__builtin_riscv_workgroup_id_x:
  csrr a0, CSR_GID_X      # Read group_id_x
  vmv.v.x v0, a0
  ret


  .text
  .global __builtin_riscv_workgroup_id_y
  .type __builtin_riscv_workgroup_id_y, @function
__builtin_riscv_workgroup_id_y:
  csrr a0, CSR_GID_Y      # Read group_id_y
  vmv.v.x v0, a0
  ret


  .text
  .global __builtin_riscv_workgroup_id_z
  .type __builtin_riscv_workgroup_id_z, @function
__builtin_riscv_workgroup_id_z:
  csrr a0, CSR_GID_Z      # Read group_id_z
  vmv.v.x v0, a0
  ret


  .text
  .global __builtin_riscv_workitem_id_x
  .type __builtin_riscv_workitem_id_x, @function
__builtin_riscv_workitem_id_x:
  addi sp, sp, 4
  sw ra, -4(sp)
  csrr a0, CSR_KNL        # Get kernel metadata buffer
  lw t0, KNL_WORK_DIM(a0) # Get work_dim
  csrr t1, CSR_TID        # tid base offset for current warp
  vid.v v2                # current thread offset
  vadd.vx v0, v2, t1      # local_id_x in 1 dim (local_linear_id)
  lw t3, KNL_LC_SIZE_X(a0) # local_size_x
  vremu.vx v0, v0, t3      # local_id_x = local_liner_id % local_size_x
  lw ra, -4(sp)    
  addi sp, sp, -4
  ret


  .text
  .global __builtin_riscv_workitem_id_y
  .type __builtin_riscv_workitem_id_y, @function
__builtin_riscv_workitem_id_y:
  addi sp, sp, 4
  sw ra, -4(sp)
  csrr a0, CSR_KNL        # Get kernel metadata buffer
  lw t0, KNL_WORK_DIM(a0) # Get work_dim
  csrr t1, CSR_TID        # tid base offset for current warp
  vid.v v2                # current thread offset
  vadd.vx v0, v2, t1      # local_linear_id
  lw t3, KNL_LC_SIZE_X(a0)  # local_size_x offset in 2 work_dims
  lw t4, KNL_LC_SIZE_Y(a0)  # local_size_y offset in 2 work_dims
  mul t4, t4, t3          # local_size_x * local_size_y
  vremu.vx v0, v0, t4     # x = local_linear_id % (local_size_x * local_size_y)
  vdivu.vx v0, v0, t3     # x / local_size_x
  lw ra, -4(sp)    
  addi sp, sp, -4
  ret


  .text
  .global __builtin_riscv_workitem_id_z
  .type __builtin_riscv_workitem_id_z, @function
__builtin_riscv_workitem_id_z:
  addi sp, sp, 4
  sw ra, -4(sp)
  csrr a0, CSR_KNL        # Get kernel metadata buffer
  csrr t1, CSR_TID        # tid base offset for current warp
  vid.v v2                # current thread offset
  vadd.vx v0, v2, t1      # local_linear_id
  lw t3, KNL_LC_SIZE_X(a0) # local_size_x
  lw t4, KNL_LC_SIZE_Y(a0) # local_size_y
  mul t4, t4, t3          # local_size_x * local_size_y
  vdivu.vx v0, v0, t4     # local_linear_id / (local_size_x * local_size_y)
  lw ra, -4(sp)    
  addi sp, sp, -4
7:
  ret


  .text
  .global __builtin_riscv_global_id_x
  .type __builtin_riscv_global_id_x, @function
__builtin_riscv_global_id_x:
  addi sp, sp, 4
  sw ra, -4(sp)
  call __builtin_riscv_workitem_id_x
  csrr a0, CSR_KNL        # Get kernel metadata buffer
  csrr t1, CSR_GID_X      # Get group_id_x
  lw t3, KNL_LC_SIZE_X(a0) # Get local_size_x
  lw t4, KNL_GL_OFFSET_X(a0) # Get global_offset_x
  mul t6, t1, t3          # CSR_GID_X * local_size_x
  add t6, t6, t4          # Get global_offset_x + CSR_GID_X * local_size_x
  vadd.vx v0,v0, t6
  lw ra, -4(sp)
  addi sp, sp, -4
  ret


  .text
  .global __builtin_riscv_global_id_y
  .type __builtin_riscv_global_id_y, @function
__builtin_riscv_global_id_y:
  addi sp, sp, 4
  sw ra, -4(sp)
  call __builtin_riscv_workitem_id_y
  csrr t1, CSR_GID_Y      # Get group_id_y
  lw t2, KNL_LC_SIZE_Y(a0) # Get local_size_y
  lw t4, KNL_GL_OFFSET_Y(a0) # Get global_offset_y
  mul t3, t1, t2          # CSR_GID_Y * local_size_y
  add t3, t3, t4          # global_offset_y  + (CSR_GID_Y * local_size_y)
  vadd.vx v0, v0, t3      # global_id_y
  lw ra, -4(sp)
  addi sp, sp, -4
  ret


  .text
  .global __builtin_riscv_global_id_z
  .type __builtin_riscv_global_id_z, @function
__builtin_riscv_global_id_z:
  addi sp, sp, 4
  sw ra, -4(sp)
  call __builtin_riscv_workitem_id_z
  csrr a0, CSR_KNL        # Get kernel metadata buffer
  csrr t1, CSR_GID_Z      # Get group_id_z
  lw t2, KNL_LC_SIZE_Z(a0) # Get local_size_z
  lw t3, KNL_GL_OFFSET_Z(a0) # Get global_offset_z
  mul t2, t2, t1          # CSR_GID_Z * local_size_z
  add t2, t2, t3          # global_offset_z + (CSR_GID_Z * local_size_z)
  vadd.vx v0, v0, t2      # global_id_z
  lw ra, -4(sp)
  addi sp, sp, -4
  ret


  .text
  .global __builtin_riscv_local_size_x
  .type __builtin_riscv_local_size_x, @function
__builtin_riscv_local_size_x:
  csrr a0, CSR_KNL          # Get kernel metadata buffer
  lw t0, KNL_LC_SIZE_X(a0)  # Load local_size_x
  vmv.v.x v0, t0
  ret


  .text
  .global __builtin_riscv_local_size_y
  .type __builtin_riscv_local_size_y, @function
__builtin_riscv_local_size_y:
  csrr a0, CSR_KNL          # Get kernel metadata buffer
  lw t0, KNL_LC_SIZE_Y(a0)  # Load local_size_y
  vmv.v.x v0, t0
  ret


  .text
  .global __builtin_riscv_local_size_z
  .type __builtin_riscv_local_size_z, @function
__builtin_riscv_local_size_z:
  csrr a0, CSR_KNL          # Get kernel metadata buffer
  lw t0, KNL_LC_SIZE_Z(a0)  # Load local_size_z
  vmv.v.x v0, t0
  ret


  .text
  .global __builtin_riscv_global_size_x
  .type __builtin_riscv_global_size_x, @function
__builtin_riscv_global_size_x:
  csrr a0, CSR_KNL          # Get kernel metadata buffer
  lw t0, KNL_GL_SIZE_X(a0)  # Get global_size_x
  vmv.v.x v0, t0
  ret


  .text
  .global __builtin_riscv_global_size_y
  .type __builtin_riscv_global_size_y, @function
__builtin_riscv_global_size_y:
  csrr a0, CSR_KNL          # Get kernel metadata buffer
  lw t0, KNL_GL_SIZE_Y(a0)  # Get global_size_y
  vmv.v.x v0, t0
  ret


  .text
  .global __builtin_riscv_global_size_z
  .type __builtin_riscv_global_size_z, @function
__builtin_riscv_global_size_z:
  csrr a0, CSR_KNL          # Get kernel metadata buffer
  lw t0, KNL_GL_SIZE_Z(a0)  # Get global_size_z
  vmv.v.x v0, t0
  ret


  .text
  .global __builtin_riscv_global_offset_x
  .type __builtin_riscv_global_offset_x, @function
__builtin_riscv_global_offset_x:
  csrr a0, CSR_KNL          # Get kernel metadata buffer
  lw t0, KNL_GL_OFFSET_X(a0)  # Get global_offset_x
  vmv.v.x v0, t0
  ret


  .text
  .global __builtin_riscv_global_offset_y
  .type __builtin_riscv_global_offset_y, @function
__builtin_riscv_global_offset_y:
  csrr a0, CSR_KNL          # Get kernel metadata buffer
  lw t0, KNL_GL_OFFSET_Y(a0)  # Get global_offset_y
  vmv.v.x v0, t0
  ret


  .text
  .global __builtin_riscv_global_offset_z
  .type __builtin_riscv_global_offset_z, @function
__builtin_riscv_global_offset_z:
  csrr a0, CSR_KNL          # Get kernel metadata buffer
  lw t0, KNL_GL_OFFSET_Z(a0)  # Get global_offset_z
  vmv.v.x v0, t0
  ret


  .text
  .global __builtin_riscv_num_groups_x
  .type __builtin_riscv_num_groups_x, @function
__builtin_riscv_num_groups_x:
  csrr a0, CSR_KNL          # Get kernel metadata buffer
  lw t1, KNL_GL_SIZE_X(a0)  # Get global_size_x
  lw t0, KNL_LC_SIZE_X(a0)  # Get local_size_x
  divu t1, t1, t0           # global_size_x / local_size_x
  vmv.v.x v0, t1
  ret


  .text
  .global __builtin_riscv_num_groups_y
  .type __builtin_riscv_num_groups_y, @function
__builtin_riscv_num_groups_y:
  csrr a0, CSR_KNL          # Get kernel metadata buffer
  lw t1, KNL_GL_SIZE_Y(a0)  # Get global_size_y
  lw t0, KNL_LC_SIZE_Y(a0)  # Get local_size_y
  divu t1, t1, t0           # global_size_y / local_size_y
  vmv.v.x v0, t1
  ret


  .text
  .global __builtin_riscv_num_groups_z
  .type __builtin_riscv_num_groups_z, @function
__builtin_riscv_num_groups_z:
  csrr a0, CSR_KNL          # Get kernel metadata buffer
  lw t1, KNL_GL_SIZE_Z(a0)  # Get global_size_z
  lw t2, KNL_LC_SIZE_Z(a0)  # Get local_size_z
  divu t1, t1, t2           # global_size_z / local_size_z
  vmv.v.x v0, t1
  ret


  .text
  .global __builtin_riscv_work_dim
  .type __builtin_riscv_work_dim, @function
__builtin_riscv_work_dim:
  csrr a0, CSR_KNL          # Get kernel metadata buffer
  lw t0, KNL_WORK_DIM(a0)   # Get work_dim
  vmv.v.x v0, t0
  ret
