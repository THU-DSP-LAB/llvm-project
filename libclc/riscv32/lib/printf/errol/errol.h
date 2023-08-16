#ifndef ERROL_H
#define ERROL_H

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/*
 * errol declarations
 */

// TODO include stdbool ?
typedef int _bool;
#define true 1
#define false 0

#define ERR_LEN   512
#define ERR_DEPTH 4

int errol0_dtoa(double val, char *buf);
int errol1_dtoa(double val, char *buf, _bool *opt);
int errol2_dtoa(double val, char *buf, _bool *opt);
int errol3_dtoa(double val, char *buf);
int errol3u_dtoa(double val, char *buf);
int errol4_dtoa(double val, char *buf);
inline int errol4u_dtoa(double val, char *buf);

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

#ifdef __cplusplus
}
#endif

#endif
