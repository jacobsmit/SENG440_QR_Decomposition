#ifndef GIVENSQ_FW_H
#define GIVENSQ_FW_H

#include <stdint.h>

/* GIVENSQ as the microcoded engine computes it: no multiplier, no divider.
   Specification for src/common/givensq_fw.S and for the VHDL. See givensq_fw.c
   for why this is separate from givensq_ref(). */
int32_t givensq_fw(int32_t opposite, int32_t adjacent);

/* Exposed so the assembly's divider can be tested in isolation, and so the
   iteration count that sets the microcode cycle budget is measurable rather
   than assumed. */
uint32_t fw_divide_restoring(uint32_t n, uint32_t d, int *iterations);

#if defined(__arm__)
/* The same instruction in hand-written ARM assembly, with no MUL and no SDIV
   (src/common/givensq_fw.S, generated from the same CSD table as the C model).
   givensq_fw() above is its specification: the two must agree bit-for-bit on
   every input, which is what tests/test_givensq_fw.c asserts on the target. */
int32_t givensq_fw_asm(int32_t opposite, int32_t adjacent);
#endif

/* Same packing as givensq.h: s in the high half, c in the low half, both Q14. */
static inline int32_t givensq_fw_c(int32_t p) { return (int16_t)(p & 0xFFFF); }
static inline int32_t givensq_fw_s(int32_t p) { return (int16_t)(p >> 16); }

#endif /* GIVENSQ_FW_H */
