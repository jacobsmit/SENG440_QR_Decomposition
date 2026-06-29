#!/bin/bash

# Exit on error
set -e

cd "$(dirname "$0")"

echo "============================="
echo " Compiling QR Profiler       "
echo "============================="
echo ""

# Compile the profiling script
gcc -Wall -Wextra -O3 -o profile_components \
    profile_components.c \
    ../src/software_base/math_utils.c \
    ../src/software_base/qr_decomp.c \
    -lm

echo "Running Profiler..."
echo ""
./profile_components

echo ""
echo "Cleaning up..."
rm profile_components
echo "Profiling completed successfully!"
