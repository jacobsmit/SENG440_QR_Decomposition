# Implementation Plan: `fixed_simd32`

## Why this variant, and why now

The callgrind breakdown says the rotations, not the trig, are where the time goes — and worse,
that fixed-point is currently **losing** to naive float there:

| per QR (instructions) | naive_float | fixed_scalar |
|---|---:|---:|
| trig / angle | 1144.8 | **910.5** |
| rotations + loops | **1074.2** | 1372.0 |

The PWL trig works (saves 234). The Q11 rotations give it all back (+298). The cause is visible in
the generated code: every Q11 multiply is three instructions.

```
smull ip, r1, r0, r3      @ 32x32 -> 64
lsr   ip, ip, #11         @ }
orr   ip, ip, r1, lsl #21 @ } reassemble the 64-bit result
```

**8 of the 20 instructions in the rotation inner loop are just `lsr`/`orr` rejoining register
pairs** — 384 instructions per QR of pure overhead. SIMD32 deletes that entirely: `SMLAD` takes
16-bit operands and produces a 32-bit result with no reassembly.

---

## The instruction pair, and why it fits perfectly

A Givens rotation updates two elements from the same two inputs:

```c
row_j[k] = c*tj + s*ti;
row_i[k] = c*ti - s*tj;
```

Pack the coefficients as `cs = (s:c)` and the data as `t = (ti:tj)` (high:low halfwords). Then:

| instruction | computes | gives us |
|---|---|---|
| `SMLAD  Rd, cs, t, 0` | `cs.lo*t.lo + cs.hi*t.hi` | `c*tj + s*ti` → `row_j[k]` |
| `SMUSDX Rd, cs, t` | `cs.lo*t.hi − cs.hi*t.lo` | `c*ti − s*tj` → `row_i[k]` |

Both outputs, two instructions, from the same two registers. Verified on Cortex-A7 via
`arm_acle.h` — gcc emits `smlad` and `smusdx` exactly as intended.

**gcc will not generate these from scalar C** (checked: it emits plain `mul`). They require
`__smlad()` / `__smusdx()` intrinsics or inline asm, which makes this a genuine hand-optimisation
contribution rather than a compiler flag.

---

## The blocking constraint: operands must be 16-bit

`SMLAD` reads signed 16-bit halves. That is fine for the coefficients but **not** for the data:

| value | range | bits needed | fits int16? |
|---|---|---:|---|
| `c`, `s` (Q11) | ≤ 1.0 → ≤ 2048 | 13 | yes |
| matrix element, 12-bit input at Q11 | ±2047 × 2048 | 23 | **no** |

So the storage format must change. This is the co-design decision the variant is really about, and
it deserves its own report section.

### Recommended: block floating point

Rather than fixing a coarse Q format, **scale the whole matrix once per decomposition**:

1. Find `max|A|`, compute `sh = clz`-derived shift so the largest element lands near `2^14`.
2. Store all elements as `int16_t` in that shifted domain.
3. Run every rotation in 16-bit.
4. Unscale `R` at the end by `sh`. **`Q` needs no unscaling** — it is dimensionless.

Why this beats a fixed Q4:
- Uses the full 16-bit range regardless of input magnitude. A fixed Q4 wastes precision on large
  inputs and destroys it on small ones (`max|A|=4` would leave ~2 significant fractional bits).
- Costs one `CLZ` plus 16 shifts per decomposition — negligible against ~2280 instructions.
- Standard DSP practice, and it is a real technique worth naming in the report.

**A useful property:** the angle computation is *unaffected*. `calculate_arctan_ratio(N, D)` uses
only the ratio `N/D`, and the block scale cancels. So `trig_pwl` needs no changes at all.

### Formats

| quantity | format | rationale |
|---|---|---|
| matrix `R` | `int16_t`, block-scaled to ~`2^14` | SIMD32 operand width |
| matrix `Q` | `int16_t` Q14 | entries are ≤ 1.0 by construction |
| `c`, `s` | `int16_t` Q14 | ≤ 1.0; convert from the Q11 trig output with `<< 3` |
| SMLAD accumulator | `int32_t` | `2^14 × 2^15 × 2` = `2^30`, fits |
| shift after SMLAD | `>> 14` | coefficients are Q14 |

