# Plan: Hand-Optimised Assembly — Reassessed Against Measurement

This supersedes the one-line sketch in `CODE_STRUCTURE.md` ("hand-written inline assembly inner
loops"). That was written before we had a cycle histogram. The measurements move the target.

---

## Two different deliverables, don't conflate them

The course asks for assembly work twice, and they are unrelated:

| | what | assembles? |
|---|---|---|
| **This document** (TODO 2.2–2.4) | Hand-optimise the *existing* instruction set. Lesson 100 step 5: *"Optimize by hand the assembly code. Assemble the code and run the executable on an ARM machine."* | **Yes** — real code, real measurement |
| `NEW_INSTRUCTION_PLAN.md` (TODO 1.3–1.4) | Instantiate the invented `GIVENSQ` instruction via `__asm__` | **No** — compile-only, `-S` listing only |

---

## What the measurements changed

`fixed_simd32` cycle histogram, per QR (weights are still placeholders):

| class | instructions | cycles | share of cycles |
|---|---:|---:|---:|
| load | 462 | 1386 | **36.6 %** |
| alu | 973 | 973 | **25.7 %** |
| store | 297 | 594 | 15.7 % |
| branch | 167 | 334 | 8.8 % |
| mac_simd32 | 96 | 288 | 7.6 % |
| mul | 21 | 63 | 1.7 % |
| div | 5.9 | 71 | 1.9 % |

**The arithmetic is 11 % of cycles.** Memory traffic is 52 %, loop control another 9 %.

So the original premise — "hand-write the inner loops" — is aimed at the wrong thing. SIMD32 already
solved the arithmetic (`mul` 42,589 → 4,189 over 200 QRs). What is left is **everything around** the
arithmetic: loads, stores, loop control and call overhead.

And gcc's SIMD32 loop is already tight. It found a better pack than the `PKHBT` the plan assumed:

```
ldrsh  r2, [ip]              @ 11 instructions total
ldrh   r3, [r5]
orr    r3, r3, r2, lsl #16   @ pack via the free barrel shift
smlad  r2, r0, r3, r7
smusdx r3, r0, r3
asr    r2, r2, #14
asr    r3, r3, #14
strh   r2, [r5], #2
strh   r3, [ip], #2
cmp/bne
```

Hand-writing that sequence will not beat it. **Do not expect a big win from rewriting the arithmetic.**

---

## Revised order of work

### Step 1 — routine → inline → macro (TODO 2.2). Do this first.

Not optional and not primarily a performance task: Lesson 100 step 1 **requires** the accelerated
operation implemented all three ways with performance reported for each. It is also cheap and may
capture much of the available win, because there are **~22 trig calls per QR**
(`calculate_arctan_ratio` 6, `arctan_fixed` 5.9, `sin_fixed` 8.2, `cos_fixed` 8.2), each paying a
`push`/`pop` frame.

- [ ] **Measure the call overhead first.** `cycles.py` now reports `stack` (push/pop/ldm/stm)
      separately from data `load`/`store` — previously they were lumped together, which hid this.
      Re-run `make cycles VARIANT=fixed_simd32` and read the `stack` row. That number is the ceiling
      on what inlining can recover.
- [ ] Three builds of the same variant, selected by a macro: `TRIG_ROUTINE`, `TRIG_INLINE`,
      `TRIG_MACRO`. Keep one source file; do not fork the variant three ways.
- [ ] Report instructions, cycles and code size for all three. Note the trade: inlining
      `sin_fixed`/`cos_fixed` duplicates them at 22 call sites and will grow code size
      significantly — that is the interesting result, not a footnote.
- [ ] Watch for the mutual recursion: `sin_fixed` calls `cos_fixed` on the angle-fold path and vice
      versa, so they **cannot both be fully inlined**. Say so in the report; it is a real constraint
      on the technique, and it is why the fold path is a good argument for `GIVENSQ`.

### Step 2 — let the compiler unroll before you write assembly (TODO 2.3)

`MATRIX_SIZE` is 4 and known at compile time, yet gcc keeps a 4-iteration loop with `cmp`/`bne` —
that is the 8.8 % branch share. Try, in order:

- [ ] `#pragma GCC unroll 4` on the rotation loops
- [ ] `-funroll-loops` as a flag set in `flagsets.sh`
- [ ] `-O3` already unrolls (`simd32` count jumps 4 → 16 in `static.csv`) but nearly doubles code
      size — quantify whether that pays

If the compiler gets it, that still counts as *"software optimization techniques (such as software
pipelining or loop unrolling)"* from Lesson 100 step 4, and it is a cheaper, more honest result than
hand-writing the same thing. **Only hand-write what the compiler demonstrably fails to do.**

### Step 3 — `fixed_asm`, targeting memory and scheduling

Now write assembly, aimed at what remains:

- [ ] **Register residency.** The whole 4×4 matrix is 16 × `int16_t` = 32 bytes = **8 registers**.
      ARM has 14 usable. A rotation touches two rows, so both can be held in registers across the
      entire rotation — load once, rotate, store once, instead of 2 loads + 2 stores per element.
      This is the single biggest structural idea available and gcc will not do it, because it cannot
      prove the aliasing.
- [ ] **Word loads instead of halfword.** `row_i` as 4 packed `int16_t` is two 32-bit loads rather
      than four `ldrh`. The catch: word loads give `(a1:a0)`, but SMLAD needs `(ti:tj)` pairing
      across *rows*, so `PKHBT`/`PKHTB` is needed to re-pair. Net saving is modest — measure before
      committing.
- [ ] **Software pipelining.** The A7 is in-order, so a load feeding the next instruction stalls.
      Issue the next iteration's loads before the current `smlad`. This is exactly the technique the
      notes name, and an in-order core is where it actually pays.
- [ ] Use `__asm__` with proper constraints and clobbers, not naked assembly, so the variant still
      links and runs. It must **assemble and execute** — that is the difference from the `GIVENSQ`
      work.

---

## Realistic expectation

Given arithmetic is 11 % of cycles and gcc's inner loop is already good, the plausible gain from
Step 3 is **1.2–1.4× on top of SIMD32**, coming from loads and loop control, not multiplies. Steps 1
and 2 may deliver a chunk of that for far less effort.

If hand assembly turns out to gain little, that is a legitimate and reportable finding: *a modern
compiler with the right intrinsics leaves little on the table for hand assembly on this kernel; the
remaining cost is memory traffic dictated by the row-major layout, which no instruction selection
can fix.* That conclusion argues directly for the custom instruction, which can read two registers
and do the packing internally.

---

## One caveat on the numbers above

The `load` figure of 462/QR is larger than the rotation loops need (96 loads + 96 stores across 48
iterations). The excess is block-scaling traffic, register spills in `qr_decomposition`, and — until
the classifier change above — function-call `push`/`pop`. **Re-run `make cycles` and split `stack`
out before choosing between Step 1 and Step 3**, otherwise you may hand-write assembly to fix
something that inlining removes for free.
