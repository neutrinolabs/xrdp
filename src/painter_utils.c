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
    int bpp;
    int Bpp;

    if ((x >= 0) && (x < bitmap->width) &&
        (y >= 0) && (y < bitmap->height))
    {
        bpp = bitmap->format >> 24;
        if (bpp < 8)
        {
            p = bitmap->data + (y * bitmap->stride_bytes) + (x / 8);
            return p;
        }
        else
        {
            Bpp = (bpp + 7) / 8;
            p = bitmap->data + (y * bitmap->stride_bytes) + (x * Bpp);
            return p;
        }
    }
    else
    {
        return NULL;
    }
}

/*****************************************************************************/
int
bitmap_get_pixel(struct painter_bitmap *bitmap, int x, int y)
{
    char *ptr;
    int rv;

    ptr = bitmap_get_ptr(bitmap, x, y);
    if (ptr == NULL)
    {
        return 0;
    }
    switch (bitmap->format)
    {
        case PT_FORMAT_c1:
            rv = *((unsigned char *) ptr);
            rv = (rv & (0x80 >> (x % 8))) != 0;
            return rv;
        case PT_FORMAT_c8:
            return *((unsigned char *) ptr);
        case PT_FORMAT_r3g3b2:
            return *((unsigned char *) ptr);
        case PT_FORMAT_a1r5g5b5:
            return *((unsigned short *) ptr);
        case PT_FORMAT_r5g6b5:
            return *((unsigned short *) ptr);
        case PT_FORMAT_a8r8g8b8:
            return *((unsigned int *) ptr);
        case PT_FORMAT_a8b8g8r8:
            return *((unsigned int *) ptr);
    }
    return 0;
}

/*****************************************************************************/
int
bitmap_set_pixel(struct painter_bitmap *bitmap, int x, int y, int pixel)
{
    char *ptr;

    ptr = bitmap_get_ptr(bitmap, x, y);
    if (ptr == NULL)
    {
        return 0;
    }
    switch (bitmap->format)
    {
        case PT_FORMAT_r3g3b2:
            *((unsigned char *) ptr) = pixel;
            break;
        case PT_FORMAT_a1r5g5b5:
            *((unsigned short *) ptr) = pixel;
            break;
        case PT_FORMAT_r5g6b5:
            *((unsigned short *) ptr) = pixel;
            break;
        case PT_FORMAT_a8r8g8b8:
            *((unsigned int *) ptr) = pixel;
            break;
        case PT_FORMAT_a8b8g8r8:
            *((unsigned int *) ptr) = pixel;
            break;
    }
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
        case PT_FORMAT_r3g3b2:
            SPLIT_r3g3b2(pixel, a, r, g, b);
            break;
        case PT_FORMAT_a1r5g5b5:
            SPLIT_a1r5g5b5(pixel, a, r, g, b);
            break;
        case PT_FORMAT_r5g6b5:
            SPLIT_r5g6b5(pixel, a, r, g, b);
            break;
        case PT_FORMAT_a8r8g8b8:
            SPLIT_a8r8g8b8(pixel, a, r, g, b);
            break;
        case PT_FORMAT_a8b8g8r8:
            SPLIT_a8b8g8r8(pixel, a, r, g, b);
            break;
    }
    rv = 0;
    switch (dst_format)
    {
        case PT_FORMAT_a1r5g5b5:
            MAKE_a1r5g5b5(rv, a, r, g, b);
            break;
        case PT_FORMAT_r5g6b5:
            MAKE_r5g6b5(rv, a, r, g, b);
            break;
        case PT_FORMAT_a8r8g8b8:
            MAKE_a8r8g8b8(rv, a, r, g, b);
            break;
        case PT_FORMAT_a8b8g8r8:
            MAKE_a8b8g8r8(rv, a, r, g, b);
            break;
    }
    return rv;
}

/*****************************************************************************/
int
painter_get_pixel(struct painter *painter, struct painter_bitmap *bitmap,
                  int x, int y)
{
    int rv;

    rv = 0;
    if ((x >= 0) && (x < bitmap->width) && (y >= 0) && (y < bitmap->height))
    {
        switch (bitmap->format)
        {
            case PT_FORMAT_c1:
                rv = bitmap_get_pixel(bitmap, x, y);
                rv = rv ? painter->fgcolor : painter->bgcolor;
                break;
            case PT_FORMAT_c8:
                rv = bitmap_get_pixel(bitmap, x, y);
                rv = painter->palette[rv & 0xff];
                break;
            default:
                rv = bitmap_get_pixel(bitmap, x, y);
                break;
        }
    }
    return rv;
}

/*****************************************************************************/
int
painter_set_pixel(struct painter *painter, struct painter_bitmap *bitmap,
                  int x, int y, int pixel, int pixel_format)
{
    if ((painter->clip_valid == 0) ||
        ((x >= painter->clip.x1) && (x < painter->clip.x2) &&
         (y >= painter->clip.y1) && (y < painter->clip.y2)))
    {
        if ((x >= 0) && (x < bitmap->width) &&
            (y >= 0) && (y < bitmap->height))
        {
            pixel = pixel_convert(pixel, pixel_format, bitmap->format,
                                  painter->palette);
            if (painter->rop != PT_ROP_S)
            {
                pixel = do_rop(painter->rop, pixel,
                               bitmap_get_pixel(bitmap, x, y));
            }
            bitmap_set_pixel(bitmap, x, y, pixel);
        }
    }
    return 0;
}

