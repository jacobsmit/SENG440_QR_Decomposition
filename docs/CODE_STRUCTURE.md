# Structuring the Project for Multiple Optimisation Variants

You are about to add CORDIC, SIMD32, hand-written assembly, and a custom instruction.
Each needs to be **built, tested for accuracy, and profiled on equal terms** so the report can put
them in one table. This is the structure to do that.

---

## The one decision that matters: compile-time variants, not runtime dispatch

The tempting design is a function-pointer table — one binary, switch implementations at runtime.
**Don't.** Three reasons, in order of importance:

1. **It defeats inlining, and measuring inlining is a course requirement.** Lesson 100 step 1
   requires the accelerated operation as a routine, an inline routine, *and* a macro, with
   performance for all three. An indirect call through a pointer cannot be inlined, so the
   comparison becomes impossible to make.
2. **It pollutes the measurement.** The dispatch overhead lands inside the numbers you are
   reporting, and static instruction counts stop describing any real variant because every
   implementation is linked into one binary.
3. **The ASIP variant cannot link at all.** The custom instruction never assembles, so that
   variant must be compile-only. A single binary containing everything can never be built.

So: **one binary per variant**, selected by the build. Each is clean, inlinable, and separately
measurable.

---

## Layout

```
src/
  common/                     shared by every variant
    fixed.h                   Q format, FIXED_MUL/FIXED_DIV, op-counter hooks
    matrix.h  matrix.c        init/convert/print, MAT_GET/MAT_SET
    trig_pwl.h  trig_pwl.c    piecewise-linear arctan/sin/cos (not used by CORDIC)
    qr_iface.h                THE CONTRACT every variant implements
    op_counters.h  op_counters.c
  variants/
    naive_float/    qr.c      math.h, floats -- the baseline every speedup is measured against
    fixed_scalar/   qr.c      current implementation
    fixed_cordic/   qr.c      shift-add rotations, no multiply or divide
    fixed_simd32/   qr.c      SMLAD/SMUSDX rotations
    fixed_asm/      qr.c      hand-written inline assembly inner loops
    fixed_asip/     qr.c      GIVENSQ custom instruction (compile-only + stub build)
tests/
  test_qr_accuracy.c          variant-agnostic: built once per variant
profiling/
  profile_ops.c               variant-agnostic: built once per variant
  arm_profile.sh              loops over variants x flag sets
```

`src/software_base/` becomes `src/variants/fixed_scalar/` plus `src/common/`. The split of
`math_utils` into `fixed.h` / `matrix` / `trig_pwl` matters: it makes visible exactly what CORDIC
replaces (all of `trig_pwl`) versus what SIMD32 replaces (only the rotation inner loop).

---

## The contract

`src/common/qr_iface.h` — keep it narrow, so adding a variant is obvious:

```c
#ifndef QR_IFACE_H
#define QR_IFACE_H
#include <stdint.h>
#include "matrix.h"

/* Every fixed-point variant implements exactly this. */
void qr_decomposition(const int32_t *A, int32_t *Q, int32_t *R);

/* Identifies the build in profiler and test output, so results cannot be
   mislabelled when several variants are run back to back. */
extern const char *const QR_VARIANT_NAME;

#endif
```

The naive float baseline gets a **separate** entry point rather than being forced through the
fixed-point signature:

```c
void qr_decomposition_f32(const float *A, float *Q, float *R);
```

Reason: it is the reference the speedups are measured against, so it should be written the way a
naive implementation actually would be — floats end to end. Wrapping it in fixed-point conversion
would bill it for work it would never do and flatter every other variant.

---

## Build matrix

One `Makefile` at the root driving variants x flag sets:

```make
VARIANTS  := naive_float fixed_scalar fixed_cordic fixed_simd32 fixed_asm fixed_asip
FLAGSETS  := cortex-a7 armv7a-nodiv cortex-a7-O3 cortex-a7-thumb

# build/<variant>/<flagset>/{test,profile,*.o,*.dis}
```

Targets worth having:

| target | does |
|---|---|
| `make test-<variant>` | accuracy suite for one variant |
| `make test-all` | every variant, non-zero exit if any fails |
| `make profile-all` | operation counts for every variant |
| `make static-all` | static instruction counts, variants x flagsets (needs ARM gcc) |
| `make asm VARIANT=fixed_asm` | `-S` listings for inspection |
| `make compare` | all three, into the two CSVs below |

`fixed_asip` is special: `make asm VARIANT=fixed_asip` produces the listing containing `GIVENSQ`
and **stops** — it cannot assemble. A parallel `VARIANT=fixed_asip STUB=1` build swaps in the C
reference model (`givensq_ref`) so the variant can still be run and accuracy-tested. That is the
`#ifdef USE_GIVENSQ_ASM` switch from `NEW_INSTRUCTION_PLAN.md` step 4b.

---

## Making the profiler variant-agnostic

The current counters (`fixed_mul`, `fixed_div`, `call_sin`, ...) are specific to the scalar
implementation. CORDIC has no multiplies at all; SIMD32 has dual-MACs. Counting `FIXED_MUL` would
report zero for CORDIC and tell you nothing.

