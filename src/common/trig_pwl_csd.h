/* GENERATED -- firmware (multiplier-free) PWL tables.
 *
 * Slopes are constrained to THREE signed powers of two so the microcoded
 * engine, which has no multiplier, can evaluate m*x as three shift-adds.
 * The constrained slope is still an exact integer, so (m*x)>>14 here is
 * bit-identical to the assembly's shift-add chain -- that equality is what
 * makes givensq_fw.S testable rather than merely plausible.
 *
 * SEPARATE from trig_pwl_tables.h on purpose: the real ARM has SMULL, so
 * the software variants keep exact slopes and their better accuracy. Only
 * the firmware and hardware are multiplier-free. */
#ifndef TRIG_PWL_CSD_H
#define TRIG_PWL_CSD_H

#include <stdint.h>

#define CSD_SEG_BITS 4
#define CSD_TRIG_Q   14
#define CSD_INDEX_SHIFT (CSD_TRIG_Q - CSD_SEG_BITS)

/* arctan: 17 segments, max error 0.000434 */
#define CSD_ARCTAN_SEGS 17
static const int32_t csd_arctan[CSD_ARCTAN_SEGS] = {
    0x00013FEC, /* [ 0] m= 16364 = -2^2 -2^4 +2^14        b=     1 */
    0x00093F70, /* [ 1] m= 16240 = -2^4 -2^7 +2^14        b=     9 */
    0x00273E80, /* [ 2] m= 16000 = +2^7 -2^9 +2^14        b=    39 */
    0x006F3D00, /* [ 3] m= 15616 = +2^8 -2^10 +2^14       b=   111 */
    0x00CE3B80, /* [ 4] m= 15232 = -2^7 -2^10 +2^14       b=   206 */
    0x01963900, /* [ 5] m= 14592 = +2^8 -2^11 +2^14       b=   406 */
    0x02583700, /* [ 6] m= 14080 = -2^8 -2^11 +2^14       b=   600 */
    0x03AA3400, /* [ 7] m= 13312 = +2^10 +2^12 +2^13      b=   938 */
    0x04AE3200, /* [ 8] m= 12800 = +2^9 +2^12 +2^13       b=  1198 */
    0x06142F80, /* [ 9] m= 12160 = -2^7 +2^12 +2^13       b=  1556 */
    0x08472C00, /* [10] m= 11264 = +2^10 +2^11 +2^13      b=  2119 */
    0x09B02A00, /* [11] m= 10752 = +2^9 +2^11 +2^13       b=  2480 */
    0x0B6227C0, /* [12] m= 10176 = -2^6 +2^11 +2^13       b=  2914 */
    0x0DA02500, /* [13] m=  9472 = +2^8 +2^10 +2^13       b=  3488 */
    0x0F652300, /* [14] m=  8960 = +2^8 +2^9 +2^13        b=  3941 */
    0x11422104, /* [15] m=  8452 = +2^2 +2^8 +2^13        b=  4418 */
    0x11422104, /* [16] m=  8452 = +2^2 +2^8 +2^13        b=  4418 */
};

/* sin: 14 segments, max error 0.000553 */
#define CSD_SIN_SEGS 14
static const int32_t csd_sin[CSD_SIN_SEGS] = {
    0x00013FF6, /* [ 0] m= 16374 = -2^1 -2^3 +2^14        b=     1 */
    0x00053FB8, /* [ 1] m= 16312 = -2^3 -2^6 +2^14        b=     5 */
    0x00133F40, /* [ 2] m= 16192 = +2^6 -2^8 +2^14        b=    19 */
    0x00373E80, /* [ 3] m= 16000 = +2^7 -2^9 +2^14        b=    55 */
    0x00773D80, /* [ 4] m= 15744 = -2^7 -2^9 +2^14        b=   119 */
    0x00DB3C40, /* [ 5] m= 15424 = +2^6 -2^10 +2^14       b=   219 */
    0x01513B00, /* [ 6] m= 15104 = -2^8 -2^10 +2^14       b=   337 */
    0x02303900, /* [ 7] m= 14592 = +2^8 -2^11 +2^14       b=   560 */
    0x03323700, /* [ 8] m= 14080 = -2^8 -2^11 +2^14       b=   818 */
    0x03BA3600, /* [ 9] m= 13824 = -2^9 -2^11 +2^14       b=   954 */
    0x06393200, /* [10] m= 12800 = +2^9 +2^12 +2^13       b=  1593 */
    0x078B3020, /* [11] m= 12320 = +2^5 +2^12 +2^13       b=  1931 */
    0x09222E00, /* [12] m= 11776 = -2^9 +2^12 +2^13       b=  2338 */
    0x09222E00, /* [13] m= 11776 = -2^9 +2^12 +2^13       b=  2338 */
};

/* cos: 14 segments, max error 0.000406 */
#define CSD_COS_SEGS 14
static const int32_t csd_cos[CSD_COS_SEGS] = {
    0x4004FE00, /* [ 0] m=  -512 = +2^0 -2^0 -2^9         b= 16388 */
    0x4044FA02, /* [ 1] m= -1534 = +2^1 +2^9 -2^11        b= 16452 */
    0x40C4F608, /* [ 2] m= -2552 = +2^3 -2^9 -2^11        b= 16580 */
    0x417FF220, /* [ 3] m= -3552 = +2^5 +2^9 -2^12        b= 16767 */
    0x4277EE40, /* [ 4] m= -4544 = +2^6 -2^9 -2^12        b= 17015 */
    0x43CDEA00, /* [ 5] m= -5632 = +2^9 +2^11 -2^13       b= 17357 */
    0x44EEE700, /* [ 6] m= -6400 = -2^8 +2^11 -2^13       b= 17646 */
    0x46ADE300, /* [ 7] m= -7424 = +2^8 +2^9 -2^13        b= 18093 */
    0x4866DF90, /* [ 8] m= -8304 = +2^4 -2^7 -2^13        b= 18534 */
    0x4A43DC40, /* [ 9] m= -9152 = +2^6 -2^10 -2^13       b= 19011 */
    0x4C4AD900, /* [10] m= -9984 = +2^8 -2^11 -2^13       b= 19530 */
    0x4E58D600, /* [11] m=-10752 = -2^9 -2^11 -2^13       b= 20056 */
    0x4FD3D400, /* [12] m=-11264 = +2^10 +2^12 -2^14      b= 20435 */
    0x4FD3D400, /* [13] m=-11264 = +2^10 +2^12 -2^14      b= 20435 */
};

#endif /* TRIG_PWL_CSD_H */
