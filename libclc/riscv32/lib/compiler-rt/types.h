#ifndef TYPES_H
#define TYPES_H

#include <float.h>

typedef char char2  __attribute__((__ext_vector_type__(2)));
typedef char char3  __attribute__((__ext_vector_type__(3)));
typedef char char4  __attribute__((__ext_vector_type__(4)));
typedef char char8  __attribute__((__ext_vector_type__(8)));
typedef char char16 __attribute__((__ext_vector_type__(16)));

typedef unsigned char uchar2  __attribute__((__ext_vector_type__(2)));
typedef unsigned char uchar3  __attribute__((__ext_vector_type__(3)));
typedef unsigned char uchar4  __attribute__((__ext_vector_type__(4)));
typedef unsigned char uchar8  __attribute__((__ext_vector_type__(8)));
typedef unsigned char uchar16 __attribute__((__ext_vector_type__(16)));

typedef short short2  __attribute__((__ext_vector_type__(2)));
typedef short short3  __attribute__((__ext_vector_type__(3)));
typedef short short4  __attribute__((__ext_vector_type__(4)));
typedef short short8  __attribute__((__ext_vector_type__(8)));
typedef short short16 __attribute__((__ext_vector_type__(16)));

typedef unsigned short ushort2  __attribute__((__ext_vector_type__(2)));
typedef unsigned short ushort3  __attribute__((__ext_vector_type__(3)));
typedef unsigned short ushort4  __attribute__((__ext_vector_type__(4)));
typedef unsigned short ushort8  __attribute__((__ext_vector_type__(8)));
typedef unsigned short ushort16 __attribute__((__ext_vector_type__(16)));

typedef int int2  __attribute__((__ext_vector_type__(2)));
typedef int int3  __attribute__((__ext_vector_type__(3)));
typedef int int4  __attribute__((__ext_vector_type__(4)));
typedef int int8  __attribute__((__ext_vector_type__(8)));
typedef int int16 __attribute__((__ext_vector_type__(16)));

typedef unsigned int uint2  __attribute__((__ext_vector_type__(2)));
typedef unsigned int uint3  __attribute__((__ext_vector_type__(3)));
typedef unsigned int uint4  __attribute__((__ext_vector_type__(4)));
typedef unsigned int uint8  __attribute__((__ext_vector_type__(8)));
typedef unsigned int uint16 __attribute__((__ext_vector_type__(16)));

typedef float float2  __attribute__((__ext_vector_type__(2)));
typedef float float3  __attribute__((__ext_vector_type__(3)));
typedef float float4  __attribute__((__ext_vector_type__(4)));
typedef float float8  __attribute__((__ext_vector_type__(8)));
typedef float float16 __attribute__((__ext_vector_type__(16)));
typedef unsigned char __u_char;
typedef unsigned short int __u_short;
typedef unsigned int __u_int;
typedef unsigned long int __u_long;

typedef signed char __int8_t;
typedef unsigned char __uint8_t;
typedef signed short int __int16_t;
typedef unsigned short int __uint16_t;
typedef signed int __int32_t;
typedef unsigned int __uint32_t;

typedef signed long int __int64_t;
typedef unsigned long int __uint64_t;

typedef __int8_t __int_least8_t;
typedef __uint8_t __uint_least8_t;
typedef __int16_t __int_least16_t;
typedef __uint16_t __uint_least16_t;
typedef __int32_t __int_least32_t;
typedef __uint32_t __uint_least32_t;
typedef __int64_t __int_least64_t;
typedef __uint64_t __uint_least64_t;
typedef long int __quad_t;
typedef unsigned long int __u_quad_t;

typedef long int __intmax_t;
typedef unsigned long int __uintmax_t;
typedef unsigned long int __dev_t;
typedef unsigned int __uid_t;
typedef unsigned int __gid_t;
typedef unsigned long int __ino_t;
typedef unsigned long int __ino64_t;
typedef unsigned int __mode_t;
typedef unsigned long int __nlink_t;
typedef long int __off_t;
typedef long int __off64_t;
typedef int __pid_t;
typedef struct {
  int __val[2];
} __fsid_t;
typedef long int __clock_t;
typedef unsigned long int __rlim_t;
typedef unsigned long int __rlim64_t;
typedef unsigned int __id_t;
typedef long int __time_t;
typedef unsigned int __useconds_t;
typedef long int __suseconds_t;
typedef long int __suseconds64_t;

typedef int __daddr_t;
typedef int __key_t;

