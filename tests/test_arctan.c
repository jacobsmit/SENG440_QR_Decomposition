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

  printf("\nRunning calculate_arctan_ratio tests...\n");
  float ratio_tests_N[] = { 1.5f, -1.5f, 0.5f, -0.5f, 2.0f };
  float ratio_tests_D[] = { 0.5f,  0.5f, 1.5f,  1.5f, 0.0f };
  
  for (int i = 0; i < 5; i++) {
    float true_angle = (ratio_tests_D[i] == 0.0f) ? (PI_OVER_2_FIXED/2048.0f) : atanf(ratio_tests_N[i] / ratio_tests_D[i]);
    
    int32_t N_fixed = FLOAT_TO_FIXED(ratio_tests_N[i]);
    int32_t D_fixed = FLOAT_TO_FIXED(ratio_tests_D[i]);
    
    int32_t fixed_angle = calculate_arctan_ratio(N_fixed, D_fixed);
    
    printf("N: %.1f, D: %.1f (Ratio: %.1f)\n", ratio_tests_N[i], ratio_tests_D[i], (ratio_tests_D[i] == 0.0f) ? 999.9f : ratio_tests_N[i]/ratio_tests_D[i]);
    printf("Standard atan(N/D): %f rad\n", true_angle);
    printf("calculate_arctan_ratio(): %d (Float: %f rad)\n\n", fixed_angle, FIXED_TO_FLOAT(fixed_angle));
  }

  return 0;
}
