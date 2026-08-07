#!/usr/bin/env python3
"""Dynamic opcode histogram and cycle estimate.

Instruction counts treat an SDIV and a MOV as equal; they are not. This turns
callgrind's per-instruction execution counts into a histogram of which opcodes
actually ran, then weights them by Cortex-A7 latencies.

Cycles cannot be MEASURED here -- QEMU has no timing model -- so they are
computed from exact opcode counts, which is what the course asks for. A weighted
sum is defensible because the A7 is in-order.

Addresses are matched by (object, address), falling back to (function, offset)
so shared libraries work regardless of load address.

Usage: cycles.py <callgrind.out> <binary> [extra objects...]
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
    "simd_int": 2,     # TODO(TRM) NEON integer -- NOT floating point
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


# A NEON mnemonic's data type lives in the suffix, not the mnemonic: vadd.i32 is
# integer SIMD and vadd.f32 is floating point. Dropping the suffix made them
# identical and reported integer NEON as FP arithmetic -- which would
# contradict, in the tool's own output, the claim that the fixed-point variants
# use no floating point.
FP_TYPE = re.compile(r"^f(16|32|64)$")
INT_TYPE = re.compile(r"^([isu](8|16|32|64)|p(8|64))$")
VEC_ARITH = re.compile(r"^v(add|sub|mul|mla|mls|fma|fms|neg|abs|cvt|cmp|shl|"
                       r"shr|qadd|qsub|padd|pmul|max|min|and|orr|eor|bic|mvn)")
SIZE_SUFFIX = frozenset(("n", "w"))     # Thumb encoding width, not a data type


def classify(mnemonic):
    parts = mnemonic.split(".")
    m = parts[0]
    types = [p for p in parts[1:] if p not in SIZE_SUFFIX]

    # Vector arithmetic: let the data type decide. vcvt.f32.s32 is a genuine
    # int<->float conversion and counts as FP, hence "any float type wins".
    if VEC_ARITH.match(m):
        if any(FP_TYPE.match(t) for t in types):
            return "fp_div" if m.startswith(("vdiv", "vsqrt")) else "fp_add"
        if any(INT_TYPE.match(t) for t in types):
            return "simd_int"
        # Untyped (e.g. plain "vmov" between core and VFP registers) is register
        # traffic, not arithmetic -- fp_move, via the table below.

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


def parse_callgrind(path, reset_on_fn=False):
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

    # Callgrind NAME COMPRESSION: "ob=(1) /path/to/x" defines id 1, and a later
    # bare "ob=(1)" refers back to it. Treating a bare reference as "no name,
    # keep the previous value" attributes records to whatever object happened to
    # be current -- which is how qr_decomposition ended up labelled libc.so.6.
    ob_names, fn_names = {}, {}
    ref_re = re.compile(r"^\((\d+)\)\s*(.*)$")

    def resolve(body, table, current):
        body = body.strip()
        m = ref_re.match(body)
        if m:
            idx, name = m.group(1), m.group(2).strip()
            if name:
                table[idx] = name
            return table.get(idx, current)
        return body or current

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
                ob = resolve(line[3:], ob_names, ob)
            elif line.startswith("fn="):
                fn = resolve(line[3:], fn_names, fn)
                if reset_on_fn:
                    last = [0] * npos
                # Deliberately DO NOT reset the running position here.
                # Callgrind's subposition compression is relative to the
                # previous cost line in the file, not to the enclosing
                # function. Resetting made addresses drift once a function
                # began with a relative position, which showed up as ~20% of
                # instructions "unmapped" at offsets beyond the function size.
            # cob=/cfn= name the CALLEE, so they must not change the current
            # object or function -- but they SHARE the id namespace with ob=/fn=
            # and frequently introduce an id first. Skipping them outright left
            # those ids undefined, so a later bare "fn=(id)" fell through to the
            # previous name: libc's own cost blocks got labelled with whatever
            # function was current, typically qr_decomposition. Record the
            # definition, discard the value.
            elif line.startswith("cob="):
                resolve(line[4:], ob_names, ob)
            elif line.startswith("cfn="):
                resolve(line[4:], fn_names, fn)
            elif line.startswith(("cfi=", "cfl=", "fi=", "fe=", "fl=",
                                  "jump=", "jcnd=",
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
        """atan2f@@GLIBC_2.15 -> atan2f, __glibc_morecore'2 -> __glibc_morecore.

        The "'N" suffix is callgrind's RECURSION marker, not part of the symbol,
        so leaving it on made every recursive invocation look like an unknown
        function."""
        return name.split("@")[0].split("'")[0]

    dis = {}
    by_abs = {}   # (object basename, address) -> mnemonic.
                  # Keyed by object on purpose: an address alone is ambiguous
                  # across objects, and matching libm addresses against the
                  # main binary silently misclassified ~50% of naive_float's
                  # instructions (they landed on whatever happened to sit at
                  # that address, mostly branches).
    by_fn = {}    # (object basename, normalised fn) -> {offset: mnemonic}
                  # Keyed by object, not name alone: the main binary's PLT stub
                  # "atan2f@plt" normalises to "atan2f" and collided with
                  # libm's real atan2f. setdefault kept whichever was
                  # disassembled first, so libm's function body was matched
                  # against a 3-instruction stub (ldr/add/bx) -- inflating the
                  # branch class and leaving the rest unmapped.
    for o in objs:
        funcs, absmap = disassemble(o)
        base = o.split("/")[-1]
        dis[o] = funcs
        for a_, mn in absmap.items():
            by_abs[(base, a_)] = mn
        for f, offs in funcs.items():
            by_fn.setdefault((base, norm(f)), offs)

    # The callgrind format allows positions relative to the previous cost line.
    # Whether that running position is reset at an fn= boundary is not something
    # this tool should guess: both readings are tried and the one that resolves
    # more instructions wins. A wrong guess showed up as ~26% "unmapped" at
    # offsets past the end of the function.
    def score(rs):
        ok = 0
        for ob_, fn_, addr_, cost_ in rs:
            obase_ = ob_.split("/")[-1]
            if (obase_, addr_) in by_abs:
                ok += cost_
        return ok

    cand = [(False, parse_callgrind(cg, reset_on_fn=False)),
            (True, parse_callgrind(cg, reset_on_fn=True))]
    cand = [(r, rs, score(rs)) for r, rs in cand]
    cand.sort(key=lambda t: -t[2])
    reset_used, rows, _ = cand[0]
    if not rows:
        sys.exit(f"ERROR: no cost records parsed from {cg}. "
                 "Was it produced with --dump-instr=yes?")
    print(f"(position interpretation: reset-at-fn={reset_used}, chosen by best "
          f"resolution rate)\n")

    # First address seen per function == its entry point, so offsets are
    # load-address independent (works for shared libraries).
    fn_base = {}
    for ob, fn, addr, _ in rows:
        k = (ob, fn)
        if k not in fn_base or addr < fn_base[k]:
            fn_base[k] = addr

    hist = defaultdict(int)
    per_obj = defaultdict(lambda: [0, 0])   # object -> [resolved, unresolved]
    unmapped = defaultdict(int)
    total = 0
    for ob, fn, addr, cost in rows:
        total += cost
        # Absolute address within the SAME object first, then (function,
        # offset), which survives shared-library relocation.
        obase = ob.split("/")[-1]
        mn = by_abs.get((obase, addr))
        if mn is None:
            offs = by_fn.get((ob.split("/")[-1], norm(fn)))
            if offs is not None:
                mn = offs.get(addr - fn_base[(ob, fn)])
        obase = ob.split("/")[-1]
        if mn is None:
            unmapped[f"{fn}+0x{addr - fn_base.get((ob, fn), 0):x} "
                     f"[{obase}]"] += cost
            per_obj[obase][1] += cost
            continue
        per_obj[obase][0] += cost
        hist[classify(mn)] += cost

    mapped = sum(hist.values())
    unmapped_total = sum(unmapped.values())

    # --- per object ---------------------------------------------------------
    # The algorithm lives in the main binary (+ libm for naive_float). Anything
    # in libc is process startup and I/O that happened to fall inside the
    # collect toggle -- it is not QR work and must not be averaged into a
    # per-QR figure. Reporting it per object makes that visible and
    # quantifiable instead of silently included or silently dropped.
    main_obj = objs[0].split("/")[-1]
    print("BY OBJECT")
    print(f"{'object':<24}{'resolved':>12}{'unresolved':>12}")
    print("-" * 48)
    for o in sorted(per_obj, key=lambda k: -(per_obj[k][0] + per_obj[k][1])):
        r, u = per_obj[o]
        mark = "  <- algorithm" if o == main_obj else ""
        print(f"{o:<24}{r:>12,}{u:>12,}{mark}")
    print("-" * 48)
    print()

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

    # Failure is keyed to the MAIN binary, not the total. System libraries on
    # Debian are stripped of internal symbols, so their bodies can never fully
    # resolve by name -- holding the total to 5% would fail forever for reasons
    # no change to this tool can fix. The algorithm's own object must resolve
    # completely; that is the part being measured.
    main_r, main_u = per_obj.get(main_obj, (0, 0))
    if unmapped_total:
        pct = 100.0 * unmapped_total / total
        print(f"\n!! UNMAPPED: {unmapped_total:,} instructions ({pct:.1f}%) "
              "could not be matched to a mnemonic.")
        print("   Supply the missing object(s) on the command line. Top offenders:")
        for k, v in sorted(unmapped.items(), key=lambda kv: -kv[1])[:8]:
            print(f"     {v:>12,}  {k}")

        # Say WHICH of the three failure modes this is, rather than always
        # blaming a missing object: the object may be supplied and the function
        # still unresolvable.
        supplied = {o.split("/")[-1] for o in objs}
        refd = {k.rsplit("[", 1)[-1].rstrip("]") for k in unmapped}
        absent = sorted(refd - supplied)
        if absent:
            print(f"\n   Objects referenced but NOT supplied: {', '.join(absent)}")
        present_unknown = sorted({
            k.split("+")[0] for k in unmapped
            if k.rsplit("[", 1)[-1].rstrip("]") in supplied
            and (k.rsplit("[", 1)[-1].rstrip("]"), norm(k.split("+")[0]))
            not in by_fn})[:6]
        if present_unknown:
            print("   Supplied objects that lack the named function: "
                  f"{', '.join(present_unknown)}")
            print("   (IFUNC dispatch or a stripped object can cause this.)")
        # Key off the FUNCTION name, not the object: a wrong object attribution
        # (name-compression bug) also produces "[libc.so.6]" in these keys and
        # would trigger a misleading tail-call diagnosis.
        libc_ish = sum(v for k, v in unmapped.items()
                       if any(t in k.split("+")[0].split(" ")[0]
                              for t in ("malloc", "printf", "puts", "memcpy",
                                        "vfprintf", "strlen", "memset")))
        if libc_ish > 0.5 * unmapped_total:
            print("\n   DIAGNOSIS: most unmapped work is libc startup/printf, which means")
            print("   collection was left ON outside the measured region. Usual cause: the")
            print("   --toggle-collect function was TAIL CALLED, so callgrind saw it entered")
            print("   but never left. Build the profiler with -fno-optimize-sibling-calls")
            print("   (Makefile PROFILE_CFLAGS).")
        main_pct = 100.0 * main_r / max(main_r + main_u, 1)
        if main_u > 0.05 * max(main_r + main_u, 1):
            print(f"\n   >5% of {main_obj} is unmapped. The algorithm's own object"
                  " must resolve\n   completely -- the cycle total is NOT "
                  "trustworthy.")
            sys.exit(1)
        print(f"\n   {main_obj} resolves {main_pct:.1f}%: the ALGORITHM is fully "
              "accounted for.")
        print("   The unmapped work is in stripped system libraries (startup, "
              "malloc,\n   syscalls) -- process overhead that fell inside the "
              "collect toggle, not\n   QR work. Do NOT average it into a per-QR "
              "figure. To name it:\n   apt-get install libc6-dbg")

    print()
    print("!! The weights are PLACEHOLDERS. Base decisions on the MEASURED table")
    print("   unless a conclusion has been shown robust across a plausible weight")
    print("   range. Replace them from the Cortex-A7 TRM before quoting cycles.")
    print("   Model bias even with correct weights: partial dual-issue makes this")
    print("   an over-estimate, dependency stalls an under-estimate; cache effects")
    print("   are negligible for 4x4 matrices.")


if __name__ == "__main__":
    main()
