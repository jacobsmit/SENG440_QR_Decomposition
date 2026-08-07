#ifndef TRIG_PWL_H
#define TRIG_PWL_H

#include "fixed.h"
#include <stdint.h>

/*
 * Piecewise-linear arctan, sin and cos, table driven.
 *
 * Segment selection is a SHIFT, not a compare chain: segments have width 2^-p
 * in the fixed-point representation, so the index is the top bits of the input
 * and costs one LSR regardless of how many segments there are. Coefficients are
 * (slope, intercept) packed into one 32-bit word, so the fetch is one load.
 * Accuracy is therefore bought with table ROM, not with instructions -- see
 * scripts/gen_pwl_tables.py, which emits the table.
 *
 * FORMATS -- deliberately NOT the Q11 of fixed.h:
 *   angle                     Q14
 *   ratio into arctan_fixed   Q14, |x| <= 1
 *   sin / cos value           Q14
 *
 * Q14 for two reasons. The tables resolve to ~7e-4, and a Q11 intercept
 * quantises at 1/2048 = 4.9e-4, which would dominate the approximation error
 * and make extra segments pointless. And Q14 is already what SMLAD and GIVENSQ
 * consume, so the coefficients need no repacking shift.
 *
 * Matrix data stays Q11 (fixed.h). Multiplying a Q14 coefficient by a Q11
 * datum is TRIG_MUL, which shifts by 14 and yields Q11.
 */

#define TRIG_FRAC_BITS 14
#define TRIG_SCALE (1 << TRIG_FRAC_BITS)

#define PI_OVER_2_FIXED 25736 /* pi/2 in Q14 */
#define PI_OVER_4_FIXED 12868 /* pi/4 in Q14 -- the angle-fold threshold */

/* Q14 coefficient x Q11 datum -> Q11. 64-bit intermediate for the same reason
   fixed_mul has one: a Q11 datum can reach 2^20 and the product would overflow
   int32. On ARM this is one SMULL. */
static inline int32_t trig_mul(int32_t coeff_q14, int32_t v) {
  return (int32_t)(((int64_t)coeff_q14 * v) >> TRIG_FRAC_BITS);
}

#ifdef PROFILE_OPS
#include "op_counters.h"
#define TRIG_MUL(c, v) (OPC(mul), trig_mul((c), (v)))
#else
#define TRIG_MUL(c, v) trig_mul((c), (v))
#endif

/* X is a Q14 ratio with |X| <= 1. Returns a Q14 angle. */
int32_t arctan_fixed(int32_t X);

/* X is a Q14 angle. Returns a Q14 value in [-1, 1]. */
int32_t sin_fixed(int32_t X);
int32_t cos_fixed(int32_t X);

/* atan2-style: Q14 angle of (N, D), with quadrant and reciprocal folding.
   N and D may be in any consistent format -- only their ratio matters. */
int32_t calculate_arctan_ratio(int32_t N, int32_t D);

#endif /* TRIG_PWL_H */
