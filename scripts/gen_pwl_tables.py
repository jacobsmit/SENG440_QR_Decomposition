#!/usr/bin/env python3
"""Generate the piecewise-linear coefficient tables for src/common/trig_pwl_tables.h.

Design constraint that drives everything here: segment selection must be a
single shift, not a compare chain, so the cost of the approximation is O(1) in
the number of segments. That requires the segment WIDTH to be a power of two in
the fixed-point representation -- 2^-p for a chosen p -- rather than the segment
COUNT being round. The index is then the top bits of the input:

    idx = |x| >> (TRIG_Q - p)

which is one LSR in software and a ROM address line in hardware.

Each segment stores (m, b) in ABSOLUTE form, f(x) ~= m*x + b, so evaluation is
one multiply and one add with no offset subtraction. Both fit in int16 and are
packed into one 32-bit word, so the coefficient fetch is a single load.

Minimax fit per segment: slope = chord slope, intercept centred on the residual
(exact minimax for a fixed slope). f'' has constant sign on every domain used
here, so this is within rounding of the true minimax line.

Usage: gen_pwl_tables.py [p] > src/common/trig_pwl_tables.h
"""

import sys
import numpy as np

TRIG_Q = 14  # fractional bits for angles, ratios and sin/cos values
SAMPLES = 200001


def csd_slope(target, nterms=3):
    """Closest sum of `nterms` signed powers of two to `target`.

    The microcoded engine has no multiplier, so a slope must be expressible as
    shift-adds. Because the result is an exact integer, m*x is still bit-exact
    against a real multiply -- the C firmware model and the assembly agree
    exactly, which is what makes the assembly testable rather than merely
    plausible."""
    best = [0, 1 << 30, []]

    def rec(cur, used, terms):
        if abs(cur - target) < best[1]:
            best[0], best[1], best[2] = cur, abs(cur - target), list(terms)
        if used == nterms:
            return
        for a in range(15):
            for sgn in (1, -1):
                terms.append((sgn, a))
                rec(cur + sgn * (1 << a), used + 1, terms)
                terms.pop()

    rec(0, 0, [])
    return best[0], best[2]


def build_csd_table(f, lo, hi, p, nterms=3):
    """Per-segment (m, b, terms) with CSD-constrained slopes.

    THE single source for the firmware coefficient tables. src/common/
    trig_pwl_csd.h, hw/givensq_pkg.vhd and src/common/givensq_fw.S are all
    emitted from this function, because when they were each built by their own
    near-copy of it the sample counts drifted apart, three minimax intercepts
    came out 1 LSB different, and the VHDL testbench failed 62 of 301 vectors
    against the C model. Bit-exactness across the three implementations is only
    real if one function produces all three tables.
    """
    width = 2.0 ** -p
    nseg = int(np.ceil((hi - lo) / width))
    ents = []
    for i in range(nseg):
        a, b = lo + i * width, min(lo + (i + 1) * width, hi)
        x = np.linspace(a, b, SAMPLES)
        xq = np.round(x * (1 << TRIG_Q)).astype(np.int64)
        mq, terms = csd_slope(int(round((f(b) - f(a)) / (b - a) * (1 << TRIG_Q))),
                              nterms)
        r = f(x) - ((mq * xq) >> TRIG_Q) / (1 << TRIG_Q)
        cq = int(round((r.max() + r.min()) / 2 * (1 << TRIG_Q)))
        ents.append((mq, cq, terms))
    ents.append(ents[-1])           # guard entry: see build_table()
    return ents


def fit_segment(f, a, b):
    """Minimax linear fit on [a, b], returned in absolute form (m, c)."""
    x = np.linspace(a, b, SAMPLES)
    m = (f(b) - f(a)) / (b - a)
    r = f(x) - m * x
    c = (r.max() + r.min()) / 2.0
    return m, c


