#ifndef QR_DECOMP_H
#define QR_DECOMP_H

#include <stdint.h>

// Perform QR decomposition on matrix A.
// A: The input matrix (not modified)
// Q: Output orthogonal matrix (must be pre-allocated 16-element array)
// R: Output upper-triangular matrix (must be pre-allocated 16-element array)
void qr_decomposition(const int32_t *A, int32_t *Q, int32_t *R);

#endif // QR_DECOMP_H
