#define MAX_WORKGROUP  32
#define MAX_THREAD_PER_WG 1024

extern __global int __wg_scratch[MAX_WORKGROUP];
extern __global int __wi_scratch[MAX_THREAD_PER_WG * MAX_WORKGROUP];
