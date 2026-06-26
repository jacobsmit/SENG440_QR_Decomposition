#include "../src/software_base/math_utils.h"
#include "../src/software_base/qr_decomp.h"
#include <math.h>
#include <stdio.h>

// --- Float Matrix Utilities for Testing ---

void matrix_multiply_float(const float *A, const float *B, float *C) {
  for (int i = 0; i < MATRIX_SIZE; i++) {
    for (int j = 0; j < MATRIX_SIZE; j++) {
      float sum = 0.0f;
      for (int k = 0; k < MATRIX_SIZE; k++) {
        sum += A[i * MATRIX_SIZE + k] * B[k * MATRIX_SIZE + j];
      }
      C[i * MATRIX_SIZE + j] = sum;
    }
  }
}

void matrix_transpose_float(const float *A, float *A_T) {
  for (int i = 0; i < MATRIX_SIZE; i++) {
    for (int j = 0; j < MATRIX_SIZE; j++) {
      A_T[j * MATRIX_SIZE + i] = A[i * MATRIX_SIZE + j];
    }
  }
}

float compute_max_absolute_error(const float *A, const float *B) {
  float max_err = 0.0f;
  for (int i = 0; i < MATRIX_ELEMENTS; i++) {
    float err = fabsf(A[i] - B[i]);
    if (err > max_err) {
      max_err = err;
    }
  }
  return max_err;
}

void fixed_to_float_matrix(const int32_t *src, float *dest) {
  for (int i = 0; i < MATRIX_ELEMENTS; i++) {
    dest[i] = FIXED_TO_FLOAT(src[i]);
  }
}

// --- Test Framework ---

void run_accuracy_test(const float *A_float, const char *test_name) {
  printf("--- Running Test: %s ---\n", test_name);

  int32_t A_matrix[MATRIX_ELEMENTS];
  int32_t Q_matrix[MATRIX_ELEMENTS];
  int32_t R_matrix[MATRIX_ELEMENTS];

  float_to_fixed_matrix(A_float, A_matrix);

  // Run the embedded QR algorithm
  qr_decomposition(A_matrix, Q_matrix, R_matrix);

  // Convert back to float for analysis
  float Q_float[MATRIX_ELEMENTS];
  float R_float[MATRIX_ELEMENTS];
  fixed_to_float_matrix(Q_matrix, Q_float);
  fixed_to_float_matrix(R_matrix, R_float);

  // 1. Reconstruction Test: A ≈ Q * R
  float QR_float[MATRIX_ELEMENTS];
  matrix_multiply_float(Q_float, R_float, QR_float);
  float recon_err = compute_max_absolute_error(A_float, QR_float);
  printf("Reconstruction Max Abs Error (A = QR): %.4f\n", recon_err);

  // 2. Orthogonality Test: I ≈ Q * Q^T
  float Q_T_float[MATRIX_ELEMENTS];
  float QQ_T_float[MATRIX_ELEMENTS];
  matrix_transpose_float(Q_float, Q_T_float);
  matrix_multiply_float(Q_float, Q_T_float, QQ_T_float);

  float Identity[MATRIX_ELEMENTS] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
                                     0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
                                     0.0f, 0.0f, 0.0f, 1.0f};
  float orth_err = compute_max_absolute_error(Identity, QQ_T_float);
  printf("Orthogonality Max Abs Error (I = Q*Q^T): %.4f\n\n", orth_err);
}

int main() {
  printf("==========================================\n");
  printf(" QR Decomposition Accuracy Test Suite\n");
  printf("==========================================\n\n");

  float test1[MATRIX_ELEMENTS] = {4.0f, 1.0f, -2.0f, 2.0f, 1.0f, 2.0f,
                                  0.0f, 1.0f, -2.0f, 0.0f, 3.0f, -2.0f,
                                  2.0f, 1.0f, -2.0f, 5.0f};
  run_accuracy_test(test1, "Standard Mixed-Sign Matrix");

  float test2[MATRIX_ELEMENTS] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
                                  0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
                                  0.0f, 0.0f, 0.0f, 1.0f};
  run_accuracy_test(test2, "Identity Matrix");

  float test3[MATRIX_ELEMENTS] = {5.5f, 2.1f, 0.5f, 1.0f, 2.1f, 4.2f,
                                  1.1f, 0.2f, 0.5f, 1.1f, 3.3f, 1.5f,
                                  1.0f, 0.2f, 1.5f, 6.0f};
  run_accuracy_test(test3, "Symmetric Positive Definite Matrix");

  float test4[MATRIX_ELEMENTS] = {-1.5f, -2.1f, 0.5f,  -1.0f, -2.1f, -4.2f,
                                  -1.1f, 0.2f,  0.5f,  -1.1f, -3.3f, -1.5f,
                                  -1.0f, 0.2f,  -1.5f, -6.0f};
  run_accuracy_test(test4, "Negative Heavy Matrix");

  return 0;
}
