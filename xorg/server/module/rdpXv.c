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

XVideo

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

#include <xf86xv.h>
#include <X11/extensions/Xv.h>
#include <fourcc.h>

#include <fb.h>

#include "rdp.h"
#include "rdpMisc.h"
#include "rdpReg.h"
#include "rdpClientCon.h"

#define LOG_LEVEL 1
#define LLOGLN(_level, _args) \
    do { if (_level < LOG_LEVEL) { ErrorF _args ; ErrorF("\n"); } } while (0)

#define T_NUM_ENCODINGS 1
static XF86VideoEncodingRec g_xrdpVidEncodings[T_NUM_ENCODINGS] =
{ { 0, "XV_IMAGE", 2046, 2046, { 1, 1 } } };

#define T_NUM_FORMATS 1
static XF86VideoFormatRec g_xrdpVidFormats[T_NUM_FORMATS] =
{ { 0, TrueColor } };

/* YV12
   I420
   12 bpp planar
   YUV 4:2:0 8 bit Y plane followed by 8 bit 2x2 subsampled
   U and V planes. */

/* YUY2
   UYVY
   16 bpp packed
   YUV 4:2:2 Y sample at every pixel, U and V sampled at
   every second pixel */

/* XVIMAGE_YV12 FOURCC_YV12 0x32315659 */
/* XVIMAGE_I420 FOURCC_I420 0x30323449 */
/* XVIMAGE_YUY2 FOURCC_YUY2 0x32595559 */
/* XVIMAGE_UYVY FOURCC_UYVY 0x59565955 */

static XF86ImageRec g_xrdpVidImages[] =
{ XVIMAGE_YV12, XVIMAGE_I420, XVIMAGE_YUY2, XVIMAGE_UYVY };

#define T_MAX_PORTS 1

/*****************************************************************************/
static int
xrdpVidPutVideo(ScrnInfoPtr pScrn, short vid_x, short vid_y,
                short drw_x, short drw_y, short vid_w, short vid_h,
                short drw_w, short drw_h, RegionPtr clipBoxes,
                pointer data, DrawablePtr pDraw)
{
    LLOGLN(0, ("xrdpVidPutVideo:"));
    return Success;
}

/*****************************************************************************/
static int
xrdpVidPutStill(ScrnInfoPtr pScrn, short vid_x, short vid_y,
                short drw_x, short drw_y, short vid_w, short vid_h,
                short drw_w, short drw_h, RegionPtr clipBoxes,
                pointer data, DrawablePtr pDraw)
{
    LLOGLN(0, ("xrdpVidPutStill:"));
    return Success;
}

/*****************************************************************************/
static int
xrdpVidGetVideo(ScrnInfoPtr pScrn, short vid_x, short vid_y,
                short drw_x, short drw_y, short vid_w, short vid_h,
                short drw_w, short drw_h, RegionPtr clipBoxes,
                pointer data, DrawablePtr pDraw)
{
    LLOGLN(0, ("xrdpVidGetVideo:"));
    return Success;
}

/*****************************************************************************/
static int
xrdpVidGetStill(ScrnInfoPtr pScrn, short vid_x, short vid_y,
                short drw_x, short drw_y, short vid_w, short vid_h,
                short drw_w, short drw_h, RegionPtr clipBoxes,
                pointer data, DrawablePtr pDraw)
{
    LLOGLN(0, ("FBDevTIVidGetStill:"));
    return Success;
}

/*****************************************************************************/
static void
xrdpVidStopVideo(ScrnInfoPtr pScrn, pointer data, Bool Cleanup)
{
    LLOGLN(0, ("xrdpVidStopVideo:"));
}

/*****************************************************************************/
static int
xrdpVidSetPortAttribute(ScrnInfoPtr pScrn, Atom attribute,
                        INT32 value, pointer data)
{
    LLOGLN(0, ("xrdpVidSetPortAttribute:"));
    return Success;
}

/*****************************************************************************/
static int
xrdpVidGetPortAttribute(ScrnInfoPtr pScrn, Atom attribute,
                        INT32 *value, pointer data)
{
    LLOGLN(0, ("xrdpVidGetPortAttribute:"));
    return Success;
}

