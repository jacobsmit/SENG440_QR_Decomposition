#include "math_utils.h"
#include "qr_decomp.h"
#include <stdio.h>

int main() {
  printf("Starting 4x4 QR Decomposition...\n\n");

  // Sample 4x4 matrix in floating point format
  float A_float[MATRIX_ELEMENTS] = {4.0f, 1.0f, -2.0f, 2.0f, 1.0f, 2.0f,
                                    0.0f, 1.0f, -2.0f, 0.0f, 3.0f, -2.0f,
                                    2.0f, 1.0f, -2.0f, 5.0f};

  // Statically allocate fixed-point 1D arrays
  int32_t A_matrix[MATRIX_ELEMENTS];
  int32_t Q_matrix[MATRIX_ELEMENTS];
  int32_t R_matrix[MATRIX_ELEMENTS];

  // Load initial float values into the fixed point matrix A
  float_to_fixed_matrix(A_float, A_matrix);

  // Print Original Matrix
  print_matrix(A_matrix, "A (Original Input)");

  // Run the fixed-point QR Decomposition
  qr_decomposition(A_matrix, Q_matrix, R_matrix);

  // Print Results
  print_matrix(R_matrix, "R (Final Upper Triangular)");
  print_matrix(Q_matrix, "Q (Final Orthogonal)");

  return 0;
}