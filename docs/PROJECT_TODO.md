# SENG 440 Project — Remaining Work

Derived from:
- `SENG440_2026_Lesson_100_Course_Notes` — "Project goal: Exposure to hardware-software co-design" (9 steps), "Project scenario – example (I)–(III)" (12 steps), "What to do for the 'firmware' section", "Report organization"
- `docs/Matrix_Diagonalization-1.md` — "Matrix diagonalization – project requirements I & II" (lines 754–826)
- Current state of `src/`, `tests/`, `profiling/`, and `SENG 440 Draft Report.pdf`

Status legend: `[x]` done · `[~]` partial · `[ ]` not started

---

## 0. Scope decision to resolve first (blocking)

- [ ] **Confirm QR-only factorization satisfies the "matrix diagonalization" project.**
  Project 12 in Lesson 100 lists "QR decomposition" as an acceptable topic, so the choice is
  fine. But Lesson 112's requirements say *"Diagonalize a square matrix using piecewise linear
  approximation of trigonometric functions and estimate..."*, and the notes explicitly call
  triangularization a **side effect** of the Jacobi method, not the goal. Right now
  `qr_decomposition()` performs **one** factorization → `A = QR`. Nothing is diagonalized,
  there are no sweeps, and no eigenvalues/singular values are produced.

  Two ways to close this, pick one:
  1. **Add the outer QR iteration** (unshifted QR algorithm): `A_{k+1} = R_k · Q_k`, repeat until
     the sub-diagonal is below a threshold. Eigenvalues then appear on the diagonal. This keeps
     all existing Givens/fixed-point/trig work, and gives you the sweep-count and convergence
     analysis the notes ask for. **Recommended** — small amount of new code, satisfies the wording.
  2. Ask the instructor to confirm a single QR factorization is in scope, and get it in writing.

  Everything below assumes the existing Givens + fixed-point + PWL-trig core is kept either way.

---

## 1. Specification requirements not yet met (Lesson 112, requirements I & II)

- [ ] **1.1 Generate square matrices of 12-bit integers.** Spec: *"Generate a square matrix of
  integers (12-bit wordlength, for example)."* Currently `main.c` and `test_qr_accuracy.c` use
  four hardcoded small float matrices converted with `FLOAT_TO_FIXED`. Need a generator producing
  random 12-bit signed integer matrices, plus a fixed seed so results are reproducible in the report.
  **This interacts with the overflow bug in §4.1 — 12-bit inputs will overflow the current Q11 macros.**
- [~] **1.2 Max error of the PWL approximations at three middle points.** Spec asks specifically for
  *"the maximum error for three middle points"* for arctan, sin and cos.
  `find_trig_approx_parameters.py` minimizes max error over the domain but does not report the
  three-middle-point errors, and the report has no error table. Produce a table:
  per-function, per-segment breakpoint, slope/intercept (float and Q11 integer), max error,
  and error at the three middle points. Compare against the reference approximation given in the
  notes (`0.928x` / `0.644x ± 0.142`) — §3.1.1 of the draft claims the error is "equal or less"
  but never shows the numbers.
- [ ] **1.3 Define the new instruction.** Must be ARM-compliant: **at most 2 input operands and 1
  result**. Note the design problem to discuss in the report: a Givens rotation needs *both* `sin`
  and `cos`, which cannot both come back from one 2-in/1-out instruction. Options to write up:
  two instructions (`TRIG_SIN`/`TRIG_COS`), one instruction returning both packed as two Q11
  halves in a 32-bit result, or a non-reentrant unit (Read/Read/Execute/Write) — the notes raise
  the interrupt/exception implications of the non-reentrant option, address them.
- [ ] **1.4 Instantiate the new instruction from C via inline assembly** and inspect the generated
  listing: `__asm__("TRIG %0,%1,%2" : "=r"(s) : "r"(a),"r"(b));`, then
  `arm-linux-gnueabihf-gcc -static -march=armv5 -S`. Compile only — it will not assemble, which
  is expected and should be stated.
- [ ] **1.5 Firmware (microcode) implementation.** Explicitly required.
  - [ ] Write the micro-operation sequence for the trig unit using the ARM instruction set
        **without multiply/divide** (so the PWL multiply becomes shift-and-add).
  - [ ] Vertical microcode (1 issue slot) — cycle count, computed by hand.
  - [ ] Horizontal microcode, 2 issue slots — cycle count; report speedup and how close to the 2×
        ceiling; note the NOP-filled slots.
  - [ ] Also do the **3 issue-slot** comparison — the notes ask for it ("How close are you to the
        maximum speed-up of 3×?").
- [ ] **1.6 Custom hardware implementation.** VHDL or Verilog for the new instruction, plus a
  testbench with known inputs/outputs, simulated to get the latency.
- [ ] **1.7 Gate count.** Spec: *"Determine the number of gates needed to implement the new
  instruction."* Educated estimate is acceptable per Lesson 100 (comparator + multiplier/shifter +
  adder + mux, assume 1 cycle per addition) — but show the breakdown.
