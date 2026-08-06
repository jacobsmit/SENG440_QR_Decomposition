#!/bin/bash
# Exact dynamic instruction count per QR, via callgrind. Feeds ops.csv.
# Only instructions inside qr_profiled are collected, so the RNG, printf and
# harness scaffolding are excluded.
#
# Usage: callgrind_count.sh <variant> [iterations] [--float]
set -u

set -u

cd "$(dirname "$0")/.."

VARIANT="${1:?usage: callgrind_count.sh <variant> [iterations]}"
ITERS="${2:-50}"
FLOAT="${3:-}"   # pass "--float" to measure a float entry point
BIN="build/$VARIANT/profile"

if ! command -v valgrind >/dev/null 2>&1; then
    echo "ERROR: valgrind not installed." >&2
    exit 2
fi
if [ ! -x "$BIN" ]; then
    echo "ERROR: $BIN not built. Run: make $BIN" >&2
    exit 2
fi

if [ "$FLOAT" = "--float" ]; then TOGGLE=qr_profiled_f32; else TOGGLE=qr_profiled; fi

OUT="build/callgrind"
mkdir -p "$OUT"
CG="$OUT/$VARIANT.out"
rm -f "$CG"

# Collect only inside qr_profiled (or qr_profiled_f32), dedicated non-recursive
# wrappers in profile_ops.c. Toggling on qr_decomposition directly left the
# toggle unbalanced for naive_float and leaked printf/malloc/linker work into
# the total.
valgrind --tool=callgrind \
         --callgrind-out-file="$CG" \
         --collect-atstart=no \
         --toggle-collect="$TOGGLE" \
         --quiet \
         "$BIN" "$ITERS" 8 $FLOAT >/dev/null 2>"$OUT/$VARIANT.log"

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
    echo "  --toggle-collect=$TOGGLE may not have matched a symbol." >&2
    echo "  Check that qr_decomposition is not static/inlined away, and that" >&2
    echo "  the binary is not stripped. See $OUT/$VARIANT.log" >&2
    exit 1
fi

printf "%s,%s,%s,%.1f\n" "$VARIANT" "$ITERS" "$IR" \
    "$(awk -v a="$IR" -v b="$ITERS" 'BEGIN{printf "%.1f", a/b}')"
