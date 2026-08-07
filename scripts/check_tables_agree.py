#!/usr/bin/env python3
"""Assert the C, VHDL and ARM-assembly coefficient tables are identical.

They are emitted by three different generators from one shared builder. When
each generator had its own copy of that builder, differing sample counts made
three minimax intercepts land 1 LSB apart and the VHDL testbench failed 62 of
301 vectors. This check exists so that cannot recur silently.
"""
import re, sys, os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
def rd(p): return open(os.path.join(ROOT, p)).read()

c, v, a = (rd("src/common/trig_pwl_csd.h"), rd("hw/givensq_pkg.vhd"),
           rd("src/common/givensq_fw.S"))
fail = 0
for name, U in (("arctan", "ARCTAN"), ("sin", "SIN"), ("cos", "COS")):
    cb = re.search(rf"csd_{name}\[[^\]]*\] = \{{(.*?)\}};", c, re.S).group(1)
    ct = [(int(m), int(b)) for m, b in
          re.findall(r"m=\s*(-?\d+).*?b=\s*(-?\d+)", cb)]
    vb = re.search(rf"ROM_{U} : coeff_array\(0 to \d+\) := \((.*?)\n  \);",
                   v, re.S).group(1)
    vt = [(int(m), int(b)) for m, b in
          re.findall(r"m =>\s*(-?\d+), b =>\s*(-?\d+)", vb)]
    tag = name[:2]
    at = [(int(m), int(b)) for m, b in
          re.findall(rf"\.L{tag}_s\d+:\s*@ m=(-?\d+) = .*?, b=(-?\d+)", a)]
    ok = (ct == vt == at)
    print(f"  {name:<7} C:{len(ct):3d} VHDL:{len(vt):3d} ASM:{len(at):3d}  "
          f"{'identical' if ok else '**DIFFER**'}")
    if not ok:
        fail = 1
        for i, t in enumerate(zip(ct, vt, at)):
            if len(set(t)) != 1:
                print(f"      [{i}] C={t[0]} VHDL={t[1]} ASM={t[2]}")
print("coefficient tables: " + ("all agree" if not fail else "MISMATCH"))
sys.exit(fail)
