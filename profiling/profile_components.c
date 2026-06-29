#include "../src/software_base/math_utils.h"
#include "../src/software_base/qr_decomp.h"
#include <stdio.h>
#include <time.h>

#define NUM_ITERATIONS_MATH 10000000 // 10 Million iterations
#define NUM_ITERATIONS_QR   1000000  // 1 Million iterations

// Volatile sink to prevent the -O3 compiler from optimizing our loops away!
volatile int32_t dummy_sink = 0;

void profile_arctan_ratio() {
    clock_t start = clock();
    int32_t sum = 0;
    for (int i = 0; i < NUM_ITERATIONS_MATH; i++) {
        // Use changing inputs to test branch prediction accurately
        int32_t N = (i % 4096) - 2048;
        int32_t D = ((i * 7) % 4096) - 2048 + 1; 
        sum += calculate_arctan_ratio(N, D);
    }
    dummy_sink = sum;
    clock_t end = clock();
    double cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("[10M ops] calculate_arctan_ratio(): \t%.4f seconds\n", cpu_time_used);
}

void profile_sin_fixed() {
    clock_t start = clock();
    int32_t sum = 0;
    for (int i = 0; i < NUM_ITERATIONS_MATH; i++) {
        int32_t angle = (i % 4096) - 2048;
        sum += sin_fixed(angle);
    }
    dummy_sink = sum;
    clock_t end = clock();
    double cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("[10M ops] sin_fixed():                \t%.4f seconds\n", cpu_time_used);
}

void profile_givens_row_rotation() {
    int32_t A[MATRIX_ELEMENTS] = {1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000, 11000, 12000, 13000, 14000, 15000, 16000};
    clock_t start = clock();

    for (int i = 0; i < NUM_ITERATIONS_MATH * 10; i++) {
        int32_t c = 1800 + (i % 10);
        int32_t s = 500 + (i % 10);
        
        // Exact logic from apply_givens_rotation
        int32_t *row_i = &A[1 * MATRIX_SIZE];
        int32_t *row_j = &A[2 * MATRIX_SIZE];
        int32_t temp_i, temp_j;

        for (int k = 0; k < MATRIX_SIZE; k++) {
            temp_i = row_i[k];
            temp_j = row_j[k];
            row_j[k] = FIXED_MUL(c, temp_j) + FIXED_MUL(s, temp_i);
            row_i[k] = FIXED_MUL(c, temp_i) - FIXED_MUL(s, temp_j);
        }
        dummy_sink = A[MATRIX_ELEMENTS - 1];
    }
    clock_t end = clock();
    double cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("[10M ops] apply_givens_rotation():    \t%.4f seconds\n", cpu_time_used);
}

void profile_givens_col_rotation() {
    int32_t Q[MATRIX_ELEMENTS] = {1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000, 11000, 12000, 13000, 14000, 15000, 16000};
    clock_t start = clock();

    for (int i = 0; i < NUM_ITERATIONS_MATH * 10; i++) {
        int32_t c = 1800 + (i % 10);
        int32_t s = 500 + (i % 10);
        
        // Exact logic from apply_givens_rotation_Q
        int32_t *col_i = &Q[1];
        int32_t *col_j = &Q[2];
        int32_t temp_i, temp_j;

        for (int k = 0; k < MATRIX_SIZE; k++) {
            temp_i = *col_i;
            temp_j = *col_j;
            *col_j = FIXED_MUL(c, temp_j) + FIXED_MUL(s, temp_i);
            *col_i = FIXED_MUL(c, temp_i) - FIXED_MUL(s, temp_j);
            col_i += MATRIX_SIZE;
            col_j += MATRIX_SIZE;
        }
        dummy_sink = Q[MATRIX_ELEMENTS - 1];
    }
    clock_t end = clock();
    double cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("[10M ops] apply_givens_rotation_Q():  \t%.4f seconds\n", cpu_time_used);
}

void profile_full_qr() {
    int32_t A[MATRIX_ELEMENTS] = {
        8192, 2048, -4096, 4096, 
        2048, 4096, 0, 2048,
        -4096, 0, 6144, -4096, 
        4096, 2048, -4096, 10240
    };
    int32_t Q[MATRIX_ELEMENTS];
    int32_t R[MATRIX_ELEMENTS];

    clock_t start = clock();
    for (int i = 0; i < NUM_ITERATIONS_QR; i++) {
        // slightly mutate A to prevent complete caching
        A[0] += (i % 2 == 0) ? 1 : -1;
        qr_decomposition(A, Q, R);
    }
    dummy_sink = Q[0] + R[0];
    clock_t end = clock();
    double cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("[ 1M ops] qr_decomposition() FULL:    \t%.4f seconds\n", cpu_time_used);
}

int main() {
    printf("==========================================\n");
    printf(" QR Decomposition Performance Profiler\n");
    printf("==========================================\n\n");

    profile_arctan_ratio();
    profile_sin_fixed();
    profile_givens_row_rotation();
    profile_givens_col_rotation();
    printf("\n");
    profile_full_qr();
    
    printf("\nProfiling Complete.\n");
    return 0;
}
