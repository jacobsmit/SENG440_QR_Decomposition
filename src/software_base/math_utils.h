#ifndef MATH_UTILS_H
#define MATH_UTILS_H

#include <stdint.h>

// --- Fixed Math Core ---
#define FIXED_SCALE 2048
#define FLOAT_TO_FIXED(f) ((int32_t)((f) * FIXED_SCALE))
#define FIXED_TO_FLOAT(i) ((float)(i) / FIXED_SCALE)

/* Fixed-point multiply and divide.
 *
 * Both compute the intermediate in 64 bits before scaling back to 32. The
 * ANSWER always fits in 32 bits; the intermediate does not. Multiplying two
 * Q11 values scales the result by 2048 twice, so the product is the answer
 * times 4 million -- that overflows int32 once a stored value exceeds
 * 2^31/2^11 = 2^20, i.e. a real value of only 512.0. Doing the multiply in
 * 64 bits and shifting before narrowing raises that ceiling to the actual
 * limit of the format (+/-2^20 as a real value).
 *
 * This costs nothing exotic on ARM: 32x32->64 is a single SMULL instruction
 * writing a register pair, present since ARMv4. The datapath stays 32-bit.
 */
static inline int32_t fixed_mul(int32_t a, int32_t b) {
  /* (int64_t)a promotes the multiply to 64 bits, so the product has room.
     >> 11 removes the doubled scale factor. The cast back to int32_t is safe
     because the scaled-down result always fits. */
  return (int32_t)(((int64_t)a * b) >> 11);
}

static inline int32_t fixed_div(int32_t a, int32_t b) {
  /* Same overflow, different shape: a << 11 blows past 32 bits once |a|
     exceeds 2^20. Doing it in 64 bits would work, but ARM has no 64/32 divide
     instruction, so that turns one SDIV into an __aeabi_ldiv library call.
     Instead, shift BOTH operands down by the same amount -- the quotient a/b
     is unchanged by scaling -- until the numerator's << 11 fits. Keeps the
     Cortex-A7's hardware SDIV. Runs 0-3 times for realistic inputs. */
  int32_t aa = (a < 0) ? -a : a;
  while (aa >= (1 << 20)) {
    a >>= 1;
    b >>= 1;
    aa >>= 1;
  }
  return (a << 11) / b;
}

/* Optional deterministic operation profiling (see profiling/op_counters.h).
   Both builds call the same fixed_mul/fixed_div above, so the instrumented
   build cannot drift from production arithmetic. */
#ifdef PROFILE_OPS
#include "../../profiling/op_counters.h"
#define OPCOUNT(field) (g_ops.field++)
#define FIXED_MUL(a, b) (OPCOUNT(fixed_mul), fixed_mul((a), (b)))
#define FIXED_DIV(a, b) (OPCOUNT(fixed_div), fixed_div((a), (b)))
#else
#define OPCOUNT(field) ((void)0)
#define FIXED_MUL(a, b) fixed_mul((a), (b))
#define FIXED_DIV(a, b) fixed_div((a), (b))
#endif

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
