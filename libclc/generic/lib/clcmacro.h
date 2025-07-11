#define _CLC_UNARY_VECTORIZE(DECLSPEC, RET_TYPE, FUNCTION, ARG1_TYPE) \
  DECLSPEC RET_TYPE##2 FUNCTION(ARG1_TYPE##2 x) { \
    return (RET_TYPE##2)(FUNCTION(x.x), FUNCTION(x.y)); \
  } \
\
  DECLSPEC RET_TYPE##3 FUNCTION(ARG1_TYPE##3 x) { \
    return (RET_TYPE##3)(FUNCTION(x.x), FUNCTION(x.y), FUNCTION(x.z)); \
  } \
\
  DECLSPEC RET_TYPE##4 FUNCTION(ARG1_TYPE##4 x) { \
    return (RET_TYPE##4)(FUNCTION(x.lo), FUNCTION(x.hi)); \
  } \
\
  DECLSPEC RET_TYPE##8 FUNCTION(ARG1_TYPE##8 x) { \
    return (RET_TYPE##8)(FUNCTION(x.lo), FUNCTION(x.hi)); \
  } \
\
  DECLSPEC RET_TYPE##16 FUNCTION(ARG1_TYPE##16 x) { \
    return (RET_TYPE##16)(FUNCTION(x.lo), FUNCTION(x.hi)); \
  }

#define _CLC_BINARY_VECTORIZE(DECLSPEC, RET_TYPE, FUNCTION, ARG1_TYPE, ARG2_TYPE) \
  DECLSPEC RET_TYPE##2 FUNCTION(ARG1_TYPE##2 x, ARG2_TYPE##2 y) { \
    return (RET_TYPE##2)(FUNCTION(x.x, y.x), FUNCTION(x.y, y.y)); \
  } \
\
  DECLSPEC RET_TYPE##3 FUNCTION(ARG1_TYPE##3 x, ARG2_TYPE##3 y) { \
    return (RET_TYPE##3)(FUNCTION(x.x, y.x), FUNCTION(x.y, y.y), \
                         FUNCTION(x.z, y.z)); \
  } \
\
  DECLSPEC RET_TYPE##4 FUNCTION(ARG1_TYPE##4 x, ARG2_TYPE##4 y) { \
    return (RET_TYPE##4)(FUNCTION(x.lo, y.lo), FUNCTION(x.hi, y.hi)); \
  } \
\
  DECLSPEC RET_TYPE##8 FUNCTION(ARG1_TYPE##8 x, ARG2_TYPE##8 y) { \
    return (RET_TYPE##8)(FUNCTION(x.lo, y.lo), FUNCTION(x.hi, y.hi)); \
  } \
\
  DECLSPEC RET_TYPE##16 FUNCTION(ARG1_TYPE##16 x, ARG2_TYPE##16 y) { \
    return (RET_TYPE##16)(FUNCTION(x.lo, y.lo), FUNCTION(x.hi, y.hi)); \
  }

#define _CLC_V_S_V_VECTORIZE(DECLSPEC, RET_TYPE, FUNCTION, ARG1_TYPE, ARG2_TYPE) \
  DECLSPEC RET_TYPE##2 FUNCTION(ARG1_TYPE x, ARG2_TYPE##2 y) { \
    return (RET_TYPE##2)(FUNCTION(x, y.lo), FUNCTION(x, y.hi)); \
  } \
\
  DECLSPEC RET_TYPE##3 FUNCTION(ARG1_TYPE x, ARG2_TYPE##3 y) { \
    return (RET_TYPE##3)(FUNCTION(x, y.x), FUNCTION(x, y.y), \
                         FUNCTION(x, y.z)); \
  } \
\
  DECLSPEC RET_TYPE##4 FUNCTION(ARG1_TYPE x, ARG2_TYPE##4 y) { \
    return (RET_TYPE##4)(FUNCTION(x, y.lo), FUNCTION(x, y.hi)); \
  } \
\
  DECLSPEC RET_TYPE##8 FUNCTION(ARG1_TYPE x, ARG2_TYPE##8 y) { \
    return (RET_TYPE##8)(FUNCTION(x, y.lo), FUNCTION(x, y.hi)); \
  } \
\
  DECLSPEC RET_TYPE##16 FUNCTION(ARG1_TYPE x, ARG2_TYPE##16 y) { \
    return (RET_TYPE##16)(FUNCTION(x, y.lo), FUNCTION(x, y.hi)); \
  } \
\

