/*
 * Accuracy regression suite -- variant agnostic.
 *
 * Built once per variant and linked against whichever qr.c is under test, so
 * every implementation is held to the same invariants on the same data.
 *
 * This is a REGRESSION test: every case has an explicit tolerance and the
 * program exits non-zero if any invariant is violated. The optimisation work
 * ahead (CORDIC, SIMD32, hand assembly, a custom instruction) all changes the
 * arithmetic; without assertions those changes can destroy correctness while
 * the suite still reports success.
 *
 * Tolerances are ~1.5x the measured baseline of the fixed_scalar variant, so
 * ordinary numerical jitter passes but a real regression fails. A variant with
 * genuinely different error characteristics (CORDIC, or the naive float
 * baseline) may need its own numbers -- record them, do not just widen these.
 */

#include "../src/common/qr_iface.h"
#include <math.h>
#include <stdio.h>

/* Q is orthogonal, so every entry must lie in [-1, 1]. A little slack absorbs
   fixed-point rounding.
   NOTE: a sanity check on Q only -- NOT an overflow detector. Measured: when
   FIXED_MUL overflows, the corruption lands in R (the data path) while Q stays
   bounded, because Q is built from c/s which never exceed 1.0. Overflow is
   caught by the RELATIVE reconstruction check instead. */
#define Q_ENTRY_LIMIT 1.05f

typedef struct {
  const char *name;
  float A[MATRIX_ELEMENTS];
  /* Reconstruction is asserted RELATIVE to max|A|, because the error scales
     with input magnitude. An absolute limit would either be vacuous for large
     inputs or unmeetable for small ones. */
  float tol_recon_rel;
  float tol_orth; /* inherently scale-free */
  int expect_fail;
  const char *note;
} test_case_t;

/* --- Randomised integer matrices ---------------------------------------
 *
 * The hand-written cases are not enough on their own. A broken FIXED_DIV was
 * measured at ~124% relative error on random matrices while all four fixed
 * cases still passed -- their particular values never hit the overflow path.
 * Fixed test data is lucky, not representative.
 *
 * These also satisfy the spec requirement for "a square matrix of integers
 * (12-bit wordlength)": the mag=2047 sweep is exactly that.
 *
 * Plain LCG with a fixed seed, so the sequence is identical on every host and
 * every run -- a failure here is always reproducible.
 */

static uint32_t rng_state;

static int32_t rng_entry(int32_t mag) {
  rng_state = rng_state * 1103515245u + 12345u;
  return (int32_t)((rng_state >> 16) % (uint32_t)(2 * mag + 1)) - mag;
}

typedef struct {
  const char *name;
  int32_t mag;
  int n_matrices;
  float tol_recon_rel;
  float tol_orth;
} random_suite_t;

static int run_random_suite(const random_suite_t *rs) {
  rng_state = 12345u;

  float worst_recon_rel = 0.0f, worst_orth = 0.0f;
  int worst_recon_at = -1, worst_orth_at = -1;

  for (int t = 0; t < rs->n_matrices; t++) {
    float A[MATRIX_ELEMENTS];
    int32_t A_fixed[MATRIX_ELEMENTS], Q_fixed[MATRIX_ELEMENTS],
        R_fixed[MATRIX_ELEMENTS];

    for (int i = 0; i < MATRIX_ELEMENTS; i++) {
      int32_t v = rng_entry(rs->mag);
      A[i] = (float)v;
      A_fixed[i] = v * FIXED_SCALE; /* integer-valued: no float rounding */
    }

    qr_decomposition(A_fixed, Q_fixed, R_fixed);

    float Q[MATRIX_ELEMENTS], R[MATRIX_ELEMENTS];
    fixed_to_float_matrix(Q_fixed, Q);
    fixed_to_float_matrix(R_fixed, R);

    float QR[MATRIX_ELEMENTS];
    matrix_multiply_f32(Q, R, QR);
    float scale = matrix_max_abs_f32(A);
    if (scale > 0.0f) {
      float rel = matrix_max_abs_diff_f32(A, QR) / scale;
      if (rel > worst_recon_rel) {
        worst_recon_rel = rel;
        worst_recon_at = t;
      }
    }

    float Q_T[MATRIX_ELEMENTS], QQ_T[MATRIX_ELEMENTS], I[MATRIX_ELEMENTS];
    matrix_transpose_f32(Q, Q_T);
    matrix_multiply_f32(Q, Q_T, QQ_T);
    init_identity_f32(I);
    float orth = matrix_max_abs_diff_f32(I, QQ_T);
    if (orth > worst_orth) {
      worst_orth = orth;
      worst_orth_at = t;
    }
  }

  int failures = 0;
  printf("--- %s ---\n", rs->name);
  printf("    (%d random integer matrices, entries in [-%d, %d], seed 12345)\n",
         rs->n_matrices, rs->mag, rs->mag);

  int ok = (worst_recon_rel <= rs->tol_recon_rel);
  printf("  %-22s %7.2f%% at #%-6d  (limit %5.2f%%)  %s\n", "worst recon rel",
         worst_recon_rel * 100.0f, worst_recon_at, rs->tol_recon_rel * 100.0f,
         ok ? "PASS" : "**FAIL**");
  if (!ok) failures++;

  ok = (worst_orth <= rs->tol_orth);
  printf("  %-22s %7.4f  at #%-6d  (limit %6.4f)  %s\n", "worst orth",
         worst_orth, worst_orth_at, rs->tol_orth, ok ? "PASS" : "**FAIL**");
  if (!ok) failures++;

  printf("\n");
  return failures;
}

