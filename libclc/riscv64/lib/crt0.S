/* RV64 Ventus device-kernel entry. */
#include "ventus.h"

.section .data
.global BUFFER_ADDR
BUFFER_ADDR:
    .dword 0
.global BUFFER_SIZE
BUFFER_SIZE:
    .dword 0

.text
.global _start
.type _start, @function
_start:
  .option push
  .option norelax
  la gp, __global_pointer$
  .option pop

  li t4,32
  vsetvli t4,t4,e32,m1,ta,ma
  li t4,0x2000
  csrrs t4, 0x300, t4
  li t4,0
  csrr t1, CSR_WID
  csrr t2, CSR_LDS
  csrr t0, CSR_KNL
  lw t6, KNL_LDS_NON_STACK_SIZE(t0)
  add t2, t2, t6
  lw t3, KNL_LDS_STACK_SIZE_PER_WF(t0)
  mul t1, t1, t3
  add sp, t1, t2
  ld tp, KNL_PDS_SIZE_PER_THREAD(t0)
  csrr t5, CSR_NUMW
  lw t3, KNL_LDS_STACK_SIZE_PER_WF(t0)
  mul t5, t5, t3
  add s0, t2, t5

  csrr t0, CSR_KNL
  ld t1, KNL_ENTRY(t0)
  ld a0, KNL_ARG_BASE(t0)
  la t6, spike_end
  csrw 0x305, t6
  jalr t1

  endprg x0, x0, x0
  j spike_end
.size _start, .-_start

.section ".tohost","aw",@progbits
.align 6
.globl tohost
tohost: .dword 0
.align 6
.globl fromhost
fromhost: .dword 0

.text
.global spike_end
.type spike_end,function
spike_end:
  li t1,1
  la t0,tohost
  sw t1,0(t0)
