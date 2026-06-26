#include "fixed_math.h"
#include "matrix_utils.h"
#include <stdint.h>

void apply_givens_rotation(int32_t *A, int32_t c, int32_t s, int32_t i,
                           int32_t j) {
  // Grab pointers to the start of row i and row j
  // This completely eliminates the integer multiplication inside the loop
  int32_t *row_i = &A[i * MATRIX_SIZE];
  int32_t *row_j = &A[j * MATRIX_SIZE];
  int32_t temp_i;
  int32_t temp_j;
  for (int k = 0; k < MATRIX_SIZE; k++) {
    temp_i = row_i[k];
    temp_j = row_j[k];

    // Pivot row (j) gets the addition
    row_j[k] = FIXED_MUL(c, temp_j) + FIXED_MUL(s, temp_i);

    // Target row (i) gets the subtraction (this zeros out A[i][j])
    row_i[k] = FIXED_MUL(c, temp_i) - FIXED_MUL(s, temp_j);
  }
}

void apply_givens_rotation_Q(int32_t *Q, int32_t c, int32_t s, int32_t i, int32_t j) {
  // Q is updated by multiplying on the RIGHT by G^T.
  // Mathematically, this means we apply the exact same transformation
  // but to the COLUMNS of Q instead of the rows!

  // Since C is row-major, columns are spread out in memory.
  // To avoid slow multiplication in the loop (row * 4 + col), we start pointers 
  // at the top of column i and column j, and jump forward by MATRIX_SIZE each time.
  int32_t *col_i = &Q[i];
  int32_t *col_j = &Q[j];
  
  int32_t temp_i;
  int32_t temp_j;

  for (int k = 0; k < MATRIX_SIZE; k++) {
    temp_i = *col_i;
    temp_j = *col_j;

    // Pivot column (j) gets the addition
    *col_j = FIXED_MUL(c, temp_j) + FIXED_MUL(s, temp_i);

    // Target column (i) gets the subtraction
    *col_i = FIXED_MUL(c, temp_i) - FIXED_MUL(s, temp_j);

    // Jump to the next row in the column
    col_i += MATRIX_SIZE;
    col_j += MATRIX_SIZE;
  }
}
