# GIVENSQ in Hardware

Deliverable for Lesson 100 scenario step 8 (automaton + testbench + simulated
latency) and Lesson 112 requirements II (gate count, hardware penalty,
hardware-vs-software speed-up).

Files: `hw/givensq.vhd`, `hw/tb_givensq.vhd`, `hw/givensq_pkg.vhd` (generated),
`hw/vectors.txt` (generated). Build with `make hw-sim`, `make hw-gates`.

---

## 1. Organisation

A Moore automaton — seven states, D flip-flops — sequencing a datapath:

```
        opp  adj
         |    |
     [ abs ][ abs ]          S_PREP    1 clock
         |    |
     [ compare + swap ]  ->  n, d, swapped, neg
         |
     [ restoring divider ]   S_DIV    14 clocks  (1 bit per clock)
         |  ratio Q14
     [ PWL: arctan ]         S_ARCTAN  1 clock
         |  angle
     [ quadrant fix-up ]     S_FIX     1 clock
         |
     [ PWL: cos ][ PWL: sin ] S_TRIG   1 clock   <-- BOTH, same clock
         |         |
     [ pack s:c ]            S_DONE    1 clock
         |
       result
```

Three properties worth stating in the report:

- **sin and cos are evaluated by two separate PWL blocks in the same clock.**
  This is the thing hardware buys that firmware cannot. The 2-issue microcode
  still interleaves them across issue slots; the hardware genuinely overlaps.
- **No branch anywhere in the datapath.** Segment selection is a ROM address
  and the `|x| > pi/4` fold is a mux, so there is nothing to mispredict — the
  data-dependent branches that cost the software ~28 decisions per QR simply do
  not exist here.
