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

#define LOG_LEVEL 1
#define LLOGLN(_level, _args) \
    do { if (_level < LOG_LEVEL) { ErrorF _args ; ErrorF("\n"); } } while (0)

/******************************************************************************/
static Bool
rdpCapture0(RegionPtr in_reg, RegionPtr out_reg,
            void *src, int src_width, int src_height,
            int src_stride, int src_format,
            void *dst, int dst_width, int dst_height,
            int dst_stride, int dst_format, int max_rects)
{
    BoxPtr prects;
    BoxRec rect;
    RegionRec reg;
    char *src_rect;
    char *dst_rect;
    int num_regions;
    int bytespp;
    int width;
    int height;
    int src_offset;
    int dst_offset;
    int bytes;
    int i;
    int j;
    Bool rv;

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
        prects = rdpRegionExtents(&reg);
        rdpRegionUninit(out_reg);
        rdpRegionInit(out_reg, prects, 0);
    }
    else
    {
        prects = REGION_RECTS(&reg);
        rdpRegionCopy(out_reg, &reg);
    }

    if ((src_format == XRDP_a8r8g8b8) && (dst_format == XRDP_a8r8g8b8))
    {
        bytespp = 4;

        for (i = 0; i < num_regions; i++)
        {
            /* get rect to copy */
            rect = prects[i];

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
    else
    {
        LLOGLN(0, ("rdpCapture0: unimp color conversion"));
    }
    rdpRegionUninit(&reg);
    return rv;
}

/**
 * Copy an array of rectangles from one memory area to another
 *****************************************************************************/
Bool
rdpCapture(RegionPtr in_reg, RegionPtr out_reg,
           void *src, int src_width, int src_height,
           int src_stride, int src_format,
           void *dst, int dst_width, int dst_height,
           int dst_stride, int dst_format, int mode)
{
    LLOGLN(10, ("rdpCapture:"));
    switch (mode)
    {
        case 0:
            return rdpCapture0(in_reg, out_reg,
                               src, src_width, src_height,
                               src_stride, src_format,
                               dst, dst_width, dst_height,
                               dst_stride, dst_format, 15);
        default:
            LLOGLN(0, ("rdpCapture: unimp mode"));
            break;
    }
    return TRUE;
}