typedef int __clockid_t;
typedef long CMP_RESULT;
typedef void *__timer_t;

typedef long int __blksize_t;

typedef long int __blkcnt_t;
typedef long int __blkcnt64_t;

typedef unsigned long int __fsblkcnt_t;
typedef unsigned long int __fsblkcnt64_t;

typedef unsigned long int __fsfilcnt_t;
typedef unsigned long int __fsfilcnt64_t;

typedef long int __fsword_t;

typedef long int __ssize_t;

typedef long int __syscall_slong_t;

typedef unsigned long int __syscall_ulong_t;

typedef __off64_t __loff_t;
typedef char *__caddr_t;

typedef long int __intptr_t;

typedef unsigned int __socklen_t;

typedef int __sig_atomic_t;

typedef __int8_t int8_t;
typedef __int16_t int16_t;
typedef __int32_t int32_t;
typedef __int64_t int64_t;

typedef __uint8_t uint8_t;
typedef __uint16_t uint16_t;
typedef __uint32_t uint32_t;
typedef __uint64_t uint64_t;

typedef __int_least8_t int_least8_t;
typedef __int_least16_t int_least16_t;
typedef __int_least32_t int_least32_t;
typedef __int_least64_t int_least64_t;

typedef __uint_least8_t uint_least8_t;
typedef __uint_least16_t uint_least16_t;
typedef __uint_least32_t uint_least32_t;
typedef __uint_least64_t uint_least64_t;

typedef signed char int_fast8_t;

typedef long int int_fast16_t;
typedef long int int_fast32_t;
typedef long int int_fast64_t;

typedef unsigned char uint_fast8_t;

typedef unsigned long int uint_fast16_t;
typedef unsigned long int uint_fast32_t;
typedef unsigned long int uint_fast64_t;

typedef __intmax_t intmax_t;
typedef __uintmax_t uintmax_t;

typedef int32_t si_int;
typedef uint32_t su_int;

typedef int64_t di_int;
typedef uint64_t du_int;

typedef union {
  di_int all;
  struct {

    su_int low;
    si_int high;

  } s;
} dwords;

typedef union {
  du_int all;
  struct {

    su_int low;
    su_int high;

  } s;
} udwords;

typedef int ti_int __attribute__((mode(TI)));
typedef unsigned tu_int __attribute__((mode(TI)));

typedef union {
  ti_int all;
  struct {

    du_int low;
    di_int high;

  } s;
} twords;

typedef union {
  tu_int all;
  struct {

    du_int low;
    du_int high;

  } s;
} utwords;

static __inline ti_int make_ti(di_int h, di_int l) {
  twords r;
  r.s.high = h;
  r.s.low = l;
  return r.all;
}

static __inline tu_int make_tu(du_int h, du_int l) {
  utwords r;
  r.s.high = h;
  r.s.low = l;
  return r.all;
}

typedef union {
  su_int u;
  float f;
} float_bits;

typedef union {
  udwords u;
  double f;
} double_bits;

typedef struct {

  udwords low;
  udwords high;

} uqwords;

typedef union {
  uqwords u;
  long double f;
} long_double_bits;

typedef float _Complex Fcomplex;
typedef double _Complex Dcomplex;
typedef long double _Complex Lcomplex;

__attribute__((noreturn)) void
__compilerrt_abort_impl(const char *file, int line, const char *function);

int __paritysi2(si_int a);
int __paritydi2(di_int a);

di_int __divdi3(di_int a, di_int b);
si_int __divsi3(si_int a, si_int b);
su_int __udivsi3(su_int n, su_int d);

su_int __udivmodsi4(su_int a, su_int b, su_int *rem);
du_int __udivmoddi4(du_int a, du_int b, du_int *rem);

int __clzti2(ti_int a);
tu_int __udivmodti4(tu_int a, tu_int b, tu_int *rem);

typedef uint32_t half_rep_t;
typedef uint64_t rep_t;
typedef int64_t srep_t;
typedef double fp_t;

static __inline int rep_clz(rep_t a) { return __builtin_clzl(a); }

static __inline void wideMultiply(rep_t a, rep_t b, rep_t *hi, rep_t *lo) {

  const uint64_t plolo = (a & 0xffffffffU) * (b & 0xffffffffU);
  const uint64_t plohi = (a & 0xffffffffU) * (b >> 32);
  const uint64_t philo = (a >> 32) * (b & 0xffffffffU);
  const uint64_t phihi = (a >> 32) * (b >> 32);

  const uint64_t r0 = (plolo & 0xffffffffU);
  const uint64_t r1 =
      (plolo >> 32) + (plohi & 0xffffffffU) + (philo & 0xffffffffU);
  *lo = r0 + (r1 << 32);

  *hi = (plohi >> 32) + (philo >> 32) + (r1 >> 32) + phihi;
}

