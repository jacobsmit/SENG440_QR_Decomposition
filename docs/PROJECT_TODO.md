# Remaining Work

Against the 12-step scenario in Lesson 100 (pp. 9-11) and the project
requirements in Lesson 112.

## Scope: settled

**One-sided Givens QR, single pass.** Justification, and its consequence for the
accuracy argument, are in `README.md`. Do not reopen.

## Status against the scenario

| # | step | state |
|---|---|---|
| 1 | specs, UML charts, blue-prints | **only the block diagram in `HARDWARE.md` §1** |
| 1 | operation as routine -> inline -> macro, perf for all three | **not done** |
| 2 | debug on workstation, build testbench | done |
| 3 | ARM assembly, profile, find bottleneck | done -- trig is 45.94 % |
| 4 | software optimisation (pipelining, unrolling) | **not done** for the rotation loop |
| 5 | hand-optimise assembly, assemble, run on ARM | done for `GIVENSQ` (`givensq_fw.S`) |
| 6 | instantiate the new instruction, inspect the listing | done -- `make asip-asm` |
| 7 | firmware, 1/2/3 issue slots, hand cycle counts | done -- `FIRMWARE_MICROCODE.md` |
| 8 | hardware, testbench, simulated latency | done -- `HARDWARE.md`, 301/301 pass |
| 9 | UML, blue-prints | **not done** |
| 10-12 | upload, report, present | **not done** |

Lesson 112: PWL error table done (`PWL_ERROR.md`); gate count, hardware penalty,
HW-vs-SW and 2-issue-FW-vs-SW speed-ups done (`HARDWARE.md`).

---

## Verification still owed

- [ ] **`make test-firmware` on the ARM VM.** Off-target it skips the
      assembly-vs-C equivalence check and says so. That check is the only thing
      standing behind "the C model, the assembly and the VHDL all agree", and
      the coefficient tables already drifted once undetected.
- [ ] Measure the dynamic instruction count of `givensq_fw_asm` under callgrind
      and compare with the hand-derived 101 cycles. Static is 485 because all 45
      segment routines are present; one executes per call.

## Measurement

- [ ] **The baseline may be overstated.** `README.md` quotes `naive_float` at
      3390.5 instr/QR, measured through the fixed-point interface and including
      libc startup that fell inside the collect toggle. At its native float
      interface, algorithm only, it is 2411 instr/QR -- which would make
      `fixed_simd32` **1.15x rather than 1.61x**. Settle before any speed-up
      figure goes in the report.
- [ ] A cycle-level comparison needs Cortex-A7 TRM latencies. Everything quoted
      today is instruction counts: defensible, but it understates the hardware.

## Design decision still open

- [ ] **The `(0,0)` case.** `atan2(0,0)` returns `+pi/2`, so the rotation is 90
      degrees rather than the identity. Harmless (still orthogonal, and `R[i][j]`
      is zeroed afterwards) and rare (0.06 per QR), but all four implementations
      follow it -- `givensq.h`, `givensq_fw.c`, `givensq_fw.S` and
      `hw/vectors.txt` -- so changing it means changing all four together.

## Software still owed

- [ ] **Routine -> inline -> macro** for the accelerated operation, with
      performance for all three (scenario step 1/3, explicitly required). Expect
      a code-size result rather than a speed-up. `sin_fixed` and `cos_fixed`
      call each other on the fold path, so neither can be fully inlined.
- [ ] **Compiler flag study**: `static.csv` covers 4 flag sets; add `-O0/-O1/-Os`
      and discuss the `-marm` vs `-mthumb` listings.
- [ ] **Software pipelining / unrolling of the rotation loop** (step 4) and hand
      assembly for it (step 5). The whole 4x4 matrix is 16 x `int16_t` = 8
      registers, so both active rows can stay in registers for a whole rotation.

## Numerical

- [ ] Broaden the test matrices: near-singular, ill-conditioned, zero-on-
      diagonal, plus a NumPy/LAPACK cross-check. Random uniform matrices are
      well-conditioned, so the `D == 0` path is barely exercised.
- [ ] State the error budget explicitly. Measured: relative error is flat across
      five orders of magnitude of input magnitude, so it is trig-limited rather
      than quantisation-limited.

## Report and submission

- [ ] Front page: title, authors, affiliation (UVic, Dept. of CS/ECE), student
      numbers, e-mail, and a blank dotted area for the submission date.
- [ ] Introduction: domain, performance requirements, enumerated contributions,
      project organisation.
- [ ] Theoretical background; design-process sections; performance/cost
      evaluation; conclusions; bibliography (Golub & van Loan, Trefethen & Bau,
      Demmel, Meyer, Brent).
- [ ] **Fix the draft's section 1.2: it claims Cortex-A9 with VFPv3.** The target
      is Cortex-A7 (ARMv7-A, VFPv4, in-order, has `SDIV`). A9 is out-of-order,
      which would also make hand cycle-counting indefensible.
- [ ] Rewrite the draft's section 4.2 methodology: it presents `clock()`
      timings, which measure the host under QEMU.
- [ ] **UML / block diagrams** (step 9): system block diagram, trig-unit
      datapath, microcode engine organisation, algorithm flowchart. The datapath
      diagram in `HARDWARE.md` §1 is a starting point.
- [ ] Formatting: 11-12 pt, single spaced, <= 20 pages.
- [ ] Fill in the Lesson 112 specification sheet; the deadline field is blank in
      the material reviewed -- confirm it.
- [ ] Presentation slides; upload C/asm/VHDL/UML + slides, then the report.
