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

#include "quantsmooth.h"

void quantsmooth_init(JQS_PARAMS jqsparams)
{
    int i;
    JCOEF coef[DCTSIZE2];

    range_limit_init();

    for (i = 0; i < DCTSIZE2; i++)
    {
        float *tab = quantsmooth_tables[i];
        float bcoef = jqsparams.border;
        int x, y, p0, p1;
        memset(coef, 0, sizeof(coef));
        coef[i] = 1;
        idct_fslow(coef, tab);

#define M1(xx, yy, j) \
    p0 = y*DCTSIZE+x; p1 = (yy)*DCTSIZE+(xx); \
    tab[j*DCTSIZE2+p0] = tab[p0] - tab[p1];
        for (y = 0; y < DCTSIZE; y++)
        {
            for (x = 0; x < DCTSIZE-1; x++)
            {
                M1(x+1,y,1)
            }
            tab[DCTSIZE2+y*DCTSIZE+x] = tab[y*DCTSIZE+x] * bcoef;
        }
        for (y = 0; y < DCTSIZE-1; y++)
            for (x = 0; x < DCTSIZE; x++)
            {
                M1(x,y+1,2)
            }
#undef M1
        for (x = 0; x < DCTSIZE2; x++) tab[x] *= bcoef;
        for (y = 0; y < DCTSIZE; y++) tab[3*DCTSIZE2+y] = tab[y*DCTSIZE];
    }
}

void quantsmooth_block(JCOEFPTR coef, UINT16 *quantval, JSAMPROW image, int stride, JQS_PARAMS jqsparams)
{

    int k, x, y, flag = 1;
    int div, coef1, add;
    int dh, dl, d0, d1;
    int32_t a0, sign;
    JSAMPLE ALIGN(32) buf[DCTSIZE2+DCTSIZE*2];
#ifdef USE_JSIMD
    JSAMPROW output_buf[8] = { buf+8*0, buf+8*1, buf+8*2, buf+8*3, buf+8*4, buf+8*5, buf+8*6, buf+8*7 };
    int output_col = 0;
#endif

    (void)x;
    for (y = 0; y < DCTSIZE; y++)
    {
        buf[DCTSIZE2+y] = image[y*stride-1];
        buf[DCTSIZE2+y+DCTSIZE] = image[y*stride+8];
    }

    for (k = DCTSIZE2-1; k > 0; k--)
    {
        int i = jpeg_natural_order[k];
        float *tab = quantsmooth_tables[i], a2 = 0, a3 = 0;
        int range = (int)((float)quantval[i] * jqsparams.gain + 0.5f);

        if (flag && zigzag_refresh[i])
        {
            idct_islow(coef, buf, DCTSIZE);
            flag = 0;
        }

#define VINIT \
    int i; JSAMPLE *p0, *p1, rborder[8]; \
    float sum[8] = { 0,0,0,0, 0,0,0,0 };

#define VCORE(tab) \
    for (i = 0; i < 4; i++) { \
        float a0, a1, a4, a5, f0; \
        VCORE1(i, a0, a1, tab) VCORE1(i+4, a4, a5, tab) \
        sum[i] += a0 + a4; sum[i+4] += a1 + a5; \
    }

#define VCORE1(i, a0, a1, tab) \
    a0 = p0[i] - p1[i]; a1 = (tab)[i]; \
    f0 = (float)range-fabsf(a0); if (f0 < 0) f0 = 0; f0 *= f0; \
    a0 *= f0; a1 *= f0; a0 *= a1; a1 *= a1;

#define VFIN \
    a2 = (sum[0] + sum[2]) + (sum[1] + sum[3]); \
    a3 = (sum[4] + sum[6]) + (sum[5] + sum[7]);

#define VLDPIX(i, a) p##i = a;
#define VRIGHT(p) p1 = rborder; memcpy(p1, p0 + 1, 7 * sizeof(JSAMPLE)); p1[7] = *(p);
#define VCOPY1 p0 = p1;

        {
            JSAMPLE lborder[8];
            VINIT

            for (y = 0; y < DCTSIZE; y++)
            {
                lborder[y] = buf[y*DCTSIZE];
                VLDPIX(0, buf+y*DCTSIZE)
                VRIGHT(buf+DCTSIZE2+DCTSIZE+y)
                VCORE(tab+64+y*DCTSIZE)
            }

            VLDPIX(0, buf)
            for (y = 0; y < DCTSIZE-1; y++)
            {
                VLDPIX(1, buf+y*DCTSIZE+DCTSIZE)
                VCORE(tab+64*2+y*DCTSIZE)
                VCOPY1
            }

            VLDPIX(0, buf)
            VLDPIX(1, image-stride)
            VCORE(tab)
            VLDPIX(0, buf+7*DCTSIZE)
            VLDPIX(1, image+8*stride)
            VCORE(tab+7*DCTSIZE)
            VLDPIX(0, lborder)
            VLDPIX(1, buf+DCTSIZE2)
            VCORE(tab+3*DCTSIZE2)

            VFIN
        }
#undef VINIT
#undef VCORE
#ifdef VCORE1
#  undef VCORE1
#endif
#undef VFIN
#undef VLDPIX
#undef VRIGHT
#undef VCOPY1

        a2 = a2 / a3;

        {
            div = quantval[i];
            coef1 = coef[i];
            d0 = (div-1) >> 1;
            d1 = div >> 1;
            // int a0 = (coef1 + (coef1 < 0 ? -div : div) / 2) / div * div;
            a0 = coef1;
            sign = a0 >> 31;
            a0 = (a0 ^ sign) - sign;
            a0 = ((a0 + (div >> 1)) / div) * div;
            a0 = (a0 ^ sign) - sign;

            dh = a0 < 0 ? d1 : d0;
            dl = a0 > 0 ? -d1 : -d0;

            add = coef1 - a0;
            add -= roundf(a2 * jqsparams.scale);
            add = (add < dh) ? add : dh;
            add = (add > dl) ? add : dl;
            add += a0;
            flag += add != coef1;
            coef[i] = add;
        }
    }
}

