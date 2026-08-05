#ifndef OP_COUNTERS_H
#define OP_COUNTERS_H

/*
 * Deterministic operation counters.
 *
 * WHY NOT WALL-CLOCK TIME:
 *   The target is a Cortex-A7 emulated by QEMU (see docs/TARGET_PLATFORM.md).
 *   QEMU's TCG translates ARM basic blocks to host code and runs them at host
 *   speed. There is no pipeline model, no cache model and no cycle accounting,
 *   so clock()/gettimeofday() measure the HOST, not the A7. Any speed-up
 *   derived from them is meaningless.
 *
 * WHAT IS VALID:
 *   Counts of architecturally-visible events are properties of the program, not
 *   of the emulator. They are exact, deterministic, host-independent and
 *   directly comparable across optimisation steps. Combined with a per-operation
 *   cost table taken from the Cortex-A7 TRM, they give a defensible cycle
 *   estimate -- which is what the course asks for ("computed manually, since no
 *   simulator is yet available", Lesson 100).
 *
 * USAGE:
 *   Compile the code under test with -DPROFILE_OPS. math_utils.h then routes
 *   FIXED_MUL / FIXED_DIV through counting wrappers that preserve the exact
 *   arithmetic of the production macros. Link against profile_ops.c, which
 *   defines the storage.
 */

#include <stdint.h>

typedef struct {
    /* primitive fixed-point operations */
    long fixed_mul;          /* FIXED_MUL invocations                        */
    long fixed_div;          /* FIXED_DIV invocations -> SDIV or __aeabi_idiv */

    /* routine invocations */
    long call_arctan;        /* arctan_fixed()                              */
    long call_sin;           /* sin_fixed()                                 */
    long call_cos;           /* cos_fixed()                                 */
    long call_atan2;         /* calculate_arctan_ratio()                    */

    /* branch-path taken inside the piecewise-linear approximations.
       These matter on an in-order core: a mispredict stalls the pipeline. */
    long pwl_seg1;           /* first  linear segment selected              */
    long pwl_seg2;           /* second linear segment selected              */
    long angle_folds;        /* sin/cos angle-reduction path taken          */
    long atan2_reciprocal;   /* |N|>|D| path (needs the pi/2 - x fixup)     */
    long div_by_zero;        /* D == 0 guard hit                            */

    /* algorithm-level */
    long rotations_row;      /* apply_givens_rotation_A() calls             */
    long rotations_col;      /* apply_givens_rotation_Q() calls             */
    long qr_calls;           /* qr_decomposition() calls                    */
} op_counts_t;

extern op_counts_t g_ops;

void ops_reset(void);
void ops_report(const char *label, long normalise_by);

#endif /* OP_COUNTERS_H */
