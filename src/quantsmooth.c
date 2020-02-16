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

#define DEFAULT_BORDER 2.0f
#define DEFAULT_GAIN 2.0f
#define DEFAULT_SCALE 1.0f
#define DEFAULT_NITER 3u

#define STRINGIFY(s) #s
#define TOSTRING(s) STRINGIFY(s)

#include "quantsmooth.h"

int main(int argc, char **argv)
{
    struct jpeg_decompress_struct srcinfo;
    struct jpeg_compress_struct dstinfo;
    struct jpeg_error_mgr jsrcerr, jdsterr;
    jvirt_barray_ptr *src_coef_arrays;
    JQS_PARAMS jqsparams = { 0, 0, 0xf, DEFAULT_BORDER, DEFAULT_GAIN, DEFAULT_SCALE, DEFAULT_NITER};

    FILE *input_file = stdin;
    FILE *output_file = stdout;
#ifndef APPNAME
    const char *progname = argv[0], *fn;
#else
    const char *progname = TOSTRING(APPNAME), *fn;
#endif

    while (argc > 1)
    {
        if (!strcmp(argv[1], "--optimize"))
        {
            jqsparams.optimize = 1;
            argv++;
            argc--;
        }
        else if (argc > 2 && !strcmp(argv[1], "--verbose"))
        {
            jqsparams.verbose_level = atoi(argv[2]);
            argv += 2;
            argc -= 2;
        }
        else if (argc > 2 && !strcmp(argv[1], "--info"))
        {
            jqsparams.smooth_info = atoi(argv[2]);
            argv += 2;
            argc -= 2;
        }
        else if (argc > 2 && !strcmp(argv[1], "--border"))
        {
            jqsparams.border = atof(argv[2]);
            jqsparams.border = (jqsparams.border < 1.0) ? DEFAULT_BORDER : jqsparams.border;
            argv += 2;
            argc -= 2;
        }
        else if (argc > 2 && !strcmp(argv[1], "--gain"))
        {
            jqsparams.gain = atof(argv[2]);
            jqsparams.gain = (jqsparams.gain < 0.0) ? DEFAULT_GAIN : jqsparams.gain;
            argv += 2;
            argc -= 2;
        }
        else if (argc > 2 && !strcmp(argv[1], "--scale"))
        {
            jqsparams.scale = atof(argv[2]);
            argv += 2;
            argc -= 2;
        }
        else if (argc > 2 && !strcmp(argv[1], "--niter"))
        {
            jqsparams.niter = atoi(argv[2]);
            jqsparams.niter = (jqsparams.niter < 1) ? DEFAULT_NITER : jqsparams.niter;
            argv += 2;
            argc -= 2;
        }
        else if (!strcmp(argv[1], "--"))
        {
            argv++;
            argc--;
            break;
        }
        else break;
    }

    if (argc != 3)
    {
        logfmt(
            "Usage:\n"
            "  %s [options] input.jpg output.jpg\n"
            "Options:\n"
            "  --optimize        Optimize Huffman table (smaller file, but slow compression)\n"
            "  --verbose level   Print libjpeg debug output\n"
            "  --info flags      Print quantsmooth debug output\n"
            "  --border N.N      Size border (default = %f)\n"
            "  --gain N.N        Gain coefficients (default = %f)\n"
            "  --scale N.N       Scale delta coefficients (default = %f)\n"
            "  --niter N         Number iteration (default = %d)\n"
            , progname, DEFAULT_BORDER, DEFAULT_GAIN, DEFAULT_SCALE, DEFAULT_NITER);
        return 1;
    }

    if (jqsparams.verbose_level)
    {
#ifdef LIBJPEG_TURBO_VERSION
        logfmt("using libjpeg-turbo version %s\n", TOSTRING(LIBJPEG_TURBO_VERSION));
#else
        logfmt("using libjpeg version %d\n", JPEG_LIB_VERSION);
#endif
        jqsparams.verbose_level--;
    }

    srcinfo.err = jpeg_std_error(&jsrcerr);
    jpeg_create_decompress(&srcinfo);
    dstinfo.err = jpeg_std_error(&jdsterr);
    jpeg_create_compress(&dstinfo);

    jsrcerr.trace_level = jdsterr.trace_level = jqsparams.verbose_level;
    srcinfo.mem->max_memory_to_use = dstinfo.mem->max_memory_to_use;

    fn = argv[1];
    if (strcmp(fn, "-"))
    {
        if ((input_file = fopen(fn, "rb")) == NULL)
        {
            logfmt("%s: can't open input file \"%s\"\n", progname, fn);
            return 1;
        }
    }
    jpeg_stdio_src(&srcinfo, input_file);

    (void) jpeg_read_header(&srcinfo, TRUE);
    src_coef_arrays = jpeg_read_coefficients(&srcinfo);
    logfmt("Options: border = %f, gain = %f, scale = %f, niter = %d\n", jqsparams.border, jqsparams.gain, jqsparams.scale, jqsparams.niter);
    do_quantsmooth(&srcinfo, src_coef_arrays, jqsparams);

    jpeg_copy_critical_parameters(&srcinfo, &dstinfo);
    if (jqsparams.optimize) dstinfo.optimize_coding = TRUE;

    // If output opened after reading coefs, then we can write result to input file
    fn = argv[2];
    if (strcmp(fn, "-"))
    {
        if ((output_file = fopen(fn, "wb")) == NULL)
        {
            logfmt("%s: can't open output file \"%s\"\n", progname, fn);
            return 1;
        }
    }
    jpeg_stdio_dest(&dstinfo, output_file);

    /* Start compressor (note no image data is actually written here) */
    jpeg_write_coefficients(&dstinfo, src_coef_arrays);

    /* Finish compression and release memory */
    jpeg_finish_compress(&dstinfo);
    jpeg_destroy_compress(&dstinfo);
    (void) jpeg_finish_decompress(&srcinfo);
    jpeg_destroy_decompress(&srcinfo);

    /* Close files, if we opened them */

    return jsrcerr.num_warnings + jdsterr.num_warnings ? 2 : 0;
}
