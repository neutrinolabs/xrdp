/**
 * painter utils
 *
 * Copyright 2015 Jay Sorg <jay.sorg@gmail.com>
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
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "painter.h"
#include "painter_utils.h"

/*****************************************************************************/
/* do a raster op */
int
do_rop(int rop, int src, int dst)
{
    switch (rop)
    {
        case PT_ROP_0:    return 0;
        case PT_ROP_DSon: return ~(src | dst);
        case PT_ROP_DSna: return (~src) & dst;
        case PT_ROP_Sn:   return ~src;
        case PT_ROP_SDna: return src & (~dst);
        case PT_ROP_Dn:   return ~(dst);
        case PT_ROP_DSx:  return src ^ dst;
        case PT_ROP_DSan: return ~(src & dst);
        case PT_ROP_DSa:  return src & dst;
        case PT_ROP_DSxn: return ~(src) ^ dst;
        case PT_ROP_D:    return dst;
        case PT_ROP_DSno: return (~src) | dst;
        case PT_ROP_S:    return src;
        case PT_ROP_SDno: return src | (~dst);
        case PT_ROP_DSo:  return src | dst;
        case PT_ROP_1:    return ~0;
    }
    return dst;
}

/*****************************************************************************/
char *
bitmap_get_ptr(struct painter_bitmap *bitmap, int x, int y)
{
    char *p;
    int Bpp;

    if ((x >= 0) && (x < bitmap->width) &&
        (y >= 0) && (y < bitmap->height))
    {
        Bpp = ((bitmap->format >> 24) + 7) / 8;
        p = bitmap->data + (y * bitmap->stride_bytes) + (x * Bpp);
        return p;
    }
    else
    {
        return NULL;
    }
}

/*****************************************************************************/
static int
bitmap_get_pixel(struct painter_bitmap *dst, int x, int y)
{
    return 0;
}

/*****************************************************************************/
static int
bitmap_set_pixel(struct painter_bitmap *dst, int x, int y, int pixel)
{
    return 0;
}

/*****************************************************************************/
int
pixel_convert(int pixel, int src_format, int dst_format, int *palette)
{
    int a;
    int r;
    int g;
    int b;
    int rv;

    if (src_format == dst_format)
    {
        return pixel;
    }
    switch (src_format)
    {
        case PT_FORMAT_a1r5g5b5:
            SPLIT_a1r5g5b5(pixel, a, r, g, b);
            break;
        case PT_FORMAT_r5g6b5:
            SPLIT_r5g6b5(pixel, a, r, g, b);
            break;
    }
    rv = 0;
    switch (dst_format)
    {
        case PT_FORMAT_a8r8g8b8:
            rv = (a << 24) | (r << 16) | (g << 8) | b;
            break;
    }
    return rv; 
}

/*****************************************************************************/
int
painter_set_pixel(struct painter *painter, struct painter_bitmap *dst,
                  int x, int y, int pixel, int pixel_format)
{
    if (painter->clip_valid == 0 ||
        (x >= painter->clip.x1 && x < painter->clip.x2 &&
         y >= painter->clip.y1 && y < painter->clip.y2))
    {
        if ((x >= 0) && (x < dst->width) &&
            (y >= 0) && (y < dst->height))
        {
            pixel = pixel_convert(pixel, pixel_format, dst->format,
                                  painter->palette);
            if (painter->rop != PT_ROP_S)
            {
                pixel = do_rop(painter->rop, pixel,
                               bitmap_get_pixel(dst, x, y));
            }
            bitmap_set_pixel(dst, x, y, pixel);
        }
    }
    return 0;
}

