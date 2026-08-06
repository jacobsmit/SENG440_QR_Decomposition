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
    (re.compile(r"^(ldr|ldrb|ldrh|ldrd|ldm|pop)"), "load"),
    (re.compile(r"^(str|strb|strh|strd|stm|push)"), "store"),
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

    funcs, cur, base = {}, None, 0
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
                funcs[cur][int(m.group(1), 16) - base] = m.group(2)
    return funcs


def parse_callgrind(path):
    """[(object, function, address, count)] -- self cost only."""
    rows, ob, fn, addr = [], "?", "?", 0
    skip_next_cost = False   # the line after calls= is inclusive call cost
    with open(path) as fh:
        for line in fh:
            line = line.rstrip("\n")
            if line.startswith("ob="):
                ob = line[3:].split(")")[-1].strip() or ob
            elif line.startswith("fn="):
                fn = line[3:].split(")")[-1].strip() or fn
                addr = 0
            elif line.startswith(("cob=", "cfi=", "cfn=", "fi=", "fe=", "fl=")):
                pass
            elif line.startswith("calls="):
                skip_next_cost = True
            elif line and (line[0].isdigit() or line[0] in "+-*0x"):
                parts = line.split()
                if len(parts) < 2:
                    continue
                pos, cost = parts[0], parts[1]
                if pos.startswith("0x"):
                    addr = int(pos, 16)
                elif pos in ("*",):
                    pass
                elif pos.startswith(("+", "-")):
                    addr += int(pos)
                else:
                    try:
                        addr = int(pos)
                    except ValueError:
                        continue
                if skip_next_cost:
                    skip_next_cost = False
                    continue
                try:
                    rows.append((ob, fn, addr, int(cost)))
                except ValueError:
                    pass
    return rows


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    cg, objs = sys.argv[1], sys.argv[2:]

    dis = {}
    for o in objs:
        dis[o] = disassemble(o)
    # function name -> (object, offsets) for name-based matching
    by_fn = {}
    for o, funcs in dis.items():
        for f, offs in funcs.items():
            by_fn.setdefault(f, (o, offs))

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
        entry = by_fn.get(fn)
        if entry is None:
            unmapped[f"{fn} [{ob.split('/')[-1]}]"] += cost
            continue
        _, offs = entry
        mn = offs.get(addr - fn_base[(ob, fn)])
        if mn is None:
            unmapped[f"{fn}+0x{addr - fn_base[(ob, fn)]:x}"] += cost
            continue
        hist[classify(mn)] += cost

    mapped = sum(hist.values())
    unmapped_total = sum(unmapped.values())

    print(f"{'class':<14}{'instructions':>14}{'cycles/instr':>14}{'cycles':>14}{'share':>8}")
    print("-" * 64)
    cycles = 0
    for cls in sorted(hist, key=lambda c: -hist[c]):
        c = hist[cls] * CLASS_CYCLES[cls]
        cycles += c
        print(f"{cls:<14}{hist[cls]:>14,}{CLASS_CYCLES[cls]:>14}{c:>14,}")
    print("-" * 64)
    print(f"{'TOTAL':<14}{mapped:>14,}{'':>14}{cycles:>14,}")
    if cycles and mapped:
        print(f"{'':<14}{'':>14}{'':>14}{'':>14}  "
              f"{cycles/mapped:.2f} cycles/instruction average")

    if unmapped_total:
        pct = 100.0 * unmapped_total / total
        print(f"\n!! UNMAPPED: {unmapped_total:,} instructions ({pct:.1f}%) "
              "could not be matched to a mnemonic.")
        print("   Supply the missing object(s) on the command line. Top offenders:")
        for k, v in sorted(unmapped.items(), key=lambda kv: -kv[1])[:8]:
            print(f"     {v:>12,}  {k}")
        if pct > 5:
            print("   >5% unmapped -- the cycle total above is NOT trustworthy.")
            sys.exit(1)

    print("\nWeights are PLACEHOLDERS from cycles.py -- replace with Cortex-A7 TRM")
    print("figures before quoting any of this. Known model bias: partial")
    print("dual-issue makes this an over-estimate; dependency stalls make it an")
    print("under-estimate; cache effects are negligible for 4x4 matrices.")


if __name__ == "__main__":
    main()
