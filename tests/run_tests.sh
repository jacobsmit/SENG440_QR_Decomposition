#!/bin/bash

# Exit on error
set -e

echo "============================="
echo " Compiling and Running Tests "
echo "============================="
echo ""

# Compile Arctan test
gcc -o test_arctan test_arctan.c ../src/software/trig_approx.c -lm
echo "[1/2] Arctan Tests:"
./test_arctan
echo ""

# Compile Trig test
gcc -o test_trig test_trig.c ../src/software/trig_approx.c -lm
echo "[2/2] Sine & Cosine Tests:"
./test_trig
echo ""

# Cleanup
rm test_arctan test_trig
echo "All tests passed successfully!"
