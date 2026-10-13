#ifndef __CLC_RISCV32_WORKGROUP_WG_H__
#define __CLC_RISCV32_WORKGROUP_WG_H__

#include <clc/clc.h>

#define CLC_RISCV32_WARP_SIZE 32U

extern __local uint *__clc_riscv32_get_work_group_scratch(void);
extern uint __clc_riscv32_shuffle_idx_i32(uint value, uint lane)
    __asm("llvm.riscv.ventus.shuffle.idx.i32");
extern uint __clc_riscv32_shuffle_up_i32(uint value, uint delta)
    __asm("llvm.riscv.ventus.shuffle.up.i32");
extern uint __clc_riscv32_shuffle_down_i32(uint value, uint delta)
    __asm("llvm.riscv.ventus.shuffle.down.i32");
extern uint __clc_riscv32_lane_id_builtin(void)
    __asm("__builtin_riscv_lane_id");

static inline __attribute__((always_inline)) uint
__clc_riscv32_mod_warp_size(uint value)
{
    volatile uint quotient = value / CLC_RISCV32_WARP_SIZE;
    return value - quotient * CLC_RISCV32_WARP_SIZE;
}

static inline __attribute__((always_inline)) uint
__clc_riscv32_work_group_size(void)
{
    return (uint)(get_local_size(0) * get_local_size(1) * get_local_size(2));
}

static inline __attribute__((always_inline)) uint
__clc_riscv32_bool_mask(int predicate)
{
    return 0U - (((uint)predicate) & 1U);
}

static inline __attribute__((always_inline)) uint
__clc_riscv32_select_u32(uint false_value, uint true_value, uint mask)
{
    return (false_value & ~mask) | (true_value & mask);
}

static inline __attribute__((always_inline)) uint
__clc_riscv32_local_linear_id(void)
{
    return (uint)((get_local_id(2) * get_local_size(1) + get_local_id(1)) *
                      get_local_size(0) +
                  get_local_id(0));
}

static inline __attribute__((always_inline)) uint __clc_riscv32_lane_id(void)
{
    return __clc_riscv32_lane_id_builtin();
}

static inline __attribute__((always_inline)) uint __clc_riscv32_warp_id(void)
{
    return __clc_riscv32_local_linear_id() / CLC_RISCV32_WARP_SIZE;
}

static inline __attribute__((always_inline)) uint
__clc_riscv32_warp_count(uint size)
{
    return (size + CLC_RISCV32_WARP_SIZE - 1U) / CLC_RISCV32_WARP_SIZE;
}

static inline __attribute__((always_inline)) uint
__clc_riscv32_active_work_item_mask(uint size)
{
    return __clc_riscv32_bool_mask(__clc_riscv32_local_linear_id() < size);
}

#define __clc_riscv32_shuffle_up_u32(value, delta)                               \
    __clc_riscv32_shuffle_up_i32((value), (delta))

#define __clc_riscv32_shuffle_down_u32(value, delta)                             \
    __clc_riscv32_shuffle_down_i32((value), (delta))

#define __CLC_RISCV32_SHUFFLE_IDX_CASE(LANE)                                    \
    case LANE:                                                                  \
        return __clc_riscv32_shuffle_idx_i32(value, LANE)

static inline __attribute__((always_inline)) uint
__clc_riscv32_shuffle_idx_var_u32(uint value, uint lane)
{
    switch (lane) {
    __CLC_RISCV32_SHUFFLE_IDX_CASE(0);
    __CLC_RISCV32_SHUFFLE_IDX_CASE(1);
    __CLC_RISCV32_SHUFFLE_IDX_CASE(2);
    __CLC_RISCV32_SHUFFLE_IDX_CASE(3);
    __CLC_RISCV32_SHUFFLE_IDX_CASE(4);
    __CLC_RISCV32_SHUFFLE_IDX_CASE(5);
    __CLC_RISCV32_SHUFFLE_IDX_CASE(6);
    __CLC_RISCV32_SHUFFLE_IDX_CASE(7);
    __CLC_RISCV32_SHUFFLE_IDX_CASE(8);
    __CLC_RISCV32_SHUFFLE_IDX_CASE(9);
    __CLC_RISCV32_SHUFFLE_IDX_CASE(10);
    __CLC_RISCV32_SHUFFLE_IDX_CASE(11);
    __CLC_RISCV32_SHUFFLE_IDX_CASE(12);
    __CLC_RISCV32_SHUFFLE_IDX_CASE(13);
    __CLC_RISCV32_SHUFFLE_IDX_CASE(14);
    __CLC_RISCV32_SHUFFLE_IDX_CASE(15);
    __CLC_RISCV32_SHUFFLE_IDX_CASE(16);
    __CLC_RISCV32_SHUFFLE_IDX_CASE(17);
    __CLC_RISCV32_SHUFFLE_IDX_CASE(18);
    __CLC_RISCV32_SHUFFLE_IDX_CASE(19);
    __CLC_RISCV32_SHUFFLE_IDX_CASE(20);
    __CLC_RISCV32_SHUFFLE_IDX_CASE(21);
    __CLC_RISCV32_SHUFFLE_IDX_CASE(22);
    __CLC_RISCV32_SHUFFLE_IDX_CASE(23);
    __CLC_RISCV32_SHUFFLE_IDX_CASE(24);
    __CLC_RISCV32_SHUFFLE_IDX_CASE(25);
    __CLC_RISCV32_SHUFFLE_IDX_CASE(26);
    __CLC_RISCV32_SHUFFLE_IDX_CASE(27);
    __CLC_RISCV32_SHUFFLE_IDX_CASE(28);
    __CLC_RISCV32_SHUFFLE_IDX_CASE(29);
    __CLC_RISCV32_SHUFFLE_IDX_CASE(30);
    __CLC_RISCV32_SHUFFLE_IDX_CASE(31);
    default:
        __builtin_unreachable();
    }
}

#undef __CLC_RISCV32_SHUFFLE_IDX_CASE

#endif
