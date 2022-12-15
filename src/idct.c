/*
 * idct_islow SSE2/AVX2/NEON intrinsic optimization:
 * Copyright (C) 2016-2020 Kurdyukov Ilya
 *
 * contains modified parts of libjpeg:
 * Copyright (C) 1991-1998, Thomas G. Lane
 *
 * This file is part of jpeg quantsmooth
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "idct.h"

static JSAMPLE range_limit_static[11 * CENTERJSAMPLE];

void range_limit_init()
{
    int i, c = CENTERJSAMPLE, m = c * 2;
    JSAMPLE *t = range_limit_static;

    for (i = 0; i < m; i++) t[i] = 0;
    t += m;
    for (i = 0; i < m; i++) t[i] = i;
    for (; i < 2 * m + c; i++) t[i] = m - 1;
    for (; i < 4 * m; i++) t[i] = 0;
    for (i = 0; i < c; i++) t[4 * m + i] = i;
}

void idct_islow(JCOEFPTR coef_block, JSAMPROW outptr, JDIMENSION stride)
{

#define M3 \
    z2 = M1(2); z3 = M1(6); \
    z1 = MUL(ADD(z2, z3), SET1(FIX_0_541196100)); \
    tmp2 = SUB(z1, MUL(z3, SET1(FIX_1_847759065))); \
    tmp3 = ADD(z1, MUL(z2, SET1(FIX_0_765366865))); \
    z2 = M1(0); z3 = M1(4); \
    tmp0 = SHL(ADD(z2, z3), CONST_BITS); \
    tmp1 = SHL(SUB(z2, z3), CONST_BITS); \
    tmp10 = ADD(tmp0, tmp3); \
    tmp13 = SUB(tmp0, tmp3); \
    tmp11 = ADD(tmp1, tmp2); \
    tmp12 = SUB(tmp1, tmp2); \
\
    tmp0 = M1(7); tmp1 = M1(5); tmp2 = M1(3); tmp3 = M1(1); \
    z1 = ADD(tmp0, tmp3); \
    z2 = ADD(tmp1, tmp2); \
    z3 = ADD(tmp0, tmp2); \
    z4 = ADD(tmp1, tmp3); \
    z5 = MUL(ADD(z3, z4), SET1(FIX_1_175875602)); /* sqrt(2) * c3 */ \
\
    tmp0 = MUL(tmp0, SET1(FIX_0_298631336)); /* sqrt(2) * (-c1+c3+c5-c7) */ \
    tmp1 = MUL(tmp1, SET1(FIX_2_053119869)); /* sqrt(2) * ( c1+c3-c5+c7) */ \
    tmp2 = MUL(tmp2, SET1(FIX_3_072711026)); /* sqrt(2) * ( c1+c3+c5-c7) */ \
    tmp3 = MUL(tmp3, SET1(FIX_1_501321110)); /* sqrt(2) * ( c1+c3-c5-c7) */ \
    z1 = MUL(z1, SET1(FIX_0_899976223)); /* sqrt(2) * (c7-c3) */ \
    z2 = MUL(z2, SET1(FIX_2_562915447)); /* sqrt(2) * (-c1-c3) */ \
    z3 = MUL(z3, SET1(FIX_1_961570560)); /* sqrt(2) * (-c3-c5) */ \
    z4 = MUL(z4, SET1(FIX_0_390180644)); /* sqrt(2) * (c5-c3) */ \
    z3 = SUB(z5, z3); \
    z4 = SUB(z5, z4); \
    tmp0 = ADD(tmp0, SUB(z3, z1)); \
    tmp1 = ADD(tmp1, SUB(z4, z2)); \
    tmp2 = ADD(tmp2, SUB(z3, z2)); \
    tmp3 = ADD(tmp3, SUB(z4, z1)); \
