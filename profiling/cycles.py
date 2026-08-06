#!/usr/bin/env python3
"""
Dynamic opcode histogram and cycle estimate.

WHY THIS EXISTS
    Instruction counts treat an SDIV and a MOV as equal. They are not. This
    turns callgrind's per-instruction execution counts into a histogram of
    which *opcodes* actually executed, then weights them by Cortex-A7
    latencies to produce a cycle estimate.

    Cycles cannot be MEASURED on this target -- QEMU has no timing model and
    the emulated PMU is fiction. Computing them from exact opcode counts is
    what the course asks for ("computed manually, since no simulator is yet
    available", Lesson 100). A weighted sum is defensible here specifically
    because the Cortex-A7 is in-order.

HOW IT WORKS
    callgrind --dump-instr=yes records an execution count per instruction
    address. Addresses are matched to mnemonics by (function name, offset from
    function start) rather than absolute address, so it works for shared
    libraries (libm) regardless of load address.

USAGE
    cycles.py <callgrind.out> <binary> [extra objects...]

    Any object appearing in the callgrind file that is not supplied on the
    command line is reported as UNMAPPED rather than silently dropped.
"""

import re
import subprocess
import sys
from collections import defaultdict

# ---------------------------------------------------------------------------
# Cycle weights. PLACEHOLDERS -- replace from the ARM Cortex-A7 MPCore TRM
# (instruction timing appendix) and cite it. Keep in sync with
# src/common/cost_model.h.
# ---------------------------------------------------------------------------
CLASS_CYCLES = {
    "alu": 1,          # mov, add, sub, and, orr, cmp, shifts...
    "mul": 3,          # TODO(TRM) mul, smull, smlal
    "mac_simd32": 3,   # TODO(TRM) smlad, smusdx
    "div": 12,         # TODO(TRM) sdiv/udiv -- iterative, early terminating
    "branch": 2,       # TODO(TRM) taken branch
    "load": 3,         # TODO(TRM) L1 hit
    "store": 2,        # TODO(TRM)
    "stack": 3,        # TODO(TRM) push/pop -- call frame overhead
    "fp_add": 4,       # TODO(TRM) vadd/vsub/vmul .f32
    "fp_div": 15,      # TODO(TRM) vdiv/vsqrt .f32 -- NOT pipelined
    "fp_move": 2,      # TODO(TRM) vmov/vldr/vstr
    "other": 1,
}

CLASSIFY = [
    (re.compile(r"^(sdiv|udiv)$"), "div"),
    (re.compile(r"^(smlad|smladx|smuad|smuadx|smusd|smusdx)$"), "mac_simd32"),
    (re.compile(r"^(mul|muls|mla|mls|smull|umull|smlal|umlal|smmul|smmla)$"), "mul"),
    (re.compile(r"^(vdiv|vsqrt)"), "fp_div"),
    (re.compile(r"^(vadd|vsub|vmul|vmla|vmls|vfma|vfms|vneg|vabs|vcvt|vcmp)"), "fp_add"),
    (re.compile(r"^(vmov|vldr|vstr|vpush|vpop|vldm|vstm)"), "fp_move"),
    # push/pop/ldm/stm separated from data traffic: they are function-call
    # frame overhead, which is fixed by inlining in C, not by hand assembly.
    # Lumping them into load/store hid ~20 trig calls per QR of prologue cost.
    (re.compile(r"^(push|pop|ldm|stm|ldmia|stmdb)"), "stack"),
    (re.compile(r"^(ldr|ldrb|ldrh|ldrd|ldrsh|ldrsb)"), "load"),
    (re.compile(r"^(str|strb|strh|strd)"), "store"),
    (re.compile(r"^(b|bl|bx|blx|cbz|cbnz)$"), "branch"),
]
COND = re.compile(r"(eq|ne|cs|hs|cc|lo|mi|pl|vs|vc|hi|ls|ge|lt|gt|le|al)$")


def classify(mnemonic):
    m = mnemonic.split(".")[0]          # drop .n/.w/.f32 suffixes
    for rx, cls in CLASSIFY:
        if rx.match(m):
            return cls
    base = COND.sub("", m)              # beq -> b, addne -> add
    for rx, cls in CLASSIFY:
        if rx.match(base):
            return cls
    return "alu" if base else "other"


