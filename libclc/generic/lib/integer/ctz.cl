#include <clc/clc.h>
#include "../clcmacro.h"

// Software implementation of ctz functions for RISCV32 architecture
#ifdef CLC_RISCV32

// Software implementation of 32-bit ctz
static uint __clc_sw_ctz32(uint x) {
  if (x == 0) return 32;
  
  uint count = 0;
  // Use binary search algorithm
  if ((x & 0xFFFFU) == 0) { count += 16; x >>= 16; }
  if ((x & 0xFFU) == 0)   { count += 8;  x >>= 8; }
  if ((x & 0xFU) == 0)    { count += 4;  x >>= 4; }
  if ((x & 0x3U) == 0)    { count += 2;  x >>= 2; }
  if ((x & 0x1U) == 0)    { count += 1; }
  
  return count;
}

// Software implementation of 16-bit ctz
static ushort __clc_sw_ctz16(ushort x) {
  if (x == 0) return 16;
  
  ushort count = 0;
  if ((x & 0xFFU) == 0) { count += 8; x >>= 8; }
  if ((x & 0xFU) == 0)  { count += 4; x >>= 4; }
  if ((x & 0x3U) == 0)  { count += 2; x >>= 2; }
  if ((x & 0x1U) == 0)  { count += 1; }
  
  return count;
}

// Software implementation of 64-bit ctz
static ulong __clc_sw_ctz64(ulong x) {
  if (x == 0) return 64;
  
  ulong count = 0;
  if ((x & 0xFFFFFFFFUL) == 0) { count += 32; x >>= 32; }
  if ((x & 0xFFFFUL) == 0)     { count += 16; x >>= 16; }
  if ((x & 0xFFUL) == 0)       { count += 8;  x >>= 8; }
  if ((x & 0xFUL) == 0)        { count += 4;  x >>= 4; }
  if ((x & 0x3UL) == 0)        { count += 2;  x >>= 2; }
  if ((x & 0x1UL) == 0)        { count += 1; }
  
  return count;
}

#endif

_CLC_OVERLOAD _CLC_DEF char ctz(char x) {
#ifdef CLC_RISCV32
  return x ? __clc_sw_ctz16((ushort)(uchar)x) : 8;
#else
  return x ? ctz((ushort)(uchar)x) : 8;
#endif
}
_CLC_OVERLOAD _CLC_DEF uchar ctz(uchar x) {
#ifdef CLC_RISCV32
  return x ? __clc_sw_ctz16((ushort)x) : 8;
#else
  return x ? ctz((ushort)x) : 8;
#endif
}

_CLC_OVERLOAD _CLC_DEF short ctz(short x) {
#ifdef CLC_RISCV32
  return x ? __clc_sw_ctz16(x) : 16;
#else
  return x ? __builtin_ctzs(x) : 16;
#endif
}

_CLC_OVERLOAD _CLC_DEF ushort ctz(ushort x) {
#ifdef CLC_RISCV32
  return x ? __clc_sw_ctz16(x) : 16;
#else
  return x ? __builtin_ctzs(x) : 16;
#endif
}

_CLC_OVERLOAD _CLC_DEF int ctz(int x) {
#ifdef CLC_RISCV32
  return x ? __clc_sw_ctz32(x) : 32;
#else
  return x ? __builtin_ctz(x) : 32;
#endif
}

_CLC_OVERLOAD _CLC_DEF uint ctz(uint x) {
#ifdef CLC_RISCV32
  return x ? __clc_sw_ctz32(x) : 32;
#else
  return x ? __builtin_ctz(x) : 32;
#endif
}

_CLC_OVERLOAD _CLC_DEF long ctz(long x) {
#ifdef CLC_RISCV32
  return x ? __clc_sw_ctz64(x) : 64;
#else
  return x ? __builtin_ctzl(x) : 64;
#endif
}

_CLC_OVERLOAD _CLC_DEF ulong ctz(ulong x) {
#ifdef CLC_RISCV32
  return x ? __clc_sw_ctz64(x) : 64;
#else
  return x ? __builtin_ctzl(x) : 64;
#endif
}

_CLC_UNARY_VECTORIZE(_CLC_OVERLOAD _CLC_DEF, char, ctz, char)
_CLC_UNARY_VECTORIZE(_CLC_OVERLOAD _CLC_DEF, uchar, ctz, uchar)
_CLC_UNARY_VECTORIZE(_CLC_OVERLOAD _CLC_DEF, short, ctz, short)
_CLC_UNARY_VECTORIZE(_CLC_OVERLOAD _CLC_DEF, ushort, ctz, ushort)
_CLC_UNARY_VECTORIZE(_CLC_OVERLOAD _CLC_DEF, int, ctz, int)
_CLC_UNARY_VECTORIZE(_CLC_OVERLOAD _CLC_DEF, uint, ctz, uint)
_CLC_UNARY_VECTORIZE(_CLC_OVERLOAD _CLC_DEF, long, ctz, long)
_CLC_UNARY_VECTORIZE(_CLC_OVERLOAD _CLC_DEF, ulong, ctz, ulong)
