#ifndef COST_MODEL_H
#define COST_MODEL_H

/*
 * Per-operation cycle weights, shared by EVERY variant.
 *
 * These live in one place on purpose. If each variant carried its own weights
 * the cross-variant comparison would be meaningless.
 *
 * !! PLACEHOLDER VALUES !!
 * Replace each with the figure from the ARM Cortex-A7 MPCore Technical
 * Reference Manual (instruction-timing appendix) and cite it in the report.
 * Do not present numbers derived from these as measurements -- they are a
 * model. The point of the model is that it is explicit and auditable.
 */

#define COST_MUL 3               /* TODO(TRM): MUL / SMULL result latency     */
#define COST_MAC 3               /* TODO(TRM): SMLAD / SMUSDX                 */
#define COST_DIV_HW 12           /* TODO(TRM): SDIV, iterative, early-term    */
#define COST_DIV_SOFT 40         /* TODO: measure __aeabi_idiv on target      */
#define COST_SHIFT_ADD 1         /* shift folds into the barrel shifter       */
#define COST_BRANCH 2            /* TODO(TRM): taken branch / mispredict cost */
#define COST_LIBM_TRANSCENDENTAL 100 /* TODO: measure sinf/cosf/atan2f        */

#endif /* COST_MODEL_H */
