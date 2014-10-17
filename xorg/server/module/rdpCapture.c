/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Laxmikant Rashinkar 2014
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Routines to copy regions from framebuffer to shared memory
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* this should be before all X11 .h files */
#include <xorg-server.h>

/* all driver need this */
#include <xf86.h>
#include <xf86_OSproc.h>

#include "rdp.h"
#include "rdpDraw.h"
#include "rdpClientCon.h"
#include "rdpReg.h"
#include "rdpMisc.h"

#define LOG_LEVEL 1
#define LLOGLN(_level, _args) \
    do { if (_level < LOG_LEVEL) { ErrorF _args ; ErrorF("\n"); } } while (0)

#define RDP_MAX_TILES 1024

/******************************************************************************/
static int
rdpLimitRects(RegionPtr reg, int max_rects, BoxPtr *rects)
{
    int nrects;

    nrects = REGION_NUM_RECTS(reg);
    if (nrects > max_rects)
    {
        nrects = 1;
        *rects = rdpRegionExtents(reg);
    }
    else
    {
        *rects = REGION_RECTS(reg);
    }
    return nrects;
}

/******************************************************************************/
/* copy rects with no error checking */
static int
rdpCopyBox_a8r8g8b8_to_a8r8g8b8(rdpClientCon *clientCon,
                                void *src, int src_stride, int srcx, int srcy,
                                void *dst, int dst_stride, int dstx, int dsty,
                                BoxPtr rects, int num_rects)
{
    char *s8;
    char *d8;
    int index;
    int jndex;
    int bytes;
    int height;
    BoxPtr box;

    for (index = 0; index < num_rects; index++)
    {
        box = rects + index;
        s8 = ((char *) src) + (box->y1 - srcy) * src_stride;
        s8 += (box->x1 - srcx) * 4;
        d8 = ((char *) dst) + (box->y1 - dsty) * dst_stride;
        d8 += (box->x1 - dstx) * 4;
        bytes = box->x2 - box->x1;
        bytes *= 4;
        height = box->y2 - box->y1;
        for (jndex = 0; jndex < height; jndex++)
        {
            g_memcpy(d8, s8, bytes);
            d8 += dst_stride;
            s8 += src_stride;
        }
    }
    return 0;
}

/******************************************************************************/
static int
rdpFillBox_yuvalp(int ax, int ay,
                 void *dst, int dst_stride)
{
    dst = ((char *) dst) + (ay << 8) * (dst_stride >> 8) + (ax << 8);
    g_memset(dst, 0, 64 * 64 * 4);
    return 0;
}

/******************************************************************************/
/* copy rects with no error checking
 * convert ARGB32 to 64x64 linear planar YUVA */
/* http://msdn.microsoft.com/en-us/library/ff635643.aspx
 * 0.299   -0.168935    0.499813
 * 0.587   -0.331665   -0.418531
 * 0.114    0.50059    -0.081282
   y = r *  0.299000 + g *  0.587000 + b *  0.114000;
   u = r * -0.168935 + g * -0.331665 + b *  0.500590;
   v = r *  0.499813 + g * -0.418531 + b * -0.081282; */
/* 19595  38470   7471
  -11071 -21736  32807
   32756 -27429  -5327 */
