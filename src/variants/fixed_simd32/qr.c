/*
 * Variant: fixed_simd32
 *
 * Givens rotations using the ARMv6 SIMD32 dual-multiply-accumulate
 * instructions (SMLAD / SMUSDX), available on Cortex-A7 via the DSP extensions
 * (__ARM_FEATURE_SIMD32).
 *
 * WHY: in fixed_scalar every Q11 multiply is three instructions --
 *   smull ip, r1, r0, r3 ; lsr ip, ip, #11 ; orr ip, ip, r1, lsl #21
 * because the 64-bit product has to be reassembled. 8 of the 20 instructions
 * in the rotation inner loop are that reassembly, 384 per QR of pure overhead.
 * SMLAD takes 16-bit operands and produces a 32-bit result directly, so none
 * of it exists.
 *
 * THE FIT: a Givens rotation produces two outputs from the same two inputs.
 * With coefficients packed as cs = (s:c) and data as t = (ti:tj):
 *     SMLAD (cs, t)  = cs.lo*t.lo + cs.hi*t.hi = c*tj + s*ti  -> row_j
 *     SMUSDX(cs, t)  = cs.lo*t.hi - cs.hi*t.lo = c*ti - s*tj  -> row_i
 * Both outputs, two instructions, same two registers.
 *
 * THE COST: SMLAD reads signed 16-bit halves, and a 12-bit input at Q11 needs
 * 23 bits. So the data format has to change -- see block scaling below. That
 * trade-off is the real subject of this variant.
 *
 * See docs/SIMD32_PLAN.md.
 */

#include "../../common/qr_iface.h"
#include "../../common/trig_pwl.h"

#ifdef PROFILE_OPS
#include "../../common/op_counters.h"
#else
#define OPC(f) ((void)0)
#define OPX(f) ((void)0)
#endif

const char *const QR_VARIANT_NAME = "fixed_simd32";

/* ------------------------------------------------------------------------
 * Formats
 *
 *   R : int16_t, block-scaled so max|A| lands near 2^13 (see below)
 *   Q : int16_t Q14 -- entries are <= 1.0 by construction
 *   c,s: int16_t Q14 -- <= 1.0; converted from the Q11 trig output with << 3
 *
 * SMLAD accumulates in 32 bits: 2^14 * 2^15 * 2 = 2^30, so no overflow.
 * Results are shifted right by 14 because the coefficients are Q14.
 * ------------------------------------------------------------------------ */
#define SIMD_COEFF_BITS 14
#define SIMD_ONE (1 << SIMD_COEFF_BITS) /* 1.0 as a Q14 coefficient */

/* ------------------------------------------------------------------------
 * Dual-MAC primitives.
 *
 * gcc does NOT generate SMLAD/SMUSDX from scalar C (verified: it emits plain
 * mul), so the intrinsics are required. The portable branch is a bit-exact
 * emulation -- signed 16x16 products summed in 32 bits -- so accuracy measured
 * on a non-ARM host matches the target exactly. Define SIMD32_PORTABLE to
 * force it and isolate the format change from the instruction change.
 * ------------------------------------------------------------------------ */
#if defined(__ARM_FEATURE_SIMD32) && !defined(SIMD32_PORTABLE)
#include <arm_acle.h>
#define DUAL_MAC(cs, t) __smlad((cs), (t), 0)   /* lo*lo + hi*hi */
#define DUAL_SUBX(cs, t) __smusdx((cs), (t))    /* lo*hi - hi*lo */
#define SIMD32_NATIVE 1
#else
static inline int32_t dual_mac_emul(int32_t rn, int32_t rm) {
  int32_t nl = (int16_t)(rn & 0xFFFF), nh = (int16_t)(rn >> 16);
  int32_t ml = (int16_t)(rm & 0xFFFF), mh = (int16_t)(rm >> 16);
  return nl * ml + nh * mh;
}
static inline int32_t dual_subx_emul(int32_t rn, int32_t rm) {
  int32_t nl = (int16_t)(rn & 0xFFFF), nh = (int16_t)(rn >> 16);
  int32_t ml = (int16_t)(rm & 0xFFFF), mh = (int16_t)(rm >> 16);
  return nl * mh - nh * ml;
}
#define DUAL_MAC(cs, t) dual_mac_emul((cs), (t))
#define DUAL_SUBX(cs, t) dual_subx_emul((cs), (t))
#define SIMD32_NATIVE 0
#endif

/* Pack two signed halfwords into one register: result.hi = hi, result.lo = lo */
static inline int32_t pack16(int16_t hi, int16_t lo) {
  return (int32_t)(((uint32_t)(uint16_t)hi << 16) | (uint32_t)(uint16_t)lo);
}

