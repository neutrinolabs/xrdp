/**
 * painter main
 *
 * Copyright 2015-2016 Jay Sorg <jay.sorg@gmail.com>
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
int
painter_create(void **handle)
{
    struct painter *pt;

    if (handle == NULL)
    {
        return PT_ERROR_PARAM;
    }
    *handle = malloc(sizeof(struct painter));
    if (*handle == NULL)
    {
        return PT_ERROR_OUT_OF_MEM;
    }
    memset(*handle, 0, sizeof(struct painter));

    pt = (struct painter *) *handle;
    pt->rop = PT_ROP_S;

    return PT_ERROR_NONE;
}

/*****************************************************************************/
int
painter_delete(void *handle)
{
    if (handle == NULL)
    {
        return PT_ERROR_NONE;
    }
    free(handle);
    return PT_ERROR_NONE;
}

/*****************************************************************************/
int
painter_set_fgcolor(void *handle, int color)
{
    struct painter *pt;

    pt = (struct painter *) handle;
    pt->fgcolor = color;
    return PT_ERROR_NONE;
}

/*****************************************************************************/
int
painter_set_bgcolor(void *handle, int color)
{
    struct painter *pt;

    pt = (struct painter *) handle;
    pt->bgcolor = color;
    return PT_ERROR_NONE;
}

/*****************************************************************************/
int
painter_set_rop(void *handle, int rop)
{
    struct painter *pt;

    pt = (struct painter *) handle;
    pt->rop = rop;
    return PT_ERROR_NONE;
}

/*****************************************************************************/
int
painter_set_pattern_mode(void *handle, int mode)
{
    struct painter *pt;

    pt = (struct painter *) handle;
    pt->pattern_mode = mode;
    return PT_ERROR_NONE;
}

/*****************************************************************************/
int
painter_set_pattern_origin(void *handle, int x, int y)
{
    struct painter *pt;

    pt = (struct painter *) handle;
    pt->origin_x = x;
    pt->origin_y = y;
    return PT_ERROR_NONE;
}

/*****************************************************************************/
int
painter_set_clip(void *handle, int x, int y, int cx, int cy)
{
    struct painter *pt;

    pt = (struct painter *) handle;
    pt->clip.x1 = x;
    pt->clip.y1 = y;
    pt->clip.x2 = x + cx;
    pt->clip.y2 = y + cy;
    pt->clip_valid = 1;
    return PT_ERROR_NONE;
}

/*****************************************************************************/
int
painter_clear_clip(void *handle)
{
    struct painter *pt;

    pt = (struct painter *) handle;
    pt->clip_valid = 0;
    return PT_ERROR_NONE;
}

/*****************************************************************************/
int
painter_fill_rect(void *handle, struct painter_bitmap *dst,
                  int x, int y, int cx, int cy)
{
    int index;
    int jndex;
    int bpp;
    int *dst32;
    short *dst16;
    struct painter *pt;

    pt = (struct painter *) handle;
    if (pt->rop == PT_ROP_S)
    {
        if (painter_warp_coords(pt, &x, &y, &cx, &cy, NULL, NULL))
        {
            bpp = dst->format >> 24;
            if (bpp == 32)
            {
                for (jndex = 0; jndex < cy; jndex++)
                {
                    dst32 = (int *) bitmap_get_ptr(dst, x, y + jndex);
                    if (dst32 != NULL)
                    {
                        for (index = 0; index < cx; index++) 
                        {
                            *(dst32++) = pt->fgcolor;
                        }
                    }
                }
                return PT_ERROR_NONE;
            }
            else if (bpp == 16)
            {
                for (jndex = 0; jndex < cy; jndex++)
                {
                    dst16 = (short *) bitmap_get_ptr(dst, x, y + jndex);
                    if (dst16 != NULL)
                    {
                        for (index = 0; index < cx; index++) 
                        {
                            *(dst16++) = pt->fgcolor;
                        }
                    }
                }
                return PT_ERROR_NONE;
            }
        }
    }
    for (jndex = 0; jndex < cy; jndex++)
    {
        for (index = 0; index < cx; index++)
        {
            painter_set_pixel(pt, dst, x + index, y + jndex, pt->fgcolor,
                              dst->format);
        }
    }
    return PT_ERROR_NONE;
}

/*****************************************************************************/
int
painter_fill_pattern(void *handle, struct painter_bitmap *dst,
                     struct painter_bitmap *pat, int patx, int paty,
                     int x, int y, int cx, int cy)
{
    int index;
    int jndex;
    int pixel;
    int lx;
    int ly;
    struct painter *pt;

    pt = (struct painter *) handle;
    if (pt->pattern_mode == PT_PATTERN_MODE_OPAQUE)
    {
        for (jndex = 0; jndex < cy; jndex++)
        {
            for (index = 0; index < cx; index++)
            {
                lx = (patx + index + pt->origin_x) % pat->width;
                ly = (paty + jndex + pt->origin_y) % pat->height;
                pixel = bitmap_get_pixel(pat, lx, ly);
                if (pixel != 0)
                {
                    painter_set_pixel(pt, dst, x + index, y + jndex,
                                      pt->fgcolor, dst->format);
                }
                else
                {
                    painter_set_pixel(pt, dst, x + index, y + jndex,
                                      pt->bgcolor, dst->format);
                }
            }
        }
    }
    else
    {
        for (jndex = 0; jndex < cy; jndex++)
        {
            for (index = 0; index < cx; index++)
            {
                lx = (patx + index + pt->origin_x) % pat->width;
                ly = (paty + jndex + pt->origin_y) % pat->height;
                pixel = bitmap_get_pixel(pat, lx, ly);
                if (pixel != 0)
                {
                    painter_set_pixel(pt, dst, x + index, y + jndex,
                                      pt->fgcolor, dst->format);
                }
            }
        }
    }
    return PT_ERROR_NONE;
}