static int
rdpCopyBox_a8r8g8b8_to_yuvalp(int ax, int ay,
                              void *src, int src_stride,
                              void *dst, int dst_stride,
                              BoxPtr rects, int num_rects)
{
    char *s8;
    char *d8;
    char *yptr;
    char *uptr;
    char *vptr;
    char *aptr;
    int *s32;
    int index;
    int jndex;
    int kndex;
    int width;
    int height;
    int pixel;
    int a;
    int r;
    int g;
    int b;
    int y;
    int u;
    int v;
    BoxPtr box;

    dst = ((char *) dst) + (ay << 8) * (dst_stride >> 8) + (ax << 8);
    for (index = 0; index < num_rects; index++)
    {
        box = rects + index;
        s8 = ((char *) src) + box->y1 * src_stride;
        s8 += box->x1 * 4;
        d8 = ((char *) dst) + (box->y1 - ay) * 64;
        d8 += box->x1 - ax;
        width = box->x2 - box->x1;
        height = box->y2 - box->y1;
        for (jndex = 0; jndex < height; jndex++)
        {
            s32 = (int *) s8;
            yptr = d8;
            uptr = yptr + 64 * 64;
            vptr = uptr + 64 * 64;
            aptr = vptr + 64 * 64;
            kndex = 0;
            while (kndex < width)
            {
                pixel = *(s32++);
                a = (pixel >> 24) & 0xff;
                r = (pixel >> 16) & 0xff;
                g = (pixel >>  8) & 0xff;
                b = (pixel >>  0) & 0xff;
                y = (r *  19595 + g *  38470 + b *   7471) >> 16;
                u = (r * -11071 + g * -21736 + b *  32807) >> 16;
                v = (r *  32756 + g * -27429 + b *  -5327) >> 16;
                y = y - 128;
                y = max(y, -128);
                u = max(u, -128);
                v = max(v, -128);
                y = min(y, 127);
                u = min(u, 127);
                v = min(v, 127);
                *(yptr++) = y;
                *(uptr++) = u;
                *(vptr++) = v;
                *(aptr++) = a;
                kndex++;
            }
            d8 += 64;
            s8 += src_stride;
        }
    }
    return 0;
}

/******************************************************************************/
int
a8r8g8b8_to_a8b8g8r8_box(char *s8, int src_stride,
                         char *d8, int dst_stride,
                         int width, int height)
{
    int index;
    int jndex;
    int red;
    int green;
    int blue;
    unsigned int *s32;
    unsigned int *d32;

    for (index = 0; index < height; index++)
    {
        s32 = (unsigned int *) s8;
        d32 = (unsigned int *) d8;
        for (jndex = 0; jndex < width; jndex++)
        {
            SPLITCOLOR32(red, green, blue, *s32);
            *d32 = COLOR24(red, green, blue);
             s32++;
             d32++;
        }
        d8 += dst_stride;
        s8 += src_stride;
    }
    return 0;
}

/******************************************************************************/
/* copy rects with no error checking */
static int
rdpCopyBox_a8r8g8b8_to_a8b8g8r8(rdpClientCon *clientCon,
                                void *src, int src_stride, int srcx, int srcy,
                                void *dst, int dst_stride, int dstx, int dsty,
                                BoxPtr rects, int num_rects)
{
    char *s8;
    char *d8;
    int index;
    int bytes;
    int width;
    int height;
    BoxPtr box;
    copy_box_proc copy_box;

    copy_box = clientCon->dev->a8r8g8b8_to_a8b8g8r8_box;
    for (index = 0; index < num_rects; index++)
    {
        box = rects + index;
        s8 = ((char *) src) + (box->y1 - srcy) * src_stride;
        s8 += (box->x1 - srcx) * 4;
        d8 = ((char *) dst) + (box->y1 - dsty) * dst_stride;
        d8 += (box->x1 - dstx) * 4;
        bytes = box->x2 - box->x1;
        bytes *= 4;
        width = box->x2 - box->x1;
        height = box->y2 - box->y1;
        copy_box(s8, src_stride, d8, dst_stride, width, height);
    }
    return 0;
}

