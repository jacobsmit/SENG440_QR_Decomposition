#include "matrix.h"
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