Split the counters into a **common core** every variant reports, plus per-variant extras:

```c
typedef struct {
  /* core -- comparable across ALL variants */
  long mul;         /* 32x32 multiplies                              */
  long mac;         /* fused multiply-accumulate (SMLAD counts as 1) */
  long divide;      /* divisions of any kind                         */
  long shift_add;   /* shift-and-add ops -- CORDIC's currency        */
  long branch;      /* data-dependent branches                       */
  long mem_r, mem_w;
  /* algorithm level -- identical meaning in every variant */
  long rotations;
  long qr_calls;
} op_core_t;
```

That table is directly comparable: CORDIC shows `mul=0, divide=0, shift_add=large`, the scalar
variant shows `mul=210, divide=6, shift_add=0`. The comparison explains *why* one is faster, not
just that it is.

Variant-specific extras (`cordic_iterations`, `simd_smlad`, ...) go in a separate struct printed
below the core table.

**One cost model, in one place.** Cycle weights (`COST_MUL_CYCLES`, `COST_DIV_CYCLES`, ...) must
live in a single header shared by all variants and be sourced from the Cortex-A7 TRM. If each
variant carries its own weights the comparison is meaningless.

---

## Emit CSV, don't retype numbers

`make compare` runs **all three** measurements — operation counts, accuracy, and static
instruction counts — and writes two files:

```
build/ops.csv       one row per variant
  variant,iterations,magnitude,mul,mac,divide,shift_add,decisions,rotations,
  mul_per_qr,divide_per_qr,worst_recon_rel_pct,worst_orth,failed_checks,result

build/static.csv    one row per variant x flagset
  variant,flagset,total_instr,mul,long_mul,hw_div,soft_idiv,simd32,clz,
  branches,mem_ops,vfp_ops
```

**Two files, not one.** These are genuinely different tables: dynamic counts and accuracy are
per variant, static counts are per variant *and* flagset. Flattening them into one CSV would
duplicate every dynamic row once per flagset.

Accuracy is included on purpose, and `compare` **exits non-zero if any variant fails its
invariants** — a fast wrong answer is not a data point. The CSV is still written on failure, with
`result=FAIL` and the failed-check count, so you can see what broke.

If there is no ARM toolchain on the host, the static half is skipped with a clear message and
`ops.csv` is still produced. Run `make compare` on the VM to fill in `static.csv`.

The report's tables are then generated, not transcribed. With six variants across four flag sets
you would otherwise hand-copy ~200 numbers, and hand-copied numbers are where report errors come
from.

---

## Rules that keep variants comparable

- **Same test vectors, same seed, for every variant.** The seeded LCG already gives this; keep the
  generator in `common/` so no variant can drift.
- **Every variant passes the same accuracy suite** before its performance number is quoted. A fast
  wrong answer is not a data point. Tolerances may legitimately differ for CORDIC (different error
  characteristics) — record the tolerance alongside the result.
- **Profile the same workload.** Same matrix count, same magnitudes.
- **Never compare across flag sets by accident.** Always label both variant and flag set.

---

## Migration status

- [x] `src/common/` created; `math_utils` split into `fixed.h` / `matrix` / `trig_pwl`
- [x] current implementation moved to `src/variants/fixed_scalar/qr.c`
- [x] `qr_iface.h` + `QR_VARIANT_NAME`
- [x] root `Makefile` with the variant x flagset matrix
- [x] `arm_profile.sh` rewritten as the static analyser, looping variants x flagsets
- [x] `naive_float` baseline added (closes TODO 2.1)
- [x] `profiling/profile_components.c` and `run_profiler.sh` deleted (superseded; their
      `[10M ops]` labels were wrong by 10x)
- [x] CSV summary via `make compare`
- [ ] `fixed_cordic`
- [ ] `fixed_simd32`
- [ ] `fixed_asm`
- [ ] `fixed_asip`

Verified behaviour-preserving: the refactored `fixed_scalar` reproduces the pre-refactor
operation counts exactly (209.94 multiplies, 5.94 divisions, 6.00 `calculate_arctan_ratio`,
5.94 `arctan_fixed`, 8.22 `sin_fixed`, 8.22 `cos_fixed` per QR) and identical accuracy.

Adding a variant is now: create `src/variants/<name>/qr.c`, add `<name>` to `VARIANTS`.

## Deviations from the original sketch

- **No `mem_r`/`mem_w` counters.** The compiler decides loads and stores, so a C-level counter
  would be fiction. Memory-operation counts come from the static disassembly analysis instead.
- **Flag sets live in `profiling/flagsets.sh`**, sourced by both `arm_profile.sh` and the
  Makefile, rather than being defined in the Makefile. One copy, not two.
- **Per-variant make targets are generated explicitly**, not with a pattern rule: GNU make
  excludes `.PHONY` targets from implicit-rule matching, so `test-%` silently does nothing once
  `test-<variant>` is `.PHONY`. Confirmed on make 3.81 (what macOS ships).
- **`test-all` depends on the per-variant targets** rather than looping in the shell. A shell
  loop with a failure flag printed "ALL VARIANTS PASS" while sub-makes were failing; depending on
  the targets makes the gate correct by construction.
