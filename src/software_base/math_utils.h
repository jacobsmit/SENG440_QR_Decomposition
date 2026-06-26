#ifndef MATH_UTILS_H
#define MATH_UTILS_H

#include <stdint.h>

// --- Fixed Math Core ---
#define FIXED_SCALE 2048
#define FLOAT_TO_FIXED(f) ((int32_t)((f) * FIXED_SCALE))
#define FIXED_TO_FLOAT(i) ((float)(i) / FIXED_SCALE)

#define FIXED_MUL(a, b) (((int32_t)(a) * (int32_t)(b)) >> 11)
#define FIXED_DIV(a, b) (((a) << 11) / (b))

#define PI_OVER_2_FIXED 3217

int32_t arctan_fixed(int32_t X);
int32_t sin_fixed(int32_t X);
int32_t cos_fixed(int32_t X);
int32_t calculate_arctan_ratio(int32_t N, int32_t D);

// --- Matrix Infrastructure ---
#define MATRIX_SIZE 4
#define MATRIX_ELEMENTS 16

#define MAT_GET(mat, row, col) ((mat)[(row) * MATRIX_SIZE + (col)])
#define MAT_SET(mat, row, col, val) ((mat)[(row) * MATRIX_SIZE + (col)] = (val))

void init_identity(int32_t* mat);
void init_zero(int32_t* mat);
void float_to_fixed_matrix(const float* src, int32_t* dest);
void print_matrix(const int32_t* mat, const char* name);
void print_matrix_raw(const int32_t* mat, const char* name);

#endif // MATH_UTILS_H