/*****************************************************************************/
static void
xrdpVidQueryBestSize(ScrnInfoPtr pScrn, Bool motion,
                     short vid_w, short vid_h, short drw_w, short drw_h,
                     unsigned int *p_w, unsigned int *p_h, pointer data)
{
    LLOGLN(0, ("xrdpVidQueryBestSize:"));
}

/*****************************************************************************/
int
YV12_to_RGB32(unsigned char *yuvs, int width, int height, int *rgbs)
{
    int size_total;
    int y;
    int u;
    int v;
    int c;
    int d;
    int e;
    int r;
    int g;
    int b;
    int t;
    int i;
    int j;

    size_total = width * height;
    for (j = 0; j < height; j++)
    {
        for (i = 0; i < width; i++)
        {
            y = yuvs[j * width + i];
            u = yuvs[(j / 2) * (width / 2) + (i / 2) + size_total];
            v = yuvs[(j / 2) * (width / 2) + (i / 2) + size_total + (size_total / 4)];
            c = y - 16;
            d = u - 128;
            e = v - 128;
            t = (298 * c + 409 * e + 128) >> 8;
            b = RDPCLAMP(t, 0, 255);
            t = (298 * c - 100 * d - 208 * e + 128) >> 8;
            g = RDPCLAMP(t, 0, 255);
            t = (298 * c + 516 * d + 128) >> 8;
            r = RDPCLAMP(t, 0, 255);
            rgbs[j * width + i] = (r << 16) | (g << 8) | b;
        }
    }
    return 0;
}

/*****************************************************************************/
int
I420_to_RGB32(unsigned char *yuvs, int width, int height, int *rgbs)
{
    int size_total;
    int y;
    int u;
    int v;
    int c;
    int d;
    int e;
    int r;
    int g;
    int b;
    int t;
    int i;
    int j;

    size_total = width * height;
    for (j = 0; j < height; j++)
    {
        for (i = 0; i < width; i++)
        {
            y = yuvs[j * width + i];
            v = yuvs[(j / 2) * (width / 2) + (i / 2) + size_total];
            u = yuvs[(j / 2) * (width / 2) + (i / 2) + size_total + (size_total / 4)];
            c = y - 16;
            d = u - 128;
            e = v - 128;
            t = (298 * c + 409 * e + 128) >> 8;
            b = RDPCLAMP(t, 0, 255);
            t = (298 * c - 100 * d - 208 * e + 128) >> 8;
            g = RDPCLAMP(t, 0, 255);
            t = (298 * c + 516 * d + 128) >> 8;
            r = RDPCLAMP(t, 0, 255);
            rgbs[j * width + i] = (r << 16) | (g << 8) | b;
        }
    }
    return 0;
}

/*****************************************************************************/
int
YUY2_to_RGB32(unsigned char *yuvs, int width, int height, int *rgbs)
{
    int y1;
    int y2;
    int u;
    int v;
    int c;
    int d;
    int e;
    int r;
    int g;
    int b;
    int t;
    int i;
    int j;

    for (j = 0; j < height; j++)
    {
        for (i = 0; i < width; i++)
        {
            y1 = *(yuvs++);
            v = *(yuvs++);
            y2 = *(yuvs++);
            u = *(yuvs++);

            c = y1 - 16;
            d = u - 128;
            e = v - 128;
            t = (298 * c + 409 * e + 128) >> 8;
            b = RDPCLAMP(t, 0, 255);
            t = (298 * c - 100 * d - 208 * e + 128) >> 8;
            g = RDPCLAMP(t, 0, 255);
            t = (298 * c + 516 * d + 128) >> 8;
            r = RDPCLAMP(t, 0, 255);
            rgbs[j * width + i] = (r << 16) | (g << 8) | b;

            i++;
            c = y2 - 16;
            d = u - 128;
            e = v - 128;
            t = (298 * c + 409 * e + 128) >> 8;
            b = RDPCLAMP(t, 0, 255);
            t = (298 * c - 100 * d - 208 * e + 128) >> 8;
            g = RDPCLAMP(t, 0, 255);
            t = (298 * c + 516 * d + 128) >> 8;
            r = RDPCLAMP(t, 0, 255);
            rgbs[j * width + i] = (r << 16) | (g << 8) | b;
        }
    }
    return 0;
}

