#!/bin/bash
# ============================================================================
# ARM profiling harness for the fixed-point QR decomposition.
#
# Produces the two things that are actually valid on this project's target
# (Cortex-A7 under QEMU -- see docs/TARGET_PLATFORM.md):
#
#   1. STATIC  instruction counts and instruction mix, per routine, per
#      compiler flag set. A property of the binary; exact.
#   2. DYNAMIC operation counts from the instrumented build. Deterministic
#      and host-independent.
#
# It deliberately does NOT report wall-clock time. QEMU's TCG has no pipeline,
# cache or cycle model, so seconds measured in the guest describe the host
# machine, not the Cortex-A7.
#
# Run natively inside the ARM VM (uses gcc/objdump), or on a host with an
# arm-linux-gnueabihf cross-toolchain.
# ============================================================================
set -u

cd "$(dirname "$0")"
SRC=../src/software_base
OUT=./_profile_out
mkdir -p "$OUT"

# --- toolchain selection ---------------------------------------------------
if [ "$(uname -m)" = "armv7l" ] || [ "$(uname -m)" = "armv6l" ]; then
    CC=${CC:-gcc}; OBJDUMP=${OBJDUMP:-objdump}; NATIVE=1
elif command -v arm-linux-gnueabihf-gcc >/dev/null 2>&1; then
    CC=${CC:-arm-linux-gnueabihf-gcc}
    OBJDUMP=${OBJDUMP:-arm-linux-gnueabihf-objdump}; NATIVE=0
else
    echo "ERROR: no ARM toolchain found."
    echo "  Run this inside the ARM VM, or install arm-linux-gnueabihf-gcc."
    exit 1
fi
echo "toolchain: $CC ($($CC -dumpmachine))  native=$NATIVE"
echo

# --- flag sets to compare --------------------------------------------------
# Names must not contain spaces (used as filenames).
FLAGSETS=(
  "debian-default:-O2 -marm"
  "cortex-a7:-O2 -marm -mcpu=cortex-a7"
  "cortex-a7-O3:-O3 -marm -mcpu=cortex-a7"
  "armv5te-nodiv:-O2 -marm -march=armv5te"
  "cortex-a7-thumb:-O2 -mthumb -mcpu=cortex-a7"
)

ROUTINES="arctan_fixed sin_fixed cos_fixed calculate_arctan_ratio qr_decomposition"

# ============================================================================
# Part 1 -- static instruction counts and instruction mix
# ============================================================================
echo "============================================================"
echo " PART 1: static instruction counts per routine"
echo "============================================================"
printf "%-18s" "routine"
for fs in "${FLAGSETS[@]}"; do printf "%16s" "${fs%%:*}"; done
echo

declare -A COUNT
for fs in "${FLAGSETS[@]}"; do
    name="${fs%%:*}"; flags="${fs#*:}"
    # shellcheck disable=SC2086
    $CC $flags -c "$SRC/math_utils.c" -o "$OUT/mu_$name.o" 2>/dev/null
    # shellcheck disable=SC2086
    $CC $flags -c "$SRC/qr_decomp.c"  -o "$OUT/qr_$name.o" 2>/dev/null
    $OBJDUMP -d "$OUT/mu_$name.o" "$OUT/qr_$name.o" > "$OUT/dis_$name.txt" 2>/dev/null

    for r in $ROUTINES; do
        n=$(awk -v fn="$r" '
            $0 ~ ("<"fn">:") {inf=1; next}
            inf && /^$/ {inf=0}
            inf && /\t/ {c++}
            END {print c+0}' "$OUT/dis_$name.txt")
        COUNT["$r,$name"]=$n
    done
done

for r in $ROUTINES; do
    printf "%-18s" "$r"
    for fs in "${FLAGSETS[@]}"; do
        printf "%16s" "${COUNT[$r,${fs%%:*}]}"
    done
    echo
done

echo
echo "============================================================"
echo " PART 2: instruction mix (whole module)"
echo "============================================================"
printf "%-24s" "class"
for fs in "${FLAGSETS[@]}"; do printf "%16s" "${fs%%:*}"; done
echo

mix_row() {
    local label="$1"; local pattern="$2"
    printf "%-24s" "$label"
    for fs in "${FLAGSETS[@]}"; do
        name="${fs%%:*}"
        n=$(grep -cEi "$pattern" "$OUT/dis_$name.txt" 2>/dev/null || echo 0)
        printf "%16s" "$n"
    done
    echo
}
mix_row "total instructions"   $'\t'
mix_row "multiply (mul/mla)"   '\<(mul|mla|muls)\>'
mix_row "long mul (smull etc)" '\<(smull|umull|smlal|smmul)\>'
mix_row "hardware divide"      '\<(sdiv|udiv)\>'
mix_row "SOFTWARE divide call" '__aeabi_idiv'
mix_row "SIMD32 dual-MAC"      '\<(smlad|smuad|smusd|smusdx|smladx)\>'
mix_row "branches"             '\<(b|bl|beq|bne|bge|blt|ble|bgt|bx)\>'
mix_row "loads/stores"         '\<(ldr|str|ldm|stm)'

echo
echo "  KEY RESULT: compare 'hardware divide' against 'SOFTWARE divide call'."
echo "  The Cortex-A7 HAS SDIV, but Debian's default armhf baseline does not"
echo "  enable it -- so the default build pays a libgcc call anyway."
echo "  This is a compiler-flag result, not a hardware result. Report both."

# ============================================================================
# Part 3 -- dynamic operation counts (deterministic)
# ============================================================================
echo
echo "============================================================"
echo " PART 3: dynamic operation counts (instrumented build)"
echo "============================================================"
$CC -O2 -marm -mcpu=cortex-a7 -DPROFILE_OPS \
    -o "$OUT/profile_ops" profile_ops.c "$SRC/math_utils.c" "$SRC/qr_decomp.c" \
    -lm 2>&1 | head -20
if [ -x "$OUT/profile_ops" ]; then
    "$OUT/profile_ops" "${ITERATIONS:-1000}" "${MAGNITUDE:-8}"
else
    echo "  build of instrumented profiler FAILED"
fi

# ============================================================================
# Part 4 -- accuracy regression (correctness must not change)
# ============================================================================
echo
echo "============================================================"
echo " PART 4: accuracy regression"
echo "============================================================"
$CC -O2 -marm -mcpu=cortex-a7 -o "$OUT/test_acc" \
    ../tests/test_qr_accuracy.c "$SRC/math_utils.c" "$SRC/qr_decomp.c" \
    -lm 2>&1 | head -10
if [ -x "$OUT/test_acc" ]; then "$OUT/test_acc"; else echo "  build FAILED"; fi

echo
echo "============================================================"
echo " Artifacts in $OUT/ : disassembly per flag set (dis_*.txt)"
echo " Re-run after each optimisation and diff the tables above."
echo "============================================================"
