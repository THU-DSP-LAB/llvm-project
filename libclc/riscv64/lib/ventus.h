/* RV64 Ventus kernel metadata ABI. */
#ifndef __VENTUS_RV64_H__
#define __VENTUS_RV64_H__

#define CSR_WID   0x805
#define CSR_LDS   0x806
#define CSR_KNL   0x803
#define CSR_NUMW  0x801

#define KNL_ENTRY 0
#define KNL_ARG_BASE 8
#define KNL_LDS_STACK_SIZE_PER_WF 68
#define KNL_LDS_NON_STACK_SIZE 72

#endif