/*****************************************************************************/
int
UYVY_to_RGB32(unsigned char *yuvs, int width, int height, int *rgbs)
{
    int y1;
    int y2;
    int u;
    int v;
    int c;
    int d;
    int e;
    int r;
    int g;
    int b;
    int t;
    int i;
    int j;

    for (j = 0; j < height; j++)
    {
        for (i = 0; i < width; i++)
        {
            v = *(yuvs++);
            y1 = *(yuvs++);
            u = *(yuvs++);
            y2 = *(yuvs++);

            c = y1 - 16;
            d = u - 128;
            e = v - 128;
            t = (298 * c + 409 * e + 128) >> 8;
            b = RDPCLAMP(t, 0, 255);
            t = (298 * c - 100 * d - 208 * e + 128) >> 8;
            g = RDPCLAMP(t, 0, 255);
            t = (298 * c + 516 * d + 128) >> 8;
            r = RDPCLAMP(t, 0, 255);
            rgbs[j * width + i] = (r << 16) | (g << 8) | b;

            i++;
            c = y2 - 16;
            d = u - 128;
            e = v - 128;
            t = (298 * c + 409 * e + 128) >> 8;
            b = RDPCLAMP(t, 0, 255);
            t = (298 * c - 100 * d - 208 * e + 128) >> 8;
            g = RDPCLAMP(t, 0, 255);
            t = (298 * c + 516 * d + 128) >> 8;
            r = RDPCLAMP(t, 0, 255);
            rgbs[j * width + i] = (r << 16) | (g << 8) | b;
        }
    }
    return 0;
}

#if 0
/*****************************************************************************/
static int
stretch_RGB32_RGB32(int *src, int src_width, int src_height,
                    int src_x, int src_y, int src_w, int src_h,
                    int *dst, int dst_w, int dst_h)
{
    int mwidth;
    int mheight;
    int index;

    mwidth = RDPMIN(src_width, dst_w);
    mheight = RDPMIN(src_height, dst_h);
    for (index = 0; index < mheight; index++)
    {
        g_memcpy(dst, src, mwidth * 4);
        src += src_width;
        dst += dst_w;
    }
    return 0;
}
#endif

/*****************************************************************************/
static int
stretch_RGB32_RGB32(int *src, int src_width, int src_height,
                    int src_x, int src_y, int src_w, int src_h,
                    int *dst, int dst_w, int dst_h)
{
    int index;
    int jndex;
    int lndex;
    int last_lndex;
    int oh;
    int ih;
    int ov;
    int iv;
    int pix;
    int *src32;
    int *dst32;

    LLOGLN(10, ("stretch_RGB32_RGB32: oh 0x%8.8x ov 0x%8.8x", oh, ov));
    oh = (src_w << 16) / dst_w;
    ov = (src_h << 16) / dst_h;
    iv = ov;
    lndex = src_y;
    last_lndex = -1;
    for (index = 0; index < dst_h; index++)
    {
        if (lndex == last_lndex)
        {
            /* repeat line */
            dst32 = dst + index * dst_w;
            src32 = dst32 - dst_w;
            g_memcpy(dst32, src32, dst_w * 4);
        }
        else
        {
            ih = oh;
            src32 = src + lndex * src_width + src_x;
            pix = *src32;
            dst32 = dst + index * dst_w;
            for (jndex = 0; jndex < dst_w; jndex++)
            {
                *dst32 = pix;
                while (ih > (1 << 16) - 1)
                {
                    ih -= 1 << 16;
                    src32++;
                }
                pix = *src32;
                ih += oh;
                dst32++;
            }
        }
        last_lndex = lndex;
        while (iv > (1 << 16) - 1)
        {
            iv -= 1 << 16;
            lndex++;
        }
        iv += ov;

    }
    LLOGLN(10, ("stretch_RGB32_RGB32: out"));
    return 0;
}

/******************************************************************************/
/* returns error */
static CARD32
rdpDeferredXvCleanup(OsTimerPtr timer, CARD32 now, pointer arg)
{
    rdpPtr dev;

    LLOGLN(0, ("rdpDeferredXvCleanup:"));
    dev = (rdpPtr) arg;
    dev->xv_timer_schedualed = 0;
    dev->xv_data_bytes = 0;
    g_free(dev->xv_data);
    dev->xv_data = 0;
    return 0;
}

