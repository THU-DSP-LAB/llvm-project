
#define assert(x)

// OpenCL decls
// #include "clc/clc.h"

#define NULL ((void *)0)

#include "errol.h"
#include "itoa_c.h"

/*
 * floating point format definitions
 */

typedef double fpnum_t;

static inline double fpnext(double val) {
  errol_bits_t bits = {val};
  bits.i++;
  return bits.d;
}

static inline double fpprev(double val) {
  errol_bits_t bits = {val};
  bits.i--;
  return bits.d;
}

static inline uint64_t fpeint(double from) {
  errol_bits_t bits = {from};
  assert((bits.i & ((1ULL << 52) - 1)) == 0 && (bits.i >> 52) >= 1023);

  return (uint64_t)1 << ((bits.i >> 52) - 1023);
}

#define ERROL0_EPSILON 0.0000001
#define ERROL1_EPSILON 8.77e-15

/**
 * High-precision data structure.
 *   @val, off: The value and offset.
 */

typedef struct hp_t {
  fpnum_t val, off;
} hp_t;

/*
 * lookup table data
 */

#include "enum3.h"
#include "enum4.h"
#include "lookup.h"

/*
 * high-precision constants
 */

/*
 * local function declarations
 */

static void inline hp_normalize(struct hp_t *hp);
static void inline hp_mul10(struct hp_t *hp);
static void inline hp_div10(struct hp_t *hp);
static inline struct hp_t hp_prod(struct hp_t *in, double val);
static int inline mismatch10(unsigned long a, unsigned long b);
static int inline table_lower_bound(unsigned long *table, int n,
                                    unsigned long k);

/*
 * inline function instantiations
 */

extern inline char *u32toa(int value, char *buffer);
extern inline char *u64toa(long value, char *buffer);

/*
 * intrinsics
 */

extern uint64_t __udivmodti4(uint64_t a, uint64_t b, uint64_t *rem);

static char *strcpy(char *dest, const char *src) {
  char *temp = dest;
  while ((*dest++ = *src++))
    ;
  return temp;
}
/**
 * Corrected Errol4 double to ASCII conversion.
 *   @val: The value.
 *   @buf: The output buffer.
 *   &returns: The exponent.
 */

int errol4_dtoa(double val, char *buf) {
  errol_bits_t k = {val};

  int n = sizeof(errol_enum4) / sizeof(unsigned long);
  int i = table_lower_bound(errol_enum4, n, k.i);
  if (i < n && errol_enum4[i] == k.i) {
    strcpy(buf, errol_enum4_data[i].str);
    return errol_enum4_data[i].exp;
  }

  return errol4u_dtoa(val, buf);
}

/**
 * Uncorrected Errol4 double to ASCII conversion.
 *   @val: The value.
 *   @buf: The output buffer.
 *   &returns: The exponent.
 */

inline int errol4u_dtoa(double val, char *buf) {
  int e;
  int exp;
  struct hp_t mid;
  double ten, lten;

  /* check if in integer or fixed range */

  if ((val >= 1.80143985094820e+16) && (val < 3.40282366920938e+38))
    return errol_int(val, buf);
  else if ((val >= 16.0) && (val <= 9.007199254740992e15))
    return errol_fixed(val, buf);

  /* normalize the midpoint */
  // FIXME: here pocl use e = expfrexp(val), maybe later we need to add
  // this builtin function support
  e = val;
  exp = 290 + (double)e * 0.30103;
  if (exp < 20)
    exp = 20;
  else if (exp >= LOOKUP_TABLE_LEN)
    exp = LOOKUP_TABLE_LEN - 1;

  mid = lookup_table[exp];
  lten = mid.val;
  mid = hp_prod(&mid, val);

  ten = 1.0;
  exp -= 290;

  if (mid.val < 1.00000000000000016e+17) {
    e = val;
    exp = 290 + (double)e * 0.30103;

    mid = hp_prod(&mid, val);
    lten *= mid.val;
    mid = hp_prod(&mid, val);
  }

  while (mid.val < 1.00000000000000016e+17)
    exp--, hp_mul10(&mid), ten *= 10.0;

  double diff = (fpnext(val) - val) * lten * ten / 2.0;
  unsigned long val64 = (unsigned long)mid.val;
  unsigned long lo64 = val64 + (unsigned long)(mid.off - diff);
  unsigned long mid64;
  unsigned long hi64 = val64 + (unsigned long)(mid.off + diff);

  if (hi64 >= 1e18)
    exp++;

  unsigned long iten;
  for (iten = 1;; iten *= 10) {
    lo64 /= 10;
    hi64 /= 10;

    if (lo64 == hi64)
      break;
  }

  mid64 = (val64 + (unsigned long)(mid.off + iten * 0.5)) / iten;

  if (hi64 > 0)
    buf = u64toa(hi64, buf);
  *buf++ = mid64 % 10 + '0';
  *buf = '\0';

  return exp;
}

/**
 * Integer conversion algorithm, guaranteed correct, optimal, and best.
 *   @val: The val.
 *   @buf: The output buffer.
 *   &return: The exponent.
 */

