#include "fixed_math.h"
#include "matrix_utils.h"
#include <stdio.h>

int main() {
  printf("Initializing QR Decomposition Infrastructure...\n\n");

  // Sample 4x4 matrix in floating point format
  float A_float[MATRIX_ELEMENTS] = {4.0f, 1.0f, -2.0f, 2.0f, 1.0f, 2.0f,
                                    0.0f, 1.0f, -2.0f, 0.0f, 3.0f, -2.0f,
                                    2.0f, 1.0f, -2.0f, 5.0f};

  // Statically allocate our fixed-point 1D arrays
  int32_t A_matrix[MATRIX_ELEMENTS];
  int32_t Q_matrix[MATRIX_ELEMENTS];

  // Load initial float values into the fixed point matrix
  float_to_fixed_matrix(A_float, A_matrix);

  // Initialize Q as Identity
  init_identity(Q_matrix);

  // Print them out to verify
  print_matrix(A_matrix, "A (Initial)");
  print_matrix_raw(A_matrix, "A (Initial)");

  print_matrix(Q_matrix, "Q (Identity)");

  // Incremently select elements below the diagonal
  int32_t opposite;
  int32_t adjacent;
  int32_t c;
  int32_t s;

  for (int j = 0; j < MATRIX_SIZE; j++) {
    for (int i = j + 1; i < MATRIX_SIZE; i++) {
      opposite = MAT_GET(A_matrix, i, j);
      adjacent = MAT_GET(A_matrix, j, j);
      c = cos_fixed(calculate_arctan_ratio(opposite, adjacent));
      s = sin_fixed(calculate_arctan_ratio(opposite, adjacent));

      apply_givens_rotation(A_matrix, c, s, i, j);
      apply_givens_rotation_Q(Q_matrix, c, s, i, j);
    }
  }

  print_matrix(A_matrix, "R (Final)");
  print_matrix_raw(A_matrix, "R (Final)");

  print_matrix(Q_matrix, "Q (Final)");
  print_matrix_raw(Q_matrix, "Q (Final)");

  return 0;
}