#ifndef OP_COUNTERS_H
#define OP_COUNTERS_H

/*
 * Deterministic operation counters, shared by every variant.
 *
 * WHY NOT WALL-CLOCK TIME:
 *   The target is a Cortex-A7 emulated by QEMU (docs/TARGET_PLATFORM.md).
 *   QEMU's TCG translates ARM basic blocks to host code and runs them at host
 *   speed -- no pipeline model, no cache model, no cycle accounting. So
 *   clock() measures the HOST. Counts of architecturally visible events are
 *   properties of the program, exact and host-independent, so they are a
 *   valid basis for comparing variants.
 *
 * CORE vs EXTRA:
 *   The core counters mean the same thing in every variant, so they can go in
 *   one comparison table: CORDIC reports mul=0, divide=0, shift_add=large,
 *   while the scalar variant reports mul=210, divide=6, shift_add=0. That
 *   explains *why* one is faster, not merely that it is.
 *   Extras are variant-specific detail printed separately.
 *
 * NOT COUNTED HERE:
 *   Loads and stores. The compiler decides those, so a C-level counter would
 *   be fiction. They come from the static disassembly analysis in
 *   profiling/arm_profile.sh instead.
 */

#include <stdint.h>

typedef struct {
  /* --- core: comparable across ALL variants --- */
  long mul;        /* 32x32 multiplies                                  */
  long mac;        /* fused multiply-accumulate (one SMLAD counts as 1) */
  long divide;     /* divisions of any kind                             */
  long shift_add;  /* shift-and-add operations -- CORDIC's currency     */
  long decisions;  /* data-dependent branches taken                     */

  /* --- algorithm level: identical meaning in every variant --- */
  long rotations;  /* Givens rotations applied (row + column)           */
  long qr_calls;   /* qr_decomposition() invocations                    */
} op_core_t;

typedef struct {
  /* piecewise-linear trig (fixed_scalar, fixed_simd32, fixed_asm) */
  long call_arctan;
  long call_sin;
  long call_cos;
  long call_atan2;
  long pwl_seg1;
  long pwl_seg2;
  long angle_folds;
  long atan2_reciprocal;
  long div_by_zero;

  /* CORDIC */
  long cordic_iterations;

  /* naive float baseline: libm transcendental calls */
  long libm_calls;

  /* custom instruction */
  long givensq_calls;
} op_extra_t;

extern op_core_t g_core;
extern op_extra_t g_extra;

#define OPC(field) (g_core.field++)
#define OPX(field) (g_extra.field++)

void ops_reset(void);
void ops_report(const char *variant, const char *label, long normalise_by);

#endif /* OP_COUNTERS_H */
