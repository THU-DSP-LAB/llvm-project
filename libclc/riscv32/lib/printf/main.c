
#include "printf.h"
int buffer_start_index = 0;
char *_printf_buffer = (char *)0xa0024000; // TODO: Value from kernel_arg
unsigned char *_printf_buffer_position = &buffer_start_index;
unsigned _printf_buffer_capacity = 1 << 10; // TODO: Value from kernel_arg

#pragma OPENCL EXTENSION __cl_clang_variadic_functions : enable

/// @param format format string locating in global space
int printf(char *format, ...) {

  struct param_t p;
  p.printf_buffer = _printf_buffer; // Thread print buffer location
  p.printf_buffer_capacity =
      _printf_buffer_capacity; // Thread print buffer size
  p.printf_buffer_index = *(uint32_t *)_printf_buffer_position; // Buffer index

  __builtin_va_list ap;
  __builtin_va_start(ap, format);
  int r = printf_format_full(format, &p, ap);

  __builtin_va_end(ap);
  *(uint32_t *)_printf_buffer_position = p.printf_buffer_index;
  return r;
}

// Driver example
// char test[1024] = "";
// int main() {
//   uint2 ui = {123, 5674};
//   float f = 123.213;
//   uchar4 uc = {0xFA, 0xFB, 0xFC, 0xFD};
//   char *p = "this is a test string\n";
//   // _printf("%s\n", p[3]);
//   _printf("%s %f\n", "fefefef", f);
//   _printf("uc = %#v4hhx\n", uc);
//   _printf("INTEGERS\n\n");

//   _printf("1\n");
//   _printf("%d\n", 2);
//   _printf("%0d\n", 3);
//   _printf("%.0d\n", 4);
//   _printf("%0.0d\n", 5);
//   _printf("%10d\n", 6);
//   _printf("%.10d\n", 7);
//   _printf("%10.10d\n", 8);
//   _printf("%5.10d\n", 9);
//   _printf("%9.4d\n", 10);
//   _printf("%-06i\n", 10);
//   _printf("unsigned short value = (%#v2hx)\n", ui);
//   // _printf("%s\n", "this is a test  string\n");
//   printf("ff%s", test);
//   return 0;
// }