/* --- One fixed test case. Returns the number of failed checks. --- */

static int run_accuracy_test(const test_case_t *tc) {
  int failures = 0;
  printf("--- %s ---\n", tc->name);
  if (tc->note) printf("    (%s)\n", tc->note);

  int32_t A_matrix[MATRIX_ELEMENTS], Q_matrix[MATRIX_ELEMENTS],
      R_matrix[MATRIX_ELEMENTS];

  float_to_fixed_matrix(tc->A, A_matrix);
  qr_decomposition(A_matrix, Q_matrix, R_matrix);

  float Q_float[MATRIX_ELEMENTS], R_float[MATRIX_ELEMENTS];
  fixed_to_float_matrix(Q_matrix, Q_float);
  fixed_to_float_matrix(R_matrix, R_float);

  /* 1. Reconstruction: A ~= Q * R */
  float QR_float[MATRIX_ELEMENTS];
  matrix_multiply_f32(Q_float, R_float, QR_float);
  float recon_err = matrix_max_abs_diff_f32(tc->A, QR_float);
  float scale = matrix_max_abs_f32(tc->A);
  float recon_rel = (scale > 0.0f) ? (recon_err / scale) : 0.0f;

  int ok = (recon_rel <= tc->tol_recon_rel);
  printf("  %-22s %8.4f abs = %7.2f%% of max|A|  (limit %5.2f%%)  %s\n",
         "recon |A-QR|", recon_err, recon_rel * 100.0f,
         tc->tol_recon_rel * 100.0f, ok ? "PASS" : "**FAIL**");
  if (!ok) failures++;

  /* 2. Orthogonality: I ~= Q * Q^T */
  float Q_T_float[MATRIX_ELEMENTS], QQ_T_float[MATRIX_ELEMENTS],
      Identity[MATRIX_ELEMENTS];
  matrix_transpose_f32(Q_float, Q_T_float);
  matrix_multiply_f32(Q_float, Q_T_float, QQ_T_float);
  init_identity_f32(Identity);
  float orth_err = matrix_max_abs_diff_f32(Identity, QQ_T_float);
  ok = (orth_err <= tc->tol_orth);
  printf("  %-22s %8.4f%29s(limit %6.4f)  %s\n", "orth |I-QQ^T|", orth_err, "",
         tc->tol_orth, ok ? "PASS" : "**FAIL**");
  if (!ok) failures++;

  /* 3. Q entries bounded. Sanity check on Q, not an overflow detector. */
  float q_max = matrix_max_abs_f32(Q_float);
  ok = (q_max <= Q_ENTRY_LIMIT);
  printf("  %-22s %8.4f%29s(limit %6.4f)  %s\n", "max |Q_ij|", q_max, "",
         (float)Q_ENTRY_LIMIT, ok ? "PASS" : "**FAIL**");
  if (!ok) failures++;

  if (tc->expect_fail) {
    if (failures) {
      printf("  => XFAIL (expected: documents a known, unfixed bug)\n\n");
      return 0;
    }
    printf("  => XPASS !! This case was expected to FAIL but passed.\n");
    printf("     Clear expect_fail so it becomes a real regression test.\n\n");
    return 0;
  }

  printf("\n");
  return failures;
}

