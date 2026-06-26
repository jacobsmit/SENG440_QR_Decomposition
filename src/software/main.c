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

  return 0;
}