# GIVENSQ — Firmware and Hardware

The instruction is defined and instantiated: see `src/common/givensq.h` for the
spec and bit-exact reference model, and `make asip-asm` for the listing that
proves ARM-compliant instantiation (2 inputs, 1 output) and the expected
assembler rejection.

`givensq_ref()` is the single source of truth. The microcode hand-simulation and
the VHDL testbench must both reproduce it bit-for-bit.

What remains is the firmware and hardware design, which is also where GIVENSQ's
latency comes from -- it cannot be measured, only designed.

---

## Step 5 — Firmware / microcode (TODO §1.5)

Constraint from the notes: use the ARM instruction set **without multiplication and division**.

### 5a. Make the PWL slopes multiplier-free

Each `m·x >> 11` becomes shift-adds. The *current* slopes are expensive; slopes restricted to
low canonical-signed-digit weight are far cheaper, and measured accuracy barely moves:

| function | current slopes | add/sub | max err | cheap slopes | add/sub | max err |
|---|---|---:|---:|---|---:|---:|
| arctan | 1900, 1319 | 7 | 0.01839 | **1920, 1280** | 2 | **0.01194** |
| sin | 1984, 1583 | 5 | 0.00522 | **1984, 1536** | 2 | 0.00757 |
| cos | 314, 1172 | 6 | 0.01212 | **256, 1152** | 1 | 0.02261 |

Total add/sub across all six slopes: **18 → 5**. arctan actually *improves* (re-fitting the
intercept minimax more than pays for the coarser slope).

The resulting micro-ops:

| slope | identity | micro-ops |
|---:|---|---|
| 1920 | 2¹¹ − 2⁷ | `x - (x>>4)` |
| 1280 | 2¹⁰ + 2⁸ | `(x>>1) + (x>>3)` |
| 1984 | 2¹¹ − 2⁶ | `x - (x>>5)` |
| 1536 | 2¹¹ − 2⁹ | `x - (x>>2)` |
| 1152 | 2¹⁰ + 2⁷ | `(x>>1) + (x>>4)` |
| 256 | 2⁸ | `x>>3` |

On ARM these fold into the barrel shifter — `SUB r0, r0, r0, ASR #4` is **one instruction**.
Worth highlighting: the shift is free on ARM, so this is 1 cycle per slope.

- [ ] **Caution on cos.** Its error doubles (0.0121 → 0.0226), and cos error enters the
      orthogonality error squared — cos is the weak link already (see TODO §3.4). Either keep a
      2-add cos slope, or add a third cos segment. Verify against the QR accuracy suite before
      committing; do not take the 1-add cos on faith.
- [ ] Re-run `find_trig_approx_parameters.py` with slopes **constrained to CSD weight ≤ 2** and
      intercepts re-optimised, rather than hand-picking. This is the principled version and is a
      genuine contribution: *co-designing the approximation to fit the datapath.*

### 5b. Schedule the microcode

- [ ] Write the vertical (1 issue slot) microprogram for `GIVENSQ`. Structure:
      `|abs| → compare/swap → restoring divide (≈12 iterations of shift/subtract) → arctan PWL →
      angle fold → sin PWL, cos PWL → pack`. Count cycles by hand.
- [ ] Horizontal, **2 issue slots**. The parallelism is real and easy to point at:
      `|o|` and `|a|` are independent; the two PWL segment candidates can be computed
      speculatively and muxed; **sin and cos are fully independent given the angle**. The
      division iterations are the serial spine and will leave NOPs — say so.
- [ ] **3 issue slots** as well — the notes explicitly ask.
- [ ] Report cycle counts and how close each gets to the 2× / 3× ceiling.

---

## Step 6 — Hardware (TODO §1.6–1.8)

- [ ] Datapath block diagram: abs/negate stage → comparator + swap mux → divider → PWL unit
      (shifter + adder + breakpoint comparator + coefficient mux) → angle-fold adder →
      two parallel PWL units for sin/cos → packer.
- [ ] **Divider is the design decision.** Evaluate both and report the trade-off:
  - Restoring divider, ~12 cycles for 11 quotient bits. Cheap: one adder + shift register + mux.
  - Reciprocal LUT + multiply, ~2–3 cycles. Costs ROM but collapses the critical path.

  This choice dominates both latency and gate count, so it is the natural centrepiece of the
  hardware section.
- [ ] Exploit what hardware buys over firmware: **sin and cos evaluate in parallel** (two PWL
      units), and the two segment candidates can be computed concurrently and muxed — no branch,
      so no misprediction. This is the concrete answer to why HW beats the 2× firmware ceiling.
- [ ] VHDL/Verilog + testbench driven by vectors exported from `givensq_ref` (Step 4a).
- [ ] Simulate for latency.
- [ ] **Gate count** with a per-block breakdown (adders, comparators, muxes, shift register, ROM
      if used). The notes permit an educated guess with "addition takes 1 cycle" as guidance —
      but show the arithmetic.
- [ ] **Penalty for the hardware solution**: gate area, plus the architectural cost the notes
      raise — a long-latency non-standard unit complicates compiler scheduling and pipelining.

---

## Step 7 — Evaluation (TODO §1.9)

- [ ] Instructions per rotation and per QR: software vs `ATAN2Q`+`SINCOSQ` vs `GIVENSQ`.
- [ ] Cycles: software (measured on ARM) vs firmware 1/2/3-slot (hand-counted) vs hardware
      (simulated).
- [ ] Speedup: HW vs SW, and 2-issue FW vs SW — the two numbers the spec names explicitly.
- [ ] Amdahl ceiling: with the trig/angle fraction *f* measured in Step 0, the whole-program
      speedup is capped at `1/(1-f)` no matter how fast the unit is. Report the capped figure,
      not just the kernel speedup — this is the honest number and it shows you understand the law.
- [ ] Accuracy unchanged? Re-run the QR accuracy suite against the `givensq_ref` path and confirm
      the instruction introduces no additional error beyond the slope changes in 5a.

---

## Suggested order

1. Step 0 dynamic profiling on the ARM VM — without it the rest is unjustified
2. Step 3 ISA spec (a page of writing; unblocks everything else)
3. Step 4a reference model + vectors — the shared source of truth
4. Step 4b inline asm + `-S` listing → **§1.3/§1.4 done**
5. Step 5a cheap slopes, verified against the accuracy suite
6. Step 5b microcode 1/2/3-slot → **§1.5 done**, the biggest untouched requirement
7. Step 6 VHDL + gate count → **§1.6–1.8 done**
8. Step 7 evaluation table → **§1.9 done**

Steps 2–4 are a day's work and close two requirements. Step 5b is the highest-value remaining
item because nothing exists for it yet and it is explicitly required.
