#ifndef OP_COUNTERS_H
#define OP_COUNTERS_H

/*
 * Deterministic operation counters, shared by every variant.
 *
 * QEMU has no timing model, so wall-clock time here would measure the host.
 * These counts are exact and host-independent instead.
 *
 * Core counters mean the same thing in every variant so they share one table;
 * extras are variant-specific detail.
 *
 * Loads and stores are NOT counted here -- the compiler decides those, so a
 * C-level counter would be fiction. They come from profiling/arm_profile.sh.
 */

#include <stdint.h>

typedef struct {
  long mul;       /* 32x32 multiplies                                  */
  long mac;       /* fused multiply-accumulate (one SMLAD counts as 1) */
  long divide;
  long shift_add; /* CORDIC's currency                                 */
  long decisions; /* data-dependent branches taken                     */
  long rotations; /* Givens rotations applied (row + column)           */
  long qr_calls;
} op_core_t;

typedef struct {
  /* piecewise-linear trig */
  long call_arctan, call_sin, call_cos, call_atan2;
  long pwl_lookups; /* table-driven PWL: one shift+load+mul+add each */
  long angle_folds, atan2_reciprocal, div_by_zero;
  long cordic_iterations;
  long libm_calls;     /* naive_float: libm transcendental calls */
  long givensq_calls;
} op_extra_t;

extern op_core_t g_core;
extern op_extra_t g_extra;

#define OPC(field) (g_core.field++)
#define OPX(field) (g_extra.field++)

void ops_reset(void);
void ops_report(const char *variant, const char *label, long normalise_by);

#endif /* OP_COUNTERS_H */