int errol_int(double val, char *buf) {
  int exp;
  errol_bits_t bits;
  uint64_t low, mid, high;
  static uint64_t pow19 = (uint64_t)1e19;

  assert((val > 9.007199254740992e15) && val < (3.40282366920938e38));

  mid = (uint64_t)val;
  low = mid - fpeint((fpnext(val) - val) / 2.0);
  high = mid + fpeint((val - fpprev(val)) / 2.0);

  bits.d = val;
  if (bits.i & 0x1)
    high--;
  else
    low--;

  uint64_t tmp1, tmp2;
  __udivmodti4(__udivmodti4(low, pow19, &tmp1), pow19, &tmp2);
  unsigned long l64 = tmp1;
  unsigned long lf = tmp2;
  __udivmodti4(__udivmodti4(high, pow19, &tmp1), pow19, &tmp2);
  unsigned long h64 = tmp1;
  unsigned long hf = tmp2;

  if (lf != hf) {
    l64 = lf;
    h64 = hf;
    mid = __udivmodti4(mid, pow19 / 10, NULL);
  }

  int mi = mismatch10(l64, h64);
  unsigned long x = 1;
  for (int i = (lf == hf); i < mi; i++)
    x *= 10;
  unsigned long m64 = __udivmodti4(mid, x, NULL);

  if (lf != hf)
    mi += 19;

  char *p = u64toa(m64, buf) - 1;

  if (mi != 0)
    p[-1] += (*p >= '5');
  else
    ++p;

  exp = p - buf + mi;
  *p = '\0';

  return exp;
}

/**
 * Fixed point conversion algorithm, guaranteed correct, optimal, and best.
 *   @val: The val.
 *   @buf: The output buffer.
 *   &return: The exponent.
 */

int errol_fixed(double val, char *buf) {
  char *p;
  int j, exp;
  double n, mid, lo, hi;
  unsigned long u;

  assert((val >= 16.0) && (val <= 9.007199254740992e15));

  u = (unsigned long)val;
  n = (double)u;

  mid = val - n;
  lo = ((fpprev(val) - n) + mid) / 2.0;
  hi = ((fpnext(val) - n) + mid) / 2.0;

  p = u64toa(u, buf);
  j = exp = p - buf;
  buf[j] = '\0';

  if (mid != 0.0) {
    while (mid != 0.0) {
      int ldig, mdig, hdig;

      lo *= 10.0;
      ldig = (int)lo;
      lo -= ldig;

      mid *= 10.0;
      mdig = (int)mid;
      mid -= mdig;

      hi *= 10.0;
      hdig = (int)hi;
      hi -= hdig;

      buf[j++] = mdig + '0';

      if (hdig != ldig || j > 50)
        break;
    }

    if (mid > 0.5)
      buf[j - 1]++;
    else if ((mid == 0.5) && (buf[j - 1] & 0x1))
      buf[j - 1]++;
  } else {
    while (buf[j - 1] == '0') {
      buf[j - 1] = '\0';
      j--;
    }
  }

  buf[j] = '\0';

  return exp;
}

/**
 * Normalize the number by factoring in the error.
 *   @hp: The float pair.
 */

static inline void hp_normalize(struct hp_t *hp) {
  fpnum_t val = hp->val;

  hp->val += hp->off;
  hp->off += val - hp->val;
}

/**
 * Multiply the high-precision number by ten.
 *   @hp: The high-precision number
 */

static inline void hp_mul10(struct hp_t *hp) {
  fpnum_t off, val = hp->val;

  hp->val *= 10.0;
  hp->off *= 10.0;

  off = hp->val;
  off -= val * 8.0;
  off -= val * 2.0;

  hp->off -= off;

  hp_normalize(hp);
}

/**
 * Divide the high-precision number by ten.
 *   @hp: The high-precision number
 */

// static inline void hp_div10(struct hp_t *hp) {
//   double val = hp->val;

//   hp->val /= 10.0;
//   hp->off /= 10.0;

//   val -= hp->val * 8.0;
//   val -= hp->val * 2.0;

//   hp->off += val / 10.0;

//   hp_normalize(hp);
// }

static inline double gethi(double in) {
  errol_bits_t v = {.d = in};

  // v.i += 0x0000000004000000;
  v.i &= 0xFFFFFFFFF8000000;

  return v.d;
}

/**
 * Split a double into two halves.
 *   @val: The double.
 *   @hi: The high bits.
 *   @lo: The low bits.
 */

static inline void split(double val, double *hi, double *lo) {
  // double t = (134217728.0 + 1.0) * val;

  //*hi = t - (t - val);
  // if(*hi != gethi(val))
  //*lo = val - *hi;

  *hi = gethi(val);
  *lo = val - *hi;
}

/**
 * Compute the product of an HP number and a double.
 *   @in: The HP number.
 *   @val: The double.
 *   &returns: The HP number.
 */

inline struct hp_t hp_prod( struct hp_t *in, double val) {
  double p, hi, lo, e;

  double hi2, lo2;
  split(in->val, &hi, &lo);
  split(val, &hi2, &lo2);

  p = in->val * val;
  e = ((hi * hi2 - p) + lo * hi2 + hi * lo2) + lo * lo2;

  return (struct hp_t){p, in->off * val + e};
}

/**
 * Given two different integers with the same length in terms of the number
 * of decimal digits, index the digits from the right-most position starting
 * from zero, find the first index where the digits in the two integers
 * divergent starting from the highest index.
 *   @a: Integer a.
 *   @b: Integer b.
 *   &returns: An index within [0, 19).
 */

static inline int mismatch10(unsigned long a, unsigned long b) {
  unsigned long pow10 = 10000000000U;
  unsigned long af = a / pow10;
  unsigned long bf = b / pow10;
  int i = 0;

  if (af != bf) {
    i = 10;
    a = af;
    b = bf;
  }

  for (;; ++i) {
    a /= 10;
    b /= 10;

    if (a == b)
      return i;
  }
}

/**
 * Find the insertion point for a key in a level-order array.
 *   @table: The target array.
 *   @n: Array size.
 *   @k: The key to find.
 *   &returns: The insertion point.
 */

static inline int table_lower_bound(unsigned long *table, int n,
                                    unsigned long k) {
  int i = n, j = 0;

  while (j < n) {
    if (table[j] < k)
      j = 2 * j + 2;
    else {
      i = j;
      j = 2 * j + 1;
    }
  }

  return i;
}
