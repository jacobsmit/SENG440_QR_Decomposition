# Plan: The New Instruction (ASIP extension)

Covers TODO items §1.3–1.9, §2.2–2.4. This is the graded core of the project: it is what turns
the work from "an optimised C program" into hardware–software co-design.

---

## Step 0 — Evidence (mostly gathered; finish on ARM)

The instruction choice has to *follow* from the bottleneck, not precede it. What is already
established, on the target architecture:

**Static instruction counts** (`arm-none-eabi-gcc -O2 -marm -march=armv5te -S`):

| routine | instructions | notes |
|---|---:|---|
| `calculate_arctan_ratio` | **89** | contains **2 × `bl __aeabi_idiv`** |
| `sin_fixed` | 51 | calls `cos_fixed` on the folded path |
| `cos_fixed` | 19 | |
| `arctan_fixed` | 26 | |

**Division: depends on the compile flag, not just the chip.** See `TARGET_PLATFORM.md` — the VM is
`-cpu cortex-a7`, and **Cortex-A7 has hardware `SDIV`**. Measured:

| target | `__aeabi_idiv` calls | native `sdiv` | `calculate_arctan_ratio` |
|---|---:|---:|---:|
| `-march=armv5te` (the notes' flag) | 2 | 0 | **89 instr** |
| `-mcpu=cortex-a7` (the actual VM) | 0 | 2 | **64 instr** |
| `-mcpu=cortex-a9` (report's claim) | 2 | 0 | 89 instr |
| `-mcpu=cortex-a15` | 0 | 2 | — |

So **do not** build the argument on "there is no hardware divide" — that is true for `armv5te` and
A9, but false for the A7 you actually run. Build it on:

- `calculate_arctan_ratio` is **still 64 instructions** on the A7, even with `SDIV`.
- `SDIV` on the A7 is an iterative, early-terminating unit — multi-cycle, not 1 cycle. Get the
  figure from the Cortex-A7 TRM and cite it.
- The PWL evaluations are **branchy** (segment selection + angle folding), and the A7 is in-order —
  a mispredict stalls.
- `GIVENSQ` collapses `64 + 51 + 20 = 135` instructions into one issue.

The instruction is still comfortably justified; the argument is total instruction count and branch
behaviour, not the absence of a divider. Reporting both flag sets is a free extra result — the
89 → 64 delta *is* the divider, quantified.

Still to do:
- [ ] `callgrind` in the ARM VM for the *dynamic* instruction share (TODO §2.6). Note from
      `TARGET_PLATFORM.md` §4: **QEMU has no timing model**, so use instruction counts, not
      `clock()`. The existing `profiling/` numbers are invalid twice over — taken on the
      Apple-Silicon host, and `clock()` under QEMU would not have been meaningful anyway.
- [ ] Cycle latencies for `SDIV`, `MUL`, `SMLAD` from the **Cortex-A7 TRM**, cited. Hand-count
      cycles from those; do not measure them in QEMU.
- [ ] Consider whether `SMLAD`/`SMUSDX` (SIMD32, see `TARGET_PLATFORM.md` §3c) already closes
      enough of the rotation-kernel gap to change which operation most deserves the custom
      instruction. The angle/trig computation is still the better candidate — SIMD32 accelerates
      the *rotation*, `GIVENSQ` accelerates the *coefficient generation* — but the report should
      show you checked rather than assumed.

---

## Step 1 — The central design problem

A Givens rotation needs **two** results, `c` and `s`. ARM allows **2 inputs, 1 output**.
Three ways out; the report should present all three and justify the choice:

| approach | verdict |
|---|---|
| Two separate instructions (`SIN`, `COS`) | Works, but 2 opcodes and 2 issues; recomputes angle reduction twice |
| **Pack both results into one 32-bit register** | **Chosen.** `c`,`s` ∈ [−1,1] → 12 bits + sign in Q11, so two fit in 32 bits as half-words. Stays reentrant, stateless, interruptible |
| Non-reentrant unit (Read/Read/Execute/Write) | The notes raise this — and raise its problems: interrupt masking, exception state save/restore. Discuss, then reject |

Packing is not a loophole: ARM already has half-word-packing instructions (`PKHBT`) and
half-word multiplies (`SMULBB`). One 32-bit destination register is one output.

---

## Step 2 — Instruction family and recommendation

Define a small family, implement two, and *compare them*. This answers the notes' own question —
*"the latency of most units ranges from 1 to 3 cycles – is it possible to define a unit with a
latency arbitrary large?"* — with data instead of prose.

| mnemonic | in | out | replaces | est. latency |
|---|---|---|---|---:|
| `PWLQ` | X, packed(m,b) | y | one segment evaluation | 2–3 |
| `ATAN2Q` | opposite, adjacent | angle (Q11) | `calculate_arctan_ratio` — 89 instr + 2 idiv | ~15 |
| `SINCOSQ` | angle | packed(c,s) | `sin_fixed` + `cos_fixed` — 70 instr | ~5 |
| **`GIVENSQ`** | opposite, adjacent | packed(c,s) | **all three — 159 instr + 2 idiv** | ~18–22 |

**Recommendation:** `GIVENSQ` as the headline result, with `ATAN2Q` + `SINCOSQ` as the decomposed
alternative for comparison.

Why both: `GIVENSQ` gives the largest instruction-count reduction but a long, non-pipelinable
latency and the most gates. The decomposed pair has shorter individual latencies, can be
scheduled around, and lets you reuse `SINCOSQ` if you later add the outer QR iteration
(TODO §0). The comparison *is* the co-design contribution — reject `PWLQ` as too fine-grained
(the call overhead exceeds the work).

---

## Step 3 — Write the ISA specification

Produce a formal spec table for the report. For `GIVENSQ`:

```
GIVENSQ  Rd, Rn, Rm

  Rn  = opposite  (R[i][j], Q11 signed)
  Rm  = adjacent  (R[j][j], Q11 signed)
  Rd  = packed rotation coefficients:
          Rd[31:16] = s = sin(theta)   in Q14, signed 16-bit field
          Rd[15:0]  = c = cos(theta)   in Q14, signed 16-bit field
        where theta = atan2(opposite, adjacent)

  ORDER AND FORMAT ARE NOT ARBITRARY. This is exactly the operand layout
  SMLAD/SMUSDX consume in fixed_simd32:
      SMLAD (cs, t) = cs.lo*t.lo + cs.hi*t.hi = c*tj + s*ti  -> row_j
      SMUSDX(cs, t) = cs.lo*t.hi - cs.hi*t.lo = c*ti - s*tj  -> row_i
  so GIVENSQ's result feeds the rotation with ZERO repacking. Q14 rather than
  Q11 for the same reason -- it is what the SIMD32 variant already uses.

  The ARM one-result limit forced packing; it turns out to be precisely the
  format the DSP extension wants. Worth making that point in the report: the
  ASIP extension and SIMD32 compose rather than compete.

  Edge cases (must be defined, and must match across C / FW / VHDL):
    adjacent == 0, opposite != 0  -> theta = +/- pi/2  -> c = 0, s = +/-16384
    adjacent == 0, opposite == 0  -> identity rotation -> c = 16384, s = 0
  Flags: unaffected.  Latency: (fill in from simulation).
```

- [ ] Decide and document the packing layout, the Q format, and every edge case.
- [ ] Note that `c = 1.0` is `16384` in Q14, which needs 15 bits + sign — confirm it fits the
      signed 16-bit field (it does; range −16384..+16384 against a limit of ±32767).

---

## Step 4 — C integration

### 4a. Bit-exact reference model first

Before any assembly, write a plain C function that computes exactly what the hardware will
compute, including the cheap slopes and the packing:

```c
/* Bit-exact software model of the GIVENSQ instruction.
   Layout matches the spec above and therefore SMLAD's operand format:
   result.hi = s, result.lo = c, both Q14. */
static inline int32_t givensq_ref(int32_t opposite, int32_t adjacent) {
    int32_t angle = calculate_arctan_ratio(opposite, adjacent);   /* Q11 angle */
    int32_t c14 = cos_fixed(angle) << 3;   /* Q11 -> Q14 */
    int32_t s14 = sin_fixed(angle) << 3;
    return (int32_t)(((uint32_t)(uint16_t)s14 << 16) | (uint32_t)(uint16_t)c14);
}
static inline int32_t givensq_c(int32_t p) { return (int16_t)(p & 0xFFFF); }
static inline int32_t givensq_s(int32_t p) { return (int16_t)(p >> 16); }

/* Feeds fixed_simd32's rotation directly -- the packed value IS the cs operand:
       row_j[k] = __smlad (cs, t, 0) >> 14;
       row_i[k] = __smusdx(cs, t)    >> 14;   */
```

This model is the **single source of truth**: it generates the test vectors for the C build, the
microcode hand-simulation, and the VHDL testbench. All three must agree bit-for-bit.

- [ ] Write it, and assert it reproduces the current `qr_decomposition` results exactly.

### 4b. Instantiate via inline assembly, with a build switch

You cannot assemble the custom opcode, so the same source must build both ways:

```c
#if defined(USE_GIVENSQ_ASM)
static inline int32_t givensq(int32_t o, int32_t a) {
    int32_t r;
    __asm__ ("GIVENSQ %0, %1, %2" : "=r"(r) : "r"(o), "r"(a));
    return r;
}
#else
#  define givensq(o, a) givensq_ref((o), (a))
#endif
```

- [ ] Rewrite `qr_decomposition()` to call `givensq()` and unpack, replacing the
      `calculate_arctan_ratio` / `cos_fixed` / `sin_fixed` trio.
- [ ] Build the functional version (`givensq_ref`) and confirm accuracy is unchanged.
- [ ] Build with `-DUSE_GIVENSQ_ASM ... -S` and capture the listing showing
      `GIVENSQ r0, r1, r2`. Compile only — **it will not assemble**, which is expected per the
      notes; say so explicitly rather than presenting it as a failure.
- [ ] Diff the two assembly listings and report instructions saved per rotation and per full QR
      (6 rotations for 4×4).

### 4c. The routine → inline → macro ladder (TODO §2.2)

Lesson 100 requires the accelerated operation implemented all three ways with performance for
each. Do this on `givensq_ref`: plain function, `static inline`, and macro. Report all three —
it also demonstrates *why* a custom instruction is needed even after inlining removes call
overhead.

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
