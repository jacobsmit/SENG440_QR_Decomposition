#include "../src/software/fixed_math.h"
#include <math.h>
#include <stdio.h>

int32_t arctan_fixed(int32_t X);

int main() {
  printf("Running Fixed Point Arctan Tests...\n\n");
  
  // Test values: 0.75, 0.25, -0.75
  float test_floats[] = {0.75f, 0.25f, -0.75f};

  for (int i = 0; i < 3; i++) {
    // 1. Calculate ground truth using standard floating point
    float true_angle = atanf(test_floats[i]);

    // 2. Convert to fixed point and run through your custom routine
    int32_t fixed_input = FLOAT_TO_FIXED(test_floats[i]);
    int32_t fixed_angle = arctan_fixed(fixed_input);

    printf("Input: %f (Fixed: %d)\n", test_floats[i], fixed_input);
    printf("Standard atan(): %f rad\n", true_angle);

    // Print the raw integer, AND convert it back to float for comparison
    printf("Custom fixed_arctan(): %d (Float equivalent: %f rad)\n\n",
           fixed_angle, FIXED_TO_FLOAT(fixed_angle));
  }

  return 0;
}
