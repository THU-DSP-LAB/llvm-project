/*
   Copyright (c) 2018 Michal Babej / Tampere University of Technology

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
*/

#include "types.h"
typedef struct {
  char zero : 1;
  char alt : 1;
  char align_left : 1;
  char uc : 1;
  char always_sign : 1;
  char sign : 1;
  char space : 1;

  char nonzeroparam : 1;
} flags_t;

typedef struct {
  char *bf;
  char *restrict printf_buffer;
  uint printf_buffer_index;
  uint printf_buffer_capacity;
  int precision;
  unsigned width;
  unsigned base;
  flags_t flags;
  char conv;
} param_t;

void printf_putchw(param_t *p);

void printf_putcf(param_t *p, char c);

void printf_puts(param_t *p, const char *string);

void printf_nonfinite(param_t *p, const char *ptr);

void printf_puts_ljust(param_t *p, const char *string, int width,
                       ssize_t max_width);

void printf_puts_rjust(param_t *p, const char *string, int width,
                       ssize_t max_width);

void printf_ptr(param_t *p, const void *ptr);

void printf_ulong(param_t *p, uint32_t u);

void printf_long(param_t *p, int32_t i);

void printf_float(param_t *p, float f);

typedef int bool;

int errol4_dtoa(double val, char *buf);
int errol4u_dtoa(double val, char *buf);

int errol_int(double val, char *buf);
int errol_fixed(double val, char *buf);

struct errol_err_t {
  double val;
  char str[18];
  int32_t exp;
};

struct errol_slab_t {
  char str[18];
  int32_t exp;
};

typedef union {
  double d;
  int64_t i;
} errol_bits_t;

int generate_float_digits(float f, char *buffer) {
  return errol4_dtoa(f, buffer);
}

void printf_putcf(param_t *p, char c) {
  if (p->printf_buffer_index < p->printf_buffer_capacity)
    p->printf_buffer[p->printf_buffer_index++] = c;
}

void printf_puts(param_t *p, const char *string) {
  char c;
  while ((c = *string++)) {
    if (p->printf_buffer_index < p->printf_buffer_capacity)
      p->printf_buffer[p->printf_buffer_index++] = c;
  }
}

void printf_ul_base(param_t *p, uint32_t num) {
  char temp[64];
  unsigned i = 0, j = 0;
  unsigned digit;
  unsigned base = p->base;
  while (num > 0) {
    digit = num % base;
    num = num / base;
    temp[i++] = '0' + digit;
  }

  if (p->precision > 0) {
    for (; i < p->precision; i++)
      temp[i] = '0';
  }

  char *out = p->bf;
  for (j = i; j > 0; --j)
    *out++ = temp[j - 1];
  *out = 0;
}

void printf_ul16(param_t *p, uint32_t num) {
  char temp[64];
  unsigned i = 0, j = 0;
  unsigned digit;
  const unsigned base = 16;
  char digit_offset = (p->flags.uc ? 'A' : 'a');
  while (num > 0) {
    digit = num % base;
    num = num / base;
    temp[i++] = ((digit < 10) ? ('0' + digit) : (digit_offset + digit - 10));
  }

  if (p->precision > 0) {
    for (; i < p->precision; i++)
      temp[i] = '0';
  }

  char *out = p->bf;
  for (j = i; j > 0; --j)
    *out++ = temp[j - 1];
  *out = 0;
}

void printf_l_base(param_t *p, int32_t num) {
  uint32_t n;
  if (num < 0) {
    if (num == (-2147483647 - 1))
      n = (uint32_t)num;
    else
      n = -num;
    p->flags.sign = 1;
  } else {
    n = num;
    p->flags.sign = 0;
  }
  printf_ul_base(p, n);
}

void printf_exp(char *out, int32_t exp, unsigned min_output_chars) {
  char temp[64];
  unsigned i = 0, j = 0;
  unsigned digit;

  if (exp < 0) {
    *out++ = '-';
    exp = -exp;
  } else
    *out++ = '+';

  do {
    digit = exp % 10;
    exp = exp / 10;
    temp[i++] = '0' + digit;
  } while (exp > 0);

  while (i < min_output_chars) {
    temp[i++] = '0';
  }

  for (j = i; j > 0; --j)
    *out++ = temp[j - 1];
  *out = 0;
}

