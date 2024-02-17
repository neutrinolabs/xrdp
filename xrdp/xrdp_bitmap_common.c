/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2014
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
 * Common bitmap functions for all xrdp_bitmap*.c files
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <limits.h>

#include "xrdp.h"

/*****************************************************************************/
/* Allocate bitmap for specified dimensions, checking for int overflow */
static char *
alloc_bitmap_data(int width, int height, int Bpp)
{
    char *result = NULL;
    if (width > 0 && height > 0 && Bpp > 0)
    {
        int len = width;
        /* g_malloc() currently takes an 'int' size */
        if (len < INT_MAX / height)
        {
            len *= height;
            if (len < INT_MAX / Bpp)
            {
                len *= Bpp;
                result = (char *)malloc(len);
            }
        }
    }

    return result;
}

/*****************************************************************************/
struct xrdp_bitmap *
xrdp_bitmap_create(int width, int height, int bpp,
                   int type, struct xrdp_wm *wm)
{
    struct xrdp_bitmap *self = (struct xrdp_bitmap *)NULL;
    int Bpp = 0;

    self = (struct xrdp_bitmap *)g_malloc(sizeof(struct xrdp_bitmap), 1);
    if (self == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_bitmap_create: no memory");
        return self;
    }

    self->type = type;
    self->width = width;
    self->height = height;
    self->bpp = bpp;
    Bpp = 4;

    switch (bpp)
    {
        case 8:
            Bpp = 1;
            break;
        case 15:
            Bpp = 2;
            break;
        case 16:
            Bpp = 2;
            break;
    }

    if (self->type == WND_TYPE_BITMAP || self->type == WND_TYPE_IMAGE)
    {
        self->data = alloc_bitmap_data(width, height, Bpp);
        if (self->data == NULL)
        {
            LOG(LOG_LEVEL_ERROR, "xrdp_bitmap_create: size overflow %dx%dx%d",
                width, height, Bpp);
            g_free(self);
            return NULL;
        }
    }

#if defined(XRDP_PAINTER)
    if (self->type == WND_TYPE_SCREEN) /* noorders */
    {
        LOG_DEVEL(LOG_LEVEL_INFO, "xrdp_bitmap_create: noorders");
        self->data = alloc_bitmap_data(width, height, Bpp);
        if (self->data == NULL)
        {
            LOG(LOG_LEVEL_ERROR, "xrdp_bitmap_create: size overflow %dx%dx%d",
                width, height, Bpp);
            g_free(self);
            return NULL;
        }
    }
#endif

    if (self->type != WND_TYPE_BITMAP)
    {
        self->child_list = list_create();
    }

    self->line_size = width * Bpp;

    if (self->type == WND_TYPE_COMBO)
    {
        self->string_list = list_create();
        self->string_list->auto_free = 1;
        self->data_list = list_create();
        self->data_list->auto_free = 1;
    }

    self->wm = wm;
    return self;
}

/*****************************************************************************/
struct xrdp_bitmap *
xrdp_bitmap_create_with_data(int width, int height,
                             int bpp, char *data,
                             struct xrdp_wm *wm)
{
    struct xrdp_bitmap *self = (struct xrdp_bitmap *)NULL;
    int Bpp;
#if defined(NEED_ALIGN)
    tintptr data_as_int;
#endif

    self = (struct xrdp_bitmap *)g_malloc(sizeof(struct xrdp_bitmap), 1);
    self->type = WND_TYPE_BITMAP;
    self->width = width;
    self->height = height;
    self->bpp = bpp;
    self->wm = wm;

