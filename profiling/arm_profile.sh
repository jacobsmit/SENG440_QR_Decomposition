#!/bin/bash
# ============================================================================
# Static ARM analysis: instruction counts and instruction mix, per variant,
# per compiler flag set.
#
# This is the half of profiling that the Makefile cannot do conveniently
# (multi-flagset objdump parsing). Dynamic operation counts and accuracy come
# from `make profile-all` and `make test-all`.
#
# It deliberately does NOT report wall-clock time. QEMU's TCG has no pipeline,
# cache or cycle model, so seconds measured in the guest describe the host
# machine, not the Cortex-A7 (docs/TARGET_PLATFORM.md).
#
# Usage:  make static-all
#     or: ./profiling/arm_profile.sh            (from the repo root or here)
# ============================================================================
set -u

cd "$(dirname "$0")/.."   # repo root
. profiling/flagsets.sh

VARIANTS="${VARIANTS:-naive_float fixed_scalar}"
FLAGSETS="${FLAGSETS:-$FLAGSETS}"
OUT=build/static
mkdir -p "$OUT"

# --- toolchain -------------------------------------------------------------
if [ "$(uname -m)" = "armv7l" ] || [ "$(uname -m)" = "armv6l" ]; then
    CC="${ARM_CC:-gcc}"; DUMP="${ARM_DUMP:-objdump}"
elif command -v arm-linux-gnueabihf-gcc >/dev/null 2>&1; then
    CC="${ARM_CC:-arm-linux-gnueabihf-gcc}"
    DUMP="${ARM_DUMP:-arm-linux-gnueabihf-objdump}"
else
    echo "ERROR: no ARM toolchain found."
    echo "  Run this inside the ARM VM, or install arm-linux-gnueabihf-gcc."
    exit 1
fi
echo "toolchain: $CC ($($CC -dumpmachine))"
echo

COMMON="src/common/matrix.c src/common/trig_pwl.c src/common/op_counters.c"

# --- build every variant x flagset ----------------------------------------
FAILED=""
for v in $VARIANTS; do
    for fs in $FLAGSETS; do
        flags="$(flags_for "$fs")"
        tag="${v}__${fs}"
        rm -f "$OUT/$tag.o"
        # shellcheck disable=SC2086
        $CC $flags -Isrc/common -c "src/variants/$v/qr.c" -o "$OUT/${tag}_qr.o" \
            2>"$OUT/$tag.err"
        ok=1
        for c in $COMMON; do
            b=$(basename "$c" .c)
            # shellcheck disable=SC2086
            $CC $flags -Isrc/common -c "$c" -o "$OUT/${tag}_${b}.o" \
                2>>"$OUT/$tag.err" || ok=0
        done
        if [ ! -f "$OUT/${tag}_qr.o" ] || [ "$ok" -eq 0 ]; then
            echo "  !! BUILD FAILED: $v / $fs ($flags)"
            sed 's/^/     /' "$OUT/$tag.err" | head -4
            FAILED="$FAILED $tag"
            : > "$OUT/$tag.dis"
            continue
        fi
        $DUMP -d "$OUT/${tag}"_*.o > "$OUT/$tag.dis" 2>/dev/null
    done
done

