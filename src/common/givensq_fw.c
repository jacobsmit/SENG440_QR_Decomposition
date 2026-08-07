/*
 * GIVENSQ, firmware model -- no multiplier, no divider.
 *
 * This is the SPECIFICATION that src/common/givensq_fw.S must reproduce
 * bit-for-bit, and that the VHDL must reproduce after it. It exists so the
 * assembly is testable: "the microcode is correct" is otherwise an assertion
 * nobody can check.
 *
 * Two constraints from Lesson 100 step 7 ("use the ARM instruction set without
 * multiplication and division") are honoured differently in the two files:
 *
 *   multiply -- the slope is a sum of three signed powers of two
 *               (trig_pwl_csd.h), so m*x is three shift-adds. Because the
 *               constrained slope is still an exact integer, THIS FILE MAY USE
 *               `*` and remain bit-identical to the assembly's shift-adds. The
 *               value is the contract; the shift-adds are the implementation.
 *
 *   divide   -- restoring division, written out below as the shift/compare/
 *               subtract loop the microcode performs, NOT as `/`. The C `/`
 *               and a restoring divider disagree on sign handling, so this one
 *               really does have to be spelled out.
 *
 * Distinct from givensq_ref() in givensq.h, which models the SOFTWARE path and
 * keeps exact slopes because the real ARM has SMULL. The two differ by the
 * slope quantisation only: ~0.00043 vs ~0.00023 max PWL error.
 */

#include "givensq_fw.h"
#include "trig_pwl_csd.h"

#define FW_Q CSD_TRIG_Q
#define FW_PI_2 25736 /* pi/2 in Q14 */
#define FW_PI_4 12868 /* pi/4 in Q14 */

/* --- restoring division ---------------------------------------------------
 * quotient = (n << 14) / d for 0 <= n <= d, computed one bit at a time:
 * shift the remainder up, compare, subtract if it fits, record the bit.
 * Exactly what the microcode loop does, and exactly what the divider in the
 * hardware will do. 15 iterations for 15 quotient bits (Q14 plus the 1.0 case).
 */
uint32_t fw_divide_restoring(uint32_t n, uint32_t d, int *iterations) {
  /* n == d would need a 15-bit quotient (exactly 1.0), which a 14-iteration
     binary loop cannot produce -- each iteration yields one quotient bit, so
     the loop requires n < d strictly. The caller already compares the two
     operands to decide the swap, so this costs nothing extra, and the table's
     guard entry exists precisely for the ratio == 1.0 index. */
  if (n >= d) {
    if (iterations) *iterations = 0;
    return 1u << FW_Q;
  }
  uint32_t rem = n, quo = 0;
  int iters = 0;
  for (int i = 0; i < FW_Q; i++) {
    rem <<= 1;               /* shift the remainder up ... */
    quo <<= 1;
    if (rem >= d) {          /* ... does the divisor fit? */
      rem -= d;              /* yes: subtract and record a 1 bit */
      quo |= 1u;
    }
    iters++;
  }
  if (iterations) *iterations = iters;
  return quo;                /* = floor(n * 2^14 / d) */
}

/* --- one PWL segment ------------------------------------------------------
 * idx is a shift, the coefficients come from the control store, and m*x is the
 * three shift-adds. No bounds check: the table carries a guard entry.
 */
static int32_t fw_pwl(const int32_t *tbl, int32_t x) {
  int32_t mb = tbl[x >> CSD_INDEX_SHIFT];
  int32_t m = (int16_t)(mb & 0xFFFF);
  int32_t b = (int16_t)(mb >> 16);
  return ((m * x) >> FW_Q) + b;
}

static int32_t fw_arctan(int32_t x) {
  int32_t a = (x < 0) ? -x : x;
  int32_t r = fw_pwl(csd_arctan, a);
  return (x < 0) ? -r : r;
}

static int32_t fw_cos(int32_t x);

static int32_t fw_sin(int32_t x) {
  int32_t a = (x < 0) ? -x : x;
  int32_t r = (a > FW_PI_4) ? fw_cos(FW_PI_2 - a) : fw_pwl(csd_sin, a);
  return (x < 0) ? -r : r;
}

static int32_t fw_cos(int32_t x) {
  int32_t a = (x < 0) ? -x : x;
  return (a > FW_PI_4) ? fw_sin(FW_PI_2 - a) : fw_pwl(csd_cos, a);
}

/* --- the instruction ------------------------------------------------------ */
int32_t givensq_fw(int32_t opposite, int32_t adjacent) {
  int32_t angle;

  if (adjacent == 0) {
    angle = (opposite >= 0) ? FW_PI_2 : -FW_PI_2;
  } else {
    uint32_t ao = (uint32_t)(opposite < 0 ? -opposite : opposite);
    uint32_t ad = (uint32_t)(adjacent < 0 ? -adjacent : adjacent);
    int swapped = ao > ad;
    uint32_t n = swapped ? ad : ao;
    uint32_t d = swapped ? ao : ad;

    /* |n| <= |d| always, so the quotient fits Q14 with 1.0 as its maximum. */
    int32_t ratio = (int32_t)fw_divide_restoring(n, d, 0);
    int neg = (opposite < 0) ^ (adjacent < 0);
    if (neg) ratio = -ratio;

    angle = fw_arctan(ratio);
    if (swapped) angle = (neg ? -FW_PI_2 : FW_PI_2) - angle;
  }

  int32_t c = fw_cos(angle);
  int32_t s = fw_sin(angle);
  return (int32_t)(((uint32_t)(uint16_t)s << 16) | (uint32_t)(uint16_t)c);
}
