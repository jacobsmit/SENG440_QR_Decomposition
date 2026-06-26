#ifndef FIXED_MATH_H
#define FIXED_MATH_H

#include <stdint.h>

// 1.0 is represented as 2^11 = 2048
#define FIXED_SCALE 2048

// Macros to convert standard floats to your 12-bit integer scale (useful for
// your testbenches)
#define FLOAT_TO_FIXED(f) ((int32_t)((f) * FIXED_SCALE))
#define FIXED_TO_FLOAT(i) ((float)(i) / FIXED_SCALE)

// Fixed-point multiplication
// We must right-shift by 11 after multiplying to prevent the fractional base
// from expanding
#define FIXED_MUL(a, b) (((int32_t)(a) * (int32_t)(b)) >> 11)
#define FIXED_DIV(a, b) (((a) << 11) / (b))

int32_t arctan_fixed(int32_t X);
int32_t sin_fixed(int32_t X);
int32_t cos_fixed(int32_t X);

#define PI_OVER_2_FIXED 3217 // 1.570796 * 2048
int32_t calculate_arctan_ratio(int32_t N, int32_t D);

#endif