/*
 * Variant: fixed_simd32
 *
 * SIMD32 dual-MAC rotations. With coefficients packed as cs = (s:c) and data as
 * t = (ti:tj), one pair of instructions produces both outputs of a Givens
 * rotation from the same two registers:
 *     SMLAD (cs, t) = cs.lo*t.lo + cs.hi*t.hi = c*tj + s*ti  -> row_j
 *     SMUSDX(cs, t) = cs.lo*t.hi - cs.hi*t.lo = c*ti - s*tj  -> row_i
 *
 * gcc will not generate these from scalar C, so the ACLE intrinsics are needed.
 * That drops the rotation inner loop from 20 instructions to 11: the Q11 path
 * spent 8 of its 20 on lsr/orr reassembling 64-bit smull products, and SMLAD
 * produces a 32-bit result from 16-bit operands directly.
 *
 * The cost: SMLAD reads signed 16-bit halves, but a 12-bit input at Q11 needs
 * 23 bits. Hence the block scaling below.
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

/* R: int16 block-scaled near 2^13.  Q and c,s: Q14 (both <= 1.0).
   SMLAD accumulates in 32 bits (2^14 * 2^15 * 2 = 2^30), then >> 14. */
#define COEFF_BITS 14

/* Dual-MAC primitives. The portable branch is a bit-exact emulation (signed
   16x16 products summed in 32 bits) so the variant can be validated off-target;
   define SIMD32_PORTABLE to force it. */
#if defined(__ARM_FEATURE_SIMD32) && !defined(SIMD32_PORTABLE)
#include <arm_acle.h>
#define DUAL_MAC(cs, t) __smlad((cs), (t), 0)
#define DUAL_SUBX(cs, t) __smusdx((cs), (t))
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
#endif

/* result.hi = hi, result.lo = lo */
static inline int32_t pack16(int16_t hi, int16_t lo) {
  return (int32_t)(((uint32_t)(uint16_t)hi << 16) | (uint32_t)(uint16_t)lo);
}

/* Block scaling: shift the whole matrix so max|A| lands near 2^13. Uses the full
   16-bit range whatever the input magnitude, leaving 2 bits for the growth a
   rotation causes; a fixed Q format would waste precision on large inputs and
   destroy it on small ones. Q needs no unscaling (dimensionless), and the angle
   is unaffected because only the ratio N/D matters -- so trig_pwl is reused
   unchanged. Positive shift scales down, negative scales up. */
static int block_shift(const int32_t *A) {
  int32_t maxv = 0;
  for (int i = 0; i < MATRIX_ELEMENTS; i++) {
    int32_t v = A[i] < 0 ? -A[i] : A[i];
    if (v > maxv) maxv = v;
  }
  if (maxv == 0) return 0;
  return (32 - 14) - __builtin_clz((uint32_t)maxv);
}

static inline int16_t scale_down(int32_t v, int sh) {
  return (int16_t)(sh >= 0 ? (v >> sh) : (v << (-sh)));
}
static inline int32_t scale_up(int16_t v, int sh) {
  return sh >= 0 ? ((int32_t)v << sh) : ((int32_t)v >> (-sh));
}

static void rotate_rows(int16_t *R, int32_t cs, int i, int j) {
  OPC(rotations);
  int16_t *row_i = &R[i * MATRIX_SIZE];
  int16_t *row_j = &R[j * MATRIX_SIZE];
  for (int k = 0; k < MATRIX_SIZE; k++) {
    /* row_i[k] and row_j[k] are in different rows, so never adjacent -- they
       must be packed each iteration. gcc does it with one orr plus a free
       barrel shift. */
    int32_t t = pack16(row_i[k], row_j[k]);
    OPC(mac);
    OPC(mac);
    row_j[k] = (int16_t)(DUAL_MAC(cs, t) >> COEFF_BITS);
    row_i[k] = (int16_t)(DUAL_SUBX(cs, t) >> COEFF_BITS);
  }
}

static void rotate_cols(int16_t *Q, int32_t cs, int i, int j) {
  OPC(rotations);
  for (int k = 0; k < MATRIX_SIZE; k++) {
    int16_t *base = &Q[k * MATRIX_SIZE];
    int32_t t = pack16(base[i], base[j]);
    OPC(mac);
    OPC(mac);
    base[j] = (int16_t)(DUAL_MAC(cs, t) >> COEFF_BITS);
    base[i] = (int16_t)(DUAL_SUBX(cs, t) >> COEFF_BITS);
  }
}

void qr_decomposition(const int32_t *A, int32_t *Q, int32_t *R) {
  OPC(qr_calls);

  const int sh = block_shift(A);

  int16_t Rs[MATRIX_ELEMENTS], Qs[MATRIX_ELEMENTS];
  for (int i = 0; i < MATRIX_ELEMENTS; i++) Rs[i] = scale_down(A[i], sh);
  for (int i = 0; i < MATRIX_SIZE; i++)
    for (int j = 0; j < MATRIX_SIZE; j++)
      Qs[i * MATRIX_SIZE + j] = (i == j) ? (1 << COEFF_BITS) : 0;

  for (int j = 0; j < MATRIX_SIZE; j++) {
    for (int i = j + 1; i < MATRIX_SIZE; i++) {
      int32_t opposite = Rs[i * MATRIX_SIZE + j];
      int32_t adjacent = Rs[j * MATRIX_SIZE + j];

      /* trig_pwl returns Q14 natively, which is COEFF_BITS -- no repacking
         shift, the coefficients drop straight into the SMLAD operand. */
      int32_t angle = calculate_arctan_ratio(opposite, adjacent);
      int32_t c14 = cos_fixed(angle);
      int32_t s14 = sin_fixed(angle);
      int32_t cs = pack16((int16_t)s14, (int16_t)c14);

      rotate_rows(Rs, cs, i, j);
      Rs[i * MATRIX_SIZE + j] = 0;
      rotate_cols(Qs, cs, i, j);
    }
  }

  for (int i = 0; i < MATRIX_ELEMENTS; i++) R[i] = scale_up(Rs[i], sh);
  for (int i = 0; i < MATRIX_ELEMENTS; i++)
    Q[i] = (int32_t)Qs[i] >> (COEFF_BITS - FIXED_FRAC_BITS);
}
