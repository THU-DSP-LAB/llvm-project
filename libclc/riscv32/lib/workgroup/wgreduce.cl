#if __OPENCL_C_VERSION__ >= 200
#include "wg.h"

#define __CLC_RISCV32_ID_U32(x) (x)
#define __CLC_RISCV32_AS_U32(x) as_uint(x)
#define __CLC_RISCV32_FROM_U32(x) (x)
#define __CLC_RISCV32_FROM_I32(x) as_int(x)
#define __CLC_RISCV32_FROM_F32(x) as_float(x)

#define __CLC_RISCV32_ADD_BITS(a, b, TO_BITS) TO_BITS((a) + (b))
#define __CLC_RISCV32_MAX_BITS(a, b, TO_BITS)                              \
    __clc_riscv32_select_u32(TO_BITS(a), TO_BITS(b),                       \
                             __clc_riscv32_bool_mask((b) > (a)))
#define __CLC_RISCV32_MIN_BITS(a, b, TO_BITS)                              \
    __clc_riscv32_select_u32(TO_BITS(a), TO_BITS(b),                       \
                             __clc_riscv32_bool_mask((b) < (a)))

#define __CLC_RISCV32_WARP_REDUCE_BITS(value, COMBINE_BITS, TYPE, TO_BITS,      \
                                       FROM_BITS)                               \
    value = COMBINE_BITS(FROM_BITS(value),                                      \
                         FROM_BITS(__clc_riscv32_shuffle_down_u32(value, 16U)), \
                         TO_BITS);                                             \
    value = COMBINE_BITS(FROM_BITS(value),                                      \
                         FROM_BITS(__clc_riscv32_shuffle_down_u32(value, 8U)),  \
                         TO_BITS);                                             \
    value = COMBINE_BITS(FROM_BITS(value),                                      \
                         FROM_BITS(__clc_riscv32_shuffle_down_u32(value, 4U)),  \
                         TO_BITS);                                             \
    value = COMBINE_BITS(FROM_BITS(value),                                      \
                         FROM_BITS(__clc_riscv32_shuffle_down_u32(value, 2U)),  \
                         TO_BITS);                                             \
    value = COMBINE_BITS(FROM_BITS(value),                                      \
                         FROM_BITS(__clc_riscv32_shuffle_down_u32(value, 1U)),  \
                         TO_BITS)

#define __CLC_RISCV32_WORK_GROUP_REDUCE(TYPE, FUNC, COMBINE_BITS, TO_BITS,      \
                                        FROM_BITS, IDENTITY)                    \
__attribute__((overloadable, weak, always_inline)) _CLC_DEF _CLC_OVERLOAD TYPE   \
FUNC(TYPE a)                                                                     \
{                                                                                \
    uint size = __clc_riscv32_work_group_size();                                 \
    uint lane = __clc_riscv32_lane_id();                                         \
    uint warp = __clc_riscv32_warp_id();                                         \
    uint warp_count = __clc_riscv32_warp_count(size);                            \
    __local uint *scratch = __clc_riscv32_get_work_group_scratch();              \
    uint value = TO_BITS(a);                                                     \
    uint active_mask = __clc_riscv32_active_work_item_mask(size);                 \
    value = __clc_riscv32_select_u32(TO_BITS((TYPE)(IDENTITY)), value,           \
                                     active_mask);                               \
                                                                                 \
    __CLC_RISCV32_WARP_REDUCE_BITS(value, COMBINE_BITS, TYPE, TO_BITS,           \
                                   FROM_BITS);                                   \
    value = __clc_riscv32_shuffle_idx_i32(value, 0);                             \
    if (lane == 0U)                                                              \
        scratch[warp] = value;                                                   \
                                                                                 \
    work_group_barrier(CLK_LOCAL_MEM_FENCE);                                     \
                                                                                 \
    uint partial = TO_BITS((TYPE)(IDENTITY));                                    \
    if (lane < warp_count)                                                       \
        partial = scratch[lane];                                                 \
                                                                                 \
    __CLC_RISCV32_WARP_REDUCE_BITS(partial, COMBINE_BITS, TYPE, TO_BITS,         \
                                   FROM_BITS);                                   \
    partial = __clc_riscv32_shuffle_idx_i32(partial, 0);                         \
                                                                                 \
    TYPE result = FROM_BITS(partial);                                            \
    work_group_barrier(CLK_LOCAL_MEM_FENCE);                                     \
    return result;                                                               \
}

#define GEN_REDUCE(TYPE, ADD_ID, MAX_ID, MIN_ID, TO_BITS, FROM_BITS)             \
__CLC_RISCV32_WORK_GROUP_REDUCE(TYPE, work_group_reduce_add,                     \
                                __CLC_RISCV32_ADD_BITS, TO_BITS, FROM_BITS,      \
                                ADD_ID)                                         \
__CLC_RISCV32_WORK_GROUP_REDUCE(TYPE, work_group_reduce_max,                     \
                                __CLC_RISCV32_MAX_BITS, TO_BITS, FROM_BITS,      \
                                MAX_ID)                                         \
__CLC_RISCV32_WORK_GROUP_REDUCE(TYPE, work_group_reduce_min,                     \
                                __CLC_RISCV32_MIN_BITS, TO_BITS, FROM_BITS,      \
                                MIN_ID)

GEN_REDUCE(int, 0, INT_MIN, INT_MAX, __CLC_RISCV32_AS_U32, __CLC_RISCV32_FROM_I32)
GEN_REDUCE(uint, 0U, 0U, UINT_MAX, __CLC_RISCV32_ID_U32, __CLC_RISCV32_FROM_U32)
GEN_REDUCE(float, 0.0f, -INFINITY, INFINITY, __CLC_RISCV32_AS_U32, __CLC_RISCV32_FROM_F32)

#undef GEN_REDUCE
#undef __CLC_RISCV32_WORK_GROUP_REDUCE
#undef __CLC_RISCV32_WARP_REDUCE_BITS
#undef __CLC_RISCV32_MIN_BITS
#undef __CLC_RISCV32_MAX_BITS
#undef __CLC_RISCV32_ADD_BITS
#undef __CLC_RISCV32_FROM_F32
#undef __CLC_RISCV32_FROM_I32
#undef __CLC_RISCV32_FROM_U32
#undef __CLC_RISCV32_AS_U32
#undef __CLC_RISCV32_ID_U32

#endif
