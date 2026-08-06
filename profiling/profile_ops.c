/*
 * Deterministic operation profiler -- variant agnostic.
 *
 * Built once per variant and linked against whichever qr.c is under test, so
 * every implementation is profiled on identical work with identical data.
 *
 * Replaces clock()-based timing, which cannot work on this target: the
 * Cortex-A7 is emulated by QEMU, which has no timing model (see
 * docs/TARGET_PLATFORM.md section 4).
 *
 * Usage: profile_ops [iterations] [magnitude] [--csv]
 */

#include "../src/common/op_counters.h"
#include "../src/common/matrix_f32.h"
#include "../src/common/qr_iface.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Deterministic LCG: identical sequence on every host and every run, so all
   variants are profiled on exactly the same matrices. */
static uint32_t rng_state = 1u;
static int32_t next_entry(int32_t mag) {
  rng_state = rng_state * 1103515245u + 12345u;
  return (int32_t)((rng_state >> 16) % (uint32_t)(2 * mag + 1)) - mag;
}

/* Dedicated, non-recursive, non-inlined toggle targets for callgrind.
 *
 * Toggling on qr_decomposition itself went wrong for naive_float: its
 * qr_decomposition is a wrapper around qr_decomposition_f32, callgrind reported
 * it as recursive (qr_decomposition'2), the collect toggle came unbalanced, and
 * ~560 instructions/QR of printf, malloc and dynamic-linker work leaked into the
 * total. These wrappers are unambiguous and never recursive. */
__attribute__((noinline)) void qr_profiled(const int32_t *A, int32_t *Q,
                                           int32_t *R) {
  qr_decomposition(A, Q, R);
}

#ifdef VARIANT_HAS_F32
/* Measures the float algorithm at its NATURAL interface -- no fixed-point
   conversion, which the naive implementation would never perform. */
__attribute__((noinline)) void qr_profiled_f32(const float *A, float *Q,
                                                float *R) {
  qr_decomposition_f32(A, Q, R);
}

static void workload_qr_f32(int iterations, int32_t mag) {
  float A[MATRIX_ELEMENTS], Q[MATRIX_ELEMENTS], R[MATRIX_ELEMENTS];
  rng_state = 1u;
  for (int t = 0; t < iterations; t++) {
    for (int i = 0; i < MATRIX_ELEMENTS; i++) A[i] = (float)next_entry(mag);
    qr_profiled_f32(A, Q, R);
  }
  volatile float sink = Q[0] + R[0];
  (void)sink;
}
#endif

static void workload_qr(int iterations, int32_t mag) {
  int32_t A[MATRIX_ELEMENTS], Q[MATRIX_ELEMENTS], R[MATRIX_ELEMENTS];
  rng_state = 1u;
  for (int t = 0; t < iterations; t++) {
    for (int i = 0; i < MATRIX_ELEMENTS; i++)
      A[i] = next_entry(mag) * FIXED_SCALE;
    qr_profiled(A, Q, R);
  }
  volatile int32_t sink = Q[0] + R[0];
  (void)sink;
}

static void emit_csv(int iterations, int32_t mag) {
  const op_core_t *c = &g_core;
  printf("variant,iterations,magnitude,mul,mac,divide,shift_add,decisions,"
         "rotations,mul_per_qr,divide_per_qr\n");
  printf("%s,%d,%d,%ld,%ld,%ld,%ld,%ld,%ld,%.2f,%.2f\n", QR_VARIANT_NAME,
         iterations, mag, c->mul, c->mac, c->divide, c->shift_add,
         c->decisions, c->rotations, (double)c->mul / iterations,
         (double)c->divide / iterations);
}

int main(int argc, char **argv) {
  int iterations = 1000;
  int32_t mag = 8;
  int csv = 0;
  int use_f32 = 0;

  /* Positional: [iterations] [magnitude], plus the --csv flag anywhere.
     Count positions explicitly -- an earlier version keyed off "iterations is
     still at its default", which silently let the magnitude argument
     overwrite the iteration count because the default was itself 1000. */
  int positional = 0;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--csv") == 0) {
      csv = 1;
      continue;
    }
    if (strcmp(argv[i], "--float") == 0) {
      use_f32 = 1;
      continue;
    }
    if (positional == 0) iterations = atoi(argv[i]);
    else if (positional == 1) mag = (int32_t)atoi(argv[i]);
    positional++;
  }
  if (iterations <= 0) iterations = 1000;
  if (mag <= 0) mag = 8;

  ops_reset();
#ifdef VARIANT_HAS_F32
  if (use_f32) workload_qr_f32(iterations, mag);
  else workload_qr(iterations, mag);
#else
  if (use_f32) {
    fprintf(stderr, "--float: this variant has no float entry point\n");
    return 2;
  }
  workload_qr(iterations, mag);
#endif

  if (csv) {
    emit_csv(iterations, mag);
    return 0;
  }

  printf("==================================================\n");
  printf(" Deterministic Operation Profile\n");
  printf(" variant: %s\n", QR_VARIANT_NAME);
  printf("==================================================\n");
  printf(" iterations=%d  entry magnitude=+/-%d\n", iterations, mag);
  printf(" Counts are exact and host-independent. Wall-clock time is\n");
  printf(" deliberately NOT reported: QEMU has no timing model, so seconds\n");
  printf(" measured here would describe the host machine.\n");

  ops_report(QR_VARIANT_NAME, "Full QR decomposition", iterations);
  printf("\nDone.\n");
  return 0;
}
