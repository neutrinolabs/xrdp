/*
Copyright 2014 Jay Sorg

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

SIMD function asign

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* this should be before all X11 .h files */
#include <xorg-server.h>
#include <xorgVersion.h>

/* all driver need this */
#include <xf86.h>
#include <xf86_OSproc.h>

#include "rdp.h"
#include "rdpXv.h"

/* use simd, run time */
int g_simd_use_accel = 1;

/* use simd, compile time, if zero, g_simd_use_accel does not matter */
#if !defined(SIMD_USE_ACCEL)
#define SIMD_USE_ACCEL 0
#endif

#if SIMD_USE_ACCEL
#if defined(__x86_64__) || defined(__AMD64__) || defined (_M_AMD64)
#include "amd64/funcs_amd64.h"
#elif defined(__x86__) || defined(_M_IX86) || defined(__i386__)
#include "x86/funcs_x86.h"
#endif
#endif

#define LOG_LEVEL 1
#define LLOGLN(_level, _args) \
    do { if (_level < LOG_LEVEL) { ErrorF _args ; ErrorF("\n"); } } while (0)

/*****************************************************************************/
Bool
rdpSimdInit(ScreenPtr pScreen, ScrnInfoPtr pScrn)
{
    rdpPtr dev;

    dev = XRDPPTR(pScrn);
    /* assign functions */
    LLOGLN(0, ("rdpSimdInit: assigning yuv functions"));
#if SIMD_USE_ACCEL
    if (g_simd_use_accel)
    {
#if defined(__x86_64__) || defined(__AMD64__) || defined (_M_AMD64)
        int ax, bx, cx, dx;
        cpuid_amd64(1, 0, &ax, &bx, &cx, &dx);
        LLOGLN(0, ("rdpSimdInit: cpuid ax 1 cx 0 return ax 0x%8.8x bx "
               "0x%8.8x cx 0x%8.8x dx 0x%8.8x", ax, bx, cx, dx));
        if (dx & (1 << 26)) /* SSE 2 */
        {
            dev->yv12_to_rgb32 = yv12_to_rgb32_amd64_sse2;
            dev->i420_to_rgb32 = i420_to_rgb32_amd64_sse2;
            dev->yuy2_to_rgb32 = yuy2_to_rgb32_amd64_sse2;
            dev->uyvy_to_rgb32 = uyvy_to_rgb32_amd64_sse2;
            LLOGLN(0, ("rdpSimdInit: sse2 amd64 yuv functions assigned"));
        }
        else
        {
            dev->yv12_to_rgb32 = YV12_to_RGB32;
            dev->i420_to_rgb32 = I420_to_RGB32;
            dev->yuy2_to_rgb32 = YUY2_to_RGB32;
            dev->uyvy_to_rgb32 = UYVY_to_RGB32;
            LLOGLN(0, ("rdpSimdInit: warning, c yuv functions assigned"));
        }
#elif defined(__x86__) || defined(_M_IX86) || defined(__i386__)
        int ax, bx, cx, dx;
        cpuid_x86(1, 0, &ax, &bx, &cx, &dx);
        LLOGLN(0, ("rdpSimdInit: cpuid ax 1 cx 0 return ax 0x%8.8x bx "
               "0x%8.8x cx 0x%8.8x dx 0x%8.8x", ax, bx, cx, dx));
        if (dx & (1 << 26)) /* SSE 2 */
        {
            dev->yv12_to_rgb32 = yv12_to_rgb32_x86_sse2;
            dev->i420_to_rgb32 = i420_to_rgb32_x86_sse2;
            dev->yuy2_to_rgb32 = yuy2_to_rgb32_x86_sse2;
            dev->uyvy_to_rgb32 = uyvy_to_rgb32_x86_sse2;
            LLOGLN(0, ("rdpSimdInit: sse2 x86 yuv functions assigned"));
        }
        else
        {
            dev->yv12_to_rgb32 = YV12_to_RGB32;
            dev->i420_to_rgb32 = I420_to_RGB32;
            dev->yuy2_to_rgb32 = YUY2_to_RGB32;
            dev->uyvy_to_rgb32 = UYVY_to_RGB32;
            LLOGLN(0, ("rdpSimdInit: warning, c yuv functions assigned"));
        }
#else
        dev->yv12_to_rgb32 = YV12_to_RGB32;
        dev->i420_to_rgb32 = I420_to_RGB32;
        dev->yuy2_to_rgb32 = YUY2_to_RGB32;
        dev->uyvy_to_rgb32 = UYVY_to_RGB32;
        LLOGLN(0, ("rdpSimdInit: warning, c yuv functions assigned"));
#endif
    }
    else
    {
        dev->yv12_to_rgb32 = YV12_to_RGB32;
        dev->i420_to_rgb32 = I420_to_RGB32;
        dev->yuy2_to_rgb32 = YUY2_to_RGB32;
        dev->uyvy_to_rgb32 = UYVY_to_RGB32;
        LLOGLN(0, ("rdpSimdInit: warning, c yuv functions assigned"));
    }
#else
        dev->yv12_to_rgb32 = YV12_to_RGB32;
        dev->i420_to_rgb32 = I420_to_RGB32;
        dev->yuy2_to_rgb32 = YUY2_to_RGB32;
        dev->uyvy_to_rgb32 = UYVY_to_RGB32;
        LLOGLN(0, ("rdpSimdInit: warning, c yuv functions assigned"));
#endif
    return 1;
}

