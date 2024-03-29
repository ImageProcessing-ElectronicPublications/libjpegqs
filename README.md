![GitHub release (latest by date)](https://img.shields.io/github/v/release/ImageProcessing-ElectronicPublications/libjpegqs)
![GitHub Release Date](https://img.shields.io/github/release-date/ImageProcessing-ElectronicPublications/libjpegqs)
![GitHub repo size](https://img.shields.io/github/repo-size/ImageProcessing-ElectronicPublications/libjpegqs)
![GitHub all releases](https://img.shields.io/github/downloads/ImageProcessing-ElectronicPublications/libjpegqs/total)
![GitHub](https://img.shields.io/github/license/ImageProcessing-ElectronicPublications/libjpegqs)

# Library JPEG Quant Smooth

This program tries to recreate lost precision of DCT coefficients based on quantization table from jpeg image.
Output saved as jpeg image with quantization set to 1 (like jpeg saved with 100% quality). You can save smoothed image with original quantization tables resulting in same DCT coefficients as in original image.

You may not notice jpeg artifacts on the screen without zooming in, but you may notice them after printing. Also, when editing compressed images, artifacts can accumulate, but if you use this program before editing - the result will be better.

## WebAssembly

Web version available [here](https://ilyakurdyukov.github.io/jpeg-quantsmooth/).
Runs in your browser, none of your data is send outside.
But without multithreading and SIMD optimizations it works noticeably slower.

## Usage

```sh
jpegqs [options] input.jpg output.jpg
```

## Options
`--optimize`
Smaller output file.

`--verbose n`
Print debug info form libjpeg.

`--info n`
Print debug info from quantsmooth (on by default, set to 0 to disable).

`--border N.N`
Size border (default = 2.0)

`--gain N.N`
Gain coefficients (default = 2.0)

`--scale N.N`
Scale delta coefficients (default = 1.0)

`--niter N`
Number iteration (default = 3)

## Examples
Note: Images 3x zoomed.
<p align="center"><b>
Original images:<br/>
<img src="images/text_orig.png"> <img src="images/lena_orig.png"><br/>
JPEG with quality increasing from 8% to 98%:<br/>
<img src="images/text_jpg.png"> <img src="images/lena_jpg.png"><br/>
After processing:<br/>
<img src="images/text_jqs.png"> <img src="images/lena_jqs.png"><br/>
</b></p>

## Buliding on Linux

If you have "libjpeg-dev" package installed, just type:
```sh
make
```
Tested with packages from Ubuntu-18.04, and from sources: libjpeg (6b), libjpeg-turbo (1.4.2, 1.5.3, 2.0.4).

If you do not want to use SSE, type:
```sh
make CFLAGS="-fopenmp -O2 -mno-sse2"
```

If you do not want to use OPENMP (MacOS), type:
```sh
make MPFLAGS=
```
Or when using libomp (MacOS): 
```sh
make MPFLAGS="-Xpreprocessor -fopenmp -lomp"
```

### With libjpeg 6b form sources
```sh
wget https://www.ijg.org/files/jpegsrc.v6b.tar.gz
tar -xvzf jpegsrc.v6b.tar.gz
(cd jpeg-6b && ./configure && make all)
make LIBS="-I jpeg-6b jpeg-6b/libjpeg.a -lm"
```

## Building on Windows
Get [MSYS2](https://www.msys2.org/), install needed packages with pacman and build with __release.sh__.
If you not familiar with building unix applications on windows, then you can download program from [releases](https://github.com/ilyakurdyukov/jpeg-quantsmooth/releases).

## Alternatives and comparison

Similar projects, and how I see them after some testing.

[jpeg2png](https://github.com/victorvde/jpeg2png):  
&nbsp;✔️ good documentation and math model  
&nbsp;✔️ has tuning options  
&nbsp;✔️ better at deblocking low quality JPEG images  
&nbsp;❓ little blurry in default mode (compared to <b>quantsmooth</b>), but can be tuned  
&nbsp;➖ 10 to 20 times slower  
&nbsp;➖ less permissive license (GPL-3.0)  

**jpeg2png** can provide roughly same quality (better in not common cases), but significantly slower.

[knusperli](https://github.com/google/knusperli):  
&nbsp;✔️ more permissive license (Apache-2.0)  
&nbsp;➖ you can hardly see any improvements on the image  
&nbsp;➖ no performance optimizations (but roughly same speed as for <b>quantsmooth</b> with optimizations)  
&nbsp;➖ no any command line options  
&nbsp;➖ uncommon build system  

**knusperli** is good for nothing, in my opinion.

## Base project page

https://github.com/ilyakurdyukov/jpeg-quantsmooth

## Project page

https://github.com/ImageProcessing-ElectronicPublications/libjpegqs

2020