\
    M2(0, ADD(tmp10, tmp3)) \
    M2(7, SUB(tmp10, tmp3)) \
    M2(1, ADD(tmp11, tmp2)) \
    M2(6, SUB(tmp11, tmp2)) \
    M2(2, ADD(tmp12, tmp1)) \
    M2(5, SUB(tmp12, tmp1)) \
    M2(3, ADD(tmp13, tmp0)) \
    M2(4, SUB(tmp13, tmp0))

    int32_t tmp0, tmp1, tmp2, tmp3;
    int32_t tmp10, tmp11, tmp12, tmp13;
    int32_t z1, z2, z3, z4, z5;
    JCOEFPTR inptr;
    JSAMPLE *range_limit = range_limit_static + (MAXJSAMPLE+1) + CENTERJSAMPLE;
    int ctr;
    int32_t *wsptr, workspace[DCTSIZE2];    /* buffers data between passes */

#define ADD(a, b) ((a) + (b))
#define SUB(a, b) ((a) - (b))
#define MUL(a, b) ((a) * (b))
#define SET1(a) (a)
#define SHL(a, b) ((a) << (b))

    /* Pass 1: process columns from input, store into work array. */
    /* Note results are scaled up by sqrt(8) compared to a true IDCT; */
    /* furthermore, we scale the results by 2**PASS1_BITS. */

#define M1(i) inptr[DCTSIZE*i]
#define M2(i, tmp) wsptr[DCTSIZE*i] = DESCALE(tmp, CONST_BITS-PASS1_BITS);
    inptr = coef_block;
    wsptr = workspace;
    for (ctr = DCTSIZE; ctr > 0; ctr--, inptr++, wsptr++)
    {
        /* Due to quantization, we will usually find that many of the input
         * coefficients are zero, especially the AC terms.  We can exploit this
         * by short-circuiting the IDCT calculation for any column in which all
         * the AC terms are zero.  In that case each output is equal to the
         * DC coefficient (with scale factor as needed).
         * With typical images and quantization tables, half or more of the
         * column DCT calculations can be simplified this way.
         */

        if (!(inptr[DCTSIZE*1] | inptr[DCTSIZE*2] | inptr[DCTSIZE*3] | inptr[DCTSIZE*4] |
                inptr[DCTSIZE*5] | inptr[DCTSIZE*6] | inptr[DCTSIZE*7]))
        {
            /* AC terms all zero */
            int dcval = SHL(M1(0), PASS1_BITS);
            wsptr[DCTSIZE*0] = dcval;
            wsptr[DCTSIZE*1] = dcval;
            wsptr[DCTSIZE*2] = dcval;
            wsptr[DCTSIZE*3] = dcval;
            wsptr[DCTSIZE*4] = dcval;
            wsptr[DCTSIZE*5] = dcval;
            wsptr[DCTSIZE*6] = dcval;
            wsptr[DCTSIZE*7] = dcval;
            continue;
        }

        M3
    }
#undef M1
#undef M2

    /* Pass 2: process rows from work array, store into output array. */
    /* Note that we must descale the results by a factor of 8 == 2**3, */
    /* and also undo the PASS1_BITS scaling. */

#define M1(i) wsptr[i]
#define M2(i, tmp) outptr[i] = range_limit[DESCALE(tmp, CONST_BITS+PASS1_BITS+3) & RANGE_MASK];
    wsptr = workspace;
    for (ctr = 0; ctr < DCTSIZE; ctr++, wsptr += DCTSIZE, outptr += stride)
    {
        /* Rows of zeroes can be exploited in the same way as we did with columns.
         * However, the column calculation has created many nonzero AC terms, so
         * the simplification applies less often (typically 5% to 10% of the time).
         * On machines with very fast multiplication, it's possible that the
         * test takes more time than it's worth.  In that case this section
         * may be commented out.
         */

#ifndef NO_ZERO_ROW_TEST
        if (!(wsptr[1] | wsptr[2] | wsptr[3] | wsptr[4] | wsptr[5] | wsptr[6] | wsptr[7]))
        {
            /* AC terms all zero */
            JSAMPLE dcval = range_limit[DESCALE(wsptr[0], PASS1_BITS+3) & RANGE_MASK];

            outptr[0] = dcval;
            outptr[1] = dcval;
            outptr[2] = dcval;
            outptr[3] = dcval;
            outptr[4] = dcval;
            outptr[5] = dcval;
            outptr[6] = dcval;
            outptr[7] = dcval;
            continue;
        }
#endif

        M3
    }
