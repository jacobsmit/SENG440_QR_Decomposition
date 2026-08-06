#ifndef MATRIX_F32_H
#define MATRIX_F32_H

#include "matrix.h"
#include <stdint.h>

/*
 * Float-touching helpers: boundary conversions, float matrix arithmetic, and
 * human-readable printing.
 *
 * Kept OUT of matrix.c on purpose. The static ARM analysis measures only the
 * algorithm (qr.c + trig_pwl.c + matrix.c), so anything in here is excluded
 * from those instruction counts. Without the split, ~500 instructions and 150
 * VFP ops of test scaffolding landed in the "fixed_scalar" totals and made the
 * vfp_ops leak-detector column useless.
 *
 * Used by: the test harness, the profiler, and naive_float's boundary wrapper.
 */

void float_to_fixed_matrix(const float *src, int32_t *dest);
void fixed_to_float_matrix(const int32_t *src, float *dest);
void print_matrix(const int32_t *mat, const char *name);

void init_identity_f32(float *mat);
void matrix_multiply_f32(const float *A, const float *B, float *C);
void matrix_transpose_f32(const float *A, float *A_T);
float matrix_max_abs_f32(const float *A);
float matrix_max_abs_diff_f32(const float *A, const float *B);

#endif /* MATRIX_F32_H */
