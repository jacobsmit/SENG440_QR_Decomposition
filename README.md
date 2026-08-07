# SENG 440 — Fixed-Point QR Decomposition on an ASIP

4×4 QR decomposition by Givens rotations on a 32-bit ARM target, extended with a
custom instruction (`GIVENSQ`) implemented in software, firmware and hardware.

## Scope

**One-sided Givens QR, single pass.** No outer iteration, no sweeps, no Jacobi.
Lesson 100 p.5 lists project 12 as *"Matrix diagonalization (QR decomposition,
eigenvalue decomposition, singular-value decomposition)"*, and Lesson 112's
"Jacobi method — side effects" slide places one-sided rotations inside the
Jacobi framework: *"Matrix triangularization can be achieved with one-side
rotations."*

A single QR **triangularizes; it does not diagonalize**, so Lesson 112's
"incomplete rotations only slow convergence" argument does not apply — there is
no iteration to converge. Accuracy has to stand on its own, which is what
`docs/PWL_ERROR.md` is for.

## Target

**ARM Cortex-A7**, ARMv7-A, 32-bit armhf, emulated by QEMU
(`qemu-system-arm -M virt -cpu cortex-a7`). Debian 13, gcc 14. Verified in
guest: CPU part `0xc07`, features `idiva idivt edsp neon vfpv4`.

- **QEMU has no timing model**, so wall-clock time in the guest measures the
  host. Instruction counts are exact and are used instead.
- **Hardware `SDIV` is off by default.** `-mcpu=cortex-a7` is required to get it.
- **`-march=armv5te` cannot be built here** — Debian armhf is hard-float only.
  `armv7-a` is the "no divider" baseline.

## Variants

| variant | what |
|---|---|
| `naive_float` | floats + libm (`atan2f`/`cosf`/`sinf`). The baseline. |
| `fixed_scalar` | Q11 fixed point, table-driven piecewise-linear trig |
| `fixed_simd32` | + SIMD32 dual-MAC rotations (`SMLAD`/`SMUSDX`), 16-bit block floating point |
| `fixed_asip` | + the `GIVENSQ` custom instruction replacing angle+sin+cos |

## Results

Instructions per QR, callgrind on the target, all measured in one run:

| variant | instr/QR | vs baseline | worst rel. error |
|---|---:|---:|---:|
| `naive_float` | 3390.5 | 1.00× | 0.14 % |
| `fixed_scalar` | 2338.9 | 1.45× | 0.47 % |
| `fixed_simd32` | 2100.9 | 1.61× | 0.30 % |
| `fixed_asip` (C model) | 2124.9 | 1.60× | 0.30 % |

`fixed_asip`'s row is the C reference model, not the instruction — it costs
slightly *more* than `fixed_simd32` because of the extra call layer. The ASIP's
real figure comes from the firmware and hardware designs, which cannot be
measured, only designed.

**Open:** the baseline may be overstated. Measured at its native float interface
with libc startup excluded, `naive_float` is 2411 instr/QR, which would make
`fixed_simd32` 1.15× rather than 1.61×. See `docs/PROJECT_TODO.md`.

### Where the instructions go

`fixed_simd32`, `make instr-detail`:

| function | instr/QR | share |
|---|---:|---:|
| `qr_decomposition` (rotations, scaling) | 1134.0 | 54.06 % |
| `sin_fixed` | 292.4 | 13.94 % |
| `cos_fixed` | 255.5 | 12.18 % |
| `calculate_arctan_ratio` | 225.9 | 10.77 % |
| `arctan_fixed` | 190.1 | 9.06 % |

Trig is **45.94 %**, so replacing it with a zero-cost `GIVENSQ` caps the
whole-program speed-up at **1.85×**.

### `GIVENSQ` across the three implementations

| implementation | cost per rotation | vs software |
|---|---:|---:|
| software (`fixed_simd32`) | 160.7 instr | 1.00× |
| firmware, 1 issue slot | 101 cycles | 1.59× |
| firmware, 2 issue slots | 68 cycles | **2.36×** |
| firmware, 3 issue slots | 59 cycles | 2.72× |
| hardware (simulated) | 20 clocks | **8.03×** |

Details and derivations in `docs/FIRMWARE_MICROCODE.md` and `docs/HARDWARE.md`.

## Trig accuracy

The PWL unit is table driven with segments of width 2⁻ᴾ, so the segment index is
one shift and the coefficients one load — **cost is O(1) in the segment count**.
Accuracy is bought with table ROM, not instructions.

| P | segments | bytes | `fixed_scalar` | `fixed_simd32` |
|---:|---:|---:|---:|---:|
| 2 | 12 | 60 | 3.16 % | 3.05 % |
| 3 | 22 | 100 | 1.07 % | 0.90 % |
| **4** | **42** | **180** | **0.47 %** | **0.30 %** |
| 5 | 84 | 348 | 0.41 % | 0.20 % |
| 6 | 166 | 676 | 0.39 % | 0.19 % |

P = 4 is the knee: past it the Q14 coefficients quantise before the
approximation does. That is also why angles, ratios and sin/cos values are Q14
and not the Q11 of the matrix data.

The last entry of each table is a duplicate guard, so `|ratio| == 1` (i.e.
`|N| == |D|`) needs no bounds check — 12 bytes of ROM for 11.8 instructions/QR.

## Layout

```
src/common/      fixed.h (Q11 matrix format)   trig_pwl (Q14 PWL sin/cos/arctan)
                 givensq.h        software model of the custom instruction
                 givensq_fw.c/.S  firmware model + ARM assembly, no MUL or SDIV
                 *_tables.h, *_csd.h           GENERATED coefficients
src/variants/    one qr.c per variant
tests/           accuracy suite; firmware/assembly equivalence
profiling/       profile_ops.c (op counts), arm_profile.sh (static analysis)
hw/              VHDL unit, testbench, generated ROMs and vectors
scripts/         coefficient generators, gate count, cross-checks
docs/            PWL_ERROR, FIRMWARE_MICROCODE, HARDWARE, PROJECT_TODO
```

**Three implementations, one set of coefficients.** The C header, the VHDL
package and the ARM assembly are all emitted by `scripts/gen_pwl_tables.py`'s
`build_csd_table()`. They drifted once when each generator had its own copy —
three intercepts landed 1 LSB apart and the VHDL testbench failed 62 of 301
vectors — so `make check-tables` now asserts they stay identical.

## Commands

```sh
make test-all                    # tables + firmware + accuracy suite, all variants
make compare                     # ops + accuracy + instruction counts + static analysis
make instr-detail VARIANT=x      # per-function instruction counts
make static-all                  # static counts, variants x compiler flag sets
make asip-asm                    # GIVENSQ listing (compiles; cannot assemble, by design)
make givensq-asm                 # regenerate the ARM assembly; count mul/div (must be 0)
make hw-sim                      # VHDL testbench: vectors + measured latency
make hw-gates                    # gate-equivalent estimate, arithmetic shown
make pwl-sweep                   # trig accuracy vs table size
make pwl-tables P=4              # regenerate all three coefficient tables
```

`CG_ITERATIONS` sets the callgrind sample; `ITERATIONS`/`MAGNITUDE` control the
profiling workload.

## Measurement rules

- Every variant passes the same accuracy suite before its performance number is
  quoted. `make compare` exits non-zero if one fails.
- Same seeded test vectors for every variant, so comparisons are like-for-like.
- Instruction counts are exact and measured. Firmware cycle counts are
  hand-derived (no simulator exists for a microcoded engine, as the notes note).
  Hardware latency is simulated.
