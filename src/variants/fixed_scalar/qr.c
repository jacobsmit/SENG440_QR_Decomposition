/*
 * Variant: fixed_scalar
 *
 * Q11 fixed-point Givens rotations with piecewise-linear trig. This is the
 * reference optimised implementation that later variants are compared to.
 */

#include "../../common/qr_iface.h"
#include "../../common/trig_pwl.h"

#ifdef PROFILE_OPS
#include "../../common/op_counters.h"
#else
#define OPC(f) ((void)0)
#define OPX(f) ((void)0)
#endif

const char *const QR_VARIANT_NAME = "fixed_scalar";

/* Apply a Givens rotation to rows i and j of A (mutates A). */
static void apply_givens_rotation_A(int32_t *A, int32_t c, int32_t s, int32_t i,
                                    int32_t j) {
  OPC(rotations);
  int32_t *row_i = &A[i * MATRIX_SIZE];
  int32_t *row_j = &A[j * MATRIX_SIZE];
  int32_t temp_i, temp_j;

  for (int k = 0; k < MATRIX_SIZE; k++) {
    temp_i = row_i[k];
    temp_j = row_j[k];
    row_j[k] = FIXED_MUL(c, temp_j) + FIXED_MUL(s, temp_i);
    row_i[k] = FIXED_MUL(c, temp_i) - FIXED_MUL(s, temp_j);
  }
}

/* Apply a Givens rotation to columns i and j of Q (mutates Q). */
static void apply_givens_rotation_Q(int32_t *Q, int32_t c, int32_t s, int32_t i,
                                    int32_t j) {
  OPC(rotations);
  int32_t *col_i = &Q[i];
  int32_t *col_j = &Q[j];
  int32_t temp_i, temp_j;

  for (int k = 0; k < MATRIX_SIZE; k++) {
    temp_i = *col_i;
    temp_j = *col_j;
    *col_j = FIXED_MUL(c, temp_j) + FIXED_MUL(s, temp_i);
    *col_i = FIXED_MUL(c, temp_i) - FIXED_MUL(s, temp_j);
    col_i += MATRIX_SIZE;
    col_j += MATRIX_SIZE;
  }
}

void qr_decomposition(const int32_t *A, int32_t *Q, int32_t *R) {
  OPC(qr_calls);

  init_identity(Q);
  for (int i = 0; i < MATRIX_ELEMENTS; i++) R[i] = A[i];

  /* Eliminate below the diagonal with Givens rotations. */
  for (int j = 0; j < MATRIX_SIZE; j++) {
    for (int i = j + 1; i < MATRIX_SIZE; i++) {
      int32_t opposite = MAT_GET(R, i, j);
      int32_t adjacent = MAT_GET(R, j, j);

      int32_t angle = calculate_arctan_ratio(opposite, adjacent);
      int32_t c = cos_fixed(angle);
      int32_t s = sin_fixed(angle);

      apply_givens_rotation_A(R, c, s, i, j);
      MAT_SET(R, i, j, 0); /* force the eliminated element to exactly 0 */
      apply_givens_rotation_Q(Q, c, s, i, j);
    }
  }
}