# --- classifier ------------------------------------------------------------
# Classify by the MNEMONIC FIELD, not by regex over the whole line. objdump
# emits "<addr>:\t<encoding>\t<mnemonic>\t<operands>", and in Thumb mode
# mnemonics carry .n/.w width suffixes plus condition codes. Matching raw
# lines undercounts Thumb branches badly.
mix_of() {
    awk -F'\t' '
      NF>=3 {
        m = $3
        gsub(/^[ \t]+|[ \t]+$/, "", m)
        sub(/\..*$/, "", m)
        total++
        base = m
        sub(/(eq|ne|cs|hs|cc|lo|mi|pl|vs|vc|hi|ls|ge|lt|gt|le|al)$/, "", base)
        if (m ~ /^(mul|mla|muls|mls)$/)                            mul++
        else if (m ~ /^(smull|umull|smlal|umlal|smmul|smmla)$/)     lmul++
        else if (m ~ /^(sdiv|udiv)$/)                              divh++
        else if (m ~ /^(smlad|smladx|smuad|smuadx|smusd|smusdx)$/)  simd++
        else if (m ~ /^clz$/)                                      clz++
        else if (m ~ /^(vmul|vadd|vsub|vdiv|vmla|vcvt|vldr|vstr|vmov)/) vfp++
        if (base ~ /^(b|bl|bx|blx|cbz|cbnz)$/) br++
        if (m ~ /^(ldr|ldrb|ldrh|ldrd|ldm|str|strb|strh|strd|stm|push|pop)/) mem++
      }
      /__aeabi_idiv/  { idiv++ }
      /(atan2f|cosf|sinf|sqrtf)/ { libm++ }
      END { printf "%d %d %d %d %d %d %d %d %d %d\n",
                   total+0,mul+0,lmul+0,divh+0,idiv+0,simd+0,clz+0,br+0,mem+0,vfp+0 }
    ' "$1"
}

for v in $VARIANTS; do
    echo "============================================================"
    echo " VARIANT: $v"
    echo "============================================================"

    # ---- per-routine static counts. Routines differ between variants, so
    #      discover them from the disassembly rather than hardcoding a list.
    ref="$OUT/${v}__$(echo "$FLAGSETS" | awk '{print $1}').dis"
    routines=$(grep -oE '^[0-9a-f]+ <[a-zA-Z_][a-zA-Z0-9_]*>:' "$ref" 2>/dev/null \
               | sed 's/.*<//;s/>:$//' | sort -u)

    printf "%-26s" "routine"
    for fs in $FLAGSETS; do printf "%16s" "$fs"; done; echo
    for r in $routines; do
        printf "%-26s" "$r"
        for fs in $FLAGSETS; do
            n=$(awk -v fn="$r" '
                $0 ~ ("<"fn">:") {inf=1; next}
                inf && /^$/ {inf=0}
                inf && /\t/ {c++}
                END {print c+0}' "$OUT/${v}__${fs}.dis" 2>/dev/null)
            printf "%16s" "$n"
        done
        echo
    done

    # ---- instruction mix
    echo
    printf "%-26s" "instruction class"
    for fs in $FLAGSETS; do printf "%16s" "$fs"; done; echo
    for fs in $FLAGSETS; do mix_of "$OUT/${v}__${fs}.dis" > "$OUT/${v}__${fs}.mix"; done
    i=1
    for label in "total instructions" "multiply (mul/mla)" "long mul (smull)" \
                 "hardware divide" "SOFTWARE idiv call" "SIMD32 dual-MAC" \
                 "clz (normalisation)" "branches" "loads/stores" "VFP/float ops"; do
        printf "%-26s" "$label"
        for fs in $FLAGSETS; do
            printf "%16s" "$(awk -v f=$i '{print $f}' "$OUT/${v}__${fs}.mix")"
        done
        echo
        i=$((i+1))
    done
    echo
done

cat <<'NOTE'
============================================================
 Reading these tables
============================================================
 * hardware divide vs SOFTWARE idiv call: the Cortex-A7 HAS SDIV, but the
   armv7-a baseline (also Debian's default armhf baseline) does not enable it,
   so a default build pays a libgcc call anyway. A compiler-flag result, not a
   hardware one -- report both.

 * These static counts UNDERSTATE the divider cost. A call to __aeabi_idiv is
   only 1-2 instructions at the call site; the expensive part is the libgcc
   routine body, which lives in another module and is not counted here. The
   penalty belongs in a hand cycle-count from the Cortex-A7 TRM.

 * "total instructions" is a COUNT, not code size. Thumb instructions are 2
   bytes against ARM's 4, so Thumb usually shows more instructions while
   producing smaller code. Use `size` on the objects for byte counts.

 * VFP/float ops should be ~0 for every fixed-point variant. A non-zero count
   there means floating point leaked into an integer code path.

 Artifacts: build/static/*.dis (disassembly), *.mix (raw classifier output)
============================================================
NOTE

if [ -n "$FAILED" ]; then
    echo
    echo "BUILDS FAILED:$FAILED"
    exit 1
fi