/******************************************************************************/
static Bool
rdpCapture0(rdpClientCon *clientCon,
            RegionPtr in_reg, BoxPtr *out_rects, int *num_out_rects,
            void *src, int src_width, int src_height,
            int src_stride, int src_format,
            void *dst, int dst_width, int dst_height,
            int dst_stride, int dst_format, int max_rects)
{
    BoxPtr psrc_rects;
    BoxRec rect;
    RegionRec reg;
    char *src_rect;
    char *dst_rect;
    int num_rects;
    int src_bytespp;
    int dst_bytespp;
    int width;
    int height;
    int src_offset;
    int dst_offset;
    int i;
    int j;
    int k;
    int red;
    int green;
    int blue;
    Bool rv;
    unsigned int *s32;
    unsigned short *d16;
    unsigned char *d8;

    LLOGLN(10, ("rdpCapture0:"));

    rv = TRUE;

    rect.x1 = 0;
    rect.y1 = 0;
    rect.x2 = RDPMIN(dst_width, src_width);
    rect.y2 = RDPMIN(dst_height, src_height);
    rdpRegionInit(&reg, &rect, 0);
    rdpRegionIntersect(&reg, in_reg, &reg);

    psrc_rects = 0;
    num_rects = rdpLimitRects(&reg, max_rects, &psrc_rects);
    if (num_rects < 1)
    {
        rdpRegionUninit(&reg);
        return FALSE;
    }

    *num_out_rects = num_rects;

    *out_rects = (BoxPtr) g_malloc(sizeof(BoxRec) * num_rects, 0);
    for (i = 0; i < num_rects; i++)
    {
        rect = psrc_rects[i];
        (*out_rects)[i] = rect;
    }

    if ((src_format == XRDP_a8r8g8b8) && (dst_format == XRDP_a8r8g8b8))
    {
        rdpCopyBox_a8r8g8b8_to_a8r8g8b8(clientCon,
                                        src, src_stride, 0, 0,
                                        dst, dst_stride, 0, 0,
                                        psrc_rects, num_rects);
    }
    else if ((src_format == XRDP_a8r8g8b8) && (dst_format == XRDP_a8b8g8r8))
    {
        rdpCopyBox_a8r8g8b8_to_a8b8g8r8(clientCon,
                                        src, src_stride, 0, 0,
                                        dst, dst_stride, 0, 0,
                                        psrc_rects, num_rects);
    }
    else if ((src_format == XRDP_a8r8g8b8) && (dst_format == XRDP_r5g6b5))
    {
        src_bytespp = 4;
        dst_bytespp = 2;

        for (i = 0; i < num_rects; i++)
        {
            /* get rect to copy */
            rect = (*out_rects)[i];

            /* get rect dimensions */
            width = rect.x2 - rect.x1;
            height = rect.y2 - rect.y1;

            /* point to start of each rect in respective memory */
            src_offset = rect.y1 * src_stride + rect.x1 * src_bytespp;
            dst_offset = rect.y1 * dst_stride + rect.x1 * dst_bytespp;
            src_rect = src + src_offset;
            dst_rect = dst + dst_offset;

            /* copy one line at a time */
            for (j = 0; j < height; j++)
            {
                s32 = (unsigned int *) src_rect;
                d16 = (unsigned short *) dst_rect;
                for (k = 0; k < width; k++)
                {
                    SPLITCOLOR32(red, green, blue, *s32);
                    *d16 = COLOR16(red, green, blue);
                    s32++;
                    d16++;
                }
                src_rect += src_stride;
                dst_rect += dst_stride;
            }
        }
    }
    else if ((src_format == XRDP_a8r8g8b8) && (dst_format == XRDP_a1r5g5b5))
    {
        src_bytespp = 4;
        dst_bytespp = 2;

        for (i = 0; i < num_rects; i++)
        {
            /* get rect to copy */
            rect = (*out_rects)[i];

            /* get rect dimensions */
            width = rect.x2 - rect.x1;
            height = rect.y2 - rect.y1;

            /* point to start of each rect in respective memory */
            src_offset = rect.y1 * src_stride + rect.x1 * src_bytespp;
            dst_offset = rect.y1 * dst_stride + rect.x1 * dst_bytespp;
            src_rect = src + src_offset;
            dst_rect = dst + dst_offset;

            /* copy one line at a time */
            for (j = 0; j < height; j++)
            {
                s32 = (unsigned int *) src_rect;
                d16 = (unsigned short *) dst_rect;
                for (k = 0; k < width; k++)
                {
                    SPLITCOLOR32(red, green, blue, *s32);
                    *d16 = COLOR15(red, green, blue);
                    s32++;
                    d16++;
                }
                src_rect += src_stride;
                dst_rect += dst_stride;
            }
        }
    }
    else if ((src_format == XRDP_a8r8g8b8) && (dst_format == XRDP_r3g3b2))
    {
        src_bytespp = 4;
        dst_bytespp = 1;

        for (i = 0; i < num_rects; i++)
        {
            /* get rect to copy */
            rect = (*out_rects)[i];

            /* get rect dimensions */
            width = rect.x2 - rect.x1;
            height = rect.y2 - rect.y1;

            /* point to start of each rect in respective memory */
            src_offset = rect.y1 * src_stride + rect.x1 * src_bytespp;
            dst_offset = rect.y1 * dst_stride + rect.x1 * dst_bytespp;
            src_rect = src + src_offset;
            dst_rect = dst + dst_offset;

            /* copy one line at a time */
            for (j = 0; j < height; j++)
            {
                s32 = (unsigned int *) src_rect;
                d8 = (unsigned char *) dst_rect;
                for (k = 0; k < width; k++)
                {
                    SPLITCOLOR32(red, green, blue, *s32);
                    *d8 = COLOR8(red, green, blue);
                    s32++;
                    d8++;
                }
                src_rect += src_stride;
                dst_rect += dst_stride;
            }
        }
    }
    else
    {
        LLOGLN(0, ("rdpCapture0: unimp color conversion"));
    }
    rdpRegionUninit(&reg);
    return rv;
}

