#ifndef GIVENSQ_H
#define GIVENSQ_H

/*
 * GIVENSQ -- the application-specific instruction (ASIP extension).
 *
 *   GIVENSQ Rd, Rn, Rm
 *     Rn = opposite   (R[i][j])
 *     Rm = adjacent   (R[j][j])
 *     Rd[31:16] = s = sin(theta)  Q14, signed
 *     Rd[15:0]  = c = cos(theta)  Q14, signed
 *   where theta = atan2(opposite, adjacent).
 *
 * ARM allows at most 2 inputs and 1 output, but a Givens rotation needs BOTH
 * sin and cos. Packing them as two 16-bit halves keeps the unit reentrant and
 * stateless -- no non-reentrant Read/Read/Execute/Write unit, so no questions
 * about masking interrupts or restoring unit state after an exception.
 *
 * The layout is not arbitrary. It is exactly what SMLAD/SMUSDX consume:
 *     SMLAD (cs, t) = cs.lo*t.lo + cs.hi*t.hi = c*tj + s*ti  -> row_j
 *     SMUSDX(cs, t) = cs.lo*t.hi - cs.hi*t.lo = c*ti - s*tj  -> row_i
 * so GIVENSQ's result drives the rotation with ZERO repacking. The constraint
 * the ISA imposed turns out to be the format the DSP extension wants.
 *
 * TWO BUILDS:
 *   default            -- givensq_ref(), a bit-exact C model. Links, runs, and
 *                         is accuracy-tested like any other variant.
 *   -DUSE_GIVENSQ_ASM  -- inline assembly instantiating the real instruction.
 *                         COMPILES ONLY. The assembler has no opcode for
 *                         GIVENSQ, so this build can never be linked; inspect
 *                         the -S listing (make asip-asm).
 *
 * givensq_ref() is the single source of truth: it generates the vectors for the
 * C build, the microcode hand-simulation, and the VHDL testbench. All three
 * must agree bit-for-bit.
 */

#include "fixed.h"
#include "trig_pwl.h"
#include <stdint.h>

#define GIVENSQ_Q 14 /* coefficient format of the packed result */

/* --- bit-exact software model ------------------------------------------- */

static inline int32_t givensq_pack(int32_t s_q14, int32_t c_q14) {
  return (int32_t)(((uint32_t)(uint16_t)s_q14 << 16) |
                   (uint32_t)(uint16_t)c_q14);
}

/* The PWL unit emits Q14 natively, so the packed result needs no rescaling.
   Enforced rather than assumed -- a change to either format must be deliberate. */
_Static_assert(GIVENSQ_Q == TRIG_FRAC_BITS,
               "GIVENSQ result format must match the trig unit's output format");

static inline int32_t givensq_ref(int32_t opposite, int32_t adjacent) {
  /* cos before sin, matching fixed_simd32, so the operation counters line up
     between the two variants and the comparison stays honest. */
  int32_t angle = calculate_arctan_ratio(opposite, adjacent); /* Q14 */
  int32_t c14 = cos_fixed(angle);
  int32_t s14 = sin_fixed(angle);
  return givensq_pack(s14, c14);
}

/* Unpack helpers. The halves are signed, hence the int16_t casts. */
static inline int32_t givensq_c(int32_t packed) {
  return (int16_t)(packed & 0xFFFF);
}
static inline int32_t givensq_s(int32_t packed) {
  return (int16_t)(packed >> 16);
}

/* --- instruction instantiation ------------------------------------------ */

#if defined(USE_GIVENSQ_ASM)
/*
 * Compiles to a listing containing "GIVENSQ r0, r1, r2" and stops there --
 * augmenting the assembler with a user-defined opcode is out of scope for the
 * project (Lesson 100: "You cannot assemble such code, since the assembler
 * cannot allocate an operation code").
 */
static inline int32_t givensq(int32_t opposite, int32_t adjacent) {
  int32_t result;
  __asm__("GIVENSQ %0, %1, %2"
          : "=r"(result)
          : "r"(opposite), "r"(adjacent));
  return result;
}
#define GIVENSQ_IMPL "inline-asm (compile-only)"
#else
static inline int32_t givensq(int32_t opposite, int32_t adjacent) {
  return givensq_ref(opposite, adjacent);
}
#define GIVENSQ_IMPL "C reference model"
#endif

/*
 * KNOWN SPEC MISMATCH -- settle before writing the VHDL.
 *
 * docs/NEW_INSTRUCTION_PLAN.md says (opposite==0, adjacent==0) should give an
 * identity rotation. The software does not: calculate_arctan_ratio(0,0) takes
 * the D==0 branch and returns +pi/2, giving a 90-degree rotation instead.
 *
 * The reference model reproduces the SOFTWARE behaviour so fixed_asip stays
 * bit-identical to fixed_simd32. It is harmless (still orthogonal, and R[i][j]
 * is zeroed afterwards) and rare -- the guard fires 0.06 times per QR -- but the
 * C model, the microcode and the VHDL must all agree on whichever is chosen.
 */

#endif /* GIVENSQ_H */
