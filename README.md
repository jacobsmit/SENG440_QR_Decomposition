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
|---|---:|---:|---:|---:|
| `naive_float` | 3390.5 | 1.00× | 0.14 % |
| `fixed_scalar` | 2350.7 | 1.44× | 0.47 % |
| `fixed_simd32` | 2112.7 | 1.61× | 0.30 % |
| `fixed_asip` (C model) | 2136.7 | 1.59× | 0.30 % |

Measured together in one callgrind run at `CG_ITERATIONS=50`, so the columns are
comparable to each other. They are **not** comparable to figures from before the
table-driven PWL landed: `naive_float` moved 3.5 % between runs despite being
untouched by that change, so something else in the measurement conditions
differs and the cause is not yet identified. To get a true before/after, run
both commits on the same machine in one sitting:

```sh
git checkout HEAD~1 && make clean && make instr-all && git checkout main
```

`fixed_asip`'s row is the C reference model, not the instruction — it costs
slightly MORE than `fixed_simd32` because of the extra call layer. The ASIP
figure cannot be measured; it comes from the firmware and hardware designs.

Caveat: the `naive_float` comparison is instruction counts, not cycles. Whether
fixed point wins on *cycles* depends on Cortex-A7 VFP latencies, which are not
yet sourced from the TRM — see `docs/PROJECT_TODO.md`.

## Commands

```sh
make test-all                    # accuracy suite, every variant; non-zero exit on failure
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
segment. 4 bytes of ROM per function removes a bounds compare and a conditional
move from every evaluation.

## Measurement rules

- Every variant passes the same accuracy suite before its performance number is
  quoted. `make compare` exits non-zero if one fails.
- Same seeded test vectors for every variant, so comparisons are like-for-like.
- Instruction counts are exact. **Cycle figures come from placeholder weights in
  `cycles.py`** — treat a conclusion as unsafe unless it survives a 2× change in
  those weights.
