#include <clc/clc.h>
#include "../clcmacro.h"

_CLC_OVERLOAD _CLC_DEF char mad_hi(char x, char y, char z) {
    return mul_hi(x, y) + z;
}

_CLC_OVERLOAD _CLC_DEF uchar mad_hi(uchar x, uchar y, uchar z) {
    return mul_hi(x, y) + z;
}

_CLC_OVERLOAD _CLC_DEF short mad_hi(short x, short y, short z) {
    return mul_hi(x, y) + z;
}

_CLC_OVERLOAD _CLC_DEF ushort mad_hi(ushort x, ushort y, ushort z) {
    return mul_hi(x, y) + z;
}

_CLC_OVERLOAD _CLC_DEF int mad_hi(int x, int y, int z) {
    return mul_hi(x, y) + z;
}

_CLC_OVERLOAD _CLC_DEF uint mad_hi(uint x, uint y, uint z) {
    return mul_hi(x, y) + z;
}

_CLC_OVERLOAD _CLC_DEF long mad_hi(long x, long y, long z) {
    return mul_hi(x, y) + z;
}

_CLC_OVERLOAD _CLC_DEF ulong mad_hi(ulong x, ulong y, ulong z) {
    return mul_hi(x, y) + z;
}


_CLC_TERNARY_VECTORIZE(_CLC_OVERLOAD _CLC_DEF, char, mad_hi, char, char, char)
_CLC_TERNARY_VECTORIZE(_CLC_OVERLOAD _CLC_DEF, uchar, mad_hi, uchar, uchar, uchar)
_CLC_TERNARY_VECTORIZE(_CLC_OVERLOAD _CLC_DEF, short, mad_hi, short, short, short)
_CLC_TERNARY_VECTORIZE(_CLC_OVERLOAD _CLC_DEF, ushort, mad_hi, ushort, ushort, ushort)
_CLC_TERNARY_VECTORIZE(_CLC_OVERLOAD _CLC_DEF, int, mad_hi, int, int, int)
_CLC_TERNARY_VECTORIZE(_CLC_OVERLOAD _CLC_DEF, uint, mad_hi, uint, uint, uint)
_CLC_TERNARY_VECTORIZE(_CLC_OVERLOAD _CLC_DEF, long, mad_hi, long, long, long)
_CLC_TERNARY_VECTORIZE(_CLC_OVERLOAD _CLC_DEF, ulong, mad_hi, ulong, ulong, ulong)