fp_t __adddf3(fp_t a, fp_t b);

static __inline rep_t toRep(fp_t x) {
  const union {
    fp_t f;
    rep_t i;
  } rep = {.f = x};
  return rep.i;
}

static __inline fp_t fromRep(rep_t x) {
  const union {
    fp_t f;
    rep_t i;
  } rep = {.i = x};
  return rep.f;
}

static __inline int normalize(rep_t *significand) {
  const int shift = rep_clz(*significand) - rep_clz((1UL << 52));
  *significand <<= shift;
  return 1 - shift;
}

static __inline void wideLeftShift(rep_t *hi, rep_t *lo, int count) {
  *hi = *hi << count | *lo >> ((sizeof(rep_t) * 8) - count);
  *lo = *lo << count;
}

static __inline void wideRightShiftWithSticky(rep_t *hi, rep_t *lo,
                                              unsigned int count) {
  if (count < (sizeof(rep_t) * 8)) {
    const _Bool sticky = (*lo << ((sizeof(rep_t) * 8) - count)) != 0;
    *lo = *hi << ((sizeof(rep_t) * 8) - count) | *lo >> count | sticky;
    *hi = *hi >> count;
  } else if (count < 2 * (sizeof(rep_t) * 8)) {
    const _Bool sticky = *hi << (2 * (sizeof(rep_t) * 8) - count) | *lo;
    *lo = *hi >> (count - (sizeof(rep_t) * 8)) | sticky;
    *hi = 0;
  } else {
    const _Bool sticky = *hi | *lo;
    *lo = sticky;
    *hi = 0;
  }
}

static __inline fp_t __compiler_rt_logbX(fp_t x) {
  rep_t rep = toRep(x);
  int exp = (rep & (((1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))) - 1U) ^
                    ((1UL << 52) - 1U))) >>
            52;

  if (exp == ((1 << ((sizeof(rep_t) * 8) - 52 - 1)) - 1)) {
    if (((rep & (1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1)))) == 0) ||
        (x != x)) {
      return x;
    } else {
      return -x;
    }
  } else if (x == 0.0) {

    return fromRep((((1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))) - 1U) ^
                    ((1UL << 52) - 1U)) |
                   (1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))));
  }

  if (exp != 0) {

    return exp - (((1 << ((sizeof(rep_t) * 8) - 52 - 1)) - 1) >> 1);
  } else {

    rep &= ((1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))) - 1U);
    const int shift = 1 - normalize(&rep);
    exp = (rep & (((1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))) - 1U) ^
                  ((1UL << 52) - 1U))) >>
          52;
    return exp - (((1 << ((sizeof(rep_t) * 8) - 52 - 1)) - 1) >> 1) - shift;
  }
}

static __inline fp_t __compiler_rt_scalbnX(fp_t x, int y) {
  const rep_t rep = toRep(x);
  int exp = (rep & (((1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1))) - 1U) ^
                    ((1UL << 52) - 1U))) >>
            52;

  if (x == 0.0 || exp == ((1 << ((sizeof(rep_t) * 8) - 52 - 1)) - 1))
    return x;

  rep_t sig = rep & ((1UL << 52) - 1U);
  if (exp == 0) {
    exp += normalize(&sig);
    sig &= ~(1UL << 52);
  }

  if (__builtin_sadd_overflow(exp, y, &exp)) {

    exp = (y >= 0) ? 2147483647 : (-2147483647 - 1);
  }

  const rep_t sign = rep & (1UL << (52 + ((sizeof(rep_t) * 8) - 52 - 1)));
  if (exp >= ((1 << ((sizeof(rep_t) * 8) - 52 - 1)) - 1)) {

    return fromRep(sign |
                   ((rep_t)(((1 << ((sizeof(rep_t) * 8) - 52 - 1)) - 1) - 1)
                    << 52)) *
           2.0f;
  } else if (exp <= 0) {

    fp_t tmp = fromRep(sign | (1UL << 52) | sig);
    exp += (((1 << ((sizeof(rep_t) * 8) - 52 - 1)) - 1) >> 1) - 1;
    if (exp < 1)
      exp = 1;
    tmp *= fromRep((rep_t)exp << 52);
    return tmp;
  } else
    return fromRep(sign | ((rep_t)exp << 52) | sig);
}

