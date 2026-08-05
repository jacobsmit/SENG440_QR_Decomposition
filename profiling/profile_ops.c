/*
 * Deterministic operation profiler for the fixed-point QR decomposition.
 *
 * Replaces the clock()-based profiler, which cannot work on this project's
 * target: the Cortex-A7 is emulated by QEMU, which has no timing model (see
 * docs/TARGET_PLATFORM.md section 4). This profiler counts architecturally
 * visible events instead. Those counts are exact, deterministic and identical
 * on any host, so they are a valid basis for before/after comparison.
 *
 * Build:  see profiling/arm_profile.sh
 */

#include "../src/software_base/math_utils.h"
#include "../src/software_base/qr_decomp.h"
#include "op_counters.h"
#include <stdio.h>
#include <stdlib.h>

op_counts_t g_ops;

void ops_reset(void) {
  op_counts_t zero = {0};
  g_ops = zero;
}

/* ---- Cortex-A7 cost model -------------------------------------------------
 * PLACEHOLDER VALUES. Replace each with the figure from the ARM Cortex-A7
 * MPCore Technical Reference Manual (instruction-timing appendix) and cite it
 * in the report. Do not present these numbers as measured -- they are a model.
 * The point of the model is that it is explicit and auditable.
 * ------------------------------------------------------------------------- */
#define COST_MUL_CYCLES 3   /* TODO(TRM): MUL / SMULL result latency        */
#define COST_DIV_CYCLES 12  /* TODO(TRM): SDIV is iterative, early-terminating; */
                            /*            ~4-20 depending on operand size  */
#define COST_DIV_SOFT 40    /* TODO: measure __aeabi_idiv on the target     */
#define COST_BRANCH_MISS 8  /* TODO(TRM): pipeline depth on mispredict      */

void ops_report(const char *label, long normalise_by) {
  const op_counts_t *o = &g_ops;
  printf("\n=== %s ===\n", label);
  if (normalise_by <= 0) normalise_by = 1;
  printf("  (per QR decomposition; %ld iterations aggregated)\n\n", normalise_by);

  printf("  %-28s %12s %12s\n", "event", "total", "per QR");
  printf("  %-28s %12s %12s\n", "----------------------------", "-----------",
         "-----------");
#define ROW(name, field)                                                       \
  printf("  %-28s %12ld %12.2f\n", name, o->field,                             \
         (double)o->field / normalise_by)
  ROW("FIXED_MUL", fixed_mul);
  ROW("FIXED_DIV", fixed_div);
  printf("\n");
  ROW("calculate_arctan_ratio()", call_atan2);
  ROW("arctan_fixed()", call_arctan);
  ROW("sin_fixed()", call_sin);
  ROW("cos_fixed()", call_cos);
  printf("\n");
  ROW("PWL segment 1 taken", pwl_seg1);
  ROW("PWL segment 2 taken", pwl_seg2);
  ROW("angle reductions", angle_folds);
  ROW("atan2 reciprocal path", atan2_reciprocal);
  ROW("divide-by-zero guard", div_by_zero);
  printf("\n");
  ROW("row rotations", rotations_row);
  ROW("col rotations", rotations_col);
  ROW("qr_decomposition()", qr_calls);
#undef ROW

  /* Model-based cycle estimate. Explicit, auditable, and clearly a model. */
  double mul_c = (double)o->fixed_mul * COST_MUL_CYCLES;
  double div_hw = (double)o->fixed_div * COST_DIV_CYCLES;
  double div_sw = (double)o->fixed_div * COST_DIV_SOFT;
  printf("\n  --- modelled cost (PLACEHOLDER latencies, see source) ---\n");
  printf("  %-40s %12.0f\n", "multiply cycles", mul_c);
  printf("  %-40s %12.0f\n", "divide cycles (hardware SDIV)", div_hw);
  printf("  %-40s %12.0f\n", "divide cycles (__aeabi_idiv)", div_sw);
  printf("  %-40s %12.1f\n", "arith cycles/QR (SDIV build)",
         (mul_c + div_hw) / normalise_by);
  printf("  %-40s %12.1f\n", "arith cycles/QR (no-SDIV build)",
         (mul_c + div_sw) / normalise_by);
  printf("  NOTE: multiply+divide only. Loads/stores, branches and loop\n");
  printf("        overhead are NOT in this figure -- take those from the\n");
  printf("        static instruction counts (arm_profile.sh).\n");
}

/* --------------------------------------------------------------------------
 * Workloads
 * -------------------------------------------------------------------------- */

/* Deterministic LCG so every run and every host produces identical input. */
static uint32_t rng_state = 1u;
static int32_t next_entry(int mag) {
  rng_state = rng_state * 1103515245u + 12345u;
  return (int32_t)((rng_state >> 16) % (uint32_t)(2 * mag + 1)) - mag;
}

static void workload_qr(int iterations, int mag) {
  int32_t A[MATRIX_ELEMENTS], Q[MATRIX_ELEMENTS], R[MATRIX_ELEMENTS];
  rng_state = 1u;
  for (int t = 0; t < iterations; t++) {
    for (int i = 0; i < MATRIX_ELEMENTS; i++)
      A[i] = next_entry(mag) * FIXED_SCALE;
    qr_decomposition(A, Q, R);
  }
  /* consume outputs so nothing is optimised away */
  volatile int32_t sink = Q[0] + R[0];
  (void)sink;
}

int main(int argc, char **argv) {
  int iterations = (argc > 1) ? atoi(argv[1]) : 1000;
  int mag = (argc > 2) ? atoi(argv[2]) : 8;

  printf("==================================================\n");
  printf(" QR Decomposition -- Deterministic Operation Profile\n");
  printf("==================================================\n");
  printf(" iterations=%d  entry magnitude=+/-%d\n", iterations, mag);
  printf(" Counts are exact and host-independent.\n");
  printf(" Wall-clock time is deliberately NOT reported: QEMU has no\n");
  printf(" timing model, so seconds measured here describe the host.\n");

  ops_reset();
  workload_qr(iterations, mag);
  ops_report("Full QR decomposition", iterations);

  printf("\nDone.\n");
  return 0;
}
