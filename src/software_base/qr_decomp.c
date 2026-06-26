#include "qr_decomp.h"
#include "math_utils.h"

// Apply a Givens rotation to rows i and j of matrix A (mutates A)
static void apply_givens_rotation_A(int32_t *A, int32_t c, int32_t s, int32_t i,
                                    int32_t j) {
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

// Apply a Givens rotation to columns i and j of matrix Q (mutates Q)
static void apply_givens_rotation_Q(int32_t *Q, int32_t c, int32_t s, int32_t i,
                                    int32_t j) {
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
  // Initialize Q as Identity
  init_identity(Q);

  // Initialize R as a copy of A
  for (int i = 0; i < MATRIX_ELEMENTS; i++) {
    R[i] = A[i];
  }

  // Sequentially eliminate elements below the diagonal using Givens rotations
  for (int j = 0; j < MATRIX_SIZE; j++) {
    for (int i = j + 1; i < MATRIX_SIZE; i++) {
      int32_t opposite = MAT_GET(R, i, j);
      int32_t adjacent = MAT_GET(R, j, j);

      // Compute rotation angles
      int32_t angle = calculate_arctan_ratio(opposite, adjacent);
      int32_t c = cos_fixed(angle);
      int32_t s = sin_fixed(angle);

      // Apply the Givens rotation to rows i and j in R
      apply_givens_rotation_A(R, c, s, i, j);

      // Explicitly force the eliminated element to 0
      MAT_SET(R, i, j, 0);

      // Accumulate the rotation in Q (applied to columns i and j)
      apply_givens_rotation_Q(Q, c, s, i, j);
    }
  }
}
