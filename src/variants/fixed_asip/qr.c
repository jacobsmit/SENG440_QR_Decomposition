/*
 * Variant: fixed_asip
 *
 * fixed_simd32 with the angle-and-coefficient computation replaced by a single
 * custom instruction, GIVENSQ (src/common/givensq.h).
 *
 * WHY THIS OPERATION: after SIMD32 fixed the rotations, the trig became the
 * dominant cost -- 910 of 2058 instructions per QR, 44.2 %. So the Amdahl
 * ceiling for replacing it is 1/(1 - 0.442) = 1.79x, and it went UP as a result
 * of the previous optimisation rather than down.
 *
 * WHY IT COMPOSES: GIVENSQ returns c and s packed as two 16-bit halves, forced
 * by ARM's one-result limit. That is precisely the operand layout SMLAD/SMUSDX
 * want, so the result feeds the rotation with no repacking at all.
 *
 * TWO BUILDS:
 *   default            GIVENSQ evaluated by the C reference model. Links, runs,
 *                      accuracy-tested. Should be BIT-IDENTICAL to fixed_simd32.
 *   -DUSE_GIVENSQ_ASM  real inline assembly. Compile-only; see `make asip-asm`.
 */

#include "../../common/givensq.h"
#include "../../common/qr_iface.h"

#ifdef PROFILE_OPS
#include "../../common/op_counters.h"
#else
#define OPC(f) ((void)0)
#define OPX(f) ((void)0)
#endif

const char *const QR_VARIANT_NAME = "fixed_asip";

/* Same SIMD32 dual-MAC primitives as fixed_simd32. The portable branch is a
   bit-exact emulation so the variant can be validated off-target. */
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

static inline int32_t pack16(int16_t hi, int16_t lo) {
  return (int32_t)(((uint32_t)(uint16_t)hi << 16) | (uint32_t)(uint16_t)lo);
}

/* Block scaling: identical to fixed_simd32. Scale so max|A| lands near 2^13,
   leaving 2 bits for the growth a rotation causes. Q needs no unscaling, and
   the angle is unaffected because only the ratio N/D matters. */
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
    int32_t t = pack16(row_i[k], row_j[k]);
    OPC(mac);
    OPC(mac);
    row_j[k] = (int16_t)(DUAL_MAC(cs, t) >> GIVENSQ_Q);
    row_i[k] = (int16_t)(DUAL_SUBX(cs, t) >> GIVENSQ_Q);
  }
}

static void rotate_cols(int16_t *Q, int32_t cs, int i, int j) {
  OPC(rotations);
  for (int k = 0; k < MATRIX_SIZE; k++) {
    int16_t *base = &Q[k * MATRIX_SIZE];
    int32_t t = pack16(base[i], base[j]);
    OPC(mac);
    OPC(mac);
    base[j] = (int16_t)(DUAL_MAC(cs, t) >> GIVENSQ_Q);
    base[i] = (int16_t)(DUAL_SUBX(cs, t) >> GIVENSQ_Q);
  }
}

void qr_decomposition(const int32_t *A, int32_t *Q, int32_t *R) {
  OPC(qr_calls);

  const int sh = block_shift(A);

  int16_t Rs[MATRIX_ELEMENTS];
  int16_t Qs[MATRIX_ELEMENTS];
  for (int i = 0; i < MATRIX_ELEMENTS; i++) Rs[i] = scale_down(A[i], sh);
  for (int i = 0; i < MATRIX_SIZE; i++)
    for (int j = 0; j < MATRIX_SIZE; j++)
      Qs[i * MATRIX_SIZE + j] = (i == j) ? (1 << GIVENSQ_Q) : 0;

  for (int j = 0; j < MATRIX_SIZE; j++) {
    for (int i = j + 1; i < MATRIX_SIZE; i++) {
      int32_t opposite = Rs[i * MATRIX_SIZE + j];
      int32_t adjacent = Rs[j * MATRIX_SIZE + j];

      /* ONE instruction replaces calculate_arctan_ratio + cos_fixed +
         sin_fixed, and its result is already in SMLAD's operand format. */
      OPX(givensq_calls);
      int32_t cs = givensq(opposite, adjacent);

      rotate_rows(Rs, cs, i, j);
      Rs[i * MATRIX_SIZE + j] = 0;
      rotate_cols(Qs, cs, i, j);
    }
  }

  for (int i = 0; i < MATRIX_ELEMENTS; i++) R[i] = scale_up(Rs[i], sh);
  for (int i = 0; i < MATRIX_ELEMENTS; i++)
    Q[i] = (int32_t)Qs[i] >> (GIVENSQ_Q - FIXED_FRAC_BITS);
}
