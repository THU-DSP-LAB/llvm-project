#if __OPENCL_C_VERSION__ >= 200
#include "wg.h"

#define __CLC_RISCV32_ID_U32(x) (x)
#define __CLC_RISCV32_AS_U32(x) as_uint(x)
#define __CLC_RISCV32_FROM_U32(x) (x)
#define __CLC_RISCV32_FROM_I32(x) as_int(x)
#define __CLC_RISCV32_FROM_F32(x) as_float(x)

#define __CLC_RISCV32_BROADCAST_BODY(SOURCE, TO_BITS, FROM_BITS)                \
    uint __source = (uint)(SOURCE);                                              \
    __local uint *scratch = __clc_riscv32_get_work_group_scratch();             \
                                                                                \
    if (__clc_riscv32_local_linear_id() == __source)                            \
        scratch[0] = TO_BITS(a);                                                \
                                                                                \
    work_group_barrier(CLK_LOCAL_MEM_FENCE);                                    \
    uint value = scratch[0];                                                    \
    work_group_barrier(CLK_LOCAL_MEM_FENCE);                                    \
    return FROM_BITS(value);

#define GEN_BROADCAST(TYPE, TO_BITS, FROM_BITS)                                 \
__attribute__((overloadable, weak, always_inline)) _CLC_DEF _CLC_OVERLOAD TYPE  \
work_group_broadcast(TYPE a, uint local_id_x)                                   \
{                                                                               \
    __CLC_RISCV32_BROADCAST_BODY(local_id_x, TO_BITS, FROM_BITS)                \
}                                                                               \
                                                                                \
__attribute__((overloadable, weak, always_inline)) _CLC_DEF _CLC_OVERLOAD TYPE  \
work_group_broadcast(TYPE a, size_t local_id_x, size_t local_id_y)              \
{                                                                               \
    size_t source = local_id_y * get_local_size(0) + local_id_x;                \
    __CLC_RISCV32_BROADCAST_BODY(source, TO_BITS, FROM_BITS)                    \
}                                                                               \
                                                                                \
__attribute__((overloadable, weak, always_inline)) _CLC_DEF _CLC_OVERLOAD TYPE  \
work_group_broadcast(TYPE a, size_t local_id_x, size_t local_id_y,              \
                     size_t local_id_z)                                        \
{                                                                               \
    size_t source = (local_id_z * get_local_size(1) + local_id_y) *             \
                        get_local_size(0) +                                     \
                    local_id_x;                                                 \
    __CLC_RISCV32_BROADCAST_BODY(source, TO_BITS, FROM_BITS)                    \
}

GEN_BROADCAST(uint, __CLC_RISCV32_ID_U32, __CLC_RISCV32_FROM_U32)
GEN_BROADCAST(int, __CLC_RISCV32_AS_U32, __CLC_RISCV32_FROM_I32)
GEN_BROADCAST(float, __CLC_RISCV32_AS_U32, __CLC_RISCV32_FROM_F32)

#undef GEN_BROADCAST
#undef __CLC_RISCV32_BROADCAST_BODY
#undef __CLC_RISCV32_FROM_F32
#undef __CLC_RISCV32_FROM_I32
#undef __CLC_RISCV32_FROM_U32
#undef __CLC_RISCV32_AS_U32
#undef __CLC_RISCV32_ID_U32

#endif