static __inline fp_t __compiler_rt_fmaxX(fp_t x, fp_t y) {

  return (__builtin_isnan((x)) || x < y) ? y : x;
}

static __inline fp_t __compiler_rt_logb(fp_t x) {
  return __compiler_rt_logbX(x);
}
static __inline fp_t __compiler_rt_scalbn(fp_t x, int y) {
  return __compiler_rt_scalbnX(x, y);
}
static __inline fp_t __compiler_rt_fmax(fp_t x, fp_t y) {

  return __compiler_rt_fmaxX(x, y);
}

typedef enum {
  CRT_FE_TONEAREST,
  CRT_FE_DOWNWARD,
  CRT_FE_UPWARD,
  CRT_FE_TOWARDZERO
} CRT_FE_ROUND_MODE;

// FIXME:
static CRT_FE_ROUND_MODE __fe_getround() { return CRT_FE_TONEAREST; }

static int __fe_raise_inexact(void) { return 0; }

typedef struct {
  float value;
  unsigned word;
} ieee_float_shape_type;

typedef union {
  double value;
  struct {
    uint32_t lsw;
    uint32_t msw;
  } parts;
  uint64_t word;
} ieee_double_shape_type;

union ieee754_double {
  double d;

  struct {

    unsigned int mantissa1;
    unsigned int mantissa0;
    unsigned int exponent;
    unsigned int negative;

  } ieee;

  struct {

    unsigned int mantissa1;
    unsigned int mantissa0;
    unsigned int quiet_nan;
    unsigned int exponent;
    unsigned int negative;

  } ieee_nan;
};

typedef struct {
  unsigned short int __control_word;
  unsigned short int __glibc_reserved1;
  unsigned short int __status_word;
  unsigned short int __glibc_reserved2;
  unsigned short int __tags;
  unsigned short int __glibc_reserved3;
  unsigned int __eip;
  unsigned short int __cs_selector;
  unsigned int __opcode;
  unsigned int __glibc_reserved4;
  unsigned int __data_offset;
  unsigned short int __data_selector;
  unsigned short int __glibc_reserved5;

  unsigned int __mxcsr;

} fenv_t;

static int fetestexcept(int excepts) { return 0; }
static int default_libc_feupdateenv_test(fenv_t *e, int ex) {
  int ret = fetestexcept(ex);
  return ret;
}

typedef union
{
  int4 i[2];
  double x;
  double d;
} mynumber;

union dshape {
	double value;
	uint64_t bits;
};

#define EXTRACT_WORDS(hi,lo,d)                                  \
do {                                                            \
  union dshape __u;                                             \
  __u.value = (d);                                              \
  (hi) = __u.bits >> 32;                                        \
  (lo) = (uint32_t)__u.bits;                                    \
} while (0)

#define INSERT_WORDS(d,hi,lo)                                   \
do {                                                            \
  union dshape __u;                                             \
  __u.bits = ((uint64_t)(hi) << 32) | (uint32_t)(lo);           \
  (d) = __u.value;                                              \
} while (0)


static int __attribute__((noinline)) clzl(unsigned long x)
//static int inline clzl(unsigned long x)
{
    for (int i = 0; i != 64; ++i)
         if ((x >> (63 - i)) & 1)
             return i;

    return 0;
}

static int ctz64(unsigned long x)
{
  int r = 63;

  x &= ~x + 1;
  if (x & 0x00000000FFFFFFFF) r -= 32;
  if (x & 0x0000FFFF0000FFFF) r -= 16;
  if (x & 0x00FF00FF00FF00FF) r -= 8;
  if (x & 0x0F0F0F0F0F0F0F0F) r -= 4;
  if (x & 0x3333333333333333) r -= 2;
  if (x & 0x5555555555555555) r -= 1;

  return r;
}

static int clz64(unsigned long x) {
  int r = 0;

  if ((x & 0xFFFFFFFF00000000) == 0) r += 32, x <<= 32;
  if ((x & 0xFFFF000000000000) == 0) r += 16, x <<= 16;
  if ((x & 0xFF00000000000000) == 0) r += 8,  x <<= 8;
  if ((x & 0xF000000000000000) == 0) r += 4,  x <<= 4;
  if ((x & 0xC000000000000000) == 0) r += 2,  x <<= 2;
  if ((x & 0x8000000000000000) == 0) r += 1,  x <<= 1;

  return r;
}

#endif // TYPES_H