- [ ] **1.8 Estimate the penalty for the hardware solution** (area/cost, and the architectural cost
  of a non-standard unit — pipeline/compilation impact, as raised in "Architectural constraints").
- [ ] **1.9 The three headline performance numbers:**
  - [ ] HW-based vs SW-based speedup
  - [ ] 2-issue-slot FW vs SW speedup
  - [ ] cost/penalty alongside each

---

## 2. Software work still missing (Lesson 100 design flow)

- [ ] **2.1 The naive floating-point baseline does not exist.** Draft report §2.3 describes a
  baseline using floats and `math.h` (`sqrt`, `atan`) — it is not in the repo and never was
  (checked the full git history). Every speedup number in the report needs it. Write
  `src/software_naive/` with the textbook float Givens implementation and measure it.
- [ ] **2.2 The routine → inline routine → macro ladder.** Lesson 100 step 1 requires the
  bottleneck operation implemented all three ways, with performance reported **for all three**.
  Right now the trig functions are plain functions only. Add `static inline` and macro variants
  and measure each.
- [ ] **2.3 Compiler flag study.** Generate and compare ARM assembly for the bottleneck across
  `-O0/-O1/-O2/-O3/-Os` and `-marm` vs `-mthumb`, as demonstrated in the notes. Include the
  interesting listings (predicated instructions etc.) in the report.
- [ ] **2.4 Hand-optimized assembly.** Loop unrolling and software pipelining of the Givens inner
  loops, eliminate branches, control register allocation. Then assemble and run on ARM.
- [ ] **2.5 Run and measure on actual ARM.** Everything so far was compiled and timed on the macOS
  host with `gcc` and `clock()`. The 32-bit ARM QEMU VM is already downloaded
  (`../ARMHF32_VM/`) with sftp config in `../ARM_VM_Local/.vscode/sftp.json`, so this is mostly
  wiring: cross-compile `arm-linux-gnueabihf-gcc -static -march=armv5`, push, run, record.
- [ ] **2.6 Use the profiling tools the course teaches**, not just `clock()`:
  - [ ] `gprof` (`-pg`) for the function-level bottleneck → this is what justifies the new instruction
  - [ ] `valgrind --tool=callgrind` + `callgrind_annotate` for instruction counts
  - [ ] `valgrind --tool=cachegrind --branch-sim=yes` for cache misses and branch stats — directly
        supports the report's §3.3.2 (branch misprediction) and §3.3.4 (strided access on Q) claims,
        which are currently asserted without evidence.
- [ ] **2.7 Amdahl's-law framing.** State what fraction of runtime the trig/angle calculation is,
  and derive the maximum achievable speedup from accelerating it. This is the argument for the
  whole ASIP exercise and it's currently absent.
- [ ] **2.8 (Optional, high value) NEON SIMD version** of the row/column update — report §5.1.
  The SBC toolchain supports it: `-mfloat-abi=softfp -mfpu=neon`. Do this only after the required
  firmware/hardware sections are done.

---

## 3. Numerical analysis the notes ask for

- [ ] **3.1 Truncation and round-off error analysis.** Lesson 100 calls these out as "of particular
  concern". Current `FIXED_MUL` uses `>> 11`, which truncates toward −∞ for negatives — an
  asymmetric rounding bias across the whole algorithm. Quantify it, and consider round-to-nearest
  (`(x + 1024) >> 11`).
- [ ] **3.2 Dynamic range / wordlength justification.** Report §3.2.1 claims "1 sign + 20 integer +
  11 fractional bits" — that's 32 bits total and is only true of the *storage* format, not of the
  intermediate products (see §4.1). Justify Q11 against the actual required range once 12-bit
  integer inputs are used.
- [ ] **3.3 Verify sin/cos have *equal* accuracy.** The notes: *"In any case with equal accuracy, or
  the rotation will no longer be a rotation."* Add a check on `c² + s² ≈ 1` in Q11 and report the
  deviation. Current orthogonality error reaches 0.048, which is this effect.
- [ ] **3.4 Accuracy budget.** Current max reconstruction error is 0.2803 (SPD test case). State what
  error is acceptable and why, rather than just reporting the number.

  Useful measured result for this section: across input magnitudes from ±1 to ±2047 (overflow
  fixed), the orthogonality error stays at **≈ 0.09 regardless of scale**, and reconstruction
  error is a constant **≈ 9 % of the input magnitude**. Scale-invariance proves the error budget
  is dominated by the **piecewise-linear trig approximation**, not by fixed-point quantisation.
  Consequence: adding segments to the sin/cos approximations buys far more accuracy than adding
  fractional bits. Worth stating explicitly — it justifies the §3.6 / CORDIC direction and it is
  exactly the "partial rotation" effect the notes describe.