/*****************************************************************************/
/* see hw/xfree86/common/xf86xv.c for info */
static int
xrdpVidPutImage(ScrnInfoPtr pScrn,
                short src_x, short src_y, short drw_x, short drw_y,
                short src_w, short src_h, short drw_w, short drw_h,
                int format, unsigned char* buf,
                short width, short height,
                Bool sync, RegionPtr clipBoxes,
                pointer data, DrawablePtr dst)
{
    rdpPtr dev;
    int *rgborg32;
    int *rgbend32;
    int index;
    int error;
    GCPtr tempGC;

    LLOGLN(10, ("xrdpVidPutImage: format 0x%8.8x", format));
    LLOGLN(10, ("xrdpVidPutImage: src_x %d srcy_y %d", src_x, src_y));
    dev = XRDPPTR(pScrn);

    if (dev->xv_timer_schedualed)
    {
        TimerCancel(dev->xv_timer);
        dev->xv_timer = TimerSet(dev->xv_timer, 0, 2000,
                                 rdpDeferredXvCleanup, dev);
    }
    else
    {
        dev->xv_timer_schedualed = 1;
        dev->xv_timer = TimerSet(dev->xv_timer, 0, 2000,
                                 rdpDeferredXvCleanup, dev);
    }

    index = width * height * 4 + drw_w * drw_h * 4 + 64;
    if (index > dev->xv_data_bytes)
    {
        g_free(dev->xv_data);
        dev->xv_data = g_malloc(index, 0);
        if (dev->xv_data == NULL)
        {
            LLOGLN(0, ("xrdpVidPutImage: memory alloc error"));
            dev->xv_data_bytes = 0;
            return Success;
        }
        dev->xv_data_bytes = index;
    }
    rgborg32 = (int *) RDPALIGN(dev->xv_data, 16);
    rgbend32 = rgborg32 + width * height;
    rgbend32 = (int *) RDPALIGN(rgbend32, 16);
    error = 0;

    switch (format)
    {
        case FOURCC_YV12:
            LLOGLN(10, ("xrdpVidPutImage: FOURCC_YV12"));
            error = dev->yv12_to_rgb32(buf, width, height, rgborg32);
            break;
        case FOURCC_I420:
            LLOGLN(10, ("xrdpVidPutImage: FOURCC_I420"));
            error = dev->i420_to_rgb32(buf, width, height, rgborg32);
            break;
        case FOURCC_YUY2:
            LLOGLN(10, ("xrdpVidPutImage: FOURCC_YUY2"));
            error = dev->yuy2_to_rgb32(buf, width, height, rgborg32);
            break;
        case FOURCC_UYVY:
            LLOGLN(10, ("xrdpVidPutImage: FOURCC_UYVY"));
            error = dev->uyvy_to_rgb32(buf, width, height, rgborg32);
            break;
        default:
            LLOGLN(0, ("xrdpVidPutImage: unknown format 0x%8.8x", format));
            return Success;
    }
    if (error != 0)
    {
        return Success;
    }
    if ((width == drw_w) && (height == drw_h))
    {
        LLOGLN(10, ("xrdpVidPutImage: strech skip"));
        rgbend32 = rgborg32;
    }
    else
    {
        error = stretch_RGB32_RGB32(rgborg32, width, height,
                                    src_x, src_y, src_w, src_h,
                                    rgbend32, drw_w, drw_h);
        if (error != 0)
        {
            return Success;
        }

    }

    tempGC = GetScratchGC(dst->depth, pScrn->pScreen);
    if (tempGC != NULL)
    {
        ValidateGC(dst, tempGC);
        (*tempGC->ops->PutImage)(dst, tempGC, 24,
                                 drw_x - dst->x, drw_y - dst->y,
                                 drw_w, drw_h, 0, ZPixmap,
                                 (char *) rgbend32);
        FreeScratchGC(tempGC);
    }

    return Success;
}

