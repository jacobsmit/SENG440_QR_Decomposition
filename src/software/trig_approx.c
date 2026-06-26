#include "fixed_math.h"
#include <stdint.h>

int32_t arctan_fixed(int32_t X) {

  // Condition 1: 0.5 < x <= 1.0
  if (X > 1024 && X <= 2048) {
    return FIXED_MUL(1319, X) + 291;
  }

  // Condition 2: -0.5 <= x <= 0.5
  else if (X >= -1024 && X <= 1024) {
    return FIXED_MUL(1900, X);
  }

  // Condition 3: -1.0 <= x < -0.5
  else if (X >= -2048 && X < -1024) {
    return FIXED_MUL(1319, X) - 291;
  }

  return 0;
}

int32_t sin_fixed(int32_t X) {
    int32_t abs_X = (X < 0) ? -X : X;
    int32_t result;
    
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
    
    // 4-segment symmetric approximation for bounded domain [-0.786, 0.786]
    if (abs_X <= 768) {          // 0.375
        return FIXED_MUL(-314, abs_X) + 2048; // cos(0) = 1.0
    } else {
        return FIXED_MUL(-1172, abs_X) + 2370;
    }
}