    Bpp = 4;
    switch (bpp)
    {
        case 8:
            Bpp = 1;
            break;
        case 15:
            Bpp = 2;
            break;
        case 16:
            Bpp = 2;
            break;
    }
    self->line_size = width * Bpp;

#if defined(NEED_ALIGN)
    data_as_int = (tintptr) data;
    if (((bpp >= 24) && (data_as_int & 3)) ||
            (((bpp == 15) || (bpp == 16)) && (data_as_int & 1)))
    {
        /* got to copy data here, it's not aligned
           other calls in this file assume alignment */
        self->data = (char *)g_malloc(width * height * Bpp, 0);
        g_memcpy(self->data, data, width * height * Bpp);
        return self;
    }
#endif
    self->data = data;
    self->do_not_free_data = 1;
    return self;
}

/*****************************************************************************/
void
xrdp_bitmap_delete(struct xrdp_bitmap *self)
{
    int i = 0;
    struct xrdp_mod_data *mod_data = (struct xrdp_mod_data *)NULL;

    if (self == 0)
    {
        return;
    }

    if (self->wm != 0)
    {
        if (self->wm->focused_window != 0)
        {
            if (self->wm->focused_window->focused_control == self)
            {
                self->wm->focused_window->focused_control = 0;
            }
        }

        if (self->wm->focused_window == self)
        {
            self->wm->focused_window = 0;
        }

        if (self->wm->dragging_window == self)
        {
            self->wm->dragging_window = 0;
        }

        if (self->wm->button_down == self)
        {
            self->wm->button_down = 0;
        }

        if (self->wm->popup_wnd == self)
        {
            self->wm->popup_wnd = 0;
        }

        if (self->wm->login_window == self)
        {
            self->wm->login_window = 0;
        }

        if (self->wm->log_wnd == self)
        {
            self->wm->log_wnd = 0;
        }
    }

    if (self->child_list != 0)
    {
        for (i = self->child_list->count - 1; i >= 0; i--)
        {
            xrdp_bitmap_delete((struct xrdp_bitmap *)self->child_list->items[i]);
        }

        list_delete(self->child_list);
    }

    if (self->parent != 0)
    {
        i = list_index_of(self->parent->child_list, (long)self);

        if (i >= 0)
        {
            list_remove_item(self->parent->child_list, i);
        }
    }

    if (self->string_list != 0) /* for combo */
    {
        list_delete(self->string_list);
    }

    if (self->data_list != 0) /* for combo */
    {
        for (i = 0; i < self->data_list->count; i++)
        {
            mod_data = (struct xrdp_mod_data *)list_get_item(self->data_list, i);

            if (mod_data != 0)
            {
                list_delete(mod_data->names);
                list_delete(mod_data->values);
            }
        }

        list_delete(self->data_list);
    }

    if (!self->do_not_free_data)
    {
        g_free(self->data);
    }

    g_free(self->caption1);
    g_free(self);
}

/*****************************************************************************/
/* returns error */
int
xrdp_bitmap_resize(struct xrdp_bitmap *self, int width, int height)
{
    int Bpp = 0;

    if ((width == self->width) && (height == self->height))
    {
        return 0;
    }

    if (self->do_not_free_data)
    {
        return 1;
    }

    Bpp = 4;

    switch (self->bpp)
    {
        case 8:
            Bpp = 1;
            break;
        case 15:
            Bpp = 2;
            break;
        case 16:
            Bpp = 2;
            break;
    }

    /* To prevent valgrind errors (particularly on a screen resize),
       clear extra memory */
    unsigned long old_size = self->width * self->height * Bpp;
    unsigned long new_size = width * height * Bpp;

    char *new_data = (char *)realloc(self->data, new_size);
    if (new_data == NULL)
    {
        return 1;
    }

    self->width = width;
    self->height = height;
    if (new_data != self->data)
    {
        self->data = new_data;
        memset(self->data, 0, new_size);
    }
    else if (new_size > old_size)
    {
        memset(self->data + old_size, 0, new_size - old_size);
    }
    self->line_size = width * Bpp;
    return 0;
}

