#ifndef FIXED_H
#define FIXED_H

#include <stdint.h>

/*
 * Q11 fixed point:  int32 = 1 sign + 20 integer + 11 fractional bits
 *   range      +/- 2^20 = +/- 1,048,576
 *   resolution 2^-11    = 0.000488
 *
 * Keep max|A| under about 2^18: Givens rotations preserve the Frobenius norm,
 * so entries of R grow to ~1.2-2.3x the input magnitude.
 */

#define FIXED_FRAC_BITS 11
#define FIXED_SCALE (1 << FIXED_FRAC_BITS) /* 2048 */

#define FLOAT_TO_FIXED(f) ((int32_t)((f) * FIXED_SCALE))
#define FIXED_TO_FLOAT(i) ((float)(i) / FIXED_SCALE)

/* The product of two Q11 values is the answer scaled by 2048 twice, so it
   overflows int32 once a stored value exceeds 2^31/2^11 = 2^20 (a real value of
   just 512). Computing in 64 bits and shifting before narrowing raises the
   ceiling to the actual limit of the format. On ARM this is one SMULL writing a
   register pair -- the datapath stays 32-bit. */
static inline int32_t fixed_mul(int32_t a, int32_t b) {
  return (int32_t)(((int64_t)a * b) >> FIXED_FRAC_BITS);
}

static inline int32_t fixed_div(int32_t a, int32_t b) {
  /* Same overflow: a << 11 exceeds 32 bits once |a| > 2^20. A 64-bit numerator
     would work but ARM has no 64/32 divide, turning one SDIV into an
     __aeabi_ldiv call. Instead shift BOTH operands down equally -- the quotient
     is unchanged -- until the numerator fits in 20 bits.

     The shift comes from CLZ, not a loop: a value needs (32 - clz) bits and we
     need at most (31 - F), so shift = (1 + F) - clz. Constant latency, which
     matters because cycles here are hand-computed rather than measured.

     Safe against b >> sh becoming 0: every caller passes |a| <= |b|. */
  int32_t aa = (a < 0) ? -a : a;
  int sh = aa ? ((1 + FIXED_FRAC_BITS) - __builtin_clz((uint32_t)aa)) : 0;
  if (sh < 0) sh = 0;
  return ((a >> sh) << FIXED_FRAC_BITS) / (b >> sh);
}

#ifdef PROFILE_OPS
#include "op_counters.h"
#define FIXED_MUL(a, b) (OPC(mul), fixed_mul((a), (b)))
#define FIXED_DIV(a, b) (OPC(divide), fixed_div((a), (b)))
#else
#define FIXED_MUL(a, b) fixed_mul((a), (b))
#define FIXED_DIV(a, b) fixed_div((a), (b))
#endif

#endif /* FIXED_H */
