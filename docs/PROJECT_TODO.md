# Remaining Work

From Lesson 100 (co-design goal, 12-step scenario, report organization) and
Lesson 112 (project requirements I & II).

Done: 4 variants, accuracy regression suite, ARM measurement on target,
`GIVENSQ` defined and instantiated. Results in `README.md`.

---

## Blocking question

- [ ] **Confirm QR alone satisfies the "matrix diagonalization" project.** Lesson
      100 lists QR decomposition as an acceptable topic, but Lesson 112 says
      *"Diagonalize a square matrix..."* and calls triangularization a side
      effect. We compute one factorization: no sweeps, no eigenvalues.
      Cheapest fix if it is not enough: add the outer QR iteration
      (`A_{k+1} = R_k Q_k` until the sub-diagonal is small), which keeps all
      existing work and supplies the convergence analysis the notes ask for.

## Firmware — §1.5, nothing exists, explicitly required

- [ ] Micro-operation sequence for `GIVENSQ` using the ARM instruction set
      **without multiply or divide** (PWL multiplies become shift-adds).
      `givensq_ref()` in `src/common/givensq.h` is the specification.
- [ ] Vertical microcode (1 issue slot) — cycle count, by hand.
- [ ] Horizontal microcode, **2 issue slots** — cycle count, speed-up, how close
      to the 2× ceiling, and where the NOPs are.
- [ ] **3 issue slots** as well; the notes ask for it.
- [ ] Slopes must be shift-add cheap. Re-run
      `scripts/trig_approx_parameters/find_trig_approx_parameters.py` with slopes
      constrained to CSD weight ≤ 2. Measured: total add/sub across the six
      slopes drops 18 → 5, and arctan improves (0.0184 → 0.0119) because
      refitting the intercept more than pays for the coarser slope. Caution: cos
      error doubles (0.0121 → 0.0226) and cos error enters orthogonality
      squared — verify against the accuracy suite.

## Hardware — §1.6–1.8, nothing exists

- [ ] VHDL/Verilog for `GIVENSQ` + testbench driven by vectors from
      `givensq_ref()`. Simulate for latency.
- [ ] **Divider is the design decision**: restoring divider (~12 cycles, cheap)
      vs reciprocal LUT + multiply (~2–3 cycles, costs ROM). It dominates both
      latency and gate count.
- [ ] Exploit what hardware buys over firmware: sin and cos evaluate in
      parallel, and both PWL segments can be computed concurrently and muxed —
      no branch, no mispredict.
- [ ] Gate count with a per-block breakdown.
- [ ] Hardware penalty: area, plus the architectural cost of a long-latency
      non-standard unit (compiler scheduling, pipelining).
- [ ] **Settle the `(0,0)` edge case first** — see the note in `givensq.h`. The C
      model, microcode and VHDL must agree.

## Evaluation — §1.9

- [ ] HW vs SW speed-up, and 2-issue-FW vs SW speed-up.
- [ ] Cortex-A7 TRM latencies for `SDIV`, `MUL`, `SMLAD`, VFP ops — cited.
      Replace the placeholders in `profiling/cycles.py`.
- [ ] Settle whether fixed point beats `naive_float` **on cycles**. On
      instructions it is 1.71×, but that flips if an average VFP op costs
      1.26–2.34 cycles, which is inside the plausible range. Needs the TRM.
- [ ] Amdahl ceiling: trig is 910 of 2058 instructions/QR in `fixed_simd32`
      (44.2 %), so `GIVENSQ` caps at 1.79×. Report the capped figure, not just
      the kernel speed-up.

## Software still owed

- [ ] **§2.2 routine → inline → macro** for the accelerated operation, with
      performance for all three. Required. Measured `stack` is only 21.5
      instructions/QR (~1 %), so expect a code-size result, not a speed-up.
      Note `sin_fixed` and `cos_fixed` call each other on the fold path and so
      cannot both be fully inlined.
- [ ] **§2.3 compiler flag study** — `static.csv` covers 4 flag sets; add
      `-O0/-O1/-Os` and discuss the `-marm` vs `-mthumb` listings.
- [ ] **§2.4 hand-optimised assembly**, aimed at memory not arithmetic:
      arithmetic is 6 % of instructions, memory 37 %, ALU 47 %. Biggest idea: the
      whole 4×4 matrix is 16 × `int16_t` = 8 registers, so both active rows can
      stay in registers for a whole rotation. Also software pipelining (the A7 is
      in-order). Must assemble and run.
- [ ] **§1.2 PWL error table** — max error and error at three middle points for
      arctan/sin/cos, against the notes' reference (`0.928x`, `0.644x ± 0.142`).
- [ ] A demo `main.c` was removed in the restructure; the test suite covers
      correctness, but check whether a runnable demo is expected for submission.

## Numerical analysis

- [ ] Truncation/round-off: `fixed_mul` truncates toward −∞ for negatives. Free
      round-to-nearest is available (`SMLAL` folds the constant in).
- [ ] Verify `c² + s² ≈ 1`; the notes warn that unequal sin/cos accuracy stops
      the rotation being a rotation. The ~0.09 orthogonality error is this.
- [ ] State the error budget. Measured: relative error is 6.9–10.3 % across five
      orders of magnitude of input, so it is trig-limited, not quantisation-
      limited. More sin/cos segments is the only lever.
- [ ] Broaden test matrices: near-singular, ill-conditioned, zero-on-diagonal,
      NumPy/LAPACK cross-check. Random uniform matrices are well-conditioned, so
      the `D == 0` path is barely exercised (0.06/QR).

## Report and submission — §5, nothing started

- [ ] Front page: title, authors, **affiliation (UVic, Dept. of CS/ECE), student
      numbers, e-mail, and a blank dotted area for the submission date**.
- [ ] Introduction: domain, performance requirements, **enumerated
      contributions**, project organisation.
- [ ] Theoretical background; design-process sections; **performance/cost
      evaluation**; conclusions; **bibliography** (Golub & van Loan, Trefethen &
      Bau, Demmel, Meyer, Brent).
- [ ] **Fix §1.2 of the draft: it claims Cortex-A9 with VFPv3.** The target is
      Cortex-A7 (ARMv7-A, VFPv4, in-order partial dual-issue, has `SDIV`). A9 is
      out-of-order, which would also make hand cycle-counting indefensible.
- [ ] Rewrite the draft's §4.2 methodology: it presents `clock()` timings, which
      measure the host under QEMU.
- [ ] **UML / block diagrams / blueprints** (scenario step 9): system block
      diagram, trig-unit datapath, microcode engine organisation, algorithm
      flowchart.
- [ ] Formatting: 11–12 pt, single spaced, **≤ 20 pages**.
- [ ] Fill in the Lesson 112 specification sheet; **the deadline field is blank
      and no date appears in any material reviewed — confirm it.**
- [ ] Presentation slides; upload C/asm/VHDL/UML + slides, then the report.
