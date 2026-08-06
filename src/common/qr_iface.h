#ifndef QR_IFACE_H
#define QR_IFACE_H

#include "matrix.h"
#include <stdint.h>

/*
 * The contract every variant implements.
 *
 * Keeping it this narrow is what lets the accuracy suite and the profiler be
 * built once and linked against any variant, so all of them are measured on
 * identical terms.
 */

/* Fixed-point QR: A = Q * R, all matrices MATRIX_SIZE x MATRIX_SIZE in Q11. */
void qr_decomposition(const int32_t *A, int32_t *Q, int32_t *R);

/* Names the build in profiler and test output, so results from different
   variants cannot be confused with each other. */
extern const char *const QR_VARIANT_NAME;

/*
 * Float entry point -- provided ONLY by the naive_float baseline.
 *
 * It gets its own signature rather than being forced through the fixed-point
 * one because it is the reference every speedup is measured against, so it
 * must be written the way a naive implementation actually would be: floats
 * end to end. Billing it for fixed-point conversion it would never do would
 * flatter every other variant.
 *
 * naive_float ALSO implements qr_decomposition() above (converting at the
 * boundary) purely so the shared accuracy suite can validate it.
 */
void qr_decomposition_f32(const float *A, float *Q, float *R);

#endif /* QR_IFACE_H */
