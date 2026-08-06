
# Compiler flag sets for the static analysis. Sourced by arm_profile.sh and the
# Makefile so there is only one copy.
#
# armv7-a is the "no hardware divider" baseline: ARMv7-A has no integer divide
# extension, so it emits __aeabi_idiv. The FPU must be spelled out or the build
# fails ("selected architecture lacks an FPU"), because Debian's gcc defaults to
# hard-float. Do not use -march=armv5te: Debian armhf is hard-float only and
# ships no gnu/stubs-soft.h.

FLAGSETS="cortex-a7 armv7a-nodiv cortex-a7-O3 cortex-a7-thumb"

flags_for() {
    case "$1" in
        cortex-a7)       echo "-O2 -marm -mcpu=cortex-a7" ;;
        armv7a-nodiv)    echo "-O2 -marm -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard" ;;
        cortex-a7-O3)    echo "-O3 -marm -mcpu=cortex-a7" ;;
        cortex-a7-thumb) echo "-O2 -mthumb -mcpu=cortex-a7" ;;
        *) echo "" ;;
    esac
}
