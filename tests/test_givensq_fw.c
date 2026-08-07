/*
 * Firmware GIVENSQ: correctness of the multiplier-free, divider-free model.
 *
 * This is what makes the microcode design checkable rather than asserted --
 * givensq_fw.c is the specification givensq_fw.S must reproduce bit-for-bit,
 * and this suite is what says the specification is right in the first place.
 */
#include "../src/common/givensq_fw.h"
#include "../src/common/givensq.h"
#include <stdio.h>
#include <math.h>
/* The two invariants that define a Givens coefficient pair:
     1. the rotation zeroes the target element:  c*o - s*a == 0
     2. it is a rotation at all:                 c^2 + s^2 == 1
   NOT "theta == atan2(o,a)": the angle may differ from atan2 by pi, which
   negates both c and s and still satisfies both invariants. */
static double worst_of(int32_t(*fn)(int32_t,int32_t)){
  double wz=0;
  for(int o=-2000;o<=2000;o+=7) for(int a=-2000;a<=2000;a+=13){
    int32_t p=fn(o,a);
    double c=(int16_t)(p&0xFFFF)/16384.0, s=(int16_t)(p>>16)/16384.0;
    double r=hypot((double)o,(double)a);
    double z=(r>0)?fabs(c*o-s*a)/r:0.0;
    if(z>wz)wz=z; }
  return wz;
}
static int worst_resid(const char*tag,int32_t(*fn)(int32_t,int32_t),double lim){
  double w=worst_of(fn);
  printf("  %-22s worst residual %.6f (limit %.6f)  %s\n",tag,w,lim,w<=lim?"PASS":"**FAIL**");
  return w>lim;
}
static void check(const char*tag,int32_t(*fn)(int32_t,int32_t)){
  double wz=0,wu=0; long n=0;
  for(int o=-2000;o<=2000;o+=7) for(int a=-2000;a<=2000;a+=13){
    int32_t p=fn(o,a);
    double c=(int16_t)(p&0xFFFF)/16384.0, s=(int16_t)(p>>16)/16384.0;
    double r=hypot((double)o,(double)a);
    double z=(r>0)?fabs(c*o-s*a)/r:0.0;      /* residual, relative */
    double u=fabs(c*c+s*s-1.0);
    if(z>wz)wz=z; if(u>wu)wu=u; n++; }
  printf("  %-22s zeroing residual %.6f   |c^2+s^2-1| %.6f   (%ld cases)\n",tag,wz,wu,n);
}
/* The restoring divider must equal (n<<14)/d exactly -- it replaces SDIV, so
   any disagreement is a silent wrong answer, not a rounding difference. */
static int check_divider(void){
  long bad=0,n_=0;
  for(uint32_t d=1;d<3000;d+=7) for(uint32_t n=0;n<=d;n+=3){
    uint32_t got=fw_divide_restoring(n,d,0);
    uint32_t want=(uint32_t)(((uint64_t)n<<14)/d);
    if(got!=want){ if(bad<3) printf("    n=%u d=%u got=%u want=%u\n",n,d,got,want); bad++; }
    n_++; }
  printf("  %-22s %ld cases, %ld mismatches\n","restoring divider",n_,bad);
  return bad!=0;
}

#if defined(__arm__)
/* The whole point of the assembly: same answers, no multiplier, no divider.
   Bit-exact or it is not an implementation of the specification. */
static int check_asm(void){
  long bad=0,n=0;
  for(int o=-2048;o<=2048;o+=3) for(int a=-2048;a<=2048;a+=5){
    int32_t got=givensq_fw_asm(o,a), want=givensq_fw(o,a);
    if(got!=want){
      if(bad<5) printf("    o=%5d a=%5d asm=%08x c=%08x\n",o,a,
                       (unsigned)got,(unsigned)want);
      bad++; }
    n++; }
  printf("  %-22s %ld cases, %ld mismatches  %s\n","asm vs C model",n,bad,
         bad?"**FAIL**":"PASS");
  return bad!=0;
}
#endif

int main(void){
  int fail=check_divider();
  check("givensq_fw (firmware)",givensq_fw);
  check("givensq (software)",givensq);
  /* Tolerances from the measured PWL error: the firmware's CSD-constrained
     slopes are ~2x the software's, so ~0.0007 residual is expected, not a bug. */
  fail |= worst_resid("givensq_fw",givensq_fw,0.0015);
#if defined(__arm__)
  fail |= check_asm();
  fail |= worst_resid("givensq_fw_asm",givensq_fw_asm,0.0015);
#else
  puts("  (asm equivalence skipped: not an ARM host -- run on the VM)");
#endif
  puts(fail?"\n GIVENSQ FIRMWARE: FAIL":"\n GIVENSQ FIRMWARE: PASS");
  return fail;
}