- [ ] **3.5 Convergence data** (needed if §0 option 1 is taken): sweeps to converge vs. trig
  accuracy, showing that low-accuracy rotations mean more sweeps — the exact trade-off the notes
  describe ("incomplete rotations should be simpler to implement, but more sweeps might be needed").
- [ ] **3.6 Consider the notes' suggestion** of restricting rotation angles to a predefined set with
  prestored/shift-computable sin/cos, and *"consult the student(s) doing the CORDIC project."*
  At minimum discuss it; the draft already promises CORDIC in §5.2.

---

## 4. Code defects and cleanup

- [ ] **4.1 `FIXED_MUL` overflows.** `math_utils.h:11` — `((int32_t)(a) * (int32_t)(b)) >> 11`
  multiplies in 32 bits *before* shifting.

  Every `FIXED_MUL` in this codebase is *coefficient × data*, where the coefficient is `c`, `s`,
  or a PWL slope, all ≤ 2048 = 2¹¹. So the product overflows when
  `|data_raw| ≥ 2³¹/2¹¹ = 2²⁰`, i.e. a matrix element magnitude of **±512.0**.
  (Not √(2³¹) ≈ ±22.6 — that bound would only apply if two same-magnitude values were
  multiplied together, which never happens here.)

  Measured over 2000 random integer matrices per magnitude, Q11:

  | max entry | trials that overflow | worst recon. error |
  |---:|---:|---:|
  | ≤ 256  | 0 %     | 22.5 |
  | ≤ 512  | 64.5 %  | 1237 |
  | ≤ 1024 | 100 %   | 2141 |
  | ≤ 2047 | 100 %   | 2918 |

  So the current code is correct only up to ±256, and the 12-bit inputs required by §1.1 break it
  completely. With a 64-bit intermediate the same sweep gives worst-case error 189 at ±2047 —
  i.e. ~9 % relative, no overflow, all magnitudes usable.

  **Fix:** `#define FIXED_MUL(a,b) ((int32_t)((((int64_t)(a) * (int32_t)(b)) + 1024) >> 11))`

  This does **not** violate the 32-bit requirement — see §4.1a. Verified codegen at
  `-O2 -marm -march=armv5te`:

  ```
  smull r0, r1, r3, r0      @ native 32x32->64, ARMv4 and later
  lsr   r0, r0, #11
  orr   r0, r0, r1, lsl #21
  ```

  One `SMULL` plus a two-instruction 64-bit funnel shift — **no `__aeabi_lmul` libgcc call**.
  Cost is 2 extra instructions over the broken 2-instruction version. Also verified: adding
  round-to-nearest (`+ 1024`) is **free** — gcc folds the constant into `SMLAL`, same instruction
  count. So take the rounding fix at the same time (see §3.1).

- [ ] **4.1a Justify the 64-bit intermediate in the report** (turn the fix into a contribution):
  - "32-bit wordlength" in the spec sheet constrains the **data word** — matrix storage, and the
    custom instruction's 2-input/1-output register interface. It does not constrain the width of
    an internal product.
  - A fixed-point multiply *mathematically* produces 2N bits; selecting the correct 32-bit field
    from a 64-bit product **is** Q-format multiplication. Truncating to 32 bits before the shift
    is simply a defect.
  - On 32-bit ARM the 32×32→64 multiply is one native instruction writing a **register pair**.
    The datapath stays 32-bit; nothing here needs a 64-bit machine.
  - This is also the faithful model of the hardware being designed in §1.6: a 32×32 multiplier
    array produces a 64-bit product internally and the unit taps bits [42:11]. Operands and
    result remain 32-bit registers, so the ARM 2-in/1-out constraint still holds.
  - Alternatives to mention (and reject, with reasons): `SMMUL` (ARMv6+, returns the top 32 bits
    in a single destination register — elegant, but needs Q31-style rescaling; Cortex-A9 has it);
    rescaling matrix storage to Q0/Q4 so 32-bit products suffice; or pre-shifting both operands
    before multiplying (cheapest, worst accuracy).
  - Practical note: the course notes' `-march=armv5` is **rejected by current gcc** ("unrecognized
    -march target"). Use `-march=armv5te`. `SMULL` is available from ARMv4 onward, so nothing is
    lost.
- [ ] **4.2 `FIXED_DIV` overflows and has no zero guard.** `math_utils.h:12` — `((a) << 11) / (b)`
  overflows for `|a| > 2²⁰`. The `D == 0` case is handled in `calculate_arctan_ratio` but not in
  the macro itself.
