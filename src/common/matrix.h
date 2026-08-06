#ifndef MATRIX_H
#define MATRIX_H

#include "fixed.h"
#include <stdint.h>

#define MATRIX_SIZE 4
#define MATRIX_ELEMENTS 16

#define MAT_GET(mat, row, col) ((mat)[(row) * MATRIX_SIZE + (col)])
#define MAT_SET(mat, row, col, val) ((mat)[(row) * MATRIX_SIZE + (col)] = (val))

void init_identity(int32_t *mat);
void init_zero(int32_t *mat);
void float_to_fixed_matrix(const float *src, int32_t *dest);
void fixed_to_float_matrix(const int32_t *src, float *dest);
void print_matrix(const int32_t *mat, const char *name);
void print_matrix_raw(const int32_t *mat, const char *name);

/* Float helpers, shared by the naive baseline and the test harness. */
void init_identity_f32(float *mat);
void matrix_multiply_f32(const float *A, const float *B, float *C);
void matrix_transpose_f32(const float *A, float *A_T);
float matrix_max_abs_f32(const float *A);
float matrix_max_abs_diff_f32(const float *A, const float *B);

#endif /* MATRIX_H */
