#include "trig_pwl.h"

#ifdef PROFILE_OPS
#include "op_counters.h"
#else
#define OPC(f) ((void)0)
#define OPX(f) ((void)0)
#endif

int32_t arctan_fixed(int32_t X) {
  OPX(call_arctan);
  OPC(decisions);
  if (X >= 0) {
    if (X <= 1024) {
      OPX(pwl_seg1);
      return FIXED_MUL(1900, X);
    } else {
      OPX(pwl_seg2);
      return FIXED_MUL(1319, X) + 291;
    }
  } else {
    if (X >= -1024) {
      OPX(pwl_seg1);
      return FIXED_MUL(1900, X);
    } else {
      OPX(pwl_seg2);
      return FIXED_MUL(1319, X) - 291;
    }
  }
}

int32_t sin_fixed(int32_t X) {
  OPX(call_sin);
  OPC(decisions);
  int32_t abs_X = (X < 0) ? -X : X;
  int32_t result;

  /* Angle reduction: beyond pi/4 (1608 in Q11), fold via sin(x)=cos(pi/2-x) */
  if (abs_X > 1608) {
    OPX(angle_folds);
    result = cos_fixed(PI_OVER_2_FIXED - abs_X);
    return (X < 0) ? -result : result;
  }

  if (abs_X <= 1024) {
    OPX(pwl_seg1);
    result = FIXED_MUL(1984, abs_X);
  } else {
    OPX(pwl_seg2);
    result = FIXED_MUL(1583, abs_X) + 201;
  }

  /* sine is odd */
  return (X < 0) ? -result : result;
}

int32_t cos_fixed(int32_t X) {
  OPX(call_cos);
  OPC(decisions);
  int32_t abs_X = (X < 0) ? -X : X;

  /* Angle reduction: beyond pi/4, fold via cos(x)=sin(pi/2-x) */
  if (abs_X > 1608) {
    OPX(angle_folds);
    return sin_fixed(PI_OVER_2_FIXED - abs_X);
  }

  if (abs_X <= 768) {
    OPX(pwl_seg1);
    return FIXED_MUL(-314, abs_X) + 2048; /* cos(0) = 1.0 */
  } else {
    OPX(pwl_seg2);
    return FIXED_MUL(-1172, abs_X) + 2370;
  }
}

int32_t calculate_arctan_ratio(int32_t N, int32_t D) {
  OPX(call_atan2);
  OPC(decisions);
  if (D == 0) {
    OPX(div_by_zero);
    return (N >= 0) ? PI_OVER_2_FIXED : -PI_OVER_2_FIXED;
  }
  int32_t abs_N = (N < 0) ? -N : N;
  int32_t abs_D = (D < 0) ? -D : D;
  if (abs_N <= abs_D) {
    return arctan_fixed(FIXED_DIV(N, D));
  } else {
    OPX(atan2_reciprocal);
    int32_t angle = arctan_fixed(FIXED_DIV(D, N));
    int32_t sign = ((N < 0) ^ (D < 0)) ? -1 : 1;
    return (sign > 0) ? (PI_OVER_2_FIXED - angle) : (-PI_OVER_2_FIXED - angle);
  }
}
