# Target Platform — What We Are Actually Running On, and Why

## 1. The definitive answer

From the instructor's own launch script, `ARMHF32_VM/ARMHF32_config_script_linux.sh`
(identical in `ARMHF32_run_wo_UEFI_win.cmd`):

```
qemu-system-arm -M virt -cpu cortex-a7 -m 2G \
  -kernel ./vmlinuz_installed -initrd ./initrd_installed.img \
  -append "console=ttyAMA0 root=/dev/vda2 rw" \
  -device virtio-blk-pci,drive=hd0 -drive file=armhf32_VHD.qcow2,... \
  -netdev user,id=net0,hostfwd=tcp::2222-:22 -device virtio-net-pci,netdev=net0 \
  -nographic -serial mon:stdio
```

| property | value |
|---|---|
| CPU | **ARM Cortex-A7** |
| Architecture | **ARMv7-A**, 32-bit (AArch32), hard-float ABI (armhf) |
| Machine model | QEMU `virt` (not a real board — virtio disk/net, PL011 UART) |
| RAM | 2 GB |
| Guest OS | Debian 13 armhf (from `ARMHF32_install_win.cmd`) |
| Host access | SSH on `localhost:2222`, root/`seng440` (see `ARM_VM_Local/.vscode/sftp.json`) |
| QEMU (local) | 11.0.1, installed at `/opt/homebrew/bin/qemu-system-arm` — `cortex-a7` confirmed available |

So the answer is **Cortex-A7**, and it is not a matter of opinion — it is pinned by the script.

## 2. This contradicts the draft report

Report §1.2 currently claims **Cortex-A9 with VFPv3**. That is wrong on several counts:

| claim in §1.2 | reality on Cortex-A7 |
|---|---|
| Cortex-A9 | Cortex-A7 |
| VFPv3 | VFPv4 (+ optional NEON) |
| "8-stage superscalar pipeline" | A7 is **in-order, partial dual-issue**; A9 is out-of-order superscalar |
| "Division is slow" | A7 has **hardware `SDIV`/`UDIV`**; A9 has none |
| "32 KB I-cache, 32 KB D-cache, shared L2" | A7 L1 is configurable 8–64 KB — and **QEMU models no cache at all** |

- [ ] **Fix §1.2 to target Cortex-A7.** This is the honest and more defensible option, since it is
      what you can actually run. If you would rather keep A9 as the notional design target, you
      must say explicitly that the A7 VM is used only for functional verification — but there is
      no reason to do this, and it invites the question of why.

## 3. What is architecturally specific to Cortex-A7 that we are using

Verified by `arm-none-eabi-gcc -mcpu=cortex-a7 -dM -E`:

```
__ARM_ARCH 7            __ARM_FEATURE_IDIV 1     __ARM_FEATURE_DSP 1
__ARM_ARCH_ISA_ARM 1    __ARM_FEATURE_CLZ 1      __ARM_FEATURE_SIMD32 1
__ARM_ARCH_ISA_THUMB 2  __VFP_FP__ 1
```

Four of these matter to this project:

### 3a. `SMULL` — 32×32→64 multiply (available since ARMv4)
The fix for the `FIXED_MUL` overflow (TODO §4.1). One instruction, writes a register pair,
datapath stays 32-bit. Also `SMLAL` folds the round-to-nearest constant in for free.

### 3b. `__ARM_FEATURE_IDIV` — hardware `SDIV` **(this changes the bottleneck story)**
Measured, compiling the actual `math_utils.c`:

| target | `__aeabi_idiv` calls | native `sdiv` | `calculate_arctan_ratio` |
|---|---:|---:|---:|
| `-march=armv5te` (what the notes tell you to use) | 2 | 0 | **89 instr** |
| `-mcpu=cortex-a7` (what the VM is) | 0 | 2 | **64 instr** |
| `-mcpu=cortex-a9` (what the report claims) | 2 | 0 | 89 instr |
| `-mcpu=cortex-a15` | 0 | 2 | — |

**The compile flag, not the hardware, decides whether division is a subroutine call.** Compile
for `armv5te` and run it on the A7 and you still pay the libgcc call, because the binary never
issues `SDIV`. This has to be stated and justified in the report — it is the single most
consequential methodological choice in the performance section.

- [ ] **Decide and document the flag.** Recommended: report **both**, since it is a free and
      genuinely interesting result — `-march=armv5te` as the "baseline embedded core without a
      divider" and `-mcpu=cortex-a7` as "the actual target". The 89 → 64 instruction delta is
      entirely the divider.
- [ ] Note the notes' `-march=armv5` is **rejected by modern gcc**; use `armv5te`.

### 3c. `__ARM_FEATURE_SIMD32` + `__ARM_FEATURE_DSP` — packed 16-bit SIMD in 32-bit registers
This is the best architectural fit in the whole project, and it is currently unused.

A Givens row update is *exactly* a pair of dual multiply-accumulates:

```c
row_j[k] = (c*tj + s*ti) >> 11;   /* SMLAD   */
row_i[k] = (c*ti - s*tj) >> 11;   /* SMUSDX  */
```

Verified codegen (`-O2 -marm -mcpu=cortex-a7`, via `arm_acle.h`):

```
giv_add_simd32:  smlad   r0, r0, r1, r3   @ Ra + Rn.lo*Rm.lo + Rn.hi*Rm.hi
                 asr     r0, r0, #11
giv_sub_simd32:  smusdx  r0, r0, r1       @ Rn.lo*Rm.hi - Rn.hi*Rm.lo
                 asr     r0, r0, #11
```

