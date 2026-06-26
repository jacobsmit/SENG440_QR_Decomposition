#include "../src/software_base/fixed_math.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

int32_t sin_fixed(int32_t X);
int32_t cos_fixed(int32_t X);

int main() {
  float max_err_sin = 0.0f;
  float max_err_cos = 0.0f;

  printf("Running Fixed Point Trig Tests...\n");
  printf("Domain: [-0.786, 0.786] rad\n\n");

  for (float angle = -0.786f; angle <= 0.786f; angle += 0.001f) {
    int32_t fixed_angle = FLOAT_TO_FIXED(angle);

    // Sine
    float true_sin = sinf(angle);
    float approx_sin = FIXED_TO_FLOAT(sin_fixed(fixed_angle));
    float err_sin = fabsf(true_sin - approx_sin);
    if (err_sin > max_err_sin)
      max_err_sin = err_sin;

    // Cosine
    float true_cos = cosf(angle);
    float approx_cos = FIXED_TO_FLOAT(cos_fixed(fixed_angle));
    float err_cos = fabsf(true_cos - approx_cos);
    if (err_cos > max_err_cos)
      max_err_cos = err_cos;
  }

  printf("Results:\n");
  printf("Max Sine Error:   %.4f (%.2f%%)\n", max_err_sin,
         max_err_sin * 100.0f);
  printf("Max Cosine Error: %.4f (%.2f%%)\n", max_err_cos,
         max_err_cos * 100.0f);

  printf("\nSpot Checks:\n");
  int32_t zero = FLOAT_TO_FIXED(0.0f);
  printf("sin(0) = %.4f\n", FIXED_TO_FLOAT(sin_fixed(zero)));
  printf("cos(0) = %.4f\n", FIXED_TO_FLOAT(cos_fixed(zero)));

  int32_t pt5 = FLOAT_TO_FIXED(0.5f);
  printf("sin(0.5) = %.4f (True: %.4f)\n", FIXED_TO_FLOAT(sin_fixed(pt5)),
         sinf(0.5f));
  printf("cos(0.5) = %.4f (True: %.4f)\n", FIXED_TO_FLOAT(cos_fixed(pt5)),
         cosf(0.5f));

  return 0;
}
