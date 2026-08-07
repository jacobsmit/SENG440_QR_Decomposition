# SENG 440 — Fixed-Point QR Decomposition on an ASIP

4×4 QR decomposition by Givens rotations, optimised for a 32-bit ARM target and
extended with a custom instruction.

## Target

**ARM Cortex-A7**, ARMv7-A, 32-bit armhf, emulated by QEMU
(`qemu-system-arm -M virt -cpu cortex-a7`). Guest is Debian 13, gcc 14.

Verified in-guest: CPU part `0xc07`, features `idiva idivt edsp neon vfpv4`.

Three consequences that shape everything here:

- **QEMU has no timing model.** Wall-clock time in the guest measures the host,
  so it is never reported. Instruction counts are exact and are used instead;
  cycles are *computed* from them, never measured.
- **Hardware `SDIV` exists but is off by default.** ARMv7-A has no integer
  divide extension, so a default build emits `__aeabi_idiv`. `-mcpu=cortex-a7`
  is required to get `SDIV`.
- **`-march=armv5te` cannot be built here.** Debian armhf is hard-float only and
  ships no `gnu/stubs-soft.h`. Use `armv7-a` as the "no divider" baseline.

## Variants

| variant | what |
|---|---|
| `naive_float` | floats + libm (`atan2f`/`cosf`/`sinf`). The baseline. |
| `fixed_scalar` | Q11 fixed point, table-driven piecewise-linear trig |
| `fixed_simd32` | + SIMD32 dual-MAC rotations (`SMLAD`/`SMUSDX`), 16-bit block floating point |
| `fixed_asip` | + the `GIVENSQ` custom instruction replacing angle+sin+cos |

Add one: create `src/variants/<name>/qr.c` implementing `qr_decomposition()`
(see `src/common/qr_iface.h`) and add `<name>` to `VARIANTS` in the Makefile.

## Results

Instructions per QR, measured with callgrind on the target:

| variant | instr/QR | vs baseline | worst rel. error |
|---|---:|---:|---:|
| `naive_float` | 3390.5 | 1.00× | 0.14 % |
| `fixed_scalar` | 2338.9 | 1.45× | 0.47 % |
| `fixed_simd32` | 2100.9 | 1.61× | 0.30 % |
| `fixed_asip` (C model) | 2124.9 | 1.60× | 0.30 % |

`fixed_asip`'s row is the C reference model, not the instruction — it costs
slightly MORE than `fixed_simd32` because of the extra call layer. The ASIP
figure cannot be measured; it comes from the firmware and hardware designs.

### Cost of the table-driven trig

A/B on the same machine in one sitting, `4fe5fb7` (2-segment, branch-selected)
against `5fac1c6` (table-driven):

| variant | before | after | delta | speed-up |
|---|---:|---:|---:|---:|
| `naive_float` | 3388.1 | 3390.5 | +0.07 % | 1.00× → 1.00× |
| `fixed_scalar` | 2290.1 | 2338.9 | +2.13 % | 1.48× → 1.45× |
| `fixed_simd32` | 2058.1 | 2100.9 | +2.08 % | 1.65× → 1.61× |
| `fixed_asip` | 2082.1 | 2124.9 | +2.06 % | 1.63× → 1.60× |

`naive_float` moves 0.07 % and is untouched by the change, which is what makes
the other three rows trustworthy. **The trade is 2.1 % of the instructions for
34× the accuracy** (10.23 % → 0.30 %). Segment count itself is free — 42
segments cost what 4 would — but the table is not free against the 2-segment
version it replaced, which held its coefficients as immediates and so did no
load and no unpacking.

An earlier figure of 3512 for `naive_float` appears in the project history. It
was stale, not a measurement artefact: the baseline is stable at ~3389 across
both commits. Any speed-up quoted against 3512 (e.g. the old 1.71×) mixed
measurements from different code states.

Caveat: the `naive_float` comparison is instruction counts, not cycles. Whether
fixed point wins on *cycles* depends on Cortex-A7 VFP latencies, which are not
yet sourced from the TRM — see `docs/PROJECT_TODO.md`.

### Instruction mix

`fixed_simd32`, exact (`make cycles VARIANT=fixed_simd32`), 2100.9 instr/QR:

| class | share |
|---|---:|
| alu | 47.6 % |
| load | 22.7 % |
| store | 13.3 % |
| branch | 6.6 % |
| `mac_simd32` (SMLAD/SMUSDX) | 4.6 % |
| stack | 2.3 % |
| mul | 1.0 % |
| div (SDIV) | 0.3 % |

Memory is 36.0 % and arithmetic only 5.9 %, which is why the hand-assembly plan
targets memory traffic rather than the arithmetic.

Two independent cross-checks that this histogram is sound: callgrind sees
5.94 SDIV/QR and 96.0 SMLAD/QR, and the C-level operation counters
independently report 5.94 divides and 96.0 MACs per QR.

### Where the instructions go

Per function, `fixed_simd32`, from `make instr-detail` (50 QRs):

