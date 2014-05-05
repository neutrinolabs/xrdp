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

/******************************************************************************/
static Bool
rdpCapture0(RegionPtr in_reg, BoxPtr *out_rects, int *num_out_rects,
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
    int bytespp;
    int src_bytespp;
    int dst_bytespp;
    int width;
    int height;
    int src_offset;
    int dst_offset;
    int bytes;
    int i;
    int j;
    int k;
    int red;
    int green;
    int blue;
    Bool rv;
    unsigned int *s32;
    unsigned int *d32;
    unsigned short *d16;
    unsigned char *d8;

    LLOGLN(10, ("rdpCapture0:"));

    rv = TRUE;

    rect.x1 = 0;
    rect.y1 = 0;
    rect.x2 = min(dst_width, src_width);
    rect.y2 = min(dst_height, src_height);
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

    *out_rects = (BoxPtr) g_malloc(sizeof(BoxRec) * num_regions, 0);
    for (i = 0; i < num_regions; i++)
    {
        rect = psrc_rects[i];
        (*out_rects)[i] = rect;
    }

    if ((src_format == XRDP_a8r8g8b8) && (dst_format == XRDP_a8r8g8b8))
    {
        bytespp = 4;

        for (i = 0; i < num_regions; i++)
        {
            /* get rect to copy */
            rect = (*out_rects)[i];

            /* get rect dimensions */
            width = rect.x2 - rect.x1;
            height = rect.y2 - rect.y1;

            /* point to start of each rect in respective memory */
            src_offset = rect.y1 * src_stride + rect.x1 * bytespp;
            dst_offset = rect.y1 * dst_stride + rect.x1 * bytespp;
            src_rect = src + src_offset;
            dst_rect = dst + dst_offset;

            /* bytes per line */
            bytes = width * bytespp;

            /* copy one line at a time */
            for (j = 0; j < height; j++)
            {
                memcpy(dst_rect, src_rect, bytes);
                src_rect += src_stride;
                dst_rect += dst_stride;
            }
        }
    }
    else if ((src_format == XRDP_a8r8g8b8) && (dst_format == XRDP_a8b8g8r8))
    {
        src_bytespp = 4;
        dst_bytespp = 4;

        for (i = 0; i < num_regions; i++)
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
                d32 = (unsigned int *) dst_rect;
                for (k = 0; k < width; k++)
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
    else if ((src_format == XRDP_a8r8g8b8) && (dst_format == XRDP_r5g6b5))
    {
        src_bytespp = 4;
        dst_bytespp = 2;

        for (i = 0; i < num_regions; i++)
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

        for (i = 0; i < num_regions; i++)
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

        for (i = 0; i < num_regions; i++)
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
rdpCapture1(RegionPtr in_reg, BoxPtr *out_rects, int *num_out_rects,
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
    int bytespp;
    int src_bytespp;
    int dst_bytespp;
    int width;
    int height;
    int min_width;
    int min_height;
    int src_offset;
    int dst_offset;
    int bytes;
    int i;
    int j;
    int k;
    int red;
    int green;
    int blue;
    Bool rv;
    unsigned int *s32;
    unsigned int *d32;
    unsigned short *d16;
    unsigned char *d8;

    LLOGLN(10, ("rdpCapture0:"));

    rv = TRUE;

    min_width = min(dst_width, src_width);
    min_height = min(dst_height, src_height);

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

    *out_rects = (BoxPtr) g_malloc(sizeof(BoxRec) * num_regions, 0);
    for (i = 0; i < num_regions; i++)
    {
        rect = psrc_rects[i];
        width = rect.x2 - rect.x1;
        height = rect.y2 - rect.y1;
        width = (width + 15) & ~15;
        height = (height + 15) & ~15;
        rect.x2 = rect.x1 + width;
        rect.y2 = rect.y1 + height;
        if (rect.x2 > min_width)
        {
            rect.x2 = min_width;
            rect.x1 = min_width - 16;
        }
        if (rect.y2 > min_height)
        {
            rect.y2 = min_height;
            rect.y1 = min_height - 16;
        }
        (*out_rects)[i] = rect;
    }

    if ((src_format == XRDP_a8r8g8b8) && (dst_format == XRDP_a8b8g8r8))
    {
        src_bytespp = 4;
        dst_bytespp = 4;

        for (i = 0; i < num_regions; i++)
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
                d32 = (unsigned int *) dst_rect;
                for (k = 0; k < width; k++)
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

    return FALSE;
}

/**
 * Copy an array of rectangles from one memory area to another
 *****************************************************************************/
Bool
rdpCapture(RegionPtr in_reg, BoxPtr *out_rects, int *num_out_rects,
           void *src, int src_width, int src_height,
           int src_stride, int src_format,
           void *dst, int dst_width, int dst_height,
           int dst_stride, int dst_format, int mode)
{
    LLOGLN(10, ("rdpCapture:"));
    switch (mode)
    {
        case 0:
            return rdpCapture0(in_reg, out_rects, num_out_rects,
                               src, src_width, src_height,
                               src_stride, src_format,
                               dst, dst_width, dst_height,
                               dst_stride, dst_format, 15);
        case 1:
            return rdpCapture1(in_reg, out_rects, num_out_rects,
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