---

## Expected cost

Current inner loop is **20 instructions per element pair**. Estimated SIMD32 loop:

```
ldrsh  ti          1     16-bit loads
ldrsh  tj          1
pkhbt  t, tj, ti   1     pack into one register
smlad  a, cs, t    1     -> row_j
smusdx b, cs, t    1     -> row_i
asr    a, #14      1
asr    b, #14      1
strh   a           1
strh   b           1
cmp/bne            2
                  ---
                   11
```

Roughly **1.8×** on the rotation kernel: 1372 → ~760 per QR, total 2282 → ~1670 (**1.37×**), and
better once cycle-weighted since three-instruction multiplies become one MAC.

**The pack is the main risk.** `row_i[k]` and `row_j[k]` live in different rows, so they are never
adjacent and must be combined with `PKHBT` every iteration. If that overhead dominates, the
fallback is the `SMULBB`/`SMLABB` family (16×16 MAC with halfword selection), which needs no
packing but costs one more multiply. Measure both rather than assuming.

---

## Steps

1. **`src/variants/fixed_simd32/qr.c`** — copy `fixed_scalar/qr.c`, set
   `QR_VARIANT_NAME = "fixed_simd32"`, add `fixed_simd32` to `VARIANTS`. Confirm it builds and
   passes before changing anything.
2. **Block scaling at the boundary.** Convert the incoming Q11 `int32_t` matrix to block-scaled
   `int16_t` on entry; unscale `R` on exit. The variant still implements the standard
   `qr_decomposition()` signature, so the harness needs no changes.
3. **Scalar 16-bit version first.** Do the rotations in `int16_t` with ordinary C, no intrinsics.
   This isolates the *format* change from the *instruction* change — if accuracy breaks, you know
   which one did it. Run `make test-fixed_simd32`.
4. **Swap in the intrinsics.** `#include <arm_acle.h>`, replace the inner loop with
   `__smlad`/`__smusdx`. Guard with `#if defined(__ARM_FEATURE_SIMD32)` and keep the scalar path
   as the `#else` so it still builds on the host.
5. **Count the MACs.** Use `OPC(mac)` instead of `OPC(mul)` so the comparison table shows the
   substitution rather than a mysterious drop to zero.
6. **Measure**: `make compare`, then `make cycles VARIANT=fixed_simd32`. Confirm `simd32` is
   non-zero in `static.csv` and `mul` has collapsed.

---

## Accuracy: expect to have to deal with this

Dropping from 11 fractional bits to a block-scaled 16-bit format **will** change the error, and the
current suite applies one set of tolerances to every variant. Two things follow:

- **The harness needs per-variant tolerance overrides.** Right now `test_qr_accuracy.c` hardcodes
  limits tuned to `fixed_scalar`. Add a per-variant table before this variant lands, otherwise the
  only options are "widen for everyone" (destroys the regression guard) or "fail". This is a small
  change and should be done first.
- **Do not widen a tolerance to make it green.** Record the new baseline and say why it moved. If
  block scaling is implemented correctly the error should be *comparable* to `fixed_scalar` —
  because the ~9% budget is dominated by the PWL trig, not quantisation. If it jumps to 30%, that
  is a bug in the scaling, not an inherent cost.

The `max|A| = 4` random sweep is the one to watch: small inputs are where a 16-bit format has the
least headroom.

---

## What would make this a failure worth reporting

If the packing overhead eats the gain and SIMD32 lands at ~1.1×, that is still a legitimate
result: *the dual-MAC instructions fit the arithmetic perfectly, but the data layout defeats them,
because the two operands live in different rows of a row-major matrix.* That is a real
architecture/algorithm mismatch, and it motivates the custom instruction — `GIVENSQ` can read two
registers and do the packing internally for free.

Either outcome is publishable in the report. Measure, do not assume.
