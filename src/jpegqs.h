/*
 * Copyright (C) 2016-2020 Kurdyukov Ilya
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

#ifndef JPEGQUANTSMOOTH_H
#define JPEGQUANTSMOOTH_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include "idct.h"
#include "jpeglib.h"

#define JPEGQS_VERSION "0.20200509"

typedef struct
{
    int optimize;
    int verbose_level;
    int smooth_info;
    float border;
    float gain;
    float scale;
    int niter;
} JQS_PARAMS;

#define WITH_LOG
#define logfmt(...) fprintf(stderr, __VA_ARGS__)

#ifdef NO_MATH_LIB
#  define roundf(x) (float)(int)((x) < 0 ? (x) - 0.5f : (x) + 0.5f)
#  define fabsf(x) (float)((x) < 0 ? -(x) : (x))
#endif

#ifdef WITH_LOG
#  ifdef _WIN32
#    define WIN32_LEAN_AND_MEAN
// conflict with libjpeg typedef
#    define INT32 INT32_WIN
#    include <windows.h>
static inline int64_t get_time_usec()
{
    LARGE_INTEGER freq, perf;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&perf);
    double d = (double)perf.QuadPart * 1000000.0 / (double)freq.QuadPart;
    return d;
}
#  else
#    include <time.h>
#    include <sys/time.h>
static inline int64_t get_time_usec()
{
    struct timeval time;
    gettimeofday(&time, NULL);
    return time.tv_sec * (int64_t)1000000 + time.tv_usec;
}
#  endif
#endif

#define ALIGN(n) __attribute__((aligned(n)))

#include "idct.h"

static float ALIGN(32) quantsmooth_tables[DCTSIZE2][3*DCTSIZE2 + DCTSIZE];
void quantsmooth_init(JQS_PARAMS jqsparams);

// When compiling with libjpeg-turbo and static linking, you can use
// optimized idct_islow from library. Which will be faster if you don't have
// processor with AVX2 support, but SSE2 is supported.

#if defined(USE_JSIMD) && defined(LIBJPEG_TURBO_VERSION)
EXTERN(void) jsimd_idct_islow_sse2(void *dct_table, JCOEFPTR coef_block, JSAMPARRAY output_buf, JDIMENSION output_col);
#  define idct_islow(coef, buf, st) jsimd_idct_islow_sse2(dct_table1, coef, output_buf, output_col)
#  define M1 1,1,1,1, 1,1,1,1
static short dct_table1[DCTSIZE2] = { M1, M1, M1, M1, M1, M1, M1, M1 };
#  undef M1
#endif

static const char zigzag_refresh[DCTSIZE2] =
{
    1, 0, 1, 0, 1, 0, 1, 0,
    1, 0, 0, 0, 0, 0, 0, 1,
    0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 1,
    0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 1,
    0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 1, 0, 1, 0, 1, 1,
};

void quantsmooth_block(JCOEFPTR coef, UINT16 *quantval, JSAMPROW image, int stride, JQS_PARAMS jqsparams);
void quantsmooth_transform(j_decompress_ptr srcinfo, jvirt_barray_ptr *src_coef_arrays, JQS_PARAMS jqsparams);
void do_quantsmooth(j_decompress_ptr srcinfo, jvirt_barray_ptr *coef_arrays, JQS_PARAMS jqsparams);

#endif //JPEGQUANTSMOOTH_H//
