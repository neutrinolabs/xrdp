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

/**
 * Copy an array of rectangles from one memory area to another
 *****************************************************************************/

Bool rdpCapture(RegionPtr in_reg, RegionPtr out_reg,
                void *src, int src_width, int src_height, int src_stride, int src_format,
                void *dst, int dst_width, int dst_height, int dst_stride, int dst_format,
                int mode)
{
    BoxRec  rect;
    char   *src_rect;
    char   *dst_rect;
    int     num_regions;
    int     bpp;
    int     width;
    int     height;
    int     offset;
    int     bytes;
    int     i;
    int     j;

    /*
     * note: mode = 0: default, one is to one copy
     * xxx_format = 0: default, 4 bytes per pixel
     */

    /* for now we only handle defaults */

    /* number of rectangles to copy */
    num_regions = REGION_NUM_RECTS(in_reg);

    /* get bytes per pixel */
    bpp = src_stride / src_width;

    for (i = 0; i < num_regions; i++)
    {
        /* get rect to copy */
        rect = REGION_RECTS(in_reg)[i];

        /* get rect dimensions */
        width = rect.x2 - rect.x1;
        height = rect.y2 - rect.y1;

        /* point to start of each rect in respective memory */
        offset = rect.y1 * src_stride + rect.x1 * bpp;
        src_rect = src + offset;
        dst_rect = dst + offset;

        /* bytes per line */
        bytes = width * bpp;

        /* copy one line at a time */
        for (j = 0; j < height; j++)
        {
            memcpy(dst_rect, src_rect, bytes);
            src_rect += src_stride;
            dst_rect += src_stride;
        }
    }

    return rdpRegionCopy(out_reg, in_reg);
}
