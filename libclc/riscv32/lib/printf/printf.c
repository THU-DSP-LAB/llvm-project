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

#include "printf.h"

void print_ints_uchar(param_t *p, const void *vals, int n, int is_unsigned) {
  ((void)0);
  flags_t saved_user_flags = p->flags;
  for (int d = 0; d < n; ++d) {
    ((void)0);
    p->flags = saved_user_flags;
    if (d != 0)
      printf_putcf(p, ',');
    if (is_unsigned)
      printf_ulong(p, (uint32_t)(((const uint8_t *)vals)[d]));
    else
      printf_long(p, (int32_t)(((const int8_t *)vals)[d]));
  }
  ((void)0);
}

void print_ints_ushort(param_t *p, const void *vals, int n, int is_unsigned) {
  ((void)0);
  flags_t saved_user_flags = p->flags;
  for (int d = 0; d < n; ++d) {
    ((void)0);
    p->flags = saved_user_flags;
    if (d != 0)
      printf_putcf(p, ',');
    if (is_unsigned)
      printf_ulong(p, (uint32_t)(((const uint16_t *)vals)[d]));
    else
      printf_long(p, (int32_t)(((const int16_t *)vals)[d]));
  }
  ((void)0);
}

void print_ints_uint(param_t *p, const void *vals, int n, int is_unsigned) {
  ((void)0);
  flags_t saved_user_flags = p->flags;
  for (int d = 0; d < n; ++d) {
    ((void)0);
    p->flags = saved_user_flags;
    if (d != 0)
      printf_putcf(p, ',');
    if (is_unsigned)
      printf_ulong(p, (uint32_t)(((const uint32_t *)vals)[d]));
    else
      printf_long(p, (int32_t)(((const int32_t *)vals)[d]));
  }
  ((void)0);
}

void print_floats_float(param_t *p, const void *vals, int n) {
  char *NANs[2] = {"nan", "NAN"};
  char *INFs[2] = {"inf", "INF"};
  ((void)0);
  flags_t saved_user_flags = p->flags;
  for (int d = 0; d < n; ++d) {
    ((void)0);
    p->flags = saved_user_flags;
    if (d != 0)
      printf_putcf(p, ',');
    float val = *((const float *)vals + d);
    const char *other = ((char *)0);
    if (val != val)
      other = NANs[p->flags.uc ? 1 : 0];
    if (val == (-(__builtin_inff()))) {
      val = (__builtin_inff());
      p->flags.sign = 1;
    }
    if (val == ((__builtin_inff())))
      other = INFs[p->flags.uc ? 1 : 0];
    if (other)
      printf_nonfinite(p, other);
    else
      printf_float(p, val);
  }
  ((void)0);
}

