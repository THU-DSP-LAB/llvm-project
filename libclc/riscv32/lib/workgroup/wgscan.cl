#if __OPENCL_C_VERSION__ >= 200
#include "wg.h"

#define __CLC_RISCV32_ID_U32(x) (x)
#define __CLC_RISCV32_AS_U32(x) as_uint(x)
#define __CLC_RISCV32_FROM_U32(x) (x)
#define __CLC_RISCV32_FROM_I32(x) as_int(x)
#define __CLC_RISCV32_FROM_F32(x) as_float(x)

#define __CLC_RISCV32_ADD_BITS(a, b, TO_BITS) TO_BITS((a) + (b))
#define __CLC_RISCV32_MAX_BITS(a, b, TO_BITS)                              \
    __clc_riscv32_select_u32(TO_BITS(b), TO_BITS(a),                       \
                             __clc_riscv32_bool_mask((a) > (b)))
#define __CLC_RISCV32_MIN_BITS(a, b, TO_BITS)                              \
    __clc_riscv32_select_u32(TO_BITS(b), TO_BITS(a),                       \
                             __clc_riscv32_bool_mask((a) < (b)))

#define __CLC_RISCV32_WARP_SCAN_BITS(value, COMBINE_BITS, TYPE, TO_BITS,        \
                                     FROM_BITS)                                 \
    do {                                                                        \
        uint __active_mask = __clc_riscv32_bool_mask(lane >= 1U);               \
        uint __prior = __clc_riscv32_shuffle_up_u32(value, 1U);                 \
        uint __combined = COMBINE_BITS(FROM_BITS(__prior), FROM_BITS(value),    \
                                       TO_BITS);                                \
        value = __clc_riscv32_select_u32(value, __combined, __active_mask);     \
        __active_mask = __clc_riscv32_bool_mask(lane >= 2U);                    \
        __prior = __clc_riscv32_shuffle_up_u32(value, 2U);                      \
        __combined = COMBINE_BITS(FROM_BITS(__prior), FROM_BITS(value),         \
                                  TO_BITS);                                     \
        value = __clc_riscv32_select_u32(value, __combined, __active_mask);     \
        __active_mask = __clc_riscv32_bool_mask(lane >= 4U);                    \
        __prior = __clc_riscv32_shuffle_up_u32(value, 4U);                      \
        __combined = COMBINE_BITS(FROM_BITS(__prior), FROM_BITS(value),         \
                                  TO_BITS);                                     \
        value = __clc_riscv32_select_u32(value, __combined, __active_mask);     \
        __active_mask = __clc_riscv32_bool_mask(lane >= 8U);                    \
        __prior = __clc_riscv32_shuffle_up_u32(value, 8U);                      \
        __combined = COMBINE_BITS(FROM_BITS(__prior), FROM_BITS(value),         \
                                  TO_BITS);                                     \
        value = __clc_riscv32_select_u32(value, __combined, __active_mask);     \
        __active_mask = __clc_riscv32_bool_mask(lane >= 16U);                   \
        __prior = __clc_riscv32_shuffle_up_u32(value, 16U);                     \
        __combined = COMBINE_BITS(FROM_BITS(__prior), FROM_BITS(value),         \
                                  TO_BITS);                                     \
        value = __clc_riscv32_select_u32(value, __combined, __active_mask);     \
    } while (0)

#define __CLC_RISCV32_LAST_LANE_IN_WARP(size, warp)                             \
    ((warp) + 1U == __clc_riscv32_warp_count(size)                              \
         ? __clc_riscv32_mod_warp_size((size) - 1U)                             \
         : CLC_RISCV32_WARP_SIZE - 1U)

#define __CLC_RISCV32_WORK_GROUP_SCAN_INCLUSIVE(TYPE, FUNC, COMBINE_BITS,       \
                                                TO_BITS, FROM_BITS, IDENTITY)   \
