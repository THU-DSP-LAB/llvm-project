#include "../clcmacro.h"
#include <clc/clc.h>

_CLC_OVERLOAD _CLC_DEF char ctz(char x) {
  uchar ux = (uchar)x;
  if (ux == 0) return 8;
  char result = 0;
  if ((x & 0x0F) == 0) {
    result += 4;
    x >>= 4;
  }
  if ((x & 0x03) == 0) {
    result += 2;
    x >>= 2;
  }
  if ((x & 0x01) == 0) {
    result += 1;
  }
  return result;
}
_CLC_OVERLOAD _CLC_DEF uchar ctz(uchar x) {
  uchar result = 0;
  if (x == 0)
    return 8;
  if ((x & 0x0F) == 0) {
    result += 4;
    x >>= 4;
  }
  if ((x & 0x03) == 0) {
    result += 2;
    x >>= 2;
  }
  if ((x & 0x01) == 0) {
    result += 1;
  }
  return result;
}

_CLC_OVERLOAD _CLC_DEF short ctz(short x) {
  ushort ux = (ushort)x;
  if (ux == 0)
    return 16;
    uchar n = 0;
    if ((x & 0x00FF) == 0) { n += 8; x >>= 8; }
    if ((x & 0x000F) == 0) { n += 4; x >>= 4; }
    if ((x & 0x0003) == 0) { n += 2; x >>= 2; }
    if ((x & 0x0001) == 0) { n += 1; }
    return n;
}

_CLC_OVERLOAD _CLC_DEF ushort ctz(ushort x) {
    if (x == 0) return 16;
    uchar n = 0;
    if ((x & 0x00FF) == 0) { n += 8; x >>= 8; }
    if ((x & 0x000F) == 0) { n += 4; x >>= 4; }
    if ((x & 0x0003) == 0) { n += 2; x >>= 2; }
    if ((x & 0x0001) == 0) { n += 1; }
    return n;
}

_CLC_OVERLOAD _CLC_DEF int ctz(int x) {
  uint ux = (uint)x;
  if (ux == 0)
    return 32;
  int n = 1;
  if ((ux & 0xffff) == 0) {
    n += 16;
    ux >>= 16;
  }
  if ((ux & 0x00ff) == 0) {
    n += 8;
    ux >>= 8;
  }
  if ((ux & 0x000f) == 0) {
    n += 4;
    ux >>= 4;
  }
  if ((ux & 0x0003) == 0) {
    n += 2;
    ux >>= 2;
  }
  return n - (ux & 1);
}

_CLC_OVERLOAD _CLC_DEF uint ctz(uint ux) {
  if (ux == 0)
    return 32;
  int n = 1;
  if ((ux & 0xffff) == 0) {
    n += 16;
    ux >>= 16;
  }
  if ((ux & 0x00ff) == 0) {
    n += 8;
    ux >>= 8;
  }
  if ((ux & 0x000f) == 0) {
    n += 4;
    ux >>= 4;
  }
  if ((ux & 0x0003) == 0) {
    n += 2;
    ux >>= 2;
  }
  return n - (ux & 1);
}

_CLC_OVERLOAD _CLC_DEF long ctz(long x) {
  ulong ux = (ulong)x;
  if (ux == 0)
    return 64;
  int n = 1;
  if ((ux & 0xffffffff) == 0) {
    n += 32;
    ux >>= 32;
  }
  if ((ux & 0x0000ffff) == 0) {
    n += 16;
    ux >>= 16;
  }
  if ((ux & 0x000000ff) == 0) {
    n += 8;
    ux >>= 8;
  }
  if ((ux & 0x0000000f) == 0) {
    n += 4;
    ux >>= 4;
  }
  if ((ux & 0x00000003) == 0) {
    n += 2;
    ux >>= 2;
  }
  return n - (ux & 1);
}

_CLC_OVERLOAD _CLC_DEF ulong ctz(ulong ux) {
  if (ux == 0)
    return 64;
  int n = 1;
  if ((ux & 0xffffffff) == 0) {
    n += 32;
    ux >>= 32;
  }
  if ((ux & 0x0000ffff) == 0) {
    n += 16;
    ux >>= 16;
  }
  if ((ux & 0x000000ff) == 0) {
    n += 8;
    ux >>= 8;
  }
  if ((ux & 0x0000000f) == 0) {
    n += 4;
    ux >>= 4;
  }
  if ((ux & 0x00000003) == 0) {
    n += 2;
    ux >>= 2;
  }
  return n - (ux & 1);
}

_CLC_UNARY_VECTORIZE(_CLC_OVERLOAD _CLC_DEF, char, ctz, char)
_CLC_UNARY_VECTORIZE(_CLC_OVERLOAD _CLC_DEF, uchar, ctz, uchar)
_CLC_UNARY_VECTORIZE(_CLC_OVERLOAD _CLC_DEF, short, ctz, short)
_CLC_UNARY_VECTORIZE(_CLC_OVERLOAD _CLC_DEF, ushort, ctz, ushort)
_CLC_UNARY_VECTORIZE(_CLC_OVERLOAD _CLC_DEF, int, ctz, int)
_CLC_UNARY_VECTORIZE(_CLC_OVERLOAD _CLC_DEF, uint, ctz, uint)
_CLC_UNARY_VECTORIZE(_CLC_OVERLOAD _CLC_DEF, long, ctz, long)
_CLC_UNARY_VECTORIZE(_CLC_OVERLOAD _CLC_DEF, ulong, ctz, ulong)