/******************************************************************************/
/* make out_rects always multiple of 16 width and height */
static Bool
rdpCapture1(rdpClientCon *clientCon,
            RegionPtr in_reg, BoxPtr *out_rects, int *num_out_rects,
            void *src, int src_width, int src_height,
            int src_stride, int src_format,
            void *dst, int dst_width, int dst_height,
            int dst_stride, int dst_format, int max_rects)
{
    BoxPtr psrc_rects;
    BoxRec rect;
    RegionRec reg;
    char *src_rect;
    char *dst_rect;
    int num_regions;
    int src_bytespp;
    int dst_bytespp;
    int width;
    int height;
    int min_width;
    int min_height;
    int src_offset;
    int dst_offset;
    int index;
    int jndex;
    int kndex;
    int red;
    int green;
    int blue;
    int ex;
    int ey;
    Bool rv;
    unsigned int *s32;
    unsigned int *d32;

    LLOGLN(10, ("rdpCapture1:"));

    rv = TRUE;

    min_width = RDPMIN(dst_width, src_width);
    min_height = RDPMIN(dst_height, src_height);

    rect.x1 = 0;
    rect.y1 = 0;
    rect.x2 = min_width;
    rect.y2 = min_height;
    rdpRegionInit(&reg, &rect, 0);
    rdpRegionIntersect(&reg, in_reg, &reg);

    num_regions = REGION_NUM_RECTS(&reg);

    if (num_regions > max_rects)
    {
        num_regions = 1;
        psrc_rects = rdpRegionExtents(&reg);
    }
    else
    {
        psrc_rects = REGION_RECTS(&reg);
    }

    if (num_regions < 1)
    {
        return FALSE;
    }

    *num_out_rects = num_regions;

    *out_rects = (BoxPtr) g_malloc(sizeof(BoxRec) * num_regions * 4, 0);
    index = 0;
    while (index < num_regions)
    {
        rect = psrc_rects[index];
        width = rect.x2 - rect.x1;
        height = rect.y2 - rect.y1;
        ex = ((width + 15) & ~15) - width;
        if (ex != 0)
        {
            rect.x2 += ex;
            if (rect.x2 > min_width)
            {
                rect.x1 -= rect.x2 - min_width;
                rect.x2 = min_width;
            }
            if (rect.x1 < 0)
            {
                rect.x1 += 16;
            }
        }
        ey = ((height + 15) & ~15) - height;
        if (ey != 0)
        {
            rect.y2 += ey;
            if (rect.y2 > min_height)
            {
                rect.y1 -= rect.y2 - min_height;
                rect.y2 = min_height;
            }
            if (rect.y1 < 0)
            {
                rect.y1 += 16;
            }
        }
#if 0
        if (rect.x1 < 0)
        {
            LLOGLN(0, ("rdpCapture1: error"));
        }
        if (rect.y1 < 0)
        {
            LLOGLN(0, ("rdpCapture1: error"));
        }
        if (rect.x2 > min_width)
        {
            LLOGLN(0, ("rdpCapture1: error"));
        }
        if (rect.y2 > min_height)
        {
            LLOGLN(0, ("rdpCapture1: error"));
        }
        if ((rect.x2 - rect.x1) % 16 != 0)
        {
            LLOGLN(0, ("rdpCapture1: error"));
        }
        if ((rect.y2 - rect.y1) % 16 != 0)
        {
            LLOGLN(0, ("rdpCapture1: error"));
        }
#endif
        (*out_rects)[index] = rect;
        index++;
    }

    if ((src_format == XRDP_a8r8g8b8) && (dst_format == XRDP_a8b8g8r8))
    {
        src_bytespp = 4;
        dst_bytespp = 4;

        for (index = 0; index < num_regions; index++)
        {
            /* get rect to copy */
            rect = (*out_rects)[index];

            /* get rect dimensions */
            width = rect.x2 - rect.x1;
            height = rect.y2 - rect.y1;

            /* point to start of each rect in respective memory */
            src_offset = rect.y1 * src_stride + rect.x1 * src_bytespp;
            dst_offset = rect.y1 * dst_stride + rect.x1 * dst_bytespp;
            src_rect = src + src_offset;
            dst_rect = dst + dst_offset;

            /* copy one line at a time */
            for (jndex = 0; jndex < height; jndex++)
            {
                s32 = (unsigned int *) src_rect;
                d32 = (unsigned int *) dst_rect;
                for (kndex = 0; kndex < width; kndex++)
                {
                    SPLITCOLOR32(red, green, blue, *s32);
                    *d32 = COLOR24(red, green, blue);
                    s32++;
                    d32++;
                }
                src_rect += src_stride;
                dst_rect += dst_stride;
            }
        }
    }
    else
    {
        LLOGLN(0, ("rdpCapture1: unimp color conversion"));
    }
    rdpRegionUninit(&reg);
    return rv;
}

