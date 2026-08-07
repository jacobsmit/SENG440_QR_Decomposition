# GIVENSQ in Firmware — Microcoded Engine

Deliverable for Lesson 100 scenario step 7 and the "firmware section" slide
(p.19): implement the bottleneck operation on a microcoded engine using the ARM
instruction set **without multiplication or division**, schedule it for 1, 2 and
3 issue slots, and report cycle counts computed by hand.

The specification is `givensq_ref()` in `src/common/givensq.h`. The microcode
must reproduce it bit-for-bit; so must the VHDL.

Cycle counts here are **hand-computed**, as the notes require — no simulator
exists for the microcoded engine. Each is derived below rather than asserted.

---

## 1. What the instruction must compute

```
GIVENSQ Rd, Rn, Rm
    Rn = opposite (o)      Rm = adjacent (a)
    Rd[31:16] = sin(theta) Q14      Rd[15:0] = cos(theta) Q14
    theta = atan2(o, a)
```

Decomposed:

1. `|o|`, `|a|`; guard `a == 0`; order them so the divide has `|N| <= |D|`
2. `ratio = N / D` as Q14
3. `angle = arctan(ratio)` by piecewise-linear table
4. quadrant fix-up: if the operands were swapped, `angle = ±pi/2 - angle`
5. `cos(angle)`, `sin(angle)` by piecewise-linear table, with the `|x| > pi/4`
   fold
6. pack the two halves

## 2. Living without a multiplier

The PWL evaluation is `m*x >> 14 + b`. A general multiply with no multiplier is
a 14-iteration shift-add loop, three times over (arctan, sin, cos) — it would
dominate the whole instruction.

Instead each segment's slope is constrained to a sum of **three signed powers of
two**, so the multiply becomes three shift-adds. On ARM the shift is free inside
the operand (`SUB r0, r0, r0, ASR #4` is one instruction), so this is 3
micro-operations.

Measured cost of that constraint (`P = 4`, max absolute error):

| function | 2 terms | **3 terms** | exact slope | notes' reference |
|---|---:|---:|---:|---:|
| arctan | 0.001781 | **0.000434** | 0.000234 | 0.01851 |
| sin | 0.001519 | **0.000553** | 0.000235 | — |
| cos | 0.001099 | **0.000406** | 0.000304 | — |

Three terms sits within ~2× of an exact multiply while needing no multiplier at
all, and is still ~40× better than the approximation the notes give. Two terms
saves one cycle per PWL and costs 3–4× the error — not worth it, because cos
error enters the orthogonality of Q squared.

**Segment coefficients live in the control store, not in a data table.** The
segment index micro-branches into a per-segment micro-routine whose shift
amounts and intercept are literals in the microinstruction. That removes the
table load and the unpacking that the C version needs, at the cost of control
store — the first of several ROM-for-speed trades in this design.

*(A note on something that does not work: writing the PWL in offset form,
`f(x0) + m*(x - x0)`, does not improve the tolerance to slope quantisation. The
minimax intercept re-fit already absorbs the segment-base term, so both forms
leave exactly the same residual. Verified, not assumed.)*

## 3. Vertical microcode — 1 issue slot

One micro-operation per microinstruction, strictly in order.

| phase | micro-operations | cycles |
|---|---|---:|
| operand prep | `|o|`, `|a|` (2 each), `a==0` guard, compare, conditional swap of N/D, record swap flag | 8 |
| **restoring divide** | 15 iterations × (`R<<1`; `CMP R,D`; `SUBHS R,R,D`; `ADC Q,Q,Q`) | **60** |
| arctan PWL | `idx = ratio >> 10`; micro-branch; 3 shift-adds; `+ b` | 6 |
| quadrant fix-up | conditional `±pi/2 - angle`, restore ratio sign | 5 |
| cos PWL | `|angle|`, compare `pi/4`, conditional fold, `idx`, micro-branch, 3 shift-adds, `+ b` | 11 |
| sin PWL | as cos, plus odd-function sign restore | 13 |
| pack | `s << 16`, `ORR` with `c & 0xFFFF` | 2 |
| | **total** | **105** |