def disassemble(obj):
    """{function_name: {offset_from_start: mnemonic}}"""
    for tool in ("arm-linux-gnueabihf-objdump", "objdump", "arm-none-eabi-objdump"):
        try:
            out = subprocess.run([tool, "-d", obj], capture_output=True,
                                 text=True, check=True).stdout
            break
        except (FileNotFoundError, subprocess.CalledProcessError):
            continue
    else:
        sys.exit(f"ERROR: could not disassemble {obj}")

    funcs, absmap, cur, base = {}, {}, None, 0
    fn_re = re.compile(r"^([0-9a-f]+) <(.+)>:")
    ins_re = re.compile(r"^\s*([0-9a-f]+):\s+[0-9a-f ]+\t\s*(\S+)")
    for line in out.splitlines():
        m = fn_re.match(line)
        if m:
            base = int(m.group(1), 16)
            cur = m.group(2)
            funcs[cur] = {}
            continue
        if cur:
            m = ins_re.match(line)
            if m:
                a = int(m.group(1), 16)
                funcs[cur][a - base] = m.group(2)
                absmap[a] = m.group(2)
    return funcs, absmap


def parse_callgrind(path):
    """[(object, function, address, count)] -- self cost only.

    The cost line layout is declared by the file's "positions:" header. With
    --dump-instr=yes callgrind emits "positions: instr line", i.e. TWO position
    fields before the cost:  <addr> <line> <cost>.  Assuming a single position
    field silently reads the line number as the cost -- which is 0 without
    debug info, so every count comes out zero. Parse the header instead.

    Each position field is independently compressed: absolute, "+n"/"-n"
    relative to that field's previous value, or "*" meaning unchanged.
    """
    rows, ob, fn = [], "?", "?"
    npos = 1
    last = [0]
    skip_next_cost = False   # the line after calls= is inclusive call cost

    def advance(tok, prev):
        if tok == "*":
            return prev
        if tok.startswith("0x") or tok.startswith("0X"):
            return int(tok, 16)
        if tok.startswith(("+", "-")):
            return prev + int(tok)
        return int(tok)

    with open(path) as fh:
        for line in fh:
            line = line.rstrip("\n")
            if line.startswith("positions:"):
                npos = len(line.split()) - 1
                last = [0] * npos
                continue
            if line.startswith("ob="):
                ob = line[3:].split(")")[-1].strip() or ob
            elif line.startswith("fn="):
                fn = line[3:].split(")")[-1].strip() or fn
                # Deliberately DO NOT reset the running position here.
                # Callgrind's subposition compression is relative to the
                # previous cost line in the file, not to the enclosing
                # function. Resetting made addresses drift once a function
                # began with a relative position, which showed up as ~20% of
                # instructions "unmapped" at offsets beyond the function size.
            elif line.startswith(("cob=", "cfi=", "cfn=", "fi=", "fe=", "fl=",
                                  "events:", "version:", "creator:", "cmd:",
                                  "part:", "desc:", "summary:", "totals:")):
                continue
            elif line.startswith("calls="):
                skip_next_cost = True
            elif line and (line[0].isdigit() or line[0] in "+-*"):
                parts = line.split()
                if len(parts) < npos + 1:
                    continue
                try:
                    for i in range(npos):
                        last[i] = advance(parts[i], last[i])
                    cost = int(parts[npos])
                except ValueError:
                    continue
                if skip_next_cost:
                    skip_next_cost = False
                    continue
                rows.append((ob, fn, last[0], cost))
    return rows


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    cg, objs = sys.argv[1], sys.argv[2:]

    def norm(name):
        """Strip glibc version suffixes: atan2f@@GLIBC_2.15 -> atan2f."""
        return name.split("@")[0]

    dis = {}
    by_abs = {}   # (object basename, address) -> mnemonic.
                  # Keyed by object on purpose: an address alone is ambiguous
                  # across objects, and matching libm addresses against the
                  # main binary silently misclassified ~50% of naive_float's
                  # instructions (they landed on whatever happened to sit at
                  # that address, mostly branches).
    by_fn = {}    # normalised function name -> {offset: mnemonic}
    for o in objs:
        funcs, absmap = disassemble(o)
        base = o.split("/")[-1]
        dis[o] = funcs
        for a_, mn in absmap.items():
            by_abs[(base, a_)] = mn
        for f, offs in funcs.items():
            by_fn.setdefault(norm(f), offs)

    rows = parse_callgrind(cg)
    if not rows:
        sys.exit(f"ERROR: no cost records parsed from {cg}. "
                 "Was it produced with --dump-instr=yes?")

    # First address seen per function == its entry point, so offsets are
    # load-address independent (works for shared libraries).
    fn_base = {}
    for ob, fn, addr, _ in rows:
        k = (ob, fn)
        if k not in fn_base or addr < fn_base[k]:
            fn_base[k] = addr

    hist = defaultdict(int)
    unmapped = defaultdict(int)
    total = 0
    for ob, fn, addr, cost in rows:
        total += cost
        # Absolute address within the SAME object first, then (function,
        # offset), which survives shared-library relocation.
        obase = ob.split("/")[-1]
        mn = by_abs.get((obase, addr))
        if mn is None:
            offs = by_fn.get(norm(fn))
            if offs is not None:
                mn = offs.get(addr - fn_base[(ob, fn)])
        if mn is None:
            unmapped[f"{fn}+0x{addr - fn_base.get((ob, fn), 0):x} "
                     f"[{ob.split('/')[-1]}]"] += cost
            continue
        hist[classify(mn)] += cost

    mapped = sum(hist.values())
    unmapped_total = sum(unmapped.values())

    # --- MEASURED: exact instruction counts, no assumptions -----------------
    print("MEASURED (exact -- no weights involved)")
    print(f"{'class':<14}{'instructions':>14}{'% of instrs':>13}")
    print("-" * 41)
    for cls in sorted(hist, key=lambda c: -hist[c]):
        print(f"{cls:<14}{hist[cls]:>14,}{100.0*hist[cls]/mapped:>12.1f}%")
    print("-" * 41)
    print(f"{'TOTAL':<14}{mapped:>14,}")

    # --- MODELLED: depends entirely on weights that are still guesses --------
    cycles = sum(hist[c] * CLASS_CYCLES[c] for c in hist)
    print()
    print("MODELLED (depends on the weights below -- NOT a measurement)")
    print(f"{'class':<14}{'cycles/instr':>14}{'cycles':>14}{'% of cycles':>13}")
    print("-" * 55)
    for cls in sorted(hist, key=lambda c: -hist[c] * CLASS_CYCLES[c]):
        c = hist[cls] * CLASS_CYCLES[cls]
        print(f"{cls:<14}{CLASS_CYCLES[cls]:>14}{c:>14,}{100.0*c/cycles:>12.1f}%")
    print("-" * 55)
    print(f"{'TOTAL':<14}{'':>14}{cycles:>14,}   "
          f"{cycles/mapped:.2f} cycles/instr avg")

    if unmapped_total:
        pct = 100.0 * unmapped_total / total
        print(f"\n!! UNMAPPED: {unmapped_total:,} instructions ({pct:.1f}%) "
              "could not be matched to a mnemonic.")
        print("   Supply the missing object(s) on the command line. Top offenders:")
        for k, v in sorted(unmapped.items(), key=lambda kv: -kv[1])[:8]:
            print(f"     {v:>12,}  {k}")
        libc_ish = sum(v for k, v in unmapped.items()
                       if "libc" in k or "ld-linux" in k
                       or any(t in k for t in ("malloc", "printf", "puts",
                                               "memcpy", "vfprintf")))
        if libc_ish > 0.5 * unmapped_total:
            print("\n   DIAGNOSIS: most unmapped work is libc startup/printf, which means")
            print("   collection was left ON outside the measured region. Usual cause: the")
            print("   --toggle-collect function was TAIL CALLED, so callgrind saw it entered")
            print("   but never left. Build the profiler with -fno-optimize-sibling-calls")
            print("   (Makefile PROFILE_CFLAGS).")
        if pct > 5:
            print("   >5% unmapped -- the cycle total above is NOT trustworthy.")
            sys.exit(1)

    print()
    print("!! The weights are PLACEHOLDERS. Base decisions on the MEASURED table")
    print("   unless a conclusion has been shown robust across a plausible weight")
    print("   range. Replace them from the Cortex-A7 TRM before quoting cycles.")
    print("   Model bias even with correct weights: partial dual-issue makes this")
    print("   an over-estimate, dependency stalls an under-estimate; cache effects")
    print("   are negligible for 4x4 matrices.")


if __name__ == "__main__":
    main()
