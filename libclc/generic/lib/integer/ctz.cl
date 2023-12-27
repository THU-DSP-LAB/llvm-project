#include "../clcmacro.h"
#include <clc/clc.h>

_CLC_OVERLOAD _CLC_DEF short ctz(short x) {
  return x ? __builtin_ctzs(x) : 16;
}

_CLC_OVERLOAD _CLC_DEF ushort ctz(ushort x) {
  return x ? __builtin_ctzs(x) : 16;
}

_CLC_OVERLOAD _CLC_DEF char ctz(char x) {
  return x ? ctz((ushort)(uchar)x) : 8;
}
_CLC_OVERLOAD _CLC_DEF uchar ctz(uchar x) {
  return x ? ctz((ushort)x) : 8;
}

_CLC_OVERLOAD _CLC_DEF int ctz(int x) {
  return x ? __builtin_ctz(x) : 32;
}

_CLC_OVERLOAD _CLC_DEF uint ctz(uint ux) {
  return ux ? __builtin_ctz(ux) : 32;
}

_CLC_OVERLOAD _CLC_DEF long ctz(long x) {
  return x ? __builtin_ctzl(x) : 64;
}

_CLC_OVERLOAD _CLC_DEF ulong ctz(ulong ux) {
  return ux ? __builtin_ctzl(ux) : 64;
}

_CLC_UNARY_VECTORIZE(_CLC_OVERLOAD _CLC_DEF, char, ctz, char)
_CLC_UNARY_VECTORIZE(_CLC_OVERLOAD _CLC_DEF, uchar, ctz, uchar)
_CLC_UNARY_VECTORIZE(_CLC_OVERLOAD _CLC_DEF, short, ctz, short)
_CLC_UNARY_VECTORIZE(_CLC_OVERLOAD _CLC_DEF, ushort, ctz, ushort)
_CLC_UNARY_VECTORIZE(_CLC_OVERLOAD _CLC_DEF, int, ctz, int)
_CLC_UNARY_VECTORIZE(_CLC_OVERLOAD _CLC_DEF, uint, ctz, uint)
_CLC_UNARY_VECTORIZE(_CLC_OVERLOAD _CLC_DEF, long, ctz, long)
_CLC_UNARY_VECTORIZE(_CLC_OVERLOAD _CLC_DEF, ulong, ctz, ulong)