void quantsmooth_transform(j_decompress_ptr srcinfo, jvirt_barray_ptr *src_coef_arrays, JQS_PARAMS jqsparams)
{
    JDIMENSION comp_width, comp_height, blk_y;
    int ci, qtblno, stride, iter;
    jpeg_component_info *compptr;
    JQUANT_TBL *qtbl;
    JSAMPROW image;

    for (ci = 0; ci < srcinfo->num_components; ci++)
    {
        compptr = srcinfo->comp_info + ci;
        stride = (srcinfo->max_h_samp_factor * DCTSIZE) / compptr->h_samp_factor;
        comp_width = (srcinfo->image_width + stride - 1) / stride;
        comp_height = compptr->height_in_blocks;

        qtblno = compptr->quant_tbl_no;
        if (qtblno < 0 || qtblno >= NUM_QUANT_TBLS || !srcinfo->quant_tbl_ptrs[qtblno]) continue;
        qtbl = srcinfo->quant_tbl_ptrs[qtblno];

        // skip already processed
        {
            int i, a = 0;
            for (i = 0; i < DCTSIZE2; i++) a |= qtbl->quantval[i];
            if (a <= 1) continue;
        }

        // keeping block pointers aligned
        stride = comp_width * DCTSIZE + 8;
        image = (JSAMPROW)malloc(((comp_height * DCTSIZE + 2) * stride + 8) * sizeof(JSAMPLE));
        if (!image) continue;
        image += 7;

#ifdef WITH_LOG
        if (jqsparams.smooth_info & 4) logfmt("component[%i] : size %ix%i\n", ci, comp_width, comp_height);
#endif
#define IMAGEPTR &image[(blk_y * DCTSIZE + 1) * stride + blk_x * DCTSIZE + 1]

#ifdef USE_JSIMD
        JSAMPROW output_buf[8] =
        {
            image+stride*0, image+stride*1, image+stride*2, image+stride*3,
            image+stride*4, image+stride*5, image+stride*6, image+stride*7
        };
#endif

        for (iter = 0; iter < jqsparams.niter; iter++)
        {
#ifdef _OPENMP
#  pragma omp parallel for schedule(dynamic)
#endif

            for (blk_y = 0; blk_y < comp_height; blk_y += 1)
            {
                JDIMENSION blk_x;
                JBLOCKARRAY buffer = (*srcinfo->mem->access_virt_barray)
                                     ((j_common_ptr)srcinfo, src_coef_arrays[ci], blk_y, 1, TRUE);

                for (blk_x = 0; blk_x < comp_width; blk_x++)
                {
                    JCOEFPTR coef = buffer[0][blk_x];
                    int i;
                    if (!iter)
                        for (i = 0; i < DCTSIZE2; i++)
                            coef[i] *= qtbl->quantval[i];
#ifdef USE_JSIMD
                    int output_col = IMAGEPTR - image;
#endif
                    idct_islow(coef, IMAGEPTR, stride);
                }
            }

            {
                int y, w = comp_width * DCTSIZE, h = comp_height * DCTSIZE;
                for (y = 1; y < h+1; y++)
                {
                    image[y*stride] = image[y*stride+1];
                    image[y*stride+w+1] = image[y*stride+w];
                }
                memcpy(image, image + stride, stride);
                memcpy(image + (h+1)*stride, image + h*stride, stride);
            }

#ifdef _OPENMP
#  pragma omp parallel for schedule(dynamic)
#endif
            for (blk_y = 0; blk_y < comp_height; blk_y += 1)
            {
                JDIMENSION blk_x;
                JBLOCKARRAY buffer = (*srcinfo->mem->access_virt_barray)
                                     ((j_common_ptr)srcinfo, src_coef_arrays[ci], blk_y, 1, TRUE);

                for (blk_x = 0; blk_x < comp_width; blk_x++)
                {
                    JCOEFPTR coef = buffer[0][blk_x];
                    quantsmooth_block(coef, qtbl->quantval, IMAGEPTR, stride, jqsparams);
                }
            }
        } // iter
#undef IMAGEPTR
        free(image - 7);
    }
}

