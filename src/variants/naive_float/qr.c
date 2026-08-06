/*
 * Variant: naive_float  -- THE BASELINE
 *
 * The straightforward implementation an engineer writes before thinking about
 * the target: single-precision floats throughout, libm for the transcendental
 * functions. Every speed-up quoted in the report is measured against this.
 *
 * Deliberately mirrors the structure of fixed_scalar (atan2 -> cos/sin ->
 * rotate) rather than the textbook sqrt-based Givens form. That way the
 * comparison isolates ONE variable -- float+libm versus fixed-point+PWL --
 * instead of confounding it with an algorithm change.
 *
 * No optimisation is attempted here on purpose. It is the control.
 */

#include "../../common/qr_iface.h"
#include <math.h>

#ifdef PROFILE_OPS
#include "../../common/op_counters.h"
#else
#define OPC(f) ((void)0)
#define OPX(f) ((void)0)
#endif

const char *const QR_VARIANT_NAME = "naive_float";

static void rotate_rows_f32(float *A, float c, float s, int i, int j) {
  OPC(rotations);
  for (int k = 0; k < MATRIX_SIZE; k++) {
    float ti = A[i * MATRIX_SIZE + k];
    float tj = A[j * MATRIX_SIZE + k];
    A[j * MATRIX_SIZE + k] = c * tj + s * ti;
    A[i * MATRIX_SIZE + k] = c * ti - s * tj;
  }
}

static void rotate_cols_f32(float *Q, float c, float s, int i, int j) {
  OPC(rotations);
  for (int k = 0; k < MATRIX_SIZE; k++) {
    float ti = Q[k * MATRIX_SIZE + i];
    float tj = Q[k * MATRIX_SIZE + j];
    Q[k * MATRIX_SIZE + j] = c * tj + s * ti;
    Q[k * MATRIX_SIZE + i] = c * ti - s * tj;
  }
}

void qr_decomposition_f32(const float *A, float *Q, float *R) {
  OPC(qr_calls);

  init_identity_f32(Q);
  for (int i = 0; i < MATRIX_ELEMENTS; i++) R[i] = A[i];

  for (int j = 0; j < MATRIX_SIZE; j++) {
    for (int i = j + 1; i < MATRIX_SIZE; i++) {
      float opposite = R[i * MATRIX_SIZE + j];
      float adjacent = R[j * MATRIX_SIZE + j];

      /* Three libm transcendental calls per rotation -- the thing the whole
         project exists to get rid of. */
      float angle = atan2f(opposite, adjacent);
      float c = cosf(angle);
      float s = sinf(angle);
      OPX(libm_calls);
      OPX(libm_calls);
      OPX(libm_calls);

      rotate_rows_f32(R, c, s, i, j);
      R[i * MATRIX_SIZE + j] = 0.0f;
      rotate_cols_f32(Q, c, s, i, j);
    }
  }
}

/*
 * Fixed-point wrapper.
 *
 * Exists ONLY so the shared accuracy suite can validate the baseline on the
 * same terms as every other variant. Performance numbers for this variant
 * should come from qr_decomposition_f32() above, which does not pay for these
 * conversions.
 */
void qr_decomposition(const int32_t *A, int32_t *Q, int32_t *R) {
  float Af[MATRIX_ELEMENTS], Qf[MATRIX_ELEMENTS], Rf[MATRIX_ELEMENTS];
  fixed_to_float_matrix(A, Af);
  qr_decomposition_f32(Af, Qf, Rf);
  float_to_fixed_matrix(Qf, Q);
  float_to_fixed_matrix(Rf, R);
}
