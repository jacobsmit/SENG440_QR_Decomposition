#include "matrix.h"
#include <math.h>
#include <stdio.h>

void init_identity(int32_t *mat) {
  for (int i = 0; i < MATRIX_SIZE; i++) {
    for (int j = 0; j < MATRIX_SIZE; j++) {
      mat[i * MATRIX_SIZE + j] = (i == j) ? FLOAT_TO_FIXED(1.0f) : 0;
    }
  }
}

void init_zero(int32_t *mat) {
  for (int i = 0; i < MATRIX_ELEMENTS; i++) mat[i] = 0;
}

void float_to_fixed_matrix(const float *src, int32_t *dest) {
  for (int i = 0; i < MATRIX_ELEMENTS; i++) dest[i] = FLOAT_TO_FIXED(src[i]);
}

void fixed_to_float_matrix(const int32_t *src, float *dest) {
  for (int i = 0; i < MATRIX_ELEMENTS; i++) dest[i] = FIXED_TO_FLOAT(src[i]);
}

void print_matrix(const int32_t *mat, const char *name) {
  printf("Matrix %s (Converted to Float):\n", name);
  for (int i = 0; i < MATRIX_SIZE; i++) {
    printf("[ ");
    for (int j = 0; j < MATRIX_SIZE; j++)
      printf("%7.3f ", FIXED_TO_FLOAT(mat[i * MATRIX_SIZE + j]));
    printf("]\n");
  }
  printf("\n");
}

void print_matrix_raw(const int32_t *mat, const char *name) {
  printf("Matrix %s (Raw Fixed-Point Integers):\n", name);
  for (int i = 0; i < MATRIX_SIZE; i++) {
    printf("[ ");
    for (int j = 0; j < MATRIX_SIZE; j++)
      printf("%7d ", mat[i * MATRIX_SIZE + j]);
    printf("]\n");
  }
  printf("\n");
}

/* --- float helpers --- */

void init_identity_f32(float *mat) {
  for (int i = 0; i < MATRIX_SIZE; i++)
    for (int j = 0; j < MATRIX_SIZE; j++)
      mat[i * MATRIX_SIZE + j] = (i == j) ? 1.0f : 0.0f;
}

void matrix_multiply_f32(const float *A, const float *B, float *C) {
  for (int i = 0; i < MATRIX_SIZE; i++) {
    for (int j = 0; j < MATRIX_SIZE; j++) {
      float sum = 0.0f;
      for (int k = 0; k < MATRIX_SIZE; k++)
        sum += A[i * MATRIX_SIZE + k] * B[k * MATRIX_SIZE + j];
      C[i * MATRIX_SIZE + j] = sum;
    }
  }
}

void matrix_transpose_f32(const float *A, float *A_T) {
  for (int i = 0; i < MATRIX_SIZE; i++)
    for (int j = 0; j < MATRIX_SIZE; j++)
      A_T[j * MATRIX_SIZE + i] = A[i * MATRIX_SIZE + j];
}

float matrix_max_abs_f32(const float *A) {
  float m = 0.0f;
  for (int i = 0; i < MATRIX_ELEMENTS; i++) {
    float v = fabsf(A[i]);
    if (v > m) m = v;
  }
  return m;
}

float matrix_max_abs_diff_f32(const float *A, const float *B) {
  float m = 0.0f;
  for (int i = 0; i < MATRIX_ELEMENTS; i++) {
    float d = fabsf(A[i] - B[i]);
    if (d > m) m = d;
  }
  return m;
}