- [ ] **4.3 `tests/run_tests.sh` always prints "All tests passed successfully!"** regardless of the
  errors measured — `test_qr_accuracy.c` only prints numbers, it never asserts. Add per-test
  tolerance thresholds and a non-zero exit on failure. Lesson 100 requires a real testbench
  ("a set of input data with known output data").
- [ ] **4.4 Profiler iteration labels are wrong by 10×.** `profiling/profile_components.c:44` and
  `:70` loop `NUM_ITERATIONS_MATH * 10` = 100M iterations but print `[10M ops]`
  (lines 63 and 91). Any per-operation cost derived from this is off by 10×. Fix before the
  numbers go in the report.
- [ ] **4.5 Broaden the test matrices.** Four hardcoded cases today. Add: near-singular,
  ill-conditioned (the notes make a point of ill-conditioning), zero on the diagonal
  (`R[j][j] == 0` → the `D == 0` path), all-zero row, and large-dynamic-range matrices.
  Cross-check `Q`/`R` against a NumPy/LAPACK reference rather than only the `A ≈ QR` invariant.
- [ ] **4.6 `MAT_SET(R, i, j, 0)` in `qr_decomp.c:60`** force-zeroes the eliminated element, which
  masks how incomplete each rotation actually was. Either measure the residual before zeroing and
  report it, or justify the choice explicitly.
- [ ] **4.7 Build system.** The `Makefile` only builds `src/software_base`; tests and profiling
  duplicate their own `gcc` command lines. Add targets for tests, profiling, native vs
  ARM cross-compile, and the `-S` assembly-listing outputs.
- [ ] **4.8 Add a `.gitignore`** for `.DS_Store` and build products (a `main` binary was committed
  earlier and later deleted; two `.DS_Store` files are untracked right now).

---

## 5. Documentation, diagrams and submission

- [ ] **5.1 UML / block diagrams / blueprints.** Lesson 100 scenario step 9: *"Describe the computing
  scenario, the organization, as well as the components of the system in UML, and provide the
  blue-prints of the design."* Nothing exists yet. Needs: system block diagram (ARM core +
  custom unit + register file), the datapath of the trig unit, the microcode engine organization
  (control store / CSAR / MIR / sequencer), and an algorithm flowchart.
- [~] **5.2 Report** — 8-page draft exists and is well structured. Against the required organization:
  - [x] Front page with title and authors
  - [ ] Front page needs **affiliation (UVic, Dept. of CS/ECE), student numbers, e-mail addresses,
        and a blank dotted area for the submission date**
  - [x] Introduction with domain description
  - [ ] Introduction needs **performance requirements, an enumerated list of contributions, and a
        project-organization paragraph**
  - [~] Theoretical background — exists but thin; add the Givens/Jacobi math and the
        diagonalization framing
  - [~] Design-process sections — good on trig/fixed-point; missing firmware, hardware, assembly,
        and the new instruction
  - [ ] **Performance/cost evaluation section** — currently only "Next Steps" placeholders; this is
        where the speedup/gate-count/penalty numbers go
  - [ ] **Conclusions**
  - [ ] **Bibliography** — none yet; the notes list Golub & van Loan, Trefethen & Bau, Demmel,
        Meyer, Brent
  - [ ] Formatting check: 11- or 12-point, single spaced, **≤ 20 pages**
  - [ ] Fill in the **Project Specification Sheet** (student names, IDs, matrix size/type,
        language, processor, wordlength, deadline) from Lesson 112
- [ ] **5.3 Presentation slides.**
- [ ] **5.4 Upload to Brightspace:** all C, assembly, VHDL/Verilog, UML files, and the slides —
  then the technical report as a separate upload.
- [ ] **5.5 Present the work and answer questions.**
- [ ] **5.6 Confirm the deadline** — the spec sheet's deadline field is blank and no date appears in
  any of the material reviewed.

---

## Suggested order

1. §0 scope decision (blocks how much else you write) — ask now, work on §4.1–4.4 while waiting
2. §4.1–4.4 defect fixes, then §1.1 12-bit integer generator → gets the numbers trustworthy
3. §2.1 naive float baseline, §2.5 get onto ARM, §2.6 real profiling → establishes the bottleneck
4. §1.2 error tables and §2.2 routine/inline/macro ladder → the "software" deliverables
5. §1.3–1.4 new instruction + inline asm, §2.3–2.4 assembly study
6. §1.5 microcode (1/2/3 issue slots) — the biggest untouched required chunk
7. §1.6–1.8 VHDL + gate count + penalty
8. §1.9 + §3 the evaluation numbers and numerical analysis
9. §5 diagrams, report, slides, submission
