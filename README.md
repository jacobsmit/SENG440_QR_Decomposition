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
| `fixed_scalar` | Q11 fixed point, piecewise-linear trig |
| `fixed_simd32` | + SIMD32 dual-MAC rotations (`SMLAD`/`SMUSDX`), 16-bit block floating point |
| `fixed_asip` | + the `GIVENSQ` custom instruction replacing angle+sin+cos |

Add one: create `src/variants/<name>/qr.c` implementing `qr_decomposition()`
(see `src/common/qr_iface.h`) and add `<name>` to `VARIANTS` in the Makefile.

## Results

Instructions per QR, measured with callgrind on the target:

| variant | instr/QR | vs baseline | worst rel. error |
|---|---:|---:|---:|
| `naive_float` | 3512 | 1.00× | 0.14 % |
| `fixed_scalar` | 2290 | 1.53× | 10.27 % |
| `fixed_simd32` | 2058 | 1.71× | 10.23 % |
| `fixed_asip` (est.) | ~1154 | ~3.0× | 10.23 % |

`fixed_asip`'s figure is an estimate: `GIVENSQ` is not a real instruction, so
that build compiles but cannot run. Its latency comes from the firmware and
hardware designs, not from measurement.

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
```

`CG_ITERATIONS=200` raises the callgrind sample; `ITERATIONS`/`MAGNITUDE` control
the profiling workload.

## Layout

```
src/common/      fixed.h (Q format, mul/div)  trig_pwl (PWL sin/cos/arctan)
                 givensq.h (custom instruction + reference model)
                 matrix, matrix_f32, qr_iface, op_counters
src/variants/    one qr.c per variant
tests/           accuracy regression suite (asserts, real exit codes)
profiling/       cycles.py (opcode histogram), profile_ops.c (op counts),
                 arm_profile.sh (static analysis), flagsets.sh
docs/            PROJECT_TODO.md (remaining work), NEW_INSTRUCTION_PLAN.md
```

## Measurement rules

- Every variant passes the same accuracy suite before its performance number is
  quoted. `make compare` exits non-zero if one fails.
- Same seeded test vectors for every variant, so comparisons are like-for-like.
- Instruction counts are exact. **Cycle figures come from placeholder weights in
  `cycles.py`** — treat a conclusion as unsafe unless it survives a 2× change in
  those weights.
