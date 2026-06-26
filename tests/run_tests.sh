#!/bin/bash

# Exit on error
set -e

echo "============================="
echo " Compiling QR Accuracy Tests "
echo "============================="
echo ""

# Compile the accuracy test suite
gcc -Wall -Wextra -O3 -o test_qr_accuracy \
    test_qr_accuracy.c \
    ../src/software_base/math_utils.c \
    ../src/software_base/qr_decomp.c \
    -lm

echo "Running Tests..."
echo ""
./test_qr_accuracy

echo ""
echo "Cleaning up..."
rm test_qr_accuracy
echo "All tests passed successfully!"