Two 16×16 multiplies plus the add/sub, in **one instruction each**. The current inner loop is
4 multiplies + 2 add/sub + 2 shifts ≈ 8 instructions; this is 4. Roughly 2× on the rotation
kernel — and it operates entirely within 32-bit registers, so it sits comfortably inside the
"32-bit wordlength" constraint in a way NEON's 64/128-bit vectors arguably do not.

**Two caveats, both worth writing up rather than hiding:**
- gcc does **not** auto-form these from scalar C (verified — it emits plain `mul`). You must use
  the ACLE intrinsics `__smlad`/`__smusdx` from `arm_acle.h`, or inline assembly. That makes this
  a legitimate hand-optimisation contribution, not a compiler freebie.
- **Operands must fit signed 16-bit.** `c`,`s` in Q11 fit (≤ 2048, 13 bits). Matrix elements do
  not: 12-bit integer inputs in Q11 need 23 bits. So SIMD32 forces a format decision — e.g.
  matrix storage in Q4 (12 integer + 4 fractional = 16 bits exactly), or a reduced input range.
  This is a real co-design trade-off: **the wordlength choice determines whether the SIMD path is
  available at all.** Exactly the "fixed-point arithmetic requires a little bit of attention"
  theme from Lesson 100.

- [ ] Replaces the vague "research any extensions to use, such as NEON" in report §5.1 with
      something concrete, measured, and defensible.

### 3d. NEON — available, but probably the wrong choice
`-mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=softfp` does define `__ARM_NEON 1`, so it is there.
But NEON registers are 64/128-bit, which cuts against the 32-bit wordlength spec, and the 4×4
matrix is small enough that load/permute overhead eats the gain. Mention it, prefer SIMD32, and
say why — that is a stronger answer than using it.

### 3e. In-order pipeline — why A7 is actually the *better* target pedagogically
Cortex-A9 is out-of-order superscalar; hand-counting cycles on it is not really possible.
Cortex-A7 is **in-order with partial dual-issue**, so a hand cycle count is tractable and
defensible. Since the course requires exactly that (see §4), the A7 is the more suitable target —
worth saying so explicitly instead of treating it as an accident of the VM image.

- [ ] Get the actual instruction latencies (multiply, `SDIV`, `SMLAD`, load/store) from the **ARM
      Cortex-A7 MPCore Technical Reference Manual**, cycle-timings appendix, and cite it. Do not
      take latency figures from me or from general recollection — the TRM is the citable source
      and the report needs one.

## 4. The methodology trap: QEMU is not a timing model

**QEMU TCG translates ARM basic blocks to host code and runs them at host speed. There is no
pipeline model, no cache model, and no cycle accounting.** Consequences:

| measurement | valid in the VM? |
|---|---|
| Functional correctness, accuracy | **Yes** — this is what the VM is for |
| Static instruction counts (`-S` and count) | **Yes** — a property of the binary |
| Dynamic instruction counts (`callgrind`) | **Yes** — architecturally meaningful |
| Wall-clock / `clock()` timing | **No** — measures host speed, not A7 cycles |
| Cache misses (`cachegrind`) | **No** — models a hypothetical cache, not the A7's |
| Branch mispredictions | **No** — no branch predictor is modelled |
| Cycle counts | **No** — must be hand-computed from the TRM |

This is consistent with the course's own framing: Lesson 100 calls it an "ARM processor
simulator" and says microcode cycle counts are *"computed manually, since no simulator is yet
available"*. The course expects hand-counted cycles.

**So the evaluation methodology should be:**
1. **Correctness** — run the test suite in the VM.
2. **Instruction counts** — static from `-S`, dynamic from `callgrind` in the VM. This is the
   primary quantitative axis and it is fully sound.
3. **Cycle counts** — hand-computed from Cortex-A7 TRM latencies, for software, and for the
   1/2/3-issue-slot microcode engines.
4. **Never** quote QEMU wall-clock as a cycle result.

- [ ] **This invalidates the current `profiling/` numbers twice over**: they were taken on the
      Apple-Silicon host (not ARM at all), and `clock()` in QEMU would not have been valid either.
      Keep the harness — retarget it to count instructions rather than seconds.
- [ ] Report §4.2 currently presents `clock()` timings as the performance method. Rewrite it
      around instruction counts + hand-derived cycles, and state the QEMU limitation explicitly.
      Naming this limitation is a *strength* in a co-design report, not an admission.

## 5. Optional: real hardware for real timing

Lesson 100 §"ARM Single-Board Computer" offers a real board on the UVic intranet:
`ssh seng440.ece.uvic.ca`, then `telnet arm` (users `user1`–`user4`, password in the notes).
Toolchain at `/opt/arm/4.3.2/bin/arm-linux-gcc`, supports `-mfpu=neon`.

This is the only place you can get **real** timing. If the schedule allows, it strengthens the
evaluation considerably.

- [ ] Run `cat /proc/cpuinfo` on the board and record the actual CPU. Do not assume — the gcc
      4.3.2 vintage plus NEON support hints at a Cortex-A8-era part, but that is a guess and the
      report should state a verified fact.
- [ ] If you use the board, note it is a *different* CPU from the A7 VM, and say which numbers
      came from which.

## 6. Action list

- [ ] Correct report §1.2 to Cortex-A7 / ARMv7-A, with the feature list from §3 above
- [ ] Choose and justify the compile flags; report `armv5te` and `cortex-a7` side by side
- [ ] Rewrite report §4.2's methodology around instruction counts, not `clock()`
- [ ] State the QEMU-is-not-cycle-accurate limitation explicitly
- [ ] Cite the Cortex-A7 TRM for all latency figures
- [ ] Add the SIMD32 (`SMLAD`/`SMUSDX`) path and the Q-format trade-off it forces
- [ ] Verify the SBC's real CPU if you use it