void printf_nibbles(param_t *p, uint32_t num, int32_t exp,
                    unsigned max_fract_digits, int exact, int print_dec) {
  char *out = p->bf;
  char temp[64];
  unsigned i = 0, available_digits = 0, written_digits = 0;
  unsigned digit, stop;
  const unsigned base = 16;
  char digit_offset = (p->flags.uc ? 'A' : 'a');
  unsigned trailing_zeroes = 0;
  int encountered_nonzero = 0;

  for (i = 0; i <= 6; ++i) {
    digit = num % base;
    num = num / base;
    temp[i] = ((digit < 10) ? ('0' + digit) : (digit_offset + digit - 10));

    if (digit)
      encountered_nonzero = 1;
    if (encountered_nonzero == 0)
      ++trailing_zeroes;
  }

  available_digits = i;

  *out++ = temp[--available_digits];

  if (max_fract_digits == 0)
    goto SKIP_DECIMAL_PART;

  if (exact)
    stop = trailing_zeroes;
  else
    stop = 0;

  if (print_dec || (available_digits > stop))
    *out++ = '.';

  while ((available_digits > stop) && (written_digits < max_fract_digits)) {
    char c = temp[--available_digits];
    *out++ = c;
    written_digits++;
  }

SKIP_DECIMAL_PART:
  *out++ = (p->flags.uc ? 'P' : 'p');
  printf_exp(out, exp, 0);
}
void printf_float_a(param_t *p, int print_dec, float f) {
  union {
    float ff;
    uint uu;
    int ii;
  } tmp;

  tmp.ff = f;
  int exp = (((tmp.ii & 0x7f800000) >> 23) - 127);
  uint mant = (tmp.uu & 0x007fffff);

  if (exp == (-127) && mant > 0) {
    while ((mant & 0x00780000) == 0) {
      exp -= 4;
      mant <<= 4;
    }
  }

  uint max_fract_digits = 0;
  int exact = 0;

  if (p->precision < 0) {
    max_fract_digits = (uint)(-1);
    exact = 1;
  } else
    max_fract_digits = (uint)p->precision;

  if (tmp.uu == 0)
    exp = 0;
  else {
    mant |= 0x00800000;

    if (max_fract_digits < 6) {

      uint shift = 23 - (max_fract_digits * 4);
      uint mask = (1UL << shift) - 1;
      uint half = (1UL << (shift - 1));
      uint rem = (mant & mask);
      uint shifted_mant = mant >> shift;
      if ((rem == half) && ((shifted_mant & 1) == 0))
        mant = (shifted_mant) << shift;
      else if (rem >= half)
        mant = (shifted_mant + 1) << shift;
    }
  }

  printf_nibbles(p, mant, exp, max_fract_digits, exact, print_dec);

  p->flags.alt = 1;
  p->base = 16;
  p->flags.nonzeroparam = 1;
}
void printf_float_round(float f, char *buf, int dec_point, int prec,
                        int e_mode) {

  if (prec < 0 || prec > 18)
    return;

  int round_point = prec;
  if (e_mode)
    round_point += 2;
  else
    round_point += dec_point;

  if (round_point < 0) {
    buf[0] = 0;
    return;
  }

  char *p = buf;
  while (*p)
    ++p;
  int len = p - buf;

  if (round_point >= len)
    return;

  int i = round_point;
  int direction = (buf[i] - '5');
  ++i;
  int is_half = (direction == 0);
  if (is_half && i < len) {
    while (i < len && (buf[i] == '0'))
      ++i;
    is_half = (i == len);
    direction = (i != len);
  }

  i = round_point - 1;

  if (is_half) {
    char last_dig = buf[i];
    if ((last_dig - '0') % 2)
      direction = 1;
  }
  if (direction > 0) {
    int carry = 0;
    do {
      char c = buf[i];
      carry = (c == '9');
      if (carry)
        buf[i] = '0';
      else
        buf[i] += 1;
      --i;
    } while (carry);
  }

  buf[round_point] = 0;
}

