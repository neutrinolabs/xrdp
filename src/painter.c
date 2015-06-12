/**
 * painter main
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
int
painter_create(void **handle)
{
    struct painter *pt;

    if (handle == NULL)
    {
        return PT_ERROR_PARAM;
    }
    *handle = malloc(sizeof(struct painter));
    if (*hanlde == NULL)
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
    pt->fgcolor = fgcolor;
    return PT_ERROR_NONE;
}

/*****************************************************************************/
int
painter_set_bgcolor(void *handle, int color)
{
    struct painter *pt;

    pt = (struct painter *) handle;
    pt->bgcolor = bgcolor;
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
painter_set_fill_mode(void *handle, int mode)
{
    struct painter *pt;

    pt = (struct painter *) handle;
    pt->fill_mode = fill_mode;
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
    return PT_ERROR_NONE;
}

/*****************************************************************************/
int
painter_fill_pattern(void *handle, struct painter_bitmap *dst,
                     struct painter_bitmap *pat, int patx, int paty,
                     int x, int y, int cx, int cy)
{
    return PT_ERROR_NONE;
}

/*****************************************************************************/
int
painter_copy(void *handle, struct painter_bitmap *dst,
             int x, int y, int cx, int cy,
             struct painter_bitmap *src, int srcx, int srcy)
{
    return PT_ERROR_NONE;
}

/*****************************************************************************/
int
painter_line(void *handle, struct painter_bitmap *dst,
             int x1, int y1, int x2, int y2, int width, int flags)
{
    return PT_ERROR_NONE;
}