/*****************************************************************************/
int
painter_copy(void *handle, struct painter_bitmap *dst,
             int x, int y, int cx, int cy,
             struct painter_bitmap *src, int srcx, int srcy)
{
    int index;
    int jndex;
    int pixel;
    int bpp;
    struct painter *pt;
    char *src_ptr;
    char *dst_ptr;

    pt = (struct painter *) handle;
    if (pt->rop == PT_ROP_S)
    {
        if (src->format == dst->format)
        {
            bpp = src->format >> 24;
            if (painter_warp_coords(pt, &x, &y, &cx, &cy, &srcx, &srcy))
            {
                /* straight right or down */
                if ((srcy < y) || ((srcy == y) && (srcx < x)))
                {
                    for (jndex = cy - 1; jndex >= 0; jndex--)
                    {
                        dst_ptr = bitmap_get_ptr(dst, x, y + jndex);
                        src_ptr = bitmap_get_ptr(src, srcx, srcy + jndex);
                        if ((src_ptr != NULL) && (dst_ptr != NULL))
                        {
                            memmove(dst_ptr, src_ptr, cx * (bpp / 8));
                        }
                    }
                }
                else
                {
                    for (jndex = 0; jndex < cy; jndex++)
                    {
                        dst_ptr = bitmap_get_ptr(dst, x, y + jndex);
                        src_ptr = bitmap_get_ptr(src, srcx, srcy + jndex);
                        if ((src_ptr != NULL) && (dst_ptr != NULL))
                        {
                            memcpy(dst_ptr, src_ptr, cx * (bpp / 8));
                        }
                    }
                }
            }
            return PT_ERROR_NONE;
        }
    }

    /* straight right or down */
    if ((srcy < y) || ((srcy == y) && (srcx < x)))
    {
        for (jndex = cy - 1; jndex >= 0; jndex--)
        {
            for (index = cx - 1; index >= 0; index--)
            {
                pixel = painter_get_pixel(pt, src, srcx + index,
                                          srcy + jndex);
                painter_set_pixel(pt, dst, x + index, y + jndex,
                                  pixel, src->format);
            }
        }
    }
    else
    {
        for (jndex = 0; jndex < cy; jndex++)
        {
            for (index = 0; index < cx; index++)
            {
                pixel = painter_get_pixel(pt, src, srcx + index,
                                          srcy + jndex);
                painter_set_pixel(pt, dst, x + index, y + jndex,
                                  pixel, src->format);
            }
        }
    }
    return PT_ERROR_NONE;
}

/*****************************************************************************/
/* Bresenham's line drawing algorithm */
int
painter_line(void *handle, struct painter_bitmap *dst,
             int x1, int y1, int x2, int y2, int width, int flags)
{
    int dx;
    int dy;
    int incx;
    int incy;
    int dpr;
    int dpru;
    int p;
    struct painter *pt;

    pt = (struct painter *) handle;
    if (x1 > x2)
    {
        dx = x1 - x2;
        incx = -1;
    }
    else
    {
        dx = x2 - x1;
        incx = 1;
    }
    if (y1 > y2)
    {
        dy = y1 - y2;
        incy = -1;
    }
    else
    {
        dy = y2 - y1;
        incy = 1;
    }
    if (dx >= dy)
    {
        dpr = dy << 1;
        dpru = dpr - (dx << 1);
        p = dpr - dx;
        for (; dx >= 0; dx--)
        {
            if (x1 != x2 || y1 != y2)
            {
                painter_set_pixel(pt, dst, x1, y1, pt->fgcolor, dst->format);
            }
            if (p > 0)
            {
                x1 += incx;
                y1 += incy;
                p += dpru;
            }
            else
            {
                x1 += incx;
                p += dpr;
            }
        }
    }
    else
    {
        dpr = dx << 1;
        dpru = dpr - (dy << 1);
        p = dpr - dy;
        for (; dy >= 0; dy--)
        {
            if (x1 != x2 || y1 != y2)
            {
                painter_set_pixel(pt, dst, x1, y1, pt->fgcolor, dst->format);
            }
            if (p > 0)
            {
                x1 += incx;
                y1 += incy;
                p += dpru;
            }
            else
            {
                y1 += incy;
                p += dpr;
            }
        }
    }
    return PT_ERROR_NONE;
}

/*****************************************************************************/
int
painter_get_version(int *major, int *minor, int *micro)
{
    *major = LIBPAINTER_VERSION_MAJOR;
    *minor = LIBPAINTER_VERSION_MINOR;
    *micro = LIBPAINTER_VERSION_MICRO;
    return 0;
}