/*****************************************************************************/
int
xrdp_bitmap_get_pixel(struct xrdp_bitmap *self, int x, int y)
{
    if (self == 0)
    {
        return 0;
    }

    if (self->data == 0)
    {
        return 0;
    }

    if (x >= 0 && x < self->width && y >= 0 && y < self->height)
    {
        if (self->bpp == 8)
        {
            return GETPIXEL8(self->data, x, y, self->width);
        }
        else if (self->bpp == 15 || self->bpp == 16)
        {
            return GETPIXEL16(self->data, x, y, self->width);
        }
        else if (self->bpp >= 24)
        {
            return GETPIXEL32(self->data, x, y, self->width);
        }
    }

    return 0;
}

/*****************************************************************************/
int
xrdp_bitmap_set_pixel(struct xrdp_bitmap *self, int x, int y, int pixel)
{
    if (self == 0)
    {
        return 0;
    }

    if (self->data == 0)
    {
        return 0;
    }

    if (x >= 0 && x < self->width && y >= 0 && y < self->height)
    {
        if (self->bpp == 8)
        {
            SETPIXEL8(self->data, x, y, self->width, pixel);
        }
        else if (self->bpp == 15 || self->bpp == 16)
        {
            SETPIXEL16(self->data, x, y, self->width, pixel);
        }
        else if (self->bpp >= 24)
        {
            SETPIXEL32(self->data, x, y, self->width, pixel);
        }
    }

    return 0;
}

/*****************************************************************************/
/* copy part of self at x, y to 0, 0 in dest */
/* returns error */
int
xrdp_bitmap_copy_box(struct xrdp_bitmap *self,
                     struct xrdp_bitmap *dest,
                     int x, int y, int cx, int cy)
{
    int i;
    int destx;
    int desty;
    int incs;
    int incd;
    tui8 *s8;
    tui8 *d8;
    tui16 *s16;
    tui16 *d16;
    tui32 *s32;
    tui32 *d32;

    if (self == 0)
    {
        return 1;
    }

    if (dest == 0)
    {
        return 1;
    }

    if (self->type != WND_TYPE_BITMAP && self->type != WND_TYPE_IMAGE)
    {
        return 1;
    }

    if (dest->type != WND_TYPE_BITMAP && dest->type != WND_TYPE_IMAGE)
    {
        return 1;
    }

    if (self->bpp != dest->bpp)
    {
        return 1;
    }

    destx = 0;
    desty = 0;

    if (!check_bounds(self, &x, &y, &cx, &cy))
    {
        return 1;
    }

    if (!check_bounds(dest, &destx, &desty, &cx, &cy))
    {
        return 1;
    }

    if (self->bpp >= 24)
    {
        s32 = ((tui32 *)(self->data)) + (self->width * y + x);
        d32 = ((tui32 *)(dest->data)) + (dest->width * desty + destx);
        incs = self->width - cx;
        incd = dest->width - cx;

        for (i = 0; i < cy; i++)
        {
            g_memcpy(d32, s32, cx * 4);
            s32 += cx;
            d32 += cx;

            s32 += incs;
            d32 += incd;
        }

    }
    else if (self->bpp == 15 || self->bpp == 16)
    {
        s16 = ((tui16 *)(self->data)) + (self->width * y + x);
        d16 = ((tui16 *)(dest->data)) + (dest->width * desty + destx);
        incs = self->width - cx;
        incd = dest->width - cx;

        for (i = 0; i < cy; i++)
        {
            g_memcpy(d16, s16, cx * 2);
            s16 += cx;
            d16 += cx;

            s16 += incs;
            d16 += incd;
        }
    }
    else if (self->bpp == 8)
    {
        s8 = ((tui8 *)(self->data)) + (self->width * y + x);
        d8 = ((tui8 *)(dest->data)) + (dest->width * desty + destx);
        incs = self->width - cx;
        incd = dest->width - cx;

        for (i = 0; i < cy; i++)
        {
            g_memcpy(d8, s8, cx);
            s8 += cx;
            d8 += cx;

            s8 += incs;
            d8 += incd;
        }
    }
    else
    {
        return 1;
    }

    return 0;
}