char *printf_float_round_buf(float f, char *buf, int *exp, int prec,
                             int e_mode) {
  printf_float_round(f, buf, *exp, prec, e_mode);

  if (buf[0] == '0' && buf[1]) {
    ++buf;
    *exp -= 1;
  }

  return buf;
}

void printf_float_e(param_t *p, char *buf, int point, int print_dec,
                    int notrailing0) {
  char *out = p->bf;
  char *in = buf;

  int prec = p->precision;

  int exp = point;
  int printed_dec = 0;

  --exp;

  char c;

  *out++ = *in++;

  if (print_dec || (prec > 0))
    *out++ = '.';

  if (prec > 0) {
    c = *in++;
    while (printed_dec < prec) {

      if (c) {
        *out++ = c;
        c = *in++;
      } else
        *out++ = '0';

      ++printed_dec;
    }
  }

  if (notrailing0) {
    char *tmp = out - 1;
    while (tmp > p->bf && (*tmp == '0'))
      --tmp;
    if (*tmp == '.')
      --tmp;
    out = tmp + 1;
  }

  *out++ = (p->flags.uc ? 'E' : 'e');
  printf_exp(out, exp, 2);
}

void printf_float_f(param_t *p, char *buf, int point, int print_dec,
                    int notrailing0) {
  int prec = p->precision;
  int i;
  char *out = p->bf;
  char *in = buf;
  char c;

  char *decpoint_ptr = ((void *)0);

  int written = 0;

  if (point <= 0) {
    int poi = point;
    *out++ = '0';
    if (print_dec || (prec > 0)) {
      decpoint_ptr = out;
      *out++ = '.';
    }
    while ((poi < 0) && (written < prec)) {
      *out++ = '0';
      ++poi;
      ++written;
    }
    c = *in++;
  }

  else {
    c = *in++;
    for (i = 0; i < point; ++i) {
      if (c) {
        *out++ = c;
        c = *in++;
      } else
        *out++ = '0';
    }
    if (print_dec || (prec > 0)) {
      decpoint_ptr = out;
      *out++ = '.';
    }
  }

  while (written < prec) {
    if (c) {
      *out++ = c;
      c = *in++;
    } else
      *out++ = '0';

    ++written;
  }

  *out = 0;

  if (notrailing0 && (decpoint_ptr != ((void *)0))) {
    out -= 1;
    unsigned i = 0;
    while (out > decpoint_ptr && *out == '0') {
      *out-- = 0;
    }
    if (*out == '.')
      *out = 0;
  }
}

void printf_putchw(param_t *p) {
  char ch;
  int n = p->width;

  int althex = (p->flags.nonzeroparam && p->flags.alt && p->base == 16);

  int altoct = (p->bf[0] != '0' && p->flags.alt && p->base == 8);
  int sign_required = (p->flags.always_sign || p->flags.sign);
  int space_required = (p->flags.space && p->flags.sign == 0);
  char *bf = p->bf;

  while (*bf++ && n > 0)
    n--;
  if (sign_required)
    n--;
  if (space_required)
    n--;
  if (althex)
    n -= 2;
  if (altoct)
    n--;

  if (!p->flags.zero && !p->flags.align_left) {
    while (n-- > 0)
      printf_putcf(p, ' ');
  }

  if (space_required)
    printf_putcf(p, ' ');

  if (sign_required) {
    printf_putcf(p, (p->flags.sign ? '-' : '+'));
  }

  if (althex) {
    printf_putcf(p, '0');
    printf_putcf(p, (p->flags.uc ? 'X' : 'x'));
  } else if (altoct) {
    printf_putcf(p, '0');
  }

  if (p->flags.zero) {
    while (n-- > 0)
      printf_putcf(p, '0');
  }

  printf_puts(p, p->bf);

  if (!p->flags.zero && p->flags.align_left) {
    while (n-- > 0)
      printf_putcf(p, ' ');
  }
}

