
typedef struct {
  float value;
  unsigned word;
} ieee_float_shape_type;

// 
float __nextafterf(float x, float y) {
  int hx, hy, ix, iy;

  do {
    ieee_float_shape_type gf_u;
    gf_u.value = (x);
    (hx) = gf_u.word;
  } while (0);
  do {
    ieee_float_shape_type gf_u;
    gf_u.value = (y);
    (hy) = gf_u.word;
  } while (0);
  ix = hx & 0x7fffffff;
  iy = hy & 0x7fffffff;

  if ((ix > 0x7f800000) || (iy > 0x7f800000))
    return x + y;
  if (x == y)
    return y;
  if (ix == 0) {
    float u;
    do {
      ieee_float_shape_type sf_u;
      sf_u.word = ((hy & 0x80000000) | 1);
      (x) = sf_u.value;
    } while (0);
    u = ({
      __typeof(x) __x = (x);
      __asm("" : "+m"(__x));
      __x;
    });
    u = u * u;
    ({
      __typeof(u) __x = (u);
      __asm __volatile__("" : : "m"(__x));
    });
    return x;
  }
  if (hx >= 0) {
    if (hx > hy) {
      hx -= 1;
    } else {
      hx += 1;
    }
  } else {
    if (hy >= 0 || hx > hy) {
      hx -= 1;
    } else {
      hx += 1;
    }
  }
  hy = hx & 0x7f800000;
  if (hy >= 0x7f800000) {
    float u = x + x;
    ({
      __typeof(u) __x = (u);
      __asm __volatile__("" : : "m"(__x));
    });
    //  ((*__errno_location ()) = (ERANGE));
  }
  if (hy < 0x00800000) {
    float u = x * x;
    ({
      __typeof(u) __x = (u);
      __asm __volatile__("" : : "m"(__x));
    });
    //  ((*__errno_location ()) = (ERANGE));
  }
  do {
    ieee_float_shape_type sf_u;
    sf_u.word = (hx);
    (x) = sf_u.value;
  } while (0);
  return x;
}