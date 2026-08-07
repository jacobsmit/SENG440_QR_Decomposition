#include "trig_pwl.h"
#include "trig_pwl_tables.h"

#ifdef PROFILE_OPS
#include "op_counters.h"
#else
#define OPC(f) ((void)0)
#define OPX(f) ((void)0)
#endif

/*
 * One segment evaluation: shift to an index, one load, one multiply, one add.
 * No branch on the segment, so the cost does not grow with PWL_SEG_BITS -- that
 * is the whole point of uniform (power-of-two) segment widths.
 *
 * The clamp catches only the exact top of the domain (|x| == 1 for arctan,
 * pi/4 for sin/cos), where the index lands one past the last segment. Every
 * other input indexes in range by construction.
 *
 * m * x cannot overflow int32: both are Q14 and bounded by 1.0 and pi/4
 * respectively, so the product stays under 2^29. No SMULL needed here, unlike
 * the rotation path.
 */
static inline int32_t pwl_eval(const int32_t *tbl, int nseg, int32_t x) {
  int idx = (int)(x >> PWL_INDEX_SHIFT);
  if (idx >= nseg) idx = nseg - 1;
  int32_t mb = tbl[idx];
  int32_t m = (int16_t)(mb & 0xFFFF);
  int32_t b = (int16_t)(mb >> 16);
  OPC(mul);
  OPX(pwl_lookups);
  return ((m * x) >> TRIG_FRAC_BITS) + b;
}

int32_t arctan_fixed(int32_t X) {
  OPX(call_arctan);
  OPC(decisions);
  int32_t a = (X < 0) ? -X : X;
  int32_t r = pwl_eval(pwl_arctan, PWL_ARCTAN_SEGS, a);
  return (X < 0) ? -r : r; /* arctan is odd */
}

int32_t sin_fixed(int32_t X) {
  OPX(call_sin);
  OPC(decisions);
  int32_t abs_X = (X < 0) ? -X : X;
  int32_t result;

  /* Beyond pi/4 the table has no coverage; fold with sin(x) = cos(pi/2 - x).
     abs_X reaches 3*pi/4 (calculate_arctan_ratio can return -pi/2 - pi/4), so
     the folded argument lands in [-pi/4, pi/4] and cos_fixed cannot re-fold --
     the mutual recursion terminates after one step. */
  if (abs_X > PI_OVER_4_FIXED) {
    OPX(angle_folds);
    result = cos_fixed(PI_OVER_2_FIXED - abs_X);
  } else {
    result = pwl_eval(pwl_sin, PWL_SIN_SEGS, abs_X);
  }

  return (X < 0) ? -result : result; /* sine is odd */
}

int32_t cos_fixed(int32_t X) {
  OPX(call_cos);
  OPC(decisions);
  int32_t abs_X = (X < 0) ? -X : X; /* cosine is even */

  if (abs_X > PI_OVER_4_FIXED) {
    OPX(angle_folds);
    return sin_fixed(PI_OVER_2_FIXED - abs_X);
  }
  return pwl_eval(pwl_cos, PWL_COS_SEGS, abs_X);
}

/*
 * a / b as a Q14 ratio, for |a| <= |b|.
 *
 * Same normalisation as fixed_div, retargeted from Q11 to Q14: a << 14 leaves
 * int32 once |a| > 2^17, so shift BOTH operands down until the numerator fits.
 * The quotient is unchanged. The shift comes from CLZ, so it is branchless and
 * constant latency -- which matters because the microcode cycle count is
 * computed by hand.
 *
 * b >> sh cannot reach zero: sh is derived from |a|, and |b| >= |a|.
 */
static int32_t ratio_div_q14(int32_t a, int32_t b) {
  int32_t aa = (a < 0) ? -a : a;
  int sh = aa ? ((1 + TRIG_FRAC_BITS) - __builtin_clz((uint32_t)aa)) : 0;
  if (sh < 0) sh = 0;
  return ((a >> sh) << TRIG_FRAC_BITS) / (b >> sh);
}

int32_t calculate_arctan_ratio(int32_t N, int32_t D) {
  OPX(call_atan2);
  OPC(decisions);
  if (D == 0) {
    OPX(div_by_zero);
    return (N >= 0) ? PI_OVER_2_FIXED : -PI_OVER_2_FIXED;
  }
  int32_t abs_N = (N < 0) ? -N : N;
  int32_t abs_D = (D < 0) ? -D : D;
  if (abs_N <= abs_D) {
    OPC(divide);
    return arctan_fixed(ratio_div_q14(N, D));
  } else {
    OPX(atan2_reciprocal);
    OPC(divide);
    int32_t angle = arctan_fixed(ratio_div_q14(D, N));
    int32_t sign = ((N < 0) ^ (D < 0)) ? -1 : 1;
    return (sign > 0) ? (PI_OVER_2_FIXED - angle) : (-PI_OVER_2_FIXED - angle);
  }
}