void printf_puts_ljust(param_t *p, const char *string, int width,
                       ssize_t max_width) {
  char c;
  int written = 0;
  if (max_width < 0)
    max_width = (4294967295U >> 1);
  while ((c = *string++)) {
    if (written < max_width)
      printf_putcf(p, c);
    ++written;
  }
  while (written < width) {
    if (written < max_width)
      printf_putcf(p, ' ');
    ++written;
  }
}

void printf_puts_rjust(param_t *p, const char *string, int width,
                       ssize_t max_width) {
  char c;
  int i, strleng = 0, written = 0;
  if (max_width < 0)
    max_width = (4294967295U >> 1);

  const char *tmp = string;
  while ((c = *tmp++))
    ++strleng;

  for (i = strleng; i < width; ++i) {
    if (written < max_width)
      printf_putcf(p, ' ');
    ++written;
  }

  while ((c = *string++)) {
    if (written < max_width)
      printf_putcf(p, c);
    ++written;
  }
}

void printf_ptr(param_t *p, const void *ptr) {
  p->base = 16;
  p->flags.uc = 0;
  p->flags.alt = 1;
  p->flags.sign = 0;
  p->flags.nonzeroparam = 1;
  printf_ul16(p, (uintptr_t)ptr);
  printf_putchw(p);
}

void printf_nonfinite(param_t *p, const char *ptr) {
  char c;
  char *dest = p->bf;
  while ((c = *ptr++))
    *dest++ = c;
  *dest = 0;

  p->flags.zero = 0;

  printf_putchw(p);
}

void printf_ulong(param_t *p, uint32_t u) {
  if (p->base == 16) {
    p->flags.nonzeroparam = (u > 0 ? 1 : 0);
    printf_ul16(p, u);
  } else
    printf_ul_base(p, u);

  printf_putchw(p);
}

void printf_long(param_t *p, int32_t i) {
  printf_l_base(p, i);
  printf_putchw(p);
}

void printf_float(param_t *p, float f) {
  if (p->conv == '\0')
    p->conv = 'f';
  int print_dec = 0;
  if (p->precision == 0) {
    if (p->flags.alt) {

      print_dec = 1;
    }
  }

  p->flags.sign = __builtin_signbitf(f) ? 1 : 0;

  if (p->flags.sign)
    f = -f;

  if (p->conv == 'a') {
    printf_float_a(p, print_dec, f);
    printf_putchw(p);
    return;
  }

  char float_digits[1200];
  char *rounded_digits = float_digits;
  int P, X, notrail0;
  int round_exp = 0, dec_point = 0;

  int saved_prec = p->precision;
  if (p->precision < 0)
    p->precision = 6;

  int nonzero = (f != 0.0f);
  float_digits[0] = '0';
  float_digits[1] = 0;
  if (nonzero)
    dec_point = generate_float_digits(f, float_digits + 1);
  round_exp = dec_point + 1;

  switch (p->conv) {
  case 'e':
    if (nonzero)
      rounded_digits =
          printf_float_round_buf(f, float_digits, &round_exp, p->precision, 1);
    printf_float_e(p, rounded_digits, round_exp, print_dec, 0);
    break;
  case 'f':
    if (nonzero)
      rounded_digits =
          printf_float_round_buf(f, float_digits, &round_exp, p->precision, 0);
    printf_float_f(p, rounded_digits, round_exp, print_dec, 0);
    break;
  case 'g':

    notrail0 = (p->flags.alt ? 0 : 1);

    P = p->precision;
    if (P == 0)
      P = 1;

    X = dec_point - 1;
    if ((P > X) && (X >= -4)) {
      p->precision = P - (X + 1);
      if (nonzero)
        rounded_digits = printf_float_round_buf(f, float_digits, &round_exp,
                                                p->precision, 0);
      printf_float_f(p, rounded_digits, round_exp, print_dec, notrail0);
    } else {
      p->precision = P - 1;
      if (nonzero)
        rounded_digits = printf_float_round_buf(f, float_digits, &round_exp,
                                                p->precision, 1);
      printf_float_e(p, rounded_digits, round_exp, print_dec, notrail0);
    }
    break;
  }

  p->precision = saved_prec;
  printf_putchw(p);
}
