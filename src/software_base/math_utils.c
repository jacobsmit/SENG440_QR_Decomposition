#include "math_utils.h"
#include <stdio.h>

// --- Trig Approximations ---
int32_t arctan_fixed(int32_t X) {
    if (X >= 0) {
        if (X <= 1024) {
            return FIXED_MUL(1900, X);
        } else {
            return FIXED_MUL(1319, X) + 291;
        }
    } else {
        if (X >= -1024) {
            return FIXED_MUL(1900, X);
        } else {
            return FIXED_MUL(1319, X) - 291;
        }
    }
}

int32_t sin_fixed(int32_t X) {
    int32_t abs_X = (X < 0) ? -X : X;
    int32_t result;
    
    // Angle Reduction: If angle is > pi/4 (1608 fixed), fold it back using sin(x) = cos(pi/2 - x)
    if (abs_X > 1608) {
        result = cos_fixed(PI_OVER_2_FIXED - abs_X);
        return (X < 0) ? -result : result;
    }
    
    // 3-segment symmetric approximation for bounded domain [-0.786, 0.786]
    if (abs_X <= 1024) {         // 0.5
        result = FIXED_MUL(1984, abs_X);
    } else {
        result = FIXED_MUL(1583, abs_X) + 201;
    }
    
    // Sine is an odd function: sin(-x) = -sin(x)
    return (X < 0) ? -result : result;
}

int32_t cos_fixed(int32_t X) {
    int32_t abs_X = (X < 0) ? -X : X;
    
    // Angle Reduction: If angle is > pi/4 (1608 fixed), fold it back using cos(x) = sin(pi/2 - x)
    if (abs_X > 1608) {
        return sin_fixed(PI_OVER_2_FIXED - abs_X);
    }
    
    // 4-segment symmetric approximation for bounded domain [-0.786, 0.786]
    if (abs_X <= 768) {          // 0.375
        return FIXED_MUL(-314, abs_X) + 2048; // cos(0) = 1.0
    } else {
        return FIXED_MUL(-1172, abs_X) + 2370;
    }
}

int32_t calculate_arctan_ratio(int32_t N, int32_t D) {
    if (D == 0) {
        return (N >= 0) ? PI_OVER_2_FIXED : -PI_OVER_2_FIXED;
    }
    int32_t abs_N = (N < 0) ? -N : N;
    int32_t abs_D = (D < 0) ? -D : D;
    if (abs_N <= abs_D) {
        int32_t x_fixed = FIXED_DIV(N, D);
        return arctan_fixed(x_fixed);
    } else {
        int32_t inv_x_fixed = FIXED_DIV(D, N);
        int32_t angle = arctan_fixed(inv_x_fixed);
        int32_t sign = ((N < 0) ^ (D < 0)) ? -1 : 1;
        if (sign > 0) {
            return PI_OVER_2_FIXED - angle;
        } else {
            return -PI_OVER_2_FIXED - angle;
        }
    }
}

// --- Matrix Utilities ---
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