**The divide is 60 of 105 cycles — 57 % of the instruction.** Everything else
put together is smaller than the divider. That single fact drives both the
horizontal schedule below and the hardware design.

## 4. Horizontal microcode — 2 issue slots

Ideal speed-up is 2×. What actually parallelises:

- **Operand prep** — `|o|` and `|a|` are independent. 8 → 5.
- **The divide does not.** Each iteration's remainder depends on the previous
  one; this is a serial dependency chain and no amount of issue width shortens
  it. Only the quotient-bit accumulate of iteration *i* can overlap the shift of
  iteration *i+1*. 60 → 45, and slot 2 is a NOP for most of it.
- **arctan PWL** — the three shifted operands are independent even though the
  additions form a chain, so compute the shifts in parallel and sum as a tree.
  6 → 4.
- **sin and cos are completely independent given the angle.** This is the single
  biggest win in the whole schedule: two PWL evaluations, 24 cycles serial,
  become 13 running side by side.
- **Pack** 2 → 1.

| phase | 1 slot | 2 slots |
|---|---:|---:|
| operand prep | 8 | 5 |
| restoring divide | 60 | 45 |
| arctan PWL | 6 | 4 |
| quadrant fix-up | 5 | 3 |
| cos + sin PWL | 24 | 13 |
| pack | 2 | 1 |
| **total** | **105** | **71** |

**Speed-up 1.48× against a ceiling of 2× — 74 % of ideal.**

The shortfall is entirely the divider. Outside the divide, the schedule achieves
45 → 26, which is 1.73× and close to ideal; inside it, 1.33×. The NOPs are
concentrated in slot 2 of the division loop, and there is no way to fill them,
because there is no other work available that does not depend on the quotient.

## 5. Horizontal microcode — 3 issue slots

| phase | 1 slot | 2 slots | 3 slots |
|---|---:|---:|---:|
| operand prep | 8 | 5 | 4 |
| restoring divide | 60 | 45 | 42 |
| arctan PWL | 6 | 4 | 3 |
| quadrant fix-up | 5 | 3 | 2 |
| cos + sin PWL | 24 | 13 | 10 |
| pack | 2 | 1 | 1 |
| **total** | **105** | **71** | **62** |

**Speed-up 1.69× against a ceiling of 3× — only 56 % of ideal.**

The third slot buys 9 cycles for 50 % more control-store width. Diminishing
returns, and the reason is the same: 42 of the 62 cycles are a divide that
cannot be widened. **Adding issue slots cannot fix a serial dependency**, which
is the general lesson this schedule demonstrates.

## 6. What this says about the hardware

The firmware analysis makes the hardware decision quantitative rather than a
matter of taste:

- Replacing the restoring divider with a **reciprocal LUT plus multiply**
  (~2–3 cycles instead of 60) would take the 1-slot figure from 105 to ~48 and
  remove the serial spine, at which point issue width would start paying off
  again.
- **Hardware evaluates sin and cos genuinely in parallel** — two PWL units,
  not a schedule that alternates. Firmware's 2-slot version already captures
  most of that particular win, which is why the hardware's advantage must come
  from the divider, not from the trig.
- The per-segment micro-routines become a coefficient ROM addressed by the
  segment index — no branch, no misprediction, and the same ROM serves any
  number of segments.

Cost/latency numbers for those options belong in the hardware section, which is
still to be written.

## 7. Open items

- [ ] Settle the `(0, 0)` case. `calculate_arctan_ratio(0,0)` currently returns
      `+pi/2`, giving a 90° rotation rather than the identity. Harmless in the
      software (still orthogonal, and `R[i][j]` is zeroed anyway) and rare
      (0.06 per QR), but the C model, this microcode and the VHDL must agree.
- [ ] Write out the full per-segment micro-routine listing for all 16 + 13 + 13
      segments, or state the control-store size and give one worked example.
- [ ] Decide whether the shipped C should also move to 3-term CSD slopes so the
      software and firmware compute bit-identical results. That costs ~2× error
      in C for exact agreement with the firmware — worth stating either way.
- [ ] Cross-check the hand cycle counts against a generated ARM listing for the
      same operation sequence.