#define _CLC_TERNARY_VECTORIZE(DECLSPEC, RET_TYPE, FUNCTION, ARG1_TYPE, ARG2_TYPE, ARG3_TYPE) \
  DECLSPEC RET_TYPE##2 FUNCTION(ARG1_TYPE##2 x, ARG2_TYPE##2 y, ARG3_TYPE##2 z) { \
    return (RET_TYPE##2)(FUNCTION(x.x, y.x, z.x), FUNCTION(x.y, y.y, z.y)); \
  } \
\
  DECLSPEC RET_TYPE##3 FUNCTION(ARG1_TYPE##3 x, ARG2_TYPE##3 y, ARG3_TYPE##3 z) { \
    return (RET_TYPE##3)(FUNCTION(x.x, y.x, z.x), FUNCTION(x.y, y.y, z.y), \
                         FUNCTION(x.z, y.z, z.z)); \
  } \
\
  DECLSPEC RET_TYPE##4 FUNCTION(ARG1_TYPE##4 x, ARG2_TYPE##4 y, ARG3_TYPE##4 z) { \
    return (RET_TYPE##4)(FUNCTION(x.lo, y.lo, z.lo), FUNCTION(x.hi, y.hi, z.hi)); \
  } \
\
  DECLSPEC RET_TYPE##8 FUNCTION(ARG1_TYPE##8 x, ARG2_TYPE##8 y, ARG3_TYPE##8 z) { \
    return (RET_TYPE##8)(FUNCTION(x.lo, y.lo, z.lo), FUNCTION(x.hi, y.hi, z.hi)); \
  } \
\
  DECLSPEC RET_TYPE##16 FUNCTION(ARG1_TYPE##16 x, ARG2_TYPE##16 y, ARG3_TYPE##16 z) { \
    return (RET_TYPE##16)(FUNCTION(x.lo, y.lo, z.lo), FUNCTION(x.hi, y.hi, z.hi)); \
  }

#define _CLC_V_S_S_V_VECTORIZE(DECLSPEC, RET_TYPE, FUNCTION, ARG1_TYPE, ARG2_TYPE, ARG3_TYPE) \
  DECLSPEC RET_TYPE##2 FUNCTION(ARG1_TYPE x, ARG2_TYPE y, ARG3_TYPE##2 z) { \
    return (RET_TYPE##2)(FUNCTION(x, y, z.lo), FUNCTION(x, y, z.hi)); \
  } \
\
  DECLSPEC RET_TYPE##3 FUNCTION(ARG1_TYPE x, ARG2_TYPE y, ARG3_TYPE##3 z) { \
    return (RET_TYPE##3)(FUNCTION(x, y, z.x), FUNCTION(x, y, z.y), \
                         FUNCTION(x, y, z.z)); \
  } \
\
  DECLSPEC RET_TYPE##4 FUNCTION(ARG1_TYPE x, ARG2_TYPE y, ARG3_TYPE##4 z) { \
    return (RET_TYPE##4)(FUNCTION(x, y, z.lo), FUNCTION(x, y, z.hi)); \
  } \
\
  DECLSPEC RET_TYPE##8 FUNCTION(ARG1_TYPE x, ARG2_TYPE y, ARG3_TYPE##8 z) { \
    return (RET_TYPE##8)(FUNCTION(x, y, z.lo), FUNCTION(x, y, z.hi)); \
  } \
\
  DECLSPEC RET_TYPE##16 FUNCTION(ARG1_TYPE x, ARG2_TYPE y, ARG3_TYPE##16 z) { \
    return (RET_TYPE##16)(FUNCTION(x, y, z.lo), FUNCTION(x, y, z.hi)); \
  } \
\

#define _CLC_V_V_VP_VECTORIZE(DECLSPEC, RET_TYPE, FUNCTION, ARG1_TYPE, ADDR_SPACE, ARG2_TYPE) \
  DECLSPEC RET_TYPE##2 FUNCTION(ARG1_TYPE##2 x, ADDR_SPACE ARG2_TYPE##2 *y) { \
    ADDR_SPACE ARG2_TYPE *ptr = (ADDR_SPACE ARG2_TYPE *)y; \
    return (RET_TYPE##2)( \
        FUNCTION(x.x, ptr), \
        FUNCTION(x.y, ptr + 1) \
    ); \
  } \
\
  DECLSPEC RET_TYPE##3 FUNCTION(ARG1_TYPE##3 x, ADDR_SPACE ARG2_TYPE##3 *y) { \
    ADDR_SPACE ARG2_TYPE *ptr = (ADDR_SPACE ARG2_TYPE *)y; \
    return (RET_TYPE##3)( \
        FUNCTION(x.x, ptr), \
        FUNCTION(x.y, ptr + 1), \
        FUNCTION(x.z, ptr + 2) \
    ); \
  } \
