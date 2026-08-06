#include "op_counters.h"
#include "cost_model.h"
#include <stdio.h>

op_core_t g_core;
op_extra_t g_extra;

void ops_reset(void) {
  op_core_t zc = {0};
  op_extra_t ze = {0};
  g_core = zc;
  g_extra = ze;
}

static void row(const char *name, long total, long n) {
  printf("  %-26s %12ld %12.2f\n", name, total, (double)total / n);
}

void ops_report(const char *variant, const char *label, long normalise_by) {
  if (normalise_by <= 0) normalise_by = 1;
  const op_core_t *c = &g_core;
  const op_extra_t *e = &g_extra;

  printf("\n=== %s : %s ===\n", variant, label);
  printf("  (%ld iterations aggregated; second column is per QR)\n\n",
         normalise_by);

  printf("  %-26s %12s %12s\n", "CORE (comparable)", "total", "per QR");
  printf("  %-26s %12s %12s\n", "--------------------------", "-----------",
         "-----------");
  row("multiplies", c->mul, normalise_by);
  row("multiply-accumulates", c->mac, normalise_by);
  row("divisions", c->divide, normalise_by);
  row("shift-adds", c->shift_add, normalise_by);
  row("data-dependent branches", c->decisions, normalise_by);
  row("rotations", c->rotations, normalise_by);
  row("qr_decomposition() calls", c->qr_calls, normalise_by);

  /* Only print the extras that this variant actually used. */
  long any_trig = e->call_atan2 + e->call_sin + e->call_cos + e->call_arctan;
  if (any_trig || e->cordic_iterations || e->libm_calls || e->givensq_calls) {
    printf("\n  %-26s %12s %12s\n", "VARIANT DETAIL", "total", "per QR");
    printf("  %-26s %12s %12s\n", "--------------------------", "-----------",
           "-----------");
    if (any_trig) {
      row("calculate_arctan_ratio()", e->call_atan2, normalise_by);
      row("arctan_fixed()", e->call_arctan, normalise_by);
      row("sin_fixed()", e->call_sin, normalise_by);
      row("cos_fixed()", e->call_cos, normalise_by);
      row("PWL segment 1", e->pwl_seg1, normalise_by);
      row("PWL segment 2", e->pwl_seg2, normalise_by);
      row("angle reductions", e->angle_folds, normalise_by);
      row("atan2 reciprocal path", e->atan2_reciprocal, normalise_by);
      row("divide-by-zero guard", e->div_by_zero, normalise_by);
    }
    if (e->cordic_iterations) row("CORDIC iterations", e->cordic_iterations, normalise_by);
    if (e->libm_calls) row("libm transcendental calls", e->libm_calls, normalise_by);
    if (e->givensq_calls) row("GIVENSQ instructions", e->givensq_calls, normalise_by);
  }

  /* Modelled cost. Weights are shared by every variant (cost_model.h) so the
     comparison is apples to apples. They are a MODEL, not a measurement. */
  double cyc = (double)c->mul * COST_MUL + (double)c->mac * COST_MAC +
               (double)c->divide * COST_DIV_HW +
               (double)c->shift_add * COST_SHIFT_ADD +
               (double)c->decisions * COST_BRANCH;
  double cyc_nodiv = cyc - (double)c->divide * COST_DIV_HW +
                     (double)c->divide * COST_DIV_SOFT;
  double libm = (double)e->libm_calls * COST_LIBM_TRANSCENDENTAL;

  printf("\n  --- modelled cost (PLACEHOLDER weights, see cost_model.h) ---\n");
  printf("  %-40s %12.1f\n", "cycles/QR, hardware SDIV build",
         (cyc + libm) / normalise_by);
  printf("  %-40s %12.1f\n", "cycles/QR, no-SDIV build",
         (cyc_nodiv + libm) / normalise_by);
  printf("  Counted operations only. Loads/stores and loop overhead are NOT\n");
  printf("  included -- take those from the static analysis in arm_profile.sh.\n");
}