#undef M1
#undef M2

#undef M3
#undef ADD
#undef SUB
#undef MUL
#undef SET1
#undef SHL
}

void idct_fslow(JCOEFPTR inptr, float *outptr)
{
    float tmp0, tmp1, tmp2, tmp3;
    float tmp10, tmp11, tmp12, tmp13;
    float z1, z2, z3, z4, z5;
    float *wsptr, workspace[DCTSIZE2];
    int ctr;

#define M3(inc1, inc2) \
    wsptr = workspace; \
    for (ctr = 0; ctr < DCTSIZE; ctr++, inc1, inc2) { \
        z2 = M1(2); z3 = M1(6); \
        z1 = (z2 + z3) * 0.541196100f; \
        tmp2 = z1 - z3 * 1.847759065f; \
        tmp3 = z1 + z2 * 0.765366865f; \
        z2 = M1(0); z3 = M1(4); \
        tmp0 = z2 + z3; \
        tmp1 = z2 - z3; \
        tmp10 = tmp0 + tmp3; \
        tmp13 = tmp0 - tmp3; \
        tmp11 = tmp1 + tmp2; \
        tmp12 = tmp1 - tmp2; \
        tmp0 = M1(7); tmp1 = M1(5); tmp2 = M1(3); tmp3 = M1(1); \
        z1 = tmp0 + tmp3; \
        z2 = tmp1 + tmp2; \
        z3 = tmp0 + tmp2; \
        z4 = tmp1 + tmp3; \
        z5 = (z3 + z4) * 1.175875602f; /* sqrt(2) * c3 */ \
        tmp0 *= 0.298631336f; /* sqrt(2) * (-c1+c3+c5-c7) */ \
        tmp1 *= 2.053119869f; /* sqrt(2) * ( c1+c3-c5+c7) */ \
        tmp2 *= 3.072711026f; /* sqrt(2) * ( c1+c3+c5-c7) */ \
        tmp3 *= 1.501321110f; /* sqrt(2) * ( c1+c3-c5-c7) */ \
        z1 *= -0.899976223f; /* sqrt(2) * (c7-c3) */ \
        z2 *= -2.562915447f; /* sqrt(2) * (-c1-c3) */ \
        z3 *= -1.961570560f; /* sqrt(2) * (-c3-c5) */ \
        z4 *= -0.390180644f; /* sqrt(2) * (c5-c3) */ \
        z3 += z5; z4 += z5; \
        tmp0 += z1 + z3; \
        tmp1 += z2 + z4; \
        tmp2 += z2 + z3; \
        tmp3 += z1 + z4; \
        M2(0, tmp10 + tmp3) \
        M2(7, tmp10 - tmp3) \
        M2(1, tmp11 + tmp2) \
        M2(6, tmp11 - tmp2) \
        M2(2, tmp12 + tmp1) \
        M2(5, tmp12 - tmp1) \
        M2(3, tmp13 + tmp0) \
        M2(4, tmp13 - tmp0) \
    }
#define M1(i) inptr[DCTSIZE*i]
#define M2(i, tmp) wsptr[DCTSIZE*i] = tmp;
    M3(inptr++, wsptr++)
#undef M1
#undef M2
#define M1(i) wsptr[i]
#define M2(i, tmp) outptr[i] = (tmp) * 0.125f;
    M3(wsptr += DCTSIZE, outptr += DCTSIZE)
#undef M1
#undef M2
#undef M3
}