/* ------------------------------------------------------------------------
 * Block scaling.
 *
 * Rather than commit to a coarse fixed Q format, scale the whole matrix once
 * so the largest element lands near 2^13. That uses the full 16-bit range
 * whatever the input magnitude -- a fixed Q4 would waste precision on large
 * inputs and destroy it on small ones -- and leaves 2 bits of headroom for the
 * growth a Givens rotation causes (entries of R reach ~1.2-2.3x the input
 * magnitude for random 4x4 matrices).
 *
 * Q needs no unscaling: it is dimensionless. And the angle computation is
 * unaffected, because calculate_arctan_ratio uses only the ratio N/D, in which
 * the block scale cancels.
 *
 * Positive shift means scale down; negative means scale up (small inputs).
 * ------------------------------------------------------------------------ */
static int block_shift(const int32_t *A) {
  int32_t maxv = 0;
  for (int i = 0; i < MATRIX_ELEMENTS; i++) {
    int32_t v = A[i] < 0 ? -A[i] : A[i];
    if (v > maxv) maxv = v;
  }
  if (maxv == 0) return 0;
  /* bits(maxv) = 32 - clz. Target 14 bits so the value sits near 2^13. */
  return (32 - 14) - __builtin_clz((uint32_t)maxv);
}

static inline int16_t scale_down(int32_t v, int sh) {
  return (int16_t)(sh >= 0 ? (v >> sh) : (v << (-sh)));
}

static inline int32_t scale_up(int16_t v, int sh) {
  return sh >= 0 ? ((int32_t)v << sh) : ((int32_t)v >> (-sh));
}

/* ------------------------------------------------------------------------
 * Rotations
 * ------------------------------------------------------------------------ */

/* Rows i and j of R. cs = (s:c) in Q14. */
static void rotate_rows(int16_t *R, int32_t cs, int i, int j) {
  OPC(rotations);
  int16_t *row_i = &R[i * MATRIX_SIZE];
  int16_t *row_j = &R[j * MATRIX_SIZE];
  for (int k = 0; k < MATRIX_SIZE; k++) {
    /* row_i[k] and row_j[k] are in different rows, so they are never adjacent
       and must be packed each iteration (PKHBT on ARM). This is the main
       overhead of the approach. */
    int32_t t = pack16(row_i[k], row_j[k]); /* (ti:tj) */
    OPC(mac);
    OPC(mac);
    row_j[k] = (int16_t)(DUAL_MAC(cs, t) >> SIMD_COEFF_BITS);
    row_i[k] = (int16_t)(DUAL_SUBX(cs, t) >> SIMD_COEFF_BITS);
  }
}

/* Columns i and j of Q. Strided access, same packing requirement. */
static void rotate_cols(int16_t *Q, int32_t cs, int i, int j) {
  OPC(rotations);
  for (int k = 0; k < MATRIX_SIZE; k++) {
    int16_t *base = &Q[k * MATRIX_SIZE];
    int32_t t = pack16(base[i], base[j]); /* (ti:tj) */
    OPC(mac);
    OPC(mac);
    base[j] = (int16_t)(DUAL_MAC(cs, t) >> SIMD_COEFF_BITS);
    base[i] = (int16_t)(DUAL_SUBX(cs, t) >> SIMD_COEFF_BITS);
  }
}

/* ------------------------------------------------------------------------ */

void qr_decomposition(const int32_t *A, int32_t *Q, int32_t *R) {
  OPC(qr_calls);

  const int sh = block_shift(A);

  int16_t Rs[MATRIX_ELEMENTS];
  int16_t Qs[MATRIX_ELEMENTS];
  for (int i = 0; i < MATRIX_ELEMENTS; i++) Rs[i] = scale_down(A[i], sh);
  for (int i = 0; i < MATRIX_SIZE; i++)
    for (int j = 0; j < MATRIX_SIZE; j++)
      Qs[i * MATRIX_SIZE + j] = (i == j) ? SIMD_ONE : 0;

  for (int j = 0; j < MATRIX_SIZE; j++) {
    for (int i = j + 1; i < MATRIX_SIZE; i++) {
      int32_t opposite = Rs[i * MATRIX_SIZE + j];
      int32_t adjacent = Rs[j * MATRIX_SIZE + j];

      /* Unchanged from fixed_scalar: the angle depends only on the ratio, so
         the block scale cancels and trig_pwl needs no modification. Output is
         a Q11 angle; c/s come back Q11 and shift up to Q14. */
      int32_t angle = calculate_arctan_ratio(opposite, adjacent);
      int32_t c11 = cos_fixed(angle);
      int32_t s11 = sin_fixed(angle);
      int32_t cs = pack16((int16_t)(s11 << 3), (int16_t)(c11 << 3));

      rotate_rows(Rs, cs, i, j);
      Rs[i * MATRIX_SIZE + j] = 0;
      rotate_cols(Qs, cs, i, j);
    }
  }

  for (int i = 0; i < MATRIX_ELEMENTS; i++) R[i] = scale_up(Rs[i], sh);
  /* Q14 -> Q11 */
  for (int i = 0; i < MATRIX_ELEMENTS; i++)
    Q[i] = (int32_t)Qs[i] >> (SIMD_COEFF_BITS - FIXED_FRAC_BITS);
}
