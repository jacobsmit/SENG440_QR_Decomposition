#include "matrix_utils.h"
#include <stdio.h>

void init_identity(int32_t* mat) {
    for (int i = 0; i < MATRIX_SIZE; i++) {
        for (int j = 0; j < MATRIX_SIZE; j++) {
            if (i == j) {
                mat[i * MATRIX_SIZE + j] = FLOAT_TO_FIXED(1.0f);
            } else {
                mat[i * MATRIX_SIZE + j] = 0;
            }
        }
    }
}

void init_zero(int32_t* mat) {
    for (int i = 0; i < MATRIX_ELEMENTS; i++) {
        mat[i] = 0;
    }
}

void float_to_fixed_matrix(const float* src, int32_t* dest) {
    for (int i = 0; i < MATRIX_ELEMENTS; i++) {
        dest[i] = FLOAT_TO_FIXED(src[i]);
    }
}

void print_matrix(const int32_t* mat, const char* name) {
    printf("Matrix %s (Converted to Float):\n", name);
    for (int i = 0; i < MATRIX_SIZE; i++) {
        printf("[ ");
        for (int j = 0; j < MATRIX_SIZE; j++) {
            float val = FIXED_TO_FLOAT(mat[i * MATRIX_SIZE + j]);
            printf("%7.3f ", val);
        }
        printf("]\n");
    }
    printf("\n");
}

void print_matrix_raw(const int32_t* mat, const char* name) {
    printf("Matrix %s (Raw Fixed-Point Integers):\n", name);
    for (int i = 0; i < MATRIX_SIZE; i++) {
        printf("[ ");
        for (int j = 0; j < MATRIX_SIZE; j++) {
            printf("%7d ", mat[i * MATRIX_SIZE + j]);
        }
        printf("]\n");
    }
    printf("\n");
}