/*****************************************************************************/
static int
xrdpVidQueryImageAttributes(ScrnInfoPtr pScrn, int id,
                            unsigned short *w, unsigned short *h,
                            int *pitches, int *offsets)
{
    int size, tmp;

    LLOGLN(10, ("xrdpVidQueryImageAttributes:"));
    /* this is same code as all drivers currently have */
    if (*w > 2046)
    {
        *w = 2046;
    }
    if (*h > 2046)
    {
        *h = 2046;
    }
    /* make w multiple of 4 so that resizing works properly */
    *w = (*w + 3) & ~3;
    if (offsets != NULL)
    {
        offsets[0] = 0;
    }
    switch (id)
    {
        case FOURCC_YV12:
        case FOURCC_I420:
            /* make h be even */
            *h = (*h + 1) & ~1;
            /* make w be multiple of 4 (ie. pad it) */
            size = (*w + 3) & ~3;
            /* width of a Y row => width of image */
            if (pitches != NULL)
            {
                pitches[0] = size;
            }
            /* offset of U plane => w * h */
            size *= *h;
            if (offsets != NULL)
            {
                offsets[1] = size;
            }
            /* width of U, V row => width / 2 */
            tmp = ((*w >> 1) + 3) & ~3;
            if (pitches != NULL)
            {
                pitches[1] = pitches[2] = tmp;
            }
            /* offset of V => Y plane + U plane (w * h + w / 2 * h / 2) */
            tmp *= (*h >> 1);
            size += tmp;
            if (offsets != NULL)
            {
                offsets[2] = size;
            }
            size += tmp;
            break;
        case FOURCC_YUY2:
        case FOURCC_UYVY:
            size = (*w) * 2;
            if (pitches != NULL)
            {
                pitches[0] = size;
            }
            size *= *h;
            break;
        default:
            LLOGLN(0, ("xrdpVidQueryImageAttributes: Unsupported image"));
            return 0;
    }
    LLOGLN(10, ("xrdpVidQueryImageAttributes: finished size %d id 0x%x", size, id));
    return size;
}

/*****************************************************************************/
Bool
rdpXvInit(ScreenPtr pScreen, ScrnInfoPtr pScrn)
{
    XF86VideoAdaptorPtr adaptor;
    DevUnion* pDevUnion;
    int bytes;

    adaptor = xf86XVAllocateVideoAdaptorRec(pScrn);
    if (adaptor == 0)
    {
        LLOGLN(0, ("rdpXvInit: xf86XVAllocateVideoAdaptorRec failed"));
        return 0;
    }
    adaptor->type = XvInputMask | XvImageMask | XvVideoMask | XvStillMask | XvWindowMask | XvPixmapMask;
    //adaptor->flags = VIDEO_NO_CLIPPING;
    //adaptor->flags = VIDEO_CLIP_TO_VIEWPORT;
    adaptor->flags = 0;
    adaptor->name = XRDP_MODULE_NAME " XVideo Adaptor";
    adaptor->nEncodings = T_NUM_ENCODINGS;
    adaptor->pEncodings = &(g_xrdpVidEncodings[0]);
    adaptor->nFormats = T_NUM_FORMATS;
    adaptor->pFormats = &(g_xrdpVidFormats[0]);
    adaptor->pFormats[0].depth = pScrn->depth;
    LLOGLN(0, ("rdpXvInit: depth %d", pScrn->depth));
    adaptor->nImages = sizeof(g_xrdpVidImages) / sizeof(XF86ImageRec);
    adaptor->pImages = g_xrdpVidImages;
    adaptor->nAttributes = 0;
    adaptor->pAttributes = 0;
    adaptor->nPorts = T_MAX_PORTS;
    bytes = sizeof(DevUnion) * T_MAX_PORTS;
    pDevUnion = (DevUnion*) g_malloc(bytes, 1);
    adaptor->pPortPrivates = pDevUnion;
    adaptor->PutVideo = xrdpVidPutVideo;
    adaptor->PutStill = xrdpVidPutStill;
    adaptor->GetVideo = xrdpVidGetVideo;
    adaptor->GetStill = xrdpVidGetStill;
    adaptor->StopVideo = xrdpVidStopVideo;
    adaptor->SetPortAttribute = xrdpVidSetPortAttribute;
    adaptor->GetPortAttribute = xrdpVidGetPortAttribute;
    adaptor->QueryBestSize = xrdpVidQueryBestSize;
    adaptor->PutImage = xrdpVidPutImage;
    adaptor->QueryImageAttributes = xrdpVidQueryImageAttributes;
    if (!xf86XVScreenInit(pScreen, &adaptor, 1))
    {
        LLOGLN(0, ("rdpXvInit: xf86XVScreenInit failed"));
        return 0;
    }
    xf86XVFreeVideoAdaptorRec(adaptor);
    return 1;
}

