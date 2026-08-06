#ifndef MATRIX_H
#define MATRIX_H

#include "fixed.h"
#include <stdint.h>

/*
 * Integer-only matrix support -- part of the algorithm, so it IS included in
 * the static ARM instruction counts.
 *
 * Float conversions, float matrix arithmetic and pretty-printing live in
 * matrix_f32.h and are deliberately excluded from those counts: they are test
 * scaffolding, not the code under measurement.
 */

#define MATRIX_SIZE 4
#define MATRIX_ELEMENTS 16

#define MAT_GET(mat, row, col) ((mat)[(row) * MATRIX_SIZE + (col)])
#define MAT_SET(mat, row, col, val) ((mat)[(row) * MATRIX_SIZE + (col)] = (val))

void init_identity(int32_t *mat);
void init_zero(int32_t *mat);
void print_matrix_raw(const int32_t *mat, const char *name);

#endif /* MATRIX_H */
