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
# NOTE on armv5te: Debian's armhf gcc defaults to -mfloat-abi=hard, and ARMv5
# has no FPU, so the build fails unless soft-float is requested explicitly.
# This flagset exists to model "an embedded core with no hardware divider",
# which is what the course notes' -march=armv5 assumes.
FLAGSETS=(
  "debian-default:-O2 -marm"
  "cortex-a7:-O2 -marm -mcpu=cortex-a7"
  "cortex-a7-O3:-O3 -marm -mcpu=cortex-a7"
  "armv5te-nodiv:-O2 -marm -march=armv5te -mfloat-abi=soft"
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
    rm -f "$OUT/mu_$name.o" "$OUT/qr_$name.o"
    # shellcheck disable=SC2086
    $CC $flags -c "$SRC/math_utils.c" -o "$OUT/mu_$name.o" 2>"$OUT/err_$name.txt"
    # shellcheck disable=SC2086
    $CC $flags -c "$SRC/qr_decomp.c"  -o "$OUT/qr_$name.o" 2>>"$OUT/err_$name.txt"
    if [ ! -f "$OUT/mu_$name.o" ] || [ ! -f "$OUT/qr_$name.o" ]; then
        echo "  !! BUILD FAILED for '$name' ($flags):"
        sed 's/^/     /' "$OUT/err_$name.txt" | head -5
        FAILED="${FAILED:-} $name"
    fi
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

# Classify by the MNEMONIC FIELD, not by regex over the whole line.
# objdump emits  "<addr>:\t<encoding>\t<mnemonic>\t<operands>", and in Thumb
# mode mnemonics carry .n/.w width suffixes (b.n, beq.w) plus condition codes.
# Matching raw lines undercounts Thumb branches badly, so strip suffixes first.
mix_all() {
    for fs in "${FLAGSETS[@]}"; do
        name="${fs%%:*}"
        awk -F'\t' -v OFS='' '
          NF>=3 {
            m = $3
            gsub(/^[ \t]+|[ \t]+$/, "", m)
            sub(/\..*$/, "", m)                      # drop .n / .w width suffix
            total++
            # strip a trailing condition code so beq/blt/... classify as branch
            base = m
            sub(/(eq|ne|cs|hs|cc|lo|mi|pl|vs|vc|hi|ls|ge|lt|gt|le|al)$/, "", base)
            if (m ~ /^(mul|mla|muls|mls)$/)                        mul++
            else if (m ~ /^(smull|umull|smlal|umlal|smmul|smmla)$/) lmul++
            else if (m ~ /^(sdiv|udiv)$/)                          divh++
            else if (m ~ /^(smlad|smladx|smuad|smuadx|smusd|smusdx)$/) simd32++
            if (base ~ /^(b|bl|bx|blx|cbz|cbnz)$/ || m ~ /^(b|bl|bx|blx)$/) br++
            if (m ~ /^(ldr|ldrb|ldrh|ldrd|ldm|str|strb|strh|strd|stm|push|pop)/) mem++
          }
          /__aeabi_idiv/ { idiv++ }
          END {
            printf "%d %d %d %d %d %d %d %d\n", total+0, mul+0, lmul+0, divh+0,
                   idiv+0, simd32+0, br+0, mem+0
          }' "$OUT/dis_$name.txt" > "$OUT/mix_$name.txt"
    done
}
mix_all
mix_row() {
    local label="$1"; local field="$2"
    printf "%-24s" "$label"
    for fs in "${FLAGSETS[@]}"; do
        name="${fs%%:*}"
        printf "%16s" "$(awk -v f="$field" '{print $f}' "$OUT/mix_$name.txt")"
    done
    echo
}
mix_row "total instructions"   1
mix_row "multiply (mul/mla)"   2
mix_row "long mul (smull etc)" 3
mix_row "hardware divide"      4
mix_row "SOFTWARE divide call" 5
mix_row "SIMD32 dual-MAC"      6
mix_row "branches"             7
mix_row "loads/stores"         8

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
