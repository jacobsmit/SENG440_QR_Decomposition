#ifndef FIXED_H
#define FIXED_H

#include <stdint.h>

/* ------------------------------------------------------------------------
 * Q11 fixed-point format.
 *
 *   int32 = 1 sign bit + 20 integer bits + 11 fractional bits
 *   representable range   +/- 2^20  = +/- 1,048,576
 *   resolution            2^-11     = 0.000488
 *
 * Leave headroom below the ceiling: Givens rotations preserve the Frobenius
 * norm, so entries of R grow to roughly 1.2-2.3x the input magnitude for
 * random 4x4 matrices. Keep max|A| under about 2^18.
 * ------------------------------------------------------------------------ */

#define FIXED_FRAC_BITS 11
#define FIXED_SCALE (1 << FIXED_FRAC_BITS) /* 2048 */

#define FLOAT_TO_FIXED(f) ((int32_t)((f) * FIXED_SCALE))
#define FIXED_TO_FLOAT(i) ((float)(i) / FIXED_SCALE)

/* Multiply and divide compute the intermediate in 64 bits before scaling back
 * to 32. The ANSWER always fits in 32 bits; the intermediate does not.
 * Multiplying two Q11 values scales the result by 2048 twice, so the product
 * is the answer times 4 million -- that overflows int32 once a stored value
 * exceeds 2^31/2^11 = 2^20, i.e. a real value of only 512.0.
 *
 * This costs nothing exotic on ARM: 32x32->64 is a single SMULL instruction
 * writing a register pair, present since ARMv4. The datapath stays 32-bit. */
static inline int32_t fixed_mul(int32_t a, int32_t b) {
  return (int32_t)(((int64_t)a * b) >> FIXED_FRAC_BITS);
}

static inline int32_t fixed_div(int32_t a, int32_t b) {
  /* Same overflow, different shape: a << 11 blows past 32 bits once |a|
     exceeds 2^20. Doing it in 64 bits would work, but ARM has no 64/32 divide
     instruction, so that turns one SDIV into an __aeabi_ldiv library call.
     Instead shift BOTH operands down equally -- the quotient is unchanged --
     until the numerator fits in 20 bits. Keeps the hardware SDIV.

     The shift comes from CLZ rather than a loop. A value needs (32 - clz)
     bits; for (value << F) to fit in 31 bits plus sign we need at most
     (31 - F) bits, so:
         shift = (32 - clz) - (31 - F) = (1 + F) - clz
     which is 12 - clz at F = 11. One instruction, and constant latency --
     necessary for hand cycle-counting, since QEMU cannot time anything
     (docs/TARGET_PLATFORM.md).
     __builtin_clz maps to the ARM CLZ instruction (ARMv5T+; __ARM_FEATURE_CLZ
     is set for cortex-a7). Undefined for 0, hence the guard.

     Safe against b >> sh becoming 0: every caller passes |a| <= |b|, so
     whenever a needs shifting b is at least as large. */
  int32_t aa = (a < 0) ? -a : a;
  int sh = aa ? ((1 + FIXED_FRAC_BITS) - __builtin_clz((uint32_t)aa)) : 0;
  if (sh < 0) sh = 0;
  return ((a >> sh) << FIXED_FRAC_BITS) / (b >> sh);
}

/* Optional deterministic operation profiling (src/common/op_counters.h).
   Both builds call the same fixed_mul/fixed_div above, so an instrumented
   build cannot drift from production arithmetic. */
#ifdef PROFILE_OPS
#include "op_counters.h"
#define FIXED_MUL(a, b) (OPC(mul), fixed_mul((a), (b)))
#define FIXED_DIV(a, b) (OPC(divide), fixed_div((a), (b)))
#else
#define FIXED_MUL(a, b) fixed_mul((a), (b))
#define FIXED_DIV(a, b) fixed_div((a), (b))
#endif

#endif /* FIXED_H */
