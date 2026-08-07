#!/bin/sh
# Accuracy/cost curve for the PWL tables: regenerate at each segment width,
# rebuild, and run the accuracy suite. Restores the committed table on exit.
#
# Usage: scripts/trig_approx_parameters/pwl_sweep.sh [p ...]
set -e
cd "$(dirname "$0")/../.."

GEN=scripts/trig_approx_parameters/gen_pwl_tables.py
TBL=src/common/trig_pwl_tables.h
SAVE=$(mktemp)
cp "$TBL" "$SAVE"
trap 'cp "$SAVE" "$TBL"; rm -f "$SAVE"' EXIT INT TERM

printf '%-4s %-7s %-7s %-24s %-24s\n' "p" "segs" "bytes" "fixed_scalar" "fixed_simd32"
printf '%-4s %-7s %-7s %-11s %-12s %-11s %-12s\n' "" "" "" "recon%" "orth" "recon%" "orth"
printf -- '---------------------------------------------------------------------------\n'

for p in "${@:-2 3 4 5 6}"; do
    python3 "$GEN" "$p" > "$TBL" 2>/dev/null
    segs=$(grep -c '^    0x' "$TBL")
    bytes=$((segs * 4))
    rm -rf build/fixed_scalar build/fixed_simd32
    make -s build/fixed_scalar/test build/fixed_simd32/test >/dev/null 2>&1
    sc=$(./build/fixed_scalar/test --csv | tail -1)
    sm=$(./build/fixed_simd32/test --csv | tail -1)
    printf '%-4s %-7s %-7s %-11s %-12s %-11s %-12s\n' \
        "$p" "$segs" "$bytes" \
        "$(echo "$sc" | cut -d, -f2)" "$(echo "$sc" | cut -d, -f3)" \
        "$(echo "$sm" | cut -d, -f2)" "$(echo "$sm" | cut -d, -f3)"
done
