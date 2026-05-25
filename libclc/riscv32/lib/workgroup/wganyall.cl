#if __OPENCL_C_VERSION__ >= 200
#include "wg.h"

#define __CLC_RISCV32_WORK_GROUP_VOTE(FUNC, OP, IDENTITY)                      \
__attribute__((overloadable, always_inline)) _CLC_DEF _CLC_OVERLOAD int          \
FUNC(int predicate)                                                              \
{                                                                                \
    uint size = __clc_riscv32_work_group_size();                                 \
    uint lane = __clc_riscv32_lane_id();                                         \
    uint warp = __clc_riscv32_warp_id();                                         \
    uint warp_count = __clc_riscv32_warp_count(size);                            \
    __local uint *scratch = __clc_riscv32_get_work_group_scratch();              \
    uint value = predicate != 0;                                                  \
    uint active_mask = __clc_riscv32_active_work_item_mask(size);                 \
    value = __clc_riscv32_select_u32(IDENTITY, value, active_mask);              \
                                                                                 \
    value = OP(value, __clc_riscv32_shuffle_down_u32(value, 16U));               \
    value = OP(value, __clc_riscv32_shuffle_down_u32(value, 8U));                \
    value = OP(value, __clc_riscv32_shuffle_down_u32(value, 4U));                \
    value = OP(value, __clc_riscv32_shuffle_down_u32(value, 2U));                \
    value = OP(value, __clc_riscv32_shuffle_down_u32(value, 1U));                \
                                                                                 \
    value = __clc_riscv32_shuffle_idx_i32(value, 0);                             \
    if (lane == 0U)                                                              \
        scratch[warp] = value;                                                   \
                                                                                 \
    work_group_barrier(CLK_LOCAL_MEM_FENCE);                                     \
                                                                                 \
    uint partial = IDENTITY;                                                     \
    if (lane < warp_count)                                                       \
        partial = scratch[lane];                                                 \
                                                                                 \
    partial = OP(partial, __clc_riscv32_shuffle_down_u32(partial, 16U));         \
    partial = OP(partial, __clc_riscv32_shuffle_down_u32(partial, 8U));          \
    partial = OP(partial, __clc_riscv32_shuffle_down_u32(partial, 4U));          \
    partial = OP(partial, __clc_riscv32_shuffle_down_u32(partial, 2U));          \
    partial = OP(partial, __clc_riscv32_shuffle_down_u32(partial, 1U));          \
                                                                                 \
    value = __clc_riscv32_shuffle_idx_i32(partial, 0);                           \
    work_group_barrier(CLK_LOCAL_MEM_FENCE);                                     \
    return value ? -1 : 0;                                                       \
}

#define __CLC_RISCV32_ANY_OP(a, b) ((a) | (b))
#define __CLC_RISCV32_ALL_OP(a, b) ((a) & (b))

__CLC_RISCV32_WORK_GROUP_VOTE(work_group_any, __CLC_RISCV32_ANY_OP, 0U)
__CLC_RISCV32_WORK_GROUP_VOTE(work_group_all, __CLC_RISCV32_ALL_OP, 1U)

#undef __CLC_RISCV32_ALL_OP
#undef __CLC_RISCV32_ANY_OP
#undef __CLC_RISCV32_WORK_GROUP_VOTE

#endif
