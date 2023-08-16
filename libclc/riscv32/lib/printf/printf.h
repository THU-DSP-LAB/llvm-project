#ifndef PRINTF_H
#define PRINTF_H

#include "types.h"
typedef char char2  __attribute__((__ext_vector_type__(2)));
typedef char char3  __attribute__((__ext_vector_type__(3)));
typedef char char4  __attribute__((__ext_vector_type__(4)));
typedef char char8  __attribute__((__ext_vector_type__(8)));
typedef char char16 __attribute__((__ext_vector_type__(16)));

typedef uchar uchar2  __attribute__((__ext_vector_type__(2)));
typedef uchar uchar3  __attribute__((__ext_vector_type__(3)));
typedef uchar uchar4  __attribute__((__ext_vector_type__(4)));
typedef uchar uchar8  __attribute__((__ext_vector_type__(8)));
typedef uchar uchar16 __attribute__((__ext_vector_type__(16)));

typedef short short2  __attribute__((__ext_vector_type__(2)));
typedef short short3  __attribute__((__ext_vector_type__(3)));
typedef short short4  __attribute__((__ext_vector_type__(4)));
typedef short short8  __attribute__((__ext_vector_type__(8)));
typedef short short16 __attribute__((__ext_vector_type__(16)));

typedef ushort ushort2  __attribute__((__ext_vector_type__(2)));
typedef ushort ushort3  __attribute__((__ext_vector_type__(3)));
typedef ushort ushort4  __attribute__((__ext_vector_type__(4)));
typedef ushort ushort8  __attribute__((__ext_vector_type__(8)));
typedef ushort ushort16 __attribute__((__ext_vector_type__(16)));

typedef int int2  __attribute__((__ext_vector_type__(2)));
typedef int int3  __attribute__((__ext_vector_type__(3)));
typedef int int4  __attribute__((__ext_vector_type__(4)));
typedef int int8  __attribute__((__ext_vector_type__(8)));
typedef int int16 __attribute__((__ext_vector_type__(16)));

typedef uint uint2  __attribute__((__ext_vector_type__(2)));
typedef uint uint3  __attribute__((__ext_vector_type__(3)));
typedef uint uint4  __attribute__((__ext_vector_type__(4)));
typedef uint uint8  __attribute__((__ext_vector_type__(8)));
typedef uint uint16 __attribute__((__ext_vector_type__(16)));

typedef float float2  __attribute__((__ext_vector_type__(2)));
typedef float float3  __attribute__((__ext_vector_type__(3)));
typedef float float4  __attribute__((__ext_vector_type__(4)));
typedef float float8  __attribute__((__ext_vector_type__(8)));
typedef float float16 __attribute__((__ext_vector_type__(16)));

typedef intptr_t sint;

typedef struct {
  char zero;
  char alt;
  char align_left;
  char uc;
  char always_sign;
  char sign;
  char space;

  char nonzeroparam;
} flags_t;

typedef struct param_t {
  char *bf;
  char *printf_buffer;
  uint printf_buffer_index;
  uint printf_buffer_capacity;
  int precision;
  unsigned width;
  unsigned base;
  flags_t flags;
  char conv;
} param_t;
// __attribute__((address_space(5)))
void printf_putchw(param_t *p);

void printf_putcf(param_t *p, char c);

void printf_puts(param_t *p, const char *string);

void printf_nonfinite(param_t *p, const char *ptr);

void printf_puts_ljust(param_t *p, const char *string, int width, sint max_width);

void printf_puts_rjust(param_t *p, const char *string, int width, sint max_width);

void printf_ptr(param_t *p, const void *ptr);

void printf_ulong(param_t *p, uint32_t u);

void printf_long(param_t *p, int32_t i);

void printf_float(param_t *p, float f);

typedef __builtin_va_list va_list;

typedef __builtin_va_list __gnuc_va_list;

void print_ints_uchar(param_t *p, const void *vals , int n, int is_unsigned);
void print_ints_ushort(param_t *p, const void *vals, int n, int is_unsigned);
void print_ints_uint(param_t *p, const void *vals, int n, int is_unsigned);
void print_floats_float(param_t *p, const void *vals, int n);
int printf_format_full(const char *format, param_t *p, va_list ap);

#endif