/******************************************************************************/
static Bool
rdpCapture2(rdpClientCon *clientCon,
            RegionPtr in_reg, BoxPtr *out_rects, int *num_out_rects,
            void *src, int src_width, int src_height,
            int src_stride, int src_format,
            void *dst, int dst_width, int dst_height,
            int dst_stride, int dst_format, int max_rects)
{
    int x;
    int y;
    int out_rect_index;
    int num_rects;
    int rcode;
    BoxRec rect;
    BoxRec extents_rect;
    BoxPtr rects;
    RegionRec tile_reg;
    RegionRec lin_reg;
    RegionRec temp_reg;
    RegionPtr pin_reg;

    LLOGLN(10, ("rdpCapture2:"));

    *out_rects = (BoxPtr) g_malloc(sizeof(BoxRec) * RDP_MAX_TILES, 0);
    if (*out_rects == NULL)
    {
        return FALSE;
    }
    out_rect_index = 0;

    /* clip for smaller of 2 */
    rect.x1 = 0;
    rect.y1 = 0;
    rect.x2 = min(dst_width, src_width);
    rect.y2 = min(dst_height, src_height);
    rdpRegionInit(&temp_reg, &rect, 0);
    rdpRegionIntersect(&temp_reg, in_reg, &temp_reg);

    /* limit the numer of rects */
    num_rects = REGION_NUM_RECTS(&temp_reg);
    if (num_rects > max_rects)
    {
        LLOGLN(10, ("rdpCapture2: too many rects"));
        rdpRegionInit(&lin_reg, rdpRegionExtents(&temp_reg), 0);
        pin_reg = &lin_reg;
    }
    else
    {
        LLOGLN(10, ("rdpCapture2: not too many rects"));
        rdpRegionInit(&lin_reg, NullBox, 0);
        pin_reg = &temp_reg;
    }
    extents_rect = *rdpRegionExtents(pin_reg);
    y = extents_rect.y1 & ~63;
    while (y < extents_rect.y2)
    {
        x = extents_rect.x1 & ~63;
        while (x < extents_rect.x2)
        {
            rect.x1 = x;
            rect.y1 = y;
            rect.x2 = rect.x1 + 64;
            rect.y2 = rect.y1 + 64;
            rcode = rdpRegionContainsRect(pin_reg, &rect);
            LLOGLN(10, ("rdpCapture2: rcode %d", rcode));

            if (rcode != rgnOUT)
            {
                if (rcode == rgnPART)
                {
                    LLOGLN(10, ("rdpCapture2: rgnPART"));
                    rdpFillBox_yuvalp(x, y, dst, dst_stride);
                    rdpRegionInit(&tile_reg, &rect, 0);
                    rdpRegionIntersect(&tile_reg, pin_reg, &tile_reg);
                    rects = REGION_RECTS(&tile_reg);
                    num_rects = REGION_NUM_RECTS(&tile_reg);
                    rdpCopyBox_a8r8g8b8_to_yuvalp(x, y,
                                                  src, src_stride,
                                                  dst, dst_stride,
                                                  rects, num_rects);
                    rdpRegionUninit(&tile_reg);
                }
                else /* rgnIN */
                {
                    LLOGLN(10, ("rdpCapture2: rgnIN"));
                    rdpCopyBox_a8r8g8b8_to_yuvalp(x, y,
                                                  src, src_stride,
                                                  dst, dst_stride,
                                                  &rect, 1);
                }
                (*out_rects)[out_rect_index] = rect;
                out_rect_index++;
                if (out_rect_index >= RDP_MAX_TILES)
                {
                    g_free(*out_rects);
                    *out_rects = NULL;
                    rdpRegionUninit(&temp_reg);
                    rdpRegionUninit(&lin_reg);
                    return FALSE;
                }
            }
            x += 64;
        }
        y += 64;
    }
    *num_out_rects = out_rect_index;
    rdpRegionUninit(&temp_reg);
    rdpRegionUninit(&lin_reg);
    return TRUE;
}