def build_table(f, lo, hi, p):
    """Uniform segments of width 2^-p covering [lo, hi]. Returns (entries, err)."""
    width = 2.0 ** -p
    nseg = int(np.ceil((hi - lo) / width))
    entries, worst = [], 0.0
    for i in range(nseg):
        a = lo + i * width
        b = min(a + width, hi)
        m, c = fit_segment(f, a, b)
        mq = int(round(m * (1 << TRIG_Q)))
        cq = int(round(c * (1 << TRIG_Q)))
        for v, name in ((mq, "slope"), (cq, "intercept")):
            if not -32768 <= v <= 32767:
                raise SystemExit(f"{name} {v} does not fit int16 on [{a},{b}]")
        # Honest error: re-measure with the QUANTISED coefficients, including
        # the truncating shift the C code actually performs.
        x = np.linspace(a, b, SAMPLES)
        xq = np.round(x * (1 << TRIG_Q)).astype(np.int64)
        approx = ((mq * xq) >> TRIG_Q) + cq
        worst = max(worst, np.max(np.abs(f(x) - approx / (1 << TRIG_Q))))
        entries.append((mq, cq))

    # GUARD ENTRY. The top of the domain indexes one past the last segment:
    # |ratio| == 1.0 gives idx == nseg for arctan, and that happens whenever
    # |N| == |D|, which is common rather than exotic. Duplicating the final
    # segment lets the evaluator drop its bounds clamp -- 4 bytes of ROM to
    # remove a compare and a conditional move from EVERY evaluation. Same
    # ROM-for-instructions trade the uniform segment width is built on.
    entries.append(entries[-1])
    return entries, worst


def emit(name, entries, err, lo, hi, p):
    print(f"/* {name}: {len(entries)} segments of width 2^-{p} over "
          f"[{lo:.4f}, {hi:.4f}], max error {err:.6f} (coefficients quantised) */")
    print(f"#define PWL_{name.upper()}_SEGS {len(entries)}")
    print(f"static const int32_t pwl_{name}[PWL_{name.upper()}_SEGS] = {{")
    for i, (m, c) in enumerate(entries):
        packed = ((c & 0xFFFF) << 16) | (m & 0xFFFF)
        print(f"    0x{packed:08X}, /* [{i}] m={m:6d} b={c:6d} */")
    print("};")
    print()


def main():
    p = int(sys.argv[1]) if len(sys.argv) > 1 else 3
    pi4 = np.pi / 4

    atan_t, atan_e = build_table(np.arctan, 0.0, 1.0, p)
    sin_t, sin_e = build_table(np.sin, 0.0, pi4, p)
    cos_t, cos_e = build_table(np.cos, 0.0, pi4, p)

    print("/* GENERATED by scripts/gen_pwl_tables.py"
          f" {p} -- do not edit. */")
    print("#ifndef TRIG_PWL_TABLES_H")
    print("#define TRIG_PWL_TABLES_H")
    print()
    print("#include <stdint.h>")
    print()
    print(f"#define PWL_SEG_BITS {p}")
    print(f"#define PWL_TRIG_Q   {TRIG_Q}")
    print()
    print("/* Segment index = |x| >> PWL_INDEX_SHIFT: one LSR, constant in the")
    print("   number of segments. Coefficients are packed b:m, one load. */")
    print(f"#define PWL_INDEX_SHIFT (PWL_TRIG_Q - PWL_SEG_BITS)")
    print()
    emit("arctan", atan_t, atan_e, 0.0, 1.0, p)
    emit("sin", sin_t, sin_e, 0.0, pi4, p)
    emit("cos", cos_t, cos_e, 0.0, pi4, p)
    print("#endif /* TRIG_PWL_TABLES_H */")

    print(f"\n/* summary: arctan {atan_e:.6f}  sin {sin_e:.6f}  cos {cos_e:.6f} */",
          file=sys.stderr)
    print(f"/* table bytes: {4 * (len(atan_t) + len(sin_t) + len(cos_t))} */",
          file=sys.stderr)


if __name__ == "__main__":
    main()