int main(void) {
  static const test_case_t cases[] = {
      {"Standard Mixed-Sign Matrix",
       {4.0f, 1.0f, -2.0f, 2.0f, 1.0f, 2.0f, 0.0f, 1.0f, -2.0f, 0.0f, 3.0f,
        -2.0f, 2.0f, 1.0f, -2.0f, 5.0f},
       0.03f, 0.06f, 0, NULL},
      {"Identity Matrix",
       {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f},
       0.01f, 0.01f, 0, NULL},
      {"Symmetric Positive Definite Matrix",
       {5.5f, 2.1f, 0.5f, 1.0f, 2.1f, 4.2f, 1.1f, 0.2f, 0.5f, 1.1f, 3.3f, 1.5f,
        1.0f, 0.2f, 1.5f, 6.0f},
       0.07f, 0.07f, 0, NULL},
      {"Negative Heavy Matrix",
       {-1.5f, -2.1f, 0.5f, -1.0f, -2.1f, -4.2f, -1.1f, 0.2f, 0.5f, -1.1f,
        -3.3f, -1.5f, -1.0f, 0.2f, -1.5f, -6.0f},
       0.05f, 0.07f, 0, NULL},

      /* The symmetric-PD case scaled by 100 (max|A| = 600). A rotation is
         scale-invariant, so the RELATIVE error must match the scale-1 case.
         Regression guard for the FIXED_MUL overflow: with a 32-bit
         intermediate it read 166%, with the 64-bit intermediate 4.74% against
         4.67% at scale 1. If this regresses, someone reintroduced a 32-bit
         product. */
      {"Large Magnitude (SPD x100) -- overflow regression guard",
       {550.0f, 210.0f, 50.0f, 100.0f, 210.0f, 420.0f, 110.0f, 20.0f, 50.0f,
        110.0f, 330.0f, 150.0f, 100.0f, 20.0f, 150.0f, 600.0f},
       0.07f, 0.07f, 0, NULL},
  };
  const int n_cases = (int)(sizeof(cases) / sizeof(cases[0]));

  /* Tolerances from a 4000-matrix measurement per magnitude on fixed_scalar:
     worst relative recon 7.0-10.3%, worst orth 0.084-0.098. Limits sit ~1.4x
     above that -- loose enough for jitter, far tighter than the ~100%+ a
     32-bit overflow produces. */
  static const random_suite_t random_suites[] = {
      {"Random small (max|A| = 4)", 4, 1000, 0.15f, 0.13f},
      {"Random at the old overflow cliff (max|A| = 512)", 512, 1000, 0.15f,
       0.13f},
      {"Random 12-bit integers (max|A| = 2047)  <-- spec requirement", 2047,
       1000, 0.15f, 0.13f},
      {"Random large (max|A| = 65536)", 65536, 1000, 0.15f, 0.13f},
      {"Random near Q11 ceiling (max|A| = 262144)", 262144, 1000, 0.15f, 0.13f},
  };
  const int n_random = (int)(sizeof(random_suites) / sizeof(random_suites[0]));

  printf("==========================================\n");
  printf(" QR Accuracy Regression Suite\n");
  printf(" variant: %s\n", QR_VARIANT_NAME);
  printf("==========================================\n\n");

  int total_failures = 0, failed_cases = 0, xfail_cases = 0;
  for (int i = 0; i < n_cases; i++) {
    int f = run_accuracy_test(&cases[i]);
    if (cases[i].expect_fail) xfail_cases++;
    total_failures += f;
    if (f) failed_cases++;
  }
  for (int i = 0; i < n_random; i++) {
    int f = run_random_suite(&random_suites[i]);
    total_failures += f;
    if (f) failed_cases++;
  }

  printf("==========================================\n");
  printf(" variant: %s\n", QR_VARIANT_NAME);
  if (total_failures == 0) {
    printf(" RESULT: PASS  (%d fixed + %d random cases within tolerance",
           n_cases - xfail_cases, n_random);
    if (xfail_cases) printf(", %d known-bug XFAIL", xfail_cases);
    printf(")\n");
    printf("==========================================\n");
    return 0;
  }
  printf(" RESULT: FAIL  (%d failed check(s) across %d case(s))\n",
         total_failures, failed_cases);
  printf("==========================================\n");
  return 1;
}
