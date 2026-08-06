#ifndef TRIG_PWL_H
#define TRIG_PWL_H

#include "fixed.h"
#include <stdint.h>

/* Piecewise-linear approximations of arctan, sin and cos in Q11.
 *
 * Used by fixed_scalar, fixed_simd32 and fixed_asm. The CORDIC variant
 * replaces this entire unit -- that is the point of keeping it separate. */

#define PI_OVER_2_FIXED 3217 /* pi/2 in Q11 */

int32_t arctan_fixed(int32_t X);
int32_t sin_fixed(int32_t X);
int32_t cos_fixed(int32_t X);

/* atan2-style: angle of (N, D) with quadrant and reciprocal folding. */
int32_t calculate_arctan_ratio(int32_t N, int32_t D);

#endif /* TRIG_PWL_H */
