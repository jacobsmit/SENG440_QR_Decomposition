#include "matrix.h"

void init_identity(int32_t *mat) {
  for (int i = 0; i < MATRIX_SIZE; i++) {
    for (int j = 0; j < MATRIX_SIZE; j++) {
      mat[i * MATRIX_SIZE + j] = (i == j) ? FLOAT_TO_FIXED(1.0f) : 0;
    }
  }
}
