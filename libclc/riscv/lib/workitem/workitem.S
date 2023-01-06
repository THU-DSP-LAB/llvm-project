/**
 * Copyright (c) 2023 Terapines Technology (Wuhan) Co., Ltd
 *
 * Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://llvm.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */
/**
 * See crt0.S for kernel metadata buffer detailed layout.
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
 * get_local_id(0) = (CSR_TID + vid.v) % (local_size_x * local_size_y)
 * get_local_id(1) = ((CSR_TID + vid.v) - get_local_id(2) * (local_size_x * local_size_y)) / local_size_x
 * get_local_id(2) = (CSR_TID + vid.v) / (local_size_x * local_size_y)
 *
 *
 * global_id (uniform methods in 1/2/3 dims):
 * get_global_id(0) = (CSR_GID_X - 1) * local_size_x + local_id_x
 * get_global_id(1) = (CSR_GID_Y - 1) * local_size_y + local_id_y
 * get_global_id(2) = (CSR_GID_Z - 1) * local_size_z + local_id_z
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


  .text
  .global __builtin_riscv_workitem_linear_id
  .type __builtin_riscv_workitem_linear_id, @function
__builtin_riscv_workitem_linear_id:
  csrr a0, CSR_KNL        # Get kernel metadata buffer
  csrr t1, CSR_TID        # tid base offset for current warp
  vid.v v2                # current thread offset
  vadd.vx v0, v2, t1      # local_linear_id
  ret
  .size  __builtin_riscv_workitem_linear_id .- __builtin_riscv_workitem_linear_id


  .text
  .global __builtin_riscv_global_linear_id
  .type __builtin_riscv_global_linear_id, @function
__builtin_riscv_global_linear_id:
  csrr a3, CSR_KNL        # Get kernel metadata buffer
  lw t0, 8(a3)            # Get work_dims
  call __builtin_riscv_global_id_x
  lw t4, 36(a3)           # Get global_offset_x
  vsub.vx v5, v0, t4      # global_linear_id1
  li t5, 1
  beq t0, t5, .GLR        # Return global_linear_id for 1 dim
.GL_2DIM:
  call __builtin_riscv_global_id_y
  lw t6, 12(a3)           # global_size_x
  lw t5, 40(a3)           # global_offset_y
  vsub.vx v6, v0, t5      # tmp = global_id_y - global_offset_y
  vmul.vx v6, v6, t6      # tmp = tmp * global_size_x
  vadd.vv v5, v5, v6      # global_linear_id2 = tmp + global_linear_id1
  li t5, 2
  beq t0, t5, .GLR        # Return global_linear_id for 2 dim
.GL_3DIM:
  call __builtin_riscv_global_id_z
  lw t6, 12(a3)           # global_size_x
  lw t7, 16(a3)           # global_size_y
  lw t5, 44(a3)           # global_offset_z
  vsub.vx v6, v0, t5      # tmp = global_id_z - global_offset_z
  vmul.vx v6, v6, t6      # tmp = tmp * global_size_x
  vmul.vx v6, v6, t7      # tmp = tmp * global_size_y
  vadd.vv v5, v5, v6      # global_linear_id3 = tmp + global_linear_id2
.GLR:
  vadd.vx v0, v5, zero    # Return global_linear_id for 1/2/3 dims
  ret
  .size  __builtin_riscv_global_linear_id .- __builtin_riscv_global_linear_id


  .text
  .global __builtin_riscv_workgroup_id_x
  .type __builtin_riscv_workgroup_id_x, @function
__builtin_riscv_workgroup_id_x:
  csrr a0, CSR_GID_X      # Read group_id_x
  vmv.s.x v0, a0
  ret
  .size  __builtin_riscv_workgroup_id_x .- __builtin_riscv_workgroup_id_x


  .text
  .global __builtin_riscv_workgroup_id_y
  .type __builtin_riscv_workgroup_id_y, @function
__builtin_riscv_workgroup_id_y:
  csrr a0, CSR_GID_Y      # Read group_id_y
  vmv.s.x v0, a0
  ret
  .size  __builtin_riscv_workgroup_id_y .- __builtin_riscv_workgroup_id_y


  .text
  .global __builtin_riscv_workgroup_id_z
  .type __builtin_riscv_workgroup_id_z, @function
__builtin_riscv_workgroup_id_z:
  csrr a0, CSR_GID_Z      # Read group_id_z
  vmv.s.x v0, a0
  ret
  .size  __builtin_riscv_workgroup_id_z .- __builtin_riscv_workgroup_id_z


  .text
  .global __builtin_riscv_workitem_id_x
  .type __builtin_riscv_workitem_id_x, @function
__builtin_riscv_workitem_id_x:
  csrr a0, CSR_KNL        # Get kernel metadata buffer
  lw t0, 8(a0)            # Get work_dim
  csrr t1, CSR_TID        # tid base offset for current warp
  vid.v v2                # current thread offset
  vadd.vx v0, v2, t1      # local_id_x in 1 dim (local_linear_id)
  li t2, 1
  beq t0, t2, .WIXR       # 1 dim
  lw t3, 24(a0)           # local_size_x
  li t2, 3
  beq t0, t2, .WIX3
.WIX2:
  vremu.vx v0, v0, t3     # local_id_x = local_liner_id % local_size_x
  ret
.WIX3:                    # 3 dims
  lw t4, 28(a0)           # local_size_y
  mul t4, t4, t3          # local_size_x * local_size_y
  vremu.vx v0, v0, t4     # local_id_x = local_liner_id % (local_size_x * local_size_y)
.WIXR:
  ret
  .size  __builtin_riscv_workitem_id_x .- __builtin_riscv_workitem_id_x


  .text
  .global __builtin_riscv_workitem_id_y
  .type __builtin_riscv_workitem_id_y, @function
__builtin_riscv_workitem_id_y:
  csrr a0, CSR_KNL        # Get kernel metadata buffer
  lw t0, 8(a0)            # Get work_dim
  csrr t1, CSR_TID        # tid base offset for current warp
  vid.v v2                # current thread offset
  vadd.vx v0, v2, t1      # local_linear_id
  lw t3, 24(a0)           # local_size_x offset in 2 work_dims
  li t2, 3
  beq t0, t2, .WIY3
.WIY2:                    # 2 dims
  vdivu.vx v0, v0, t3     # local_id_y = local_liner_id / local_size_x
  ret
.WIY3:                    # 3 dims
  lw t4, 28(a0)           # local_size_y
  mul t4, t4, t3          # local_size_x * local_size_y
  vremu.vx v0, v0, t4     # x = local_linear_id % (local_size_x * local_size_y)
  vdivu.ux v0, v0, t3     # x / local_size_x
  ret
  .size  __builtin_riscv_workitem_id_y .- __builtin_riscv_workitem_id_y


  .text
  .global __builtin_riscv_workitem_id_z
  .type __builtin_riscv_workitem_id_z, @function
__builtin_riscv_workitem_id_z:
  csrr a0, CSR_KNL        # Get kernel metadata buffer
  csrr t1, CSR_TID        # tid base offset for current warp
  vid.v v2                # current thread offset
  vadd.vx v0, v2, t1      # local_linear_id
  lw t3, 24(a0)           # local_size_x
  lw t4, 28(a0)           # local_size_y
  mul t4, t4, t3          # local_size_x * local_size_y
  vdivu.vx v0, v0, t4     # local_linear_id / (local_size_x * local_size_y)
7:
  ret
  .size  __builtin_riscv_workitem_id_z .- __builtin_riscv_workitem_id_z


  .text
  .global __builtin_riscv_global_id_x
  .type __builtin_riscv_global_id_x, @function
__builtin_riscv_global_id_x:
  csrr a0, CSR_KNL        # Get kernel metadata buffer
  csrr t1, CSR_GID_X      # Get group_id_x
  sub t1, t1, 1           # group_id_x - 1
  csrr t2, CSR_TID
  vid.v v2
  vadd.vx v2, v2, t2      # workitem_id_x
  lw t3, 24(a0)           # Get local_size_x
  mul t3, t1, t3          # (CSR_GID_X - 1) * local_size_x
  vadd.vx v0, v2, t3      # global_id_x
  ret
  .size  __builtin_riscv_global_id_x .- __builtin_riscv_global_idx_x


  .text
  .global __builtin_riscv_global_id_y
  .type __builtin_riscv_global_id_y, @function
__builtin_riscv_global_id_y:
  csrr a0, CSR_KNL        # Get kernel metadata buffer
  csrr t1, CSR_GID_Y      # Get group_id_y
  sub t1, t1, 1           # group_id_y - 1
  csrr t2, CSR_TID
  vid.v v2
  vadd.vx v2, v2, t2      # workitem_id_y
  lw t3, 28(a0)           # Get local_size_y
  mul t3, t1, t3          # (CSR_GID_Y - 1) * local_size_y
  vadd.vx v0, v2, t3      # global_id_y
  ret
  .size  __builtin_riscv_global_id_y .- __builtin_riscv_global_idx_y


  .text
  .global __builtin_riscv_global_id_z
  .type __builtin_riscv_global_id_z, @function
__builtin_riscv_global_id_z:
  csrr a0, CSR_KNL        # Get kernel metadata buffer
  csrr t1, CSR_GID_Z      # Get group_id_z
  sub t1, t1, 1           # group_id_z - 1
  csrr t2, CSR_TID
  vid.v v2
  vadd.vx v2, v2, t2      # workitem_id_z
  lw t3, 32(a0)           # Get local_size_z
  mul t3, t1, t3          # (CSR_GID_Z - 1) * local_size_z
  vadd.vx v0, v2, t3      # global_id_z
  ret
  .size  __builtin_riscv_global_id_z .- __builtin_riscv_global_idx_z


  .text
  .global __builtin_riscv_local_size_x
  .type __builtin_riscv_local_size_x, @function
__builtin_riscv_local_size_x:
  csrr a0, CSR_KNL          # Get kernel metadata buffer
  lw t0, 8(a0)              # Get work_dim
  addi t2, zero, 4
  mul t0, t0, t2            # Skip offset of global_size_xyz slots
  add a0, a0, t0
  lw t0, 0(a0)              # load local_size_x
  vmv.s.x v0, t0
  ret
  .size  __builtin_riscv_local_size_x, .-__builtin_riscv_local_size_x


  .text
  .global __builtin_riscv_local_size_y
  .type __builtin_riscv_local_size_y, @function
__builtin_riscv_local_size_y:
  csrr a0, CSR_KNL          # Get kernel metadata buffer
  lw t0, 8(a0)              # Get work_dim
  addi t2, zero, 4
  mul t0, t0, t2            # Skip offset of global_size_xyz slots
  lw t0, 4(a0)              # Load local_size_y
  vmv.s.x v0, t0
  ret
  .size  __builtin_riscv_local_size_y, .-__builtin_riscv_local_size_y


  .text
  .global __builtin_riscv_local_size_z
  .type __builtin_riscv_local_size_z, @function
__builtin_riscv_local_size_z:
  csrr a0, CSR_KNL          # Get kernel metadata buffer
  lw t0, 8(a0)              # Get work_dim
  addi t2, zero, 4
  mul t0, t0, t2            # Skip offset of global_size_xyz slots
  lw t0, 8(a0)              # Load local_size_x
  vmv.s.x v0, t0
  ret
  .size  __builtin_riscv_local_size_z, .-__builtin_riscv_local_size_z


  .text
  .global __builtin_riscv_global_size_x
  .type __builtin_riscv_global_size_x, @function
__builtin_riscv_global_size_x:
  csrr a0, CSR_KNL          # Get kernel metadata buffer
  lw t0, 12(a0)             # Get global_size_x
  vmv.s.x v0, t0
  ret
  .size  __builtin_riscv_global_size_x, .-__builtin_riscv_global_size_x


  .text
  .global __builtin_riscv_global_size_y
  .type __builtin_riscv_global_size_y, @function
__builtin_riscv_global_size_y:
  csrr a0, CSR_KNL          # Get kernel metadata buffer
  lw t0, 16(a0)             # Get global_size_y
  vmv.s.x v0, t0
  ret
  .size  __builtin_riscv_global_size_y, .-__builtin_riscv_global_size_y


  .text
  .global __builtin_riscv_global_size_z
  .type __builtin_riscv_global_size_z, @function
__builtin_riscv_global_size_z:
  csrr a0, CSR_KNL          # Get kernel metadata buffer
  lw t0, 20(a0)             # Get global_size_z
  vmv.s.x v0, t0
  ret
  .size  __builtin_riscv_global_size_z, .-__builtin_riscv_global_size_z


  .text
  .global __builtin_riscv_num_groups_x
  .type __builtin_riscv_num_groups_x, @function
__builtin_riscv_num_groups_x:
  csrr a0, CSR_KNL          # Get kernel metadata buffer
  lw t0, 8(a0)              # Get work_dim
  lw t1, 12(a0)             # Get global_size_x
  addi t2, zero, 4          # 4 bytes per slot
  mul t0, t0, t2
  add a0, a0, t0            # Skip offset of global_size_xyz slots
  lw t0, 0(a0)              # Get local_size_x
  divu t1, t1, t0           # global_size_x / local_size_x
  vmv.s.x v0, t1
  ret
  .size  __builtin_riscv_num_groups_x, .-__builtin_riscv_num_groups_x


  .text
  .global __builtin_riscv_num_groups_y
  .type __builtin_riscv_num_groups_y, @function
__builtin_riscv_num_groups_y:
  csrr a0, CSR_KNL          # Get kernel metadata buffer
  lw t0, 8(a0)              # Get work_dim
  lw t1, 16(a0)             # Get global_size_y
  addi t2, zero, 4          # 4 bytes per slot
  mul t0, t0, t2
  add a0, a0, t0            # Skip offset of global_size_xyz slots
  lw t0, 4(a0)              # Get local_size_y
  divu t1, t1, t0           # global_size_y / local_size_y
  vmv.s.x v0, t1
  ret
  .size  __builtin_riscv_num_groups_y, .-__builtin_riscv_num_groups_y


  .text
  .global __builtin_riscv_num_groups_z
  .type __builtin_riscv_num_groups_z, @function
__builtin_riscv_num_groups_z:
  csrr a0, CSR_KNL          # Get kernel metadata buffer
  lw t1, 20(a0)             # Get global_size_z
  lw t2, 32(a0)             # Get local_size_z
  divu t1, t1, t2           # global_size_z / local_size_z
  vmv.s.x v0, t1
  ret
  .size  __builtin_riscv_num_groups_z, .-__builtin_riscv_num_groups_z


  .text
  .global __builtin_riscv_work_dim
  .type __builtin_riscv_work_dim, @function
__builtin_riscv_work_dim:
  csrr a0, CSR_KNL          # Get kernel metadata buffer
  lw t0, 8(a0)              # Get work_dim
  vmv.s.x v0, t0
  ret
  .size  __builtin_riscv_work_dim, .-__builtin_riscv_work_dim