\
  DECLSPEC RET_TYPE##4 FUNCTION(ARG1_TYPE##4 x, ADDR_SPACE ARG2_TYPE##4 *y) { \
    ADDR_SPACE ARG2_TYPE *ptr = (ADDR_SPACE ARG2_TYPE *)y; \
    return (RET_TYPE##4)( \
        FUNCTION(x.lo.x, ptr), FUNCTION(x.lo.y, ptr + 1), \
        FUNCTION(x.hi.x, ptr + 2), FUNCTION(x.hi.y, ptr + 3) \
    ); \
  } \
\
  DECLSPEC RET_TYPE##8 FUNCTION(ARG1_TYPE##8 x, ADDR_SPACE ARG2_TYPE##8 *y) { \
    ADDR_SPACE ARG2_TYPE *ptr = (ADDR_SPACE ARG2_TYPE *)y; \
    return (RET_TYPE##8)( \
        FUNCTION(x.lo.lo.x, ptr), FUNCTION(x.lo.lo.y, ptr + 1), \
        FUNCTION(x.lo.hi.x, ptr + 2), FUNCTION(x.lo.hi.y, ptr + 3), \
        FUNCTION(x.hi.lo.x, ptr + 4), FUNCTION(x.hi.lo.y, ptr + 5), \
        FUNCTION(x.hi.hi.x, ptr + 6), FUNCTION(x.hi.hi.y, ptr + 7) \
    ); \
  } \
\
  DECLSPEC RET_TYPE##16 FUNCTION(ARG1_TYPE##16 x, ADDR_SPACE ARG2_TYPE##16 *y) { \
    ADDR_SPACE ARG2_TYPE *ptr = (ADDR_SPACE ARG2_TYPE *)y; \
    return (RET_TYPE##16)( \
        FUNCTION(x.lo.lo.lo.x, ptr), FUNCTION(x.lo.lo.lo.y, ptr + 1), \
        FUNCTION(x.lo.lo.hi.x, ptr + 2), FUNCTION(x.lo.lo.hi.y, ptr + 3), \
        FUNCTION(x.lo.hi.lo.x, ptr + 4), FUNCTION(x.lo.hi.lo.y, ptr + 5), \
        FUNCTION(x.lo.hi.hi.x, ptr + 6), FUNCTION(x.lo.hi.hi.y, ptr + 7), \
        FUNCTION(x.hi.lo.lo.x, ptr + 8), FUNCTION(x.hi.lo.lo.y, ptr + 9), \
        FUNCTION(x.hi.lo.hi.x, ptr + 10), FUNCTION(x.hi.lo.hi.y, ptr + 11), \
        FUNCTION(x.hi.hi.lo.x, ptr + 12), FUNCTION(x.hi.hi.lo.y, ptr + 13), \
        FUNCTION(x.hi.hi.hi.x, ptr + 14), FUNCTION(x.hi.hi.hi.y, ptr + 15) \
    ); \
  }

#define _CLC_DEFINE_BINARY_BUILTIN(RET_TYPE, FUNCTION, BUILTIN, ARG1_TYPE, ARG2_TYPE) \
_CLC_DEF _CLC_OVERLOAD RET_TYPE FUNCTION(ARG1_TYPE x, ARG2_TYPE y) { \
  return BUILTIN(x, y); \
} \
_CLC_BINARY_VECTORIZE(_CLC_OVERLOAD _CLC_DEF, RET_TYPE, FUNCTION, ARG1_TYPE, ARG2_TYPE)

#define _CLC_DEFINE_BINARY_BUILTIN_WITH_SCALAR_SECOND_ARG(RET_TYPE, FUNCTION, BUILTIN, ARG1_TYPE, ARG2_TYPE) \
_CLC_DEFINE_BINARY_BUILTIN(RET_TYPE, FUNCTION, BUILTIN, ARG1_TYPE, ARG2_TYPE) \
_CLC_BINARY_VECTORIZE_SCALAR_SECOND_ARG(_CLC_OVERLOAD _CLC_DEF, RET_TYPE, FUNCTION, ARG1_TYPE, ARG2_TYPE)

#define _CLC_DEFINE_UNARY_BUILTIN(RET_TYPE, FUNCTION, BUILTIN, ARG1_TYPE) \
_CLC_DEF _CLC_OVERLOAD RET_TYPE FUNCTION(ARG1_TYPE x) { \
  return BUILTIN(x); \
} \
_CLC_UNARY_VECTORIZE(_CLC_OVERLOAD _CLC_DEF, RET_TYPE, FUNCTION, ARG1_TYPE)
