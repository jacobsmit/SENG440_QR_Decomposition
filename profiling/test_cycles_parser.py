#!/usr/bin/env python3
"""Regression tests for the callgrind parser in cycles.py.

This parser has broken four separate times (cost-field layout, name
compression, position drift across function boundaries, and callee-id
namespacing), each time silently -- the histogram still printed, it was just
attributing instructions to the wrong function. A wrong cycle model is worse
than no cycle model, so the parsing rules get pinned down here.

Fixtures are synthetic callgrind files, so this runs anywhere. No ARM, no
valgrind, no VM.

Usage: python3 profiling/test_cycles_parser.py
"""

import importlib.util
import os
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("cycles", os.path.join(HERE, "cycles.py"))
cycles = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cycles)

failures = []


def check(name, got, want):
    if got != want:
        failures.append(f"{name}\n     got:  {got}\n     want: {want}")
        print(f"  FAIL  {name}")
    else:
        print(f"  ok    {name}")


def parse(text, **kw):
    with tempfile.NamedTemporaryFile("w", suffix=".callgrind", delete=False) as fh:
        fh.write(text)
        path = fh.name
    try:
        return [(ob.split("/")[-1], fn, addr, cost)
                for ob, fn, addr, cost in cycles.parse_callgrind(path, **kw)]
    finally:
        os.unlink(path)


HEADER = "version: 1\nevents: Ir\npositions: instr line\n\n"

# --- 1. callee ids share the ob=/fn= namespace ------------------------------
# cob=/cfn= name the callee and must NOT change the current context, but they
# often introduce an id first. Ignoring them left the id undefined, so a later
# bare fn=(id) fell back to the previous name and libc's own cost blocks were
# reported as qr_decomposition.
CALLEE_IDS = HEADER + """ob=(1) /build/profile
fl=(1) ???
fn=(1) qr_decomposition
0x8500 0 100
cob=(2) /lib/libc.so.6
cfi=(2) ???
cfn=(2) memcpy
calls=1 0x9000
+4 0 999
ob=(2)
fl=(2)
fn=(2)
0x9000 0 415
ob=(1)
fl=(1)
fn=(1)
0x8600 0 50
"""

check("callee-defined ids resolve on later bare references",
      parse(CALLEE_IDS),
      [("profile", "qr_decomposition", 0x8500, 100),
       ("libc.so.6", "memcpy", 0x9000, 415),
       ("profile", "qr_decomposition", 0x8600, 50)])

# --- 2. the cost line after calls= is inclusive, not self cost --------------
# Counting it double-counts the whole callee subtree into the caller.
check("call-site cost line is skipped",
      [r for r in parse(CALLEE_IDS) if r[3] == 999], [])

# --- 3. two position fields before the cost --------------------------------
# "positions: instr line" means <addr> <line> <cost>. Reading a single position
# field takes the LINE NUMBER as the cost -- zero without debug info, so every
# count silently comes out zero.
check("cost is read from the field after all positions",
      [c for _, _, _, c in parse(CALLEE_IDS)], [100, 415, 50])

# --- 4. position compression ------------------------------------------------
COMPRESSION = HEADER + """ob=(1) /build/profile
fn=(1) f
0x1000 0 1
+4 0 2
* 0 3
-4 0 4
"""
check("absolute, +n, *, and -n position forms",
      [a for _, _, a, _ in parse(COMPRESSION)],
      [0x1000, 0x1004, 0x1004, 0x1000])

# --- 5. names are only defined once ----------------------------------------
REDEF = HEADER + """ob=(1) /build/profile
fn=(1) first
0x10 0 1
fn=(2) second
0x20 0 2
fn=(1)
0x30 0 3
"""
check("bare id reference recovers the earlier name",
      [f for _, f, _, _ in parse(REDEF)], ["first", "second", "first"])

print()
if failures:
    print(f"{len(failures)} FAILED\n")
    for f in failures:
        print("  " + f)
    raise SystemExit(1)
print("callgrind parser: all checks pass")