int printf_format_full(const char *format, param_t *p, va_list ap) {
  char ch;
  ((void)0);
  char bf[1200];
  p->bf = bf;

  unsigned errcode;

  while ((ch = *format++)) {
    if (ch == '%') {

      ch = *format++;
      if (ch == 0) {
        errcode = 0x11;
        goto error;
      }

      if (ch == '%') {
        ((void)0);
        printf_putcf(p, '%');
      } else {
        flags_t flags;
        flags.align_left = 0;
        flags.sign = 0;
        flags.space = 0;
        flags.alt = 0;
        flags.zero = 0;
        flags.uc = 0;
        flags.always_sign = 0;
        while (ch != '\0') {
          switch (ch) {
          case '-':
            if (flags.align_left) {
              errcode = 0x12;
              goto error;
            }
            flags.align_left = 1;
            break;
          case '+':
            if (flags.always_sign) {
              errcode = 0x13;
              goto error;
            }
            flags.always_sign = 1;
            break;
          case ' ':
            if (flags.space) {
              errcode = 0x14;
              goto error;
            }
            flags.space = 1;
            break;
          case '#':
            if (flags.alt) {
              errcode = 0x15;
              goto error;
            }
            flags.alt = 1;
            break;
          case '0':
            if (flags.zero) {
              errcode = 0x16;
              goto error;
            }
            if (flags.align_left == 0)
              flags.zero = 1;
            break;
          default:
            goto flags_done;
          }
          ch = *format++;
        }

      flags_done:;
        ((void)0);

        int field_width = 0;
        while (ch >= '0' && ch <= '9') {
          if (ch == '0' && field_width == 0) {
            errcode = 0x17;
            goto error;
          }
          if (field_width > (2147483647 - 9) / 10) {
            errcode = 0x18;
            goto error;
          }
          field_width = 10 * field_width + (ch - '0');
          ch = *format++;
        }
        ((void)0);

        int precision = -1;
        if (ch == '.') {
          precision = 0;
          ch = *format++;
          while (ch >= '0' && ch <= '9') {
            if (precision > (2147483647 - 9) / 10) {
              errcode = 0x19;
              goto error;
            }
            precision = 10 * precision + (ch - '0');
            ch = *format++;
          }
        }
        ((void)0);

        int vector_length = 0;
        if (ch == 'v') {
          ch = *format++;
          while (ch >= '0' && ch <= '9') {
            if (ch == '0' && vector_length == 0) {
              errcode = 0x20;
              goto error;
            }
            if (vector_length > (2147483647 - 9) / 10) {
              errcode = 0x21;
              goto error;
            }
            vector_length = 10 * vector_length + (ch - '0');
            ch = *format++;
          }
          if (!(vector_length == 2 || vector_length == 3 ||
                vector_length == 4 || vector_length == 8 ||
                vector_length == 16)) {
            errcode = 0x22;
            goto error;
          }
        }
        ((void)0);

        int length = 0;
        if (ch == 'h') {
          ch = *format++;
          if (ch == 'h') {
            ch = *format++;
            length = 1;
          } else if (ch == 'l') {
            ch = *format++;
            length = 4;
          } else {
            length = 2;
          }
        } else if (ch == 'l') {
          ch = *format++;
          length = 8;
        }
        if (vector_length > 0 && length == 0) {
          errcode = 0x23;
          goto error;
        }

        if (vector_length == 0 && length == 4) {
          errcode = 0x24;
          goto error;
        }

        if (vector_length == 0)
          vector_length = 1;

        ((void)0);

        p->flags = flags;
        p->conv = ch;
        p->width = field_width;
        p->precision = precision;

        if (ch == 'd' || ch == 'i' || ch == 'o' || ch == 'u' || ch == 'x' ||
            ch == 'X') {
          unsigned base = 10;
          if (ch == 'x' || ch == 'X')
            base = 16;
          if (ch == 'o')
            base = 8;
          if (ch == 'X')
            p->flags.uc = 1;
          int is_unsigned = (ch == 'u') || (base != 10);

          if (p->precision > 0)
            p->flags.zero = 0;
          if (precision < 0)
            precision = p->precision = 1;
          p->base = base;
          // switch (length) {
          if (length == 1) {
            uchar16 val;
            switch (vector_length) {
            default:
              __builtin_unreachable();
            case 1:
              val.s0 = __builtin_va_arg(ap, uint);
              break;
            case 2:
              val.s01 = __builtin_va_arg(ap, uchar2);
              break;
            case 3:
            case 4:
              val.s0123 = __builtin_va_arg(ap, uchar4);
              break;
            case 8:
              val.lo = __builtin_va_arg(ap, uchar8);
              break;
            case 16:
              val = __builtin_va_arg(ap, uchar16);
              break;
            }
            print_ints_uchar(p, &val, vector_length, is_unsigned);
          }
          else if (length == 2) {
            ushort16 val;
            switch (vector_length) {
            default:
              __builtin_unreachable();
            case 1:
              val.s0 = __builtin_va_arg(ap, uint);
              break;
            case 2:
              val.s01 = __builtin_va_arg(ap, ushort2);
              break;
            case 3:
            case 4:
              val.s0123 = __builtin_va_arg(ap, ushort4);
              break;
            case 8:
              val.lo = __builtin_va_arg(ap, ushort8);
              break;
            case 16:
              val = __builtin_va_arg(ap, ushort16);
              break;
            }
            print_ints_ushort(p, &val, vector_length, is_unsigned);
          }
          else if (length == 0 || length == 4) {
            uint16 val;
            switch (vector_length) {
            default:
              __builtin_unreachable();
            case 1:
              val.s0 = __builtin_va_arg(ap, uint);
              break;
            case 2:
              val.s01 = __builtin_va_arg(ap, uint2);
              break;
            case 3:
            case 4:
              val.s0123 = __builtin_va_arg(ap, uint4);
              break;
            case 8:
              val.lo = __builtin_va_arg(ap, uint8);
              break;
            case 16:
              val = __builtin_va_arg(ap, uint16);
              break;
            }
            print_ints_uint(p, &val, vector_length, is_unsigned);
          }

        }
        else if (ch == 'f' || ch == 'F' || ch == 'e' || ch == 'E' ||
                   ch == 'g' || ch == 'G' || ch == 'a' || ch == 'A') {
          p->base = 10;
          if (ch < 'X') {
            p->flags.uc = 1;
            p->conv += 32;
          }

          if (length == 0 || length == 4) {
            float16 val;
            switch (vector_length) {
            default:
              __builtin_unreachable();
            case 1:
              val.s0 = __builtin_va_arg(ap, double);
              break;
            case 2:
              val.s01 = __builtin_va_arg(ap, float2);
              break;
            case 3:
            case 4:
              val.s0123 = __builtin_va_arg(ap, float4);
              break;
            case 8:
              val.lo = __builtin_va_arg(ap, float8);
              break;
            case 16:
              val = __builtin_va_arg(ap, float16);
              break;
            }
            print_floats_float(p, &val, vector_length);
            // }; break;
          }
        }

        else if (ch == 'c') {
          ((void)0);
          if (flags.always_sign || flags.space || flags.alt || flags.zero) {
            errcode = 0x25;
            goto error;
          }
          ((void)0);
          if (precision >= 0) {
            errcode = 0x25;
            goto error;
          }
          ((void)0);
          if (vector_length != 1) {
            errcode = 0x25;
            goto error;
          }
          ((void)0);
          if (length != 0) {
            errcode = 0x25;
            goto error;
          }
          ((void)0);

          int i = __builtin_va_arg(ap, int);
          bf[0] = (char)i;
          bf[1] = 0;
          printf_putchw(p);
          break;
        }

        else if (ch == 's') {
          if (flags.always_sign || flags.space || flags.alt || flags.zero) {
            errcode = 0x26;
            goto error;
          }
          if (vector_length != 1) {
            errcode = 0x27;
            goto error;
          }
          if (length != 0) {
            errcode = 0x28;
            goto error;
          }

          const char *val = __builtin_va_arg(ap, const char *);
          if (val == 0)
            printf_puts_ljust(p, "(null)", field_width, precision);
          else if (flags.align_left)
            printf_puts_ljust(p, val, field_width, precision);
          else
            printf_puts_rjust(p, val, field_width, precision);
          break;
        }

        else if (ch == 'p') {
          if (flags.always_sign || flags.space || flags.alt || flags.zero) {
            errcode = 0x29;
            goto error;
          }
          if (precision >= 0) {
            errcode = 0x30;
            goto error;
          }
          if (vector_length != 1) {
            errcode = 0x31;
            goto error;
          }
          if (length != 0) {
            errcode = 0x32;
            goto error;
          }

          const void *val = __builtin_va_arg(ap, const void *);
          printf_ptr(p, val);
          break;
        }

        else {
          errcode = 0x33;
          goto error;
        }
      }
    } else {
      printf_putcf(p, ch);
    }
  }
  return 0;


error:;
  const char *err_str = " printf format string error: 0x";
  printf_puts(p, err_str);
  char c1 = '0' + (char)(errcode >> 4);
  char c2 = '0' + (char)(errcode & 7);
  printf_putcf(p, c1);
  printf_putcf(p, c2);
  printf_putcf(p, '\n');
  return -1;
  // FIXME: else statement will cause build error, why????
}
