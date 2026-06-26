#ifndef MATRIX_UTILS_H
#define MATRIX_UTILS_H

#include "fixed_math.h"
#include <stdint.h>

#define MATRIX_SIZE 4
#define MATRIX_ELEMENTS 16

// Helper macros to access 1D array as a 2D matrix (row-major)
#define MAT_GET(mat, row, col) ((mat)[(row) * MATRIX_SIZE + (col)])
#define MAT_SET(mat, row, col, val) ((mat)[(row) * MATRIX_SIZE + (col)] = (val))

// Initialize a 1D matrix as an Identity Matrix
void init_identity(int32_t* mat);

// Initialize a 1D matrix with all zeros
void init_zero(int32_t* mat);

// Convert a standard float array (16 elements) into a fixed-point matrix
void float_to_fixed_matrix(const float* src, int32_t* dest);

// Convert a fixed-point matrix back to float and print it
void print_matrix(const int32_t* mat, const char* name);

// Print the raw integer values of the matrix (for debugging fixed-point logic)
void print_matrix_raw(const int32_t* mat, const char* name);

#endif // MATRIX_UTILS_H