__attribute__((overloadable, weak, always_inline)) _CLC_DEF _CLC_OVERLOAD TYPE  \
FUNC(TYPE a)                                                                    \
{                                                                               \
    uint size = __clc_riscv32_work_group_size();                                \
    uint lane = __clc_riscv32_lane_id();                                        \
    uint warp = __clc_riscv32_warp_id();                                        \
    uint warp_count = __clc_riscv32_warp_count(size);                           \
    __local uint *scratch = __clc_riscv32_get_work_group_scratch();             \
    uint value = TO_BITS(a);                                                    \
    uint active_mask = __clc_riscv32_active_work_item_mask(size);                \
    value = __clc_riscv32_select_u32(TO_BITS((TYPE)(IDENTITY)), value,          \
                                     active_mask);                              \
                                                                                \
    __CLC_RISCV32_WARP_SCAN_BITS(value, COMBINE_BITS, TYPE, TO_BITS, FROM_BITS);\
    uint warp_total = __clc_riscv32_shuffle_idx_var_u32(                        \
        value, __CLC_RISCV32_LAST_LANE_IN_WARP(size, warp));                    \
    if (lane == 0U)                                                             \
        scratch[warp] = warp_total;                                             \
                                                                                \
    work_group_barrier(CLK_LOCAL_MEM_FENCE);                                    \
                                                                                \
    uint partial = TO_BITS((TYPE)(IDENTITY));                                   \
    if (lane < warp_count)                                                      \
        partial = scratch[lane];                                                \
    __CLC_RISCV32_WARP_SCAN_BITS(partial, COMBINE_BITS, TYPE, TO_BITS,          \
                                 FROM_BITS);                                    \
    uint has_previous = __clc_riscv32_bool_mask(lane != 0U);                    \
    uint previous_partial = __clc_riscv32_shuffle_up_u32(partial, 1U);          \
    uint prefix = __clc_riscv32_select_u32(TO_BITS((TYPE)(IDENTITY)),           \
                                           previous_partial, has_previous);      \
    if (warp == 0U)                                                             \
        scratch[lane] = prefix;                                                 \
                                                                                \
    work_group_barrier(CLK_LOCAL_MEM_FENCE);                                    \
                                                                                \
    uint warp_prefix = scratch[warp];                                           \
    uint result_bits = COMBINE_BITS(FROM_BITS(warp_prefix), FROM_BITS(value),   \
                                    TO_BITS);                                   \
                                                                                \
    TYPE result = FROM_BITS(result_bits);                                       \
    work_group_barrier(CLK_LOCAL_MEM_FENCE);                                    \
    return result;                                                              \
}

#define __CLC_RISCV32_WORK_GROUP_SCAN_EXCLUSIVE(TYPE, FUNC, COMBINE_BITS,       \
                                                IDENTITY, TO_BITS, FROM_BITS)   \
__attribute__((overloadable, weak, always_inline)) _CLC_DEF _CLC_OVERLOAD TYPE  \
FUNC(TYPE a)                                                                    \
{                                                                               \
    uint size = __clc_riscv32_work_group_size();                                \
    uint lane = __clc_riscv32_lane_id();                                        \
    uint warp = __clc_riscv32_warp_id();                                        \
    uint warp_count = __clc_riscv32_warp_count(size);                           \
    __local uint *scratch = __clc_riscv32_get_work_group_scratch();             \
    uint value = TO_BITS(a);                                                    \
    uint active_mask = __clc_riscv32_active_work_item_mask(size);                \
    value = __clc_riscv32_select_u32(TO_BITS((TYPE)(IDENTITY)), value,          \
                                     active_mask);                              \
                                                                                \
    __CLC_RISCV32_WARP_SCAN_BITS(value, COMBINE_BITS, TYPE, TO_BITS, FROM_BITS);\
    uint warp_total = __clc_riscv32_shuffle_idx_var_u32(                        \
        value, __CLC_RISCV32_LAST_LANE_IN_WARP(size, warp));                    \
    if (lane == 0U)                                                             \
        scratch[warp] = warp_total;                                             \
                                                                                \
    work_group_barrier(CLK_LOCAL_MEM_FENCE);                                    \
                                                                                \
    uint partial = TO_BITS((TYPE)(IDENTITY));                                   \
    if (lane < warp_count)                                                      \
        partial = scratch[lane];                                                \
    __CLC_RISCV32_WARP_SCAN_BITS(partial, COMBINE_BITS, TYPE, TO_BITS,          \
                                 FROM_BITS);                                    \
    uint has_previous_warp = __clc_riscv32_bool_mask(lane != 0U);               \
    uint previous_partial = __clc_riscv32_shuffle_up_u32(partial, 1U);          \
    uint prefix = __clc_riscv32_select_u32(TO_BITS((TYPE)(IDENTITY)),           \
                                           previous_partial, has_previous_warp); \
    if (warp == 0U)                                                             \
        scratch[lane] = prefix;                                                 \
                                                                                \
    work_group_barrier(CLK_LOCAL_MEM_FENCE);                                    \
                                                                                \
    uint warp_prefix = scratch[warp];                                           \
    uint previous_lane = __clc_riscv32_shuffle_up_u32(value, 1U);               \
    uint has_previous_lane = __clc_riscv32_bool_mask(lane != 0U);               \
    uint lane_prefix = __clc_riscv32_select_u32(TO_BITS((TYPE)(IDENTITY)),      \
                                                previous_lane,                  \
                                                has_previous_lane);             \
    uint result_bits = COMBINE_BITS(FROM_BITS(warp_prefix),                    \
                                    FROM_BITS(lane_prefix), TO_BITS);           \
    TYPE result = FROM_BITS(result_bits);                                       \
    work_group_barrier(CLK_LOCAL_MEM_FENCE);                                    \
    return result;                                                              \
}

