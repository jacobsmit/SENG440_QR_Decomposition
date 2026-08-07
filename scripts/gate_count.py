#!/usr/bin/env python3
"""Gate-equivalent estimate for the GIVENSQ hardware unit.

The notes permit an educated guess with guidance, but ask to see the
arithmetic. This is the arithmetic. One GE = one 2-input NAND; the primitive
costs below are the usual first-order figures for a standard-cell library.

Usage: gate_count.py            (table)
       gate_count.py --md       (markdown, for docs/HARDWARE.md)
"""

import sys

# --- primitive costs, in gate equivalents ---------------------------------
GE_XOR = 2.5      # 2-input XOR
GE_FA = 5.0       # full adder
GE_DFF = 6.0      # D flip-flop
GE_MUX = 3.0      # 1-bit 2:1 mux
GE_ROM_BIT = 0.5  # dense ROM cell


def reg(n):        return n * GE_DFF
def adder(n):      return n * GE_FA
def mux(n):        return n * GE_MUX
def neg(n):        return n * GE_XOR + n * GE_FA / 2   # invert + increment
def rom(entries, width):
    # cells plus an address decoder, roughly 2 GE per decoded line
    return entries * width * GE_ROM_BIT + entries * 2.0


def array_mult(a, b):
    """a x b array multiplier: partial products plus the adder rows."""
    return a * b * 1.0 + (b - 1) * a * GE_FA


def barrel(width, positions):
    """log-structured barrel shifter: log2(positions) stages of 2:1 muxes."""
    stages = positions.bit_length() - 1
    return stages * width * GE_MUX


W = 16          # datapath width for data and coefficients
WP = 32         # product width

blocks = []

# 1. input stage: |o|, |a|, compare, swap
blocks.append(("input stage (abs x2, compare, swap muxes)",
               2 * neg(W) + adder(W) + 2 * mux(W)))

# 2. restoring divider, sequential: 14 clocks
blocks.append(("restoring divider (rem/quo/divisor regs, subtractor, mux, counter)",
               reg(W) + reg(W - 1) + reg(W) + adder(W) + mux(W)
               + reg(4) + adder(4)))

# 3. coefficient ROM, shared, dual-read (sin and cos in the same cycle)
N_SEGMENTS = 17 + 14 + 14
blocks.append((f"coefficient ROM ({N_SEGMENTS} entries x 32 bits, dual read)",
               rom(N_SEGMENTS, 32) + 120))     # second decoder for port 2

# 4. two PWL datapaths, so sin and cos evaluate in the SAME cycle
pwl_one = array_mult(W, W) + adder(W) + adder(W) + adder(W) + 2 * mux(W)
blocks.append(("PWL datapath x2 (multiplier, +b adder, fold compare/sub, muxes)",
               2 * pwl_one))

# 5. the automaton
blocks.append(("FSM controller (7 states, next-state and output logic)",
               reg(3) + 120))

# 6. output register
blocks.append(("output register (32 bit)", reg(32)))

total = sum(c for _, c in blocks)

# --- the two design alternatives -------------------------------------------
mult_cost = array_mult(W, W)
csd_cost = 3 * barrel(WP, 16) + 2 * adder(WP)

recip_rom = rom(256, W)
recip_extra = recip_rom + array_mult(W, W)

md = "--md" in sys.argv
if md:
    print("| block | gate equivalents | share |")
    print("|---|---:|---:|")
    for name, c in blocks:
        print(f"| {name} | {c:,.0f} | {100 * c / total:.0f} % |")
    print(f"| **total** | **{total:,.0f}** | |")
else:
    for name, c in blocks:
        print(f"  {name:<62s} {c:8,.0f}  ({100 * c / total:4.1f}%)")
    print(f"  {'TOTAL':<62s} {total:8,.0f}")

print()
print(f"multiplier (16x16 array)      {mult_cost:8,.0f} GE")
print(f"CSD 3-term shift-add equiv    {csd_cost:8,.0f} GE   "
      f"({csd_cost / mult_cost:.2f}x the multiplier)")
print()
print(f"reciprocal-LUT divider adds   {recip_extra:8,.0f} GE "
      f"(+{100 * recip_extra / total:.0f}% area) to save 11 of 19 clocks")
