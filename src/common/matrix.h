#ifndef MATRIX_H
#define MATRIX_H

#include "fixed.h"
#include <stdint.h>

/* Integer matrix support -- part of the algorithm, so it IS counted by the
   static ARM analysis. Float helpers live in matrix_f32.h and are excluded:
   they are test scaffolding, not code under measurement. */

#define MATRIX_SIZE 4
#define MATRIX_ELEMENTS 16

#define MAT_GET(mat, row, col) ((mat)[(row) * MATRIX_SIZE + (col)])
#define MAT_SET(mat, row, col, val) ((mat)[(row) * MATRIX_SIZE + (col)] = (val))

void init_identity(int32_t *mat);

#endif /* MATRIX_H */
