/*
 * Emit hw/vectors.txt: "opposite adjacent expected" per line.
 *
 * Expected values come from givensq_fw(), the same C model that specifies
 * src/common/givensq_fw.S. One vector file therefore validates the C, the ARM
 * assembly and the VHDL against each other -- which is the point of having a
 * single generator behind all three coefficient tables.
 *
 * The set is chosen to exercise the paths, not to be large: a VHDL simulation
 * runs far slower than native code, so coverage per vector matters.
 */
#include "../src/common/givensq_fw.h"
#include <stdio.h>
#include <stdlib.h>

static void emit(FILE *f, int o, int a, long *n) {
  fprintf(f, "%d %d %d\n", o, a, (int)givensq_fw(o, a));
  (*n)++;
}

int main(int argc, char **argv) {
  const char *path = (argc > 1) ? argv[1] : "hw/vectors.txt";
  FILE *f = fopen(path, "w");
  if (!f) { perror(path); return 1; }
  long n = 0;

  /* Edge cases first -- these are the ones that break implementations. */
  emit(f, 0, 0, &n);          /* both zero: the disputed case            */
  emit(f, 0, 1000, &n);       /* zero opposite: angle 0                  */
  emit(f, 1000, 0, &n);       /* zero adjacent: +pi/2, divide skipped    */
  emit(f, -1000, 0, &n);      /* zero adjacent, negative: -pi/2          */
  emit(f, 1000, 1000, &n);    /* |n| == |d|: ratio exactly 1.0           */
  emit(f, -1000, 1000, &n);
  emit(f, 1000, -1000, &n);
  emit(f, -1000, -1000, &n);
  emit(f, 1, 2047, &n);       /* smallest non-zero ratio                 */
  emit(f, 2047, 1, &n);       /* largest ratio, swap path                */
  emit(f, 2047, 2047, &n);
  emit(f, -2048, 2047, &n);

  /* All four sign combinations across the swap boundary. */
  for (int o = -2000; o <= 2000; o += 250)
    for (int a = -2000; a <= 2000; a += 250)
      emit(f, o, a, &n);

  fclose(f);
  fprintf(stderr, "wrote %ld vectors to %s\n", n, path);
  return 0;
}
