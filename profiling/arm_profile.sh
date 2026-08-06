#!/bin/bash
# Static ARM analysis: instruction counts and instruction mix, per variant, per
# compiler flag set. Dynamic counts come from `make profile-all` and
# `make cycles`. Wall-clock time is never reported -- QEMU has no timing model.
#
# Usage: make static-all
set -u

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

# The ALGORITHM only. matrix_f32.c (boundary conversions, float matrix maths,
# printing) and op_counters.c (instrumentation) are test/report scaffolding --
# including them added ~500 instructions and 150 VFP ops of noise to every
# variant and made the vfp_ops leak-detector column meaningless.
#
# Sources are chosen PER VARIANT: counting trig_pwl.c against naive_float (which
# never calls it) or against a future CORDIC variant would inflate them with
# dead code. Detected from what the variant's qr.c actually includes.
algo_sources_for() {
    srcs="src/common/matrix.c"
    if grep -q 'trig_pwl\.h' "src/variants/$1/qr.c"; then
        srcs="$srcs src/common/trig_pwl.c"
    fi
    echo "$srcs"
}

# --- build every variant x flagset ----------------------------------------
FAILED=""
for v in $VARIANTS; do
    for fs in $FLAGSETS; do
        flags="$(flags_for "$fs")"
        tag="${v}__${fs}"
        # Remove EVERY object for this tag, not "$tag.o" which never exists --
        # the objects are ${tag}_<basename>.o. Getting this wrong left stale
        # objects from earlier runs (op_counters.o in particular) to be picked
        # up by the glob below, adding ~400 instructions and ~130 phantom VFP
        # ops to every variant.
        rm -f "$OUT/${tag}"_*.o "$OUT/$tag.dis" "$OUT/$tag.mix"
        # shellcheck disable=SC2086
        $CC $flags -Isrc/common -c "src/variants/$v/qr.c" -o "$OUT/${tag}_qr.o" \
            2>"$OUT/$tag.err"
        ok=1
        for c in $(algo_sources_for "$v"); do
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
        # Genuine floating-point arithmetic -- a real leak in an integer path.
        else if (m ~ /^(vmul|vadd|vsub|vdiv|vmla|vmls|vfma|vfms|vneg|vabs|vsqrt|vcvt|vcmp)/) fparith++
        # VFP registers used for data movement. NOT necessarily float: gcc will
        # park 64-bit integer values (e.g. smull register pairs) in VFP double
        # registers. Counted separately so it cannot be mistaken for a leak.
        else if (m ~ /^(vldr|vstr|vmov|vpush|vpop|vldm|vstm)/) vfpmove++
        if (base ~ /^(b|bl|bx|blx|cbz|cbnz)$/) br++
        if (m ~ /^(ldr|ldrb|ldrh|ldrd|ldm|str|strb|strh|strd|stm|push|pop)/) mem++
      }
      /__aeabi_idiv/  { idiv++ }
      /(atan2f|cosf|sinf|sqrtf)/ { libm++ }
      END { printf "%d %d %d %d %d %d %d %d %d %d %d\n",
                   total+0,mul+0,lmul+0,divh+0,idiv+0,simd+0,clz+0,br+0,mem+0,
                   fparith+0,vfpmove+0 }
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
                 "clz (normalisation)" "branches" "loads/stores" \
                 "FP arithmetic (leak?)" "VFP reg moves (benign)"; do
        printf "%-26s" "$label"
        for fs in $FLAGSETS; do
            printf "%16s" "$(awk -v f=$i '{print $f}' "$OUT/${v}__${fs}.mix")"
        done
        echo
        i=$((i+1))
    done

    # If genuine FP arithmetic appears in a fixed-point variant, show exactly
    # which instructions rather than leaving it to guesswork.
    for fs in $FLAGSETS; do
        n=$(awk -v f=10 '{print $f}' "$OUT/${v}__${fs}.mix" 2>/dev/null)
        if [ "${n:-0}" -gt 0 ] && [ "$v" != "naive_float" ]; then
            echo
            echo "  !! $fs: $n floating-point arithmetic instruction(s) in an"
            echo "     integer variant. Offending instructions:"
            grep -oE '\t(vmul|vadd|vsub|vdiv|vmla|vmls|vfma|vfms|vneg|vabs|vsqrt|vcvt|vcmp)[a-z0-9.]*' \
                "$OUT/${v}__${fs}.dis" | sort | uniq -c | sed 's/^/       /'
        fi
    done
    echo
done

# --- machine-readable summary for `make compare` -------------------------
CSV=build/static.csv
{
  echo "variant,flagset,total_instr,mul,long_mul,hw_div,soft_idiv,simd32,clz,branches,mem_ops,fp_arith,vfp_moves"
  for v in $VARIANTS; do
    for fs in $FLAGSETS; do
      m="$OUT/${v}__${fs}.mix"
      [ -f "$m" ] || continue
      awk -v v="$v" -v fs="$fs" '{printf "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n",
        v,fs,$1,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11}' "$m"
    done
  done
} > "$CSV"
echo "wrote $CSV"
echo

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

 * FP arithmetic should be 0 for every fixed-point variant; a non-zero count is
   a real leak and the offending instructions are dumped above. "VFP reg moves"
   is NOT a leak -- gcc parks 64-bit integer values (smull register pairs) in
   VFP double registers, which is legitimate.

 * DO NOT compare naive_float's total against a fixed variant's and call it a
   speed-up. naive_float looks small because its real work is inside libm --
   atan2f/cosf/sinf are hundreds of instructions each, called 18 times per QR,
   and none of that is in this module. Same caveat as __aeabi_idiv above.

 Artifacts: build/static/*.dis (disassembly), *.mix (raw classifier output)
============================================================
NOTE

if [ -n "$FAILED" ]; then
    echo
    echo "BUILDS FAILED:$FAILED"
    exit 1
fi