| function | instr/QR | share |
|---|---:|---:|
| `qr_decomposition` (rotations, scaling) | 1134.0 | 54.06 % |
| `sin_fixed` | 292.4 | 13.94 % |
| `cos_fixed` | 255.5 | 12.18 % |
| `calculate_arctan_ratio` | 225.9 | 10.77 % |
| `arctan_fixed` | 190.1 | 9.06 % |

Trig is **963.9 instr/QR, 45.94 %**, so replacing all of it with a zero-cost
`GIVENSQ` caps the whole-program speed-up at **1.85×** over `fixed_simd32` —
report the capped figure, not the kernel speed-up. That implies ~1140 instr/QR
for the ASIP, ~2.97× over `naive_float`.

The ceiling *rose* from 1.79× (trig was 44.22 % with the 2-segment tables):
making the trig more accurate gave `GIVENSQ` more work to absorb. The 1.85× is
an instruction-count bound and assumes `GIVENSQ` issues as one instruction; its
real cost is a multi-cycle latency that comes from the firmware and hardware
designs, so this is an upper bound, not a prediction.

## Commands

```sh
make test-all                    # parser tests + accuracy suite, every variant
make test-parser                 # callgrind parser regression tests (no VM needed)
make compare                     # ops + accuracy + instruction counts + static analysis
make cycles VARIANT=fixed_simd32 # dynamic opcode histogram
make instr-detail VARIANT=x      # per-function instruction counts
make static-all                  # static counts, variants x compiler flag sets
make asip-asm                    # GIVENSQ listing (compiles; cannot assemble, by design)
make pwl-sweep                   # trig accuracy vs table size
make pwl-tables P=4              # regenerate the PWL coefficients
```

`CG_ITERATIONS=200` raises the callgrind sample; `ITERATIONS`/`MAGNITUDE` control
the profiling workload.

## Layout

```
src/common/      fixed.h (Q11 matrix format)  trig_pwl (Q14 PWL sin/cos/arctan)
                 trig_pwl_tables.h (GENERATED coefficients -- make pwl-tables)
                 givensq.h (custom instruction + reference model)
                 matrix, matrix_f32, qr_iface, op_counters
src/variants/    one qr.c per variant
tests/           accuracy regression suite (asserts, real exit codes)
profiling/       cycles.py (opcode histogram), profile_ops.c (op counts),
                 arm_profile.sh (static analysis), flagsets.sh
scripts/         gen_pwl_tables.py (emits the PWL tables), pwl_sweep.sh
docs/            PROJECT_TODO.md (remaining work), NEW_INSTRUCTION_PLAN.md
```

## Trig accuracy

The PWL unit is table driven with segments of width 2⁻ᴾ, so the segment index is
one shift and the coefficients one load — **cost is O(1) in the segment count**.
42 segments cost what 4 would; accuracy is bought with table ROM.

O(1) is not free, though. Against the previous *2-segment* trig it is about
+55 instructions/QR (2.6 %), because that version held its coefficients as
immediates and so did no load and no unpacking. The trade is 34× the accuracy
for 2.6 % of the instructions, which is worth taking — but it is a trade, not a
win on both axes.

| P | segments | table bytes | `fixed_scalar` rel. err | `fixed_simd32` rel. err |
|---:|---:|---:|---:|---:|
| 2 | 12 | 60 | 3.16 % | 3.05 % |
| 3 | 22 | 100 | 1.07 % | 0.90 % |
| **4** | **42** | **180** | **0.47 %** | **0.30 %** |
| 5 | 84 | 348 | 0.41 % | 0.20 % |
| 6 | 166 | 676 | 0.39 % | 0.19 % |

P = 4 is the knee. Past it the Q14 coefficients quantise before the
approximation does, so the extra ROM buys almost nothing — which is why angles,
ratios and sin/cos values are all Q14 and not the Q11 of the matrix data.

The previous 2-segment, branch-selected trig gave 10.27 % / 10.23 %.

The last table entry of each function is a duplicate guard: `|ratio| == 1`
(i.e. `|N| == |D|`, common in these matrices) indexes one past the final
segment. 12 bytes of ROM removes the bounds check entirely — worth 11.8
instructions/QR, measured.

Only `arctan` was actually paying for that check. In `sin`/`cos` the enclosing
`if (abs_X > PI_OVER_4_FIXED)` proves the index is in range, so gcc had already
eliminated it; `arctan_fixed` has no such visible bound, since `|X| <= 1` is
guaranteed by its caller across a function boundary. 5.94 arctan calls/QR × 2
instructions = 11.9 predicted against 11.8 measured.

## Measurement rules

- Every variant passes the same accuracy suite before its performance number is
  quoted. `make compare` exits non-zero if one fails.
- Same seeded test vectors for every variant, so comparisons are like-for-like.
- Instruction counts are exact. **Cycle figures come from placeholder weights in
  `cycles.py`** — treat a conclusion as unsafe unless it survives a 2× change in
  those weights.
