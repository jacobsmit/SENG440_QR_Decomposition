#!/bin/bash
# Convenience wrapper -- the build matrix lives in the root Makefile.
#
#   ./tests/run_tests.sh              every variant
#   ./tests/run_tests.sh fixed_scalar one variant
#
# Exits non-zero if any invariant is outside tolerance, so it is safe as a gate
# before/after an optimisation (or in a git hook).

cd "$(dirname "$0")/.." || exit 2

if [ $# -eq 0 ]; then
    exec make test-all
fi

status=0
for v in "$@"; do
    if [ ! -d "src/variants/$v" ]; then
        echo "ERROR: no such variant '$v'."
        echo "  available: $(ls src/variants | tr '\n' ' ')"
        exit 2
    fi
    make "test-$v" || status=1
done

if [ "$status" -ne 0 ]; then
    echo ""
    echo "TESTS FAILED -- an invariant is outside tolerance."
    echo "Do not widen a tolerance just to make this green; if the change was"
    echo "intentional, record the new baseline in tests/test_qr_accuracy.c."
fi
exit "$status"
