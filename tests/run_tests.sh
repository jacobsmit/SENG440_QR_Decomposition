#!/bin/bash
# Build and run the QR accuracy regression suite.
# Exits non-zero if any invariant is outside tolerance, so this is safe to use
# as a gate before/after an optimisation (and in a git hook or CI).
#
# Overridable:  CC=gcc  CFLAGS="-O2 -marm -mcpu=cortex-a7"  ./run_tests.sh

cd "$(dirname "$0")" || exit 2

CC=${CC:-gcc}
if [ -z "${CFLAGS+x}" ]; then
    case "$(uname -m)" in
        armv7l|armv6l) CFLAGS="-O2 -marm -mcpu=cortex-a7" ;;
        *)             CFLAGS="-O2" ;;
    esac
fi
SRC=../src/software_base
BIN=./test_qr_accuracy

echo "============================="
echo " Compiling QR Accuracy Tests "
echo "============================="
echo "  CC=$CC"
echo "  CFLAGS=$CFLAGS"
echo ""

# shellcheck disable=SC2086
if ! $CC -Wall -Wextra $CFLAGS -o "$BIN" \
        test_qr_accuracy.c "$SRC/math_utils.c" "$SRC/qr_decomp.c" -lm; then
    echo ""
    echo "BUILD FAILED"
    exit 2
fi

echo "Running Tests..."
echo ""
"$BIN"
STATUS=$?

rm -f "$BIN"

echo ""
if [ "$STATUS" -eq 0 ]; then
    echo "All tests passed."
else
    echo "TESTS FAILED (exit $STATUS) -- an invariant is outside tolerance."
    echo "Do not widen a tolerance just to make this green; if the change was"
    echo "intentional, record the new baseline in test_qr_accuracy.c."
fi
exit "$STATUS"
