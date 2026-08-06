# Compiler flag sets used for the static ARM analysis.
#
# Single source of truth: arm_profile.sh sources this, and the Makefile
# delegates to arm_profile.sh rather than keeping a second copy.
#
# Modelling "a core with no hardware divider":
#   armv7-a has no integer-divide extension, so -march=armv7-a already yields a
#   __aeabi_idiv call instead of SDIV. That is the no-divider comparison and it
#   builds cleanly. It is also exactly Debian's default armhf baseline.
#
#   Spell the FPU out. Passing -march=armv7-a alone resets the FP spec to
#   "none", and Debian's gcc defaults to -mfloat-abi=hard, so the bare form
#   dies with "selected architecture lacks an FPU". vfpv3-d16 + hard is
#   Debian armhf's baseline. ("+idiv" is not a valid armv7-a feature name --
#   getting SDIV means -mcpu=cortex-a7.)
#
#   Do NOT reach for -march=armv5te here: Debian armhf is a hard-float-only
#   port, and -mfloat-abi=soft fails because glibc ships only gnu/stubs-hard.h.
#   For a genuine ARMv5 comparison, build bare-metal with arm-none-eabi-gcc.

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