#define GEN_SCAN(TYPE, ADD_ID, MAX_ID, MIN_ID, TO_BITS, FROM_BITS)              \
__CLC_RISCV32_WORK_GROUP_SCAN_INCLUSIVE(TYPE, work_group_scan_inclusive_add,    \
                                        __CLC_RISCV32_ADD_BITS, TO_BITS,        \
                                        FROM_BITS, ADD_ID)                      \
__CLC_RISCV32_WORK_GROUP_SCAN_INCLUSIVE(TYPE, work_group_scan_inclusive_max,    \
                                        __CLC_RISCV32_MAX_BITS, TO_BITS,        \
                                        FROM_BITS, MAX_ID)                      \
__CLC_RISCV32_WORK_GROUP_SCAN_INCLUSIVE(TYPE, work_group_scan_inclusive_min,    \
                                        __CLC_RISCV32_MIN_BITS, TO_BITS,        \
                                        FROM_BITS, MIN_ID)                      \
__CLC_RISCV32_WORK_GROUP_SCAN_EXCLUSIVE(TYPE, work_group_scan_exclusive_add,    \
                                        __CLC_RISCV32_ADD_BITS, ADD_ID,         \
                                        TO_BITS, FROM_BITS)                     \
__CLC_RISCV32_WORK_GROUP_SCAN_EXCLUSIVE(TYPE, work_group_scan_exclusive_max,    \
                                        __CLC_RISCV32_MAX_BITS, MAX_ID,         \
                                        TO_BITS, FROM_BITS)                     \
__CLC_RISCV32_WORK_GROUP_SCAN_EXCLUSIVE(TYPE, work_group_scan_exclusive_min,    \
                                        __CLC_RISCV32_MIN_BITS, MIN_ID,         \
                                        TO_BITS, FROM_BITS)

GEN_SCAN(int, 0, INT_MIN, INT_MAX, __CLC_RISCV32_AS_U32, __CLC_RISCV32_FROM_I32)
GEN_SCAN(uint, 0U, 0U, UINT_MAX, __CLC_RISCV32_ID_U32, __CLC_RISCV32_FROM_U32)
GEN_SCAN(float, 0.0f, -INFINITY, INFINITY, __CLC_RISCV32_AS_U32, __CLC_RISCV32_FROM_F32)

#undef GEN_SCAN
#undef __CLC_RISCV32_LAST_LANE_IN_WARP
#undef __CLC_RISCV32_WORK_GROUP_SCAN_EXCLUSIVE
#undef __CLC_RISCV32_WORK_GROUP_SCAN_INCLUSIVE
#undef __CLC_RISCV32_WARP_SCAN_BITS
#undef __CLC_RISCV32_MIN_BITS
#undef __CLC_RISCV32_MAX_BITS
#undef __CLC_RISCV32_ADD_BITS
#undef __CLC_RISCV32_FROM_F32
#undef __CLC_RISCV32_FROM_I32
#undef __CLC_RISCV32_FROM_U32
#undef __CLC_RISCV32_AS_U32
#undef __CLC_RISCV32_ID_U32

#endif