/**
 * Copy an array of rectangles from one memory area to another
 *****************************************************************************/
Bool
rdpCapture(rdpClientCon *clientCon,
           RegionPtr in_reg, BoxPtr *out_rects, int *num_out_rects,
           void *src, int src_width, int src_height,
           int src_stride, int src_format,
           void *dst, int dst_width, int dst_height,
           int dst_stride, int dst_format, int mode)
{
    LLOGLN(10, ("rdpCapture:"));
    LLOGLN(10, ("rdpCapture: src %p dst %p", src, dst));
    switch (mode)
    {
        case 0:
            return rdpCapture0(clientCon, in_reg, out_rects, num_out_rects,
                               src, src_width, src_height,
                               src_stride, src_format,
                               dst, dst_width, dst_height,
                               dst_stride, dst_format, 15);
        case 1:
            return rdpCapture1(clientCon, in_reg, out_rects, num_out_rects,
                               src, src_width, src_height,
                               src_stride, src_format,
                               dst, dst_width, dst_height,
                               dst_stride, dst_format, 15);
        case 2:
            return rdpCapture2(clientCon, in_reg, out_rects, num_out_rects,
                               src, src_width, src_height,
                               src_stride, src_format,
                               dst, dst_width, dst_height,
                               dst_stride, dst_format, 15);
        default:
            LLOGLN(0, ("rdpCapture: unimp mode"));
            break;
    }
    return FALSE;
}
