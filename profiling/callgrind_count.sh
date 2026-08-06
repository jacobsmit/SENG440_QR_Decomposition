#!/bin/bash
# ============================================================================
# Exact dynamic instruction count per QR decomposition, via callgrind.
#
# THIS is the metric that answers "did the optimisation work?" for every kind
# of change -- including hand-written assembly and loop unrolling, which alter
# the instruction stream without changing operation counts at all, and are
# therefore invisible in ops.csv.
#
# Valid under QEMU: callgrind counts instructions actually executed, which is a
# property of the program and the ISA, not of the emulator. (Cycle counts and
# cache statistics are NOT valid -- QEMU has no pipeline or cache model, and
# cachegrind would be modelling a hypothetical cache, not the A7's.)
#
# Only instructions executed INSIDE qr_decomposition are collected
# (--collect-atstart=no --toggle-collect), so the RNG, printf and harness
# scaffolding are excluded.
#
# Usage: callgrind_count.sh <variant> [iterations]
#        prints:  variant,cg_iterations,ir_total,ir_per_qr
# ============================================================================
set -u

cd "$(dirname "$0")/.."

VARIANT="${1:?usage: callgrind_count.sh <variant> [iterations]}"
ITERS="${2:-50}"
BIN="build/$VARIANT/profile"

if ! command -v valgrind >/dev/null 2>&1; then
    echo "ERROR: valgrind not installed." >&2
    exit 2
fi
if [ ! -x "$BIN" ]; then
    echo "ERROR: $BIN not built. Run: make $BIN" >&2
    exit 2
fi

OUT="build/callgrind"
mkdir -p "$OUT"
CG="$OUT/$VARIANT.out"
rm -f "$CG"

# Collect only inside qr_decomposition. Note for naive_float this includes its
# fixed-point boundary wrapper as well as the float algorithm -- that is the
# interface every variant is measured at, so the comparison stays apples to
# apples. Its pure-float entry point is qr_decomposition_f32 if you want that
# number separately.
valgrind --tool=callgrind \
         --callgrind-out-file="$CG" \
         --collect-atstart=no \
         --toggle-collect=qr_decomposition \
         --quiet \
         "$BIN" "$ITERS" 8 >/dev/null 2>"$OUT/$VARIANT.log"

if [ ! -f "$CG" ]; then
    echo "ERROR: callgrind produced no output; see $OUT/$VARIANT.log" >&2
    exit 1
fi

# Prefer callgrind_annotate's PROGRAM TOTALS; fall back to the summary line.
IR=""
if command -v callgrind_annotate >/dev/null 2>&1; then
    IR=$(callgrind_annotate "$CG" 2>/dev/null \
         | awk '/PROGRAM TOTALS/ {gsub(/[^0-9]/,"",$1); print $1; exit}')
fi
if [ -z "$IR" ]; then
    IR=$(awk '/^summary:/ {print $2; exit}' "$CG")
fi
if [ -z "$IR" ]; then
    IR=$(awk '/^totals:/ {print $2; exit}' "$CG")
fi

if [ -z "$IR" ] || [ "$IR" = "0" ]; then
    echo "ERROR: collected 0 instructions for $VARIANT." >&2
    echo "  --toggle-collect=qr_decomposition may not have matched a symbol." >&2
    echo "  Check that qr_decomposition is not static/inlined away, and that" >&2
    echo "  the binary is not stripped. See $OUT/$VARIANT.log" >&2
    exit 1
fi

printf "%s,%s,%s,%.1f\n" "$VARIANT" "$ITERS" "$IR" \
    "$(awk -v a="$IR" -v b="$ITERS" 'BEGIN{printf "%.1f", a/b}')"
