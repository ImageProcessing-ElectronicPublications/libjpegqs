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

#ifndef IDCT_H
#define IDCT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include "jpeglib.h"

static const char jpeg_natural_order[DCTSIZE2] =
{
     0,  1,  8, 16,  9,  2,  3, 10,
    17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63
};

#define CONST_BITS  13
#define PASS1_BITS  2

#if CONST_BITS == 13
#  define FIX_0_298631336  2446        /* FIX(0.298631336) */
#  define FIX_0_390180644  3196        /* FIX(0.390180644) */
#  define FIX_0_541196100  4433        /* FIX(0.541196100) */
#  define FIX_0_765366865  6270        /* FIX(0.765366865) */
#  define FIX_0_899976223  7373        /* FIX(0.899976223) */
#  define FIX_1_175875602  9633        /* FIX(1.175875602) */
#  define FIX_1_501321110  12299       /* FIX(1.501321110) */
#  define FIX_1_847759065  15137       /* FIX(1.847759065) */
#  define FIX_1_961570560  16069       /* FIX(1.961570560) */
#  define FIX_2_053119869  16819       /* FIX(2.053119869) */
#  define FIX_2_562915447  20995       /* FIX(2.562915447) */
#  define FIX_3_072711026  25172       /* FIX(3.072711026) */
#else
#  define FIX_0_298631336  FIX(0.298631336)
#  define FIX_0_390180644  FIX(0.390180644)
#  define FIX_0_541196100  FIX(0.541196100)
#  define FIX_0_765366865  FIX(0.765366865)
#  define FIX_0_899976223  FIX(0.899976223)
#  define FIX_1_175875602  FIX(1.175875602)
#  define FIX_1_501321110  FIX(1.501321110)
#  define FIX_1_847759065  FIX(1.847759065)
#  define FIX_1_961570560  FIX(1.961570560)
#  define FIX_2_053119869  FIX(2.053119869)
#  define FIX_2_562915447  FIX(2.562915447)
#  define FIX_3_072711026  FIX(3.072711026)
#endif

#define DESCALE(x,n)  (((x) + (1 << ((n)-1))) >> (n))
#define RANGE_MASK  (MAXJSAMPLE * 4 + 3) /* 2 bits wider than legal samples */

#define MULTIPLY(var,const) ((var) * (const))

void range_limit_init();
void idct_islow(JCOEFPTR coef_block, JSAMPROW outptr, JDIMENSION stride);
void idct_fslow(JCOEFPTR inptr, float *outptr);

#endif //IDCT_H//