void do_quantsmooth(j_decompress_ptr srcinfo, jvirt_barray_ptr *coef_arrays, JQS_PARAMS jqsparams)
{
    int ci, qtblno, x, y;
    jpeg_component_info *compptr;
    JQUANT_TBL *qtbl, *c_quant;

#ifdef WITH_LOG
    if (jqsparams.smooth_info & 1)
        for (ci = 0, compptr = srcinfo->comp_info; ci < srcinfo->num_components; ci++, compptr++)
        {
            qtblno = compptr->quant_tbl_no;
            logfmt("component[%i] : table %i, samp %ix%i\n", ci, qtblno, compptr->h_samp_factor, 1);
            /* Make sure specified quantization table is present */
            if (qtblno < 0 || qtblno >= NUM_QUANT_TBLS || srcinfo->quant_tbl_ptrs[qtblno] == NULL) continue;
        }

    if (jqsparams.smooth_info & 2)
        for (qtblno = 0; qtblno < NUM_QUANT_TBLS; qtblno++)
        {
            qtbl = srcinfo->quant_tbl_ptrs[qtblno];
            if (!qtbl) continue;
            logfmt("quant[%i]:\n", qtblno);

            for (y = 0; y < DCTSIZE; y++)
            {
                for (x = 0; x < DCTSIZE; x++)
                {
                    logfmt("%04x ", qtbl->quantval[y * DCTSIZE + x]);
                }
                logfmt("\n");
            }
        }
#endif

    {
#ifdef WITH_LOG
        int64_t time = 0;
        if (jqsparams.smooth_info & 8) time = get_time_usec();
#endif
        quantsmooth_init(jqsparams);
        quantsmooth_transform(srcinfo, coef_arrays, jqsparams);
#ifdef WITH_LOG
        if (jqsparams.smooth_info & 8)
        {
            time = get_time_usec() - time;
            logfmt("quantsmooth = %.3fms\n", time * 0.001);
        }
#endif
    }

    for (qtblno = 0; qtblno < NUM_QUANT_TBLS; qtblno++)
    {
        qtbl = srcinfo->quant_tbl_ptrs[qtblno];
        if (!qtbl) continue;
        for (x = 0; x < DCTSIZE2; x++) qtbl->quantval[x] = 1;
    }

    for (ci = 0, compptr = srcinfo->comp_info; ci < srcinfo->num_components; ci++, compptr++)
    {
        qtblno = compptr->quant_tbl_no;
        if (qtblno < 0 || qtblno >= NUM_QUANT_TBLS || !srcinfo->quant_tbl_ptrs[qtblno]) continue;
        qtbl = srcinfo->quant_tbl_ptrs[qtblno];
        c_quant = compptr->quant_table;
        if (c_quant) memcpy(c_quant, qtbl, sizeof(qtbl[0]));
    }
}