- **The unit is stateless between invocations.** Nothing is retained, so there
  is no question about masking interrupts or restoring unit state after an
  exception — the packed-result design (forced by ARM's one-destination limit)
  is what makes that possible.

## 2. Latency

**Measured** by simulation, `make hw-sim`, 301/301 vectors passing:

| path | clocks |
|---|---:|
| typical (divide taken) | **20** |
| `abs(n) >= abs(d)` (ratio exactly 1.0, divide skipped) | 6 |
| `adjacent == 0` (no divide, no arctan) | 4 |

The divide is **14 of the 20 clocks — 70 %**. It is data independent, so the
typical figure is also the worst case.

## 3. Gate count

`make hw-gates` shows the arithmetic. One GE = one 2-input NAND; primitives are
the usual first-order figures (full adder 5 GE, D flip-flop 6 GE, 2:1 mux 3 GE,
ROM cell 0.5 GE).

| block | gate equivalents | share |
|---|---:|---:|
| input stage (abs ×2, compare, swap muxes) | 336 | 6 % |
| restoring divider (registers, subtractor, mux, counter) | 454 | 8 % |
| coefficient ROM (45 entries × 32 bits, dual read) | 930 | 17 % |
| PWL datapath ×2 (multiplier, +b adder, fold logic, muxes) | 3,584 | 64 % |
| FSM controller (7 states) | 138 | 2 % |
| output register | 192 | 3 % |
| **total** | **5,634** | |

**The two multipliers are 2,912 GE — 52 % of the unit on their own.** Duplicating
the PWL datapath is what buys the parallel sin/cos, and it is not cheap.

## 4. The multiplier-free trick does not transfer to hardware

The firmware constrains every slope to three signed powers of two so the
microcoded engine needs no multiplier (docs/FIRMWARE_MICROCODE.md §2). The
obvious move is to do the same in hardware. It buys nothing:

| PWL multiply | gate equivalents |
|---|---:|
| 16×16 array multiplier | 1,456 |
| three CSD shift-adds (3 barrel shifters + 2 adders) | 1,472 |

**1.01×.** The reason is that a shift is only free when the barrel shifter
already exists. In firmware it does — it is part of the ARM operand encoding, so
`SUB r0, r0, r0, ASR #4` costs one instruction. In hardware the barrel shifters
must be built, and three of them cost what one multiplier costs.

Consequence: the hardware may as well use a real multiplier with **exact**
slopes, which would halve its approximation error (0.00023 vs 0.00043) for the
same area. The VHDL as written keeps the CSD coefficients so that the C model,
the ARM assembly and the hardware stay bit-identical and one vector file
validates all three; switching to exact slopes would be a deliberate trade of
that property for accuracy.

## 5. The divider is the design decision

| option | clocks | area | note |
|---|---:|---:|---|
| **restoring, 1 bit/clock** (implemented) | 14 | 454 GE | cheap, serial |
| reciprocal LUT + multiply | ~3 | ~4,470 GE | 256×16 ROM + a multiplier |

The LUT removes 11 of 20 clocks — a **2.2× faster instruction** — for **+71 %
area** (5,634 → 9,650 GE). Whether that is worth it depends on how much of the
whole program the instruction accounts for, which §6 answers: it is not.

This is also the same conclusion the firmware reached from the other direction.
There, the serial divide capped the 2-issue speed-up at 1.49× against a ceiling
of 2×, because adding issue slots cannot shorten a dependency chain. Both
analyses point at the same block.

## 6. Performance

Software cost of one angle + sin + cos, measured: trig is 963.9 instructions per
QR (`make instr-detail`) over 6 rotations = **160.7 instructions per rotation**.

| implementation | cost per rotation | vs software |
|---|---:|---:|
| software (`fixed_simd32`) | 160.7 instr | 1.00× |
| firmware, 1 issue slot | 101 cycles | 1.59× |
| firmware, 2 issue slots | 68 cycles | **2.36×** |
| firmware, 3 issue slots | 59 cycles | 2.72× |
| **hardware** | **20 clocks** | **8.03×** |

Hardware against firmware: **5.05×** over 1 slot, **3.4×** over 2 slots.

Instruction-for-cycle parity is assumed above, which *understates* the hardware:
the software mix averages ~1.87 cycles/instruction under the model in
`profiling/cycles.py`, which would put hardware at ~15×. That model still uses
placeholder weights, so the conservative figure is the one quoted.

### Whole-program effect

Amdahl, using the measured trig fraction (45.94 % of 2100.9 instructions/QR):

```
  current                    2100.9 instr/QR
  minus trig                 -963.9
  plus 6 rotations x 20        +120
  ------------------------------------
  with hardware GIVENSQ      ~1257 per QR   ->  1.67x whole-QR speed-up
  ceiling (GIVENSQ free)                        1.85x
```

**1.67× of a possible 1.85× — 90 % of the ceiling.** Making the instruction
faster still cannot beat 1.85×, which is why the reciprocal-LUT divider is not
worth its 71 % area: it would move the whole-program figure from 1.67× to about
1.77×, buying 6 % for nearly double the gates.

## 7. Penalty for the hardware solution

Required explicitly by Lesson 112 requirements II.

1. **Area.** 5,634 GE for one instruction. A small 32-bit integer core is on the
   order of 20–30 k GE, so this is roughly a **20–25 % enlargement of the core**
   to accelerate one algorithm.
2. **Multi-cycle latency breaks the single-cycle assumption.** At 20 clocks
   `GIVENSQ` cannot issue and retire like an ALU operation. The pipeline needs
   an interlock or a stall, which complicates control logic across the whole
   processor rather than only inside this unit.
3. **Compiler scheduling.** The notes raise this and it is real: a scheduler can
   only hide a 20-cycle latency if it has 20 cycles of independent work, and the
   Givens loop does not — the rotation immediately consumes c and s.
4. **Utilisation.** The unit is idle for every workload that is not doing a QR
   decomposition. That area is dead for general-purpose code.
5. **Verification.** A non-standard instruction needs its own model, vectors and
   testbench, all of which must be maintained in step — which is why they are
   generated from one source here.

Against those, one genuine non-cost: the unit is **stateless and reentrant**, so
it raises no interrupt, context-switch or exception-restart questions.

## 8. Open items

- [x] Simulated: 301/301 vectors pass, latency 20 clocks (`make hw-sim`).
      The first run failed 62 of 301 — three minimax intercepts were 1 LSB
      apart because each generator had its own copy of the table builder with a
      different sample count. All three now share one builder, and
      `make check-tables` asserts they stay identical.
- [ ] **Settle the `(0,0)` case.** `hw/vectors.txt` currently encodes the
      software behaviour: `atan2(0,0)` returns `+pi/2`, giving a 90° rotation
      rather than the identity. Harmless (still orthogonal, and `R[i][j]` is
      zeroed afterwards) and rare (0.06 per QR), but the C, the assembly and the
      VHDL all follow the vectors, so changing it means changing all three.
- [ ] Synthesise for real numbers instead of the estimate, if a toolchain is
      available; the per-block breakdown above is what the notes ask for, but a
      synthesis report would be stronger.
- [ ] Critical path is currently unbounded in the model — `S_TRIG` does a fold,
      a ROM read, a multiply and an add in one clock. A real design would
      register between the ROM read and the multiply, adding one clock and
      raising f_max.
