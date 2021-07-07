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
 * Load xrdp_bitmap from file
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "xrdp.h"
#include "log.h"

/**************************************************************************//**
 * Private routine to swap pixel data between two pixmaps
 * @param a First bitmap
 * @param b Second bitmap
 *
 * The main use-case for this routine is to modify an existing bitmap using
 * the following logic:-
 * - Create a temporary WND_TYPE_BITMAP
 * - Process the data in a bitmap in some way, moving it to the temporary
 * - Call this routine
 * - Delete the temporary
 *
 */
static void
swap_pixel_data(struct xrdp_bitmap *a, struct xrdp_bitmap *b)
{
    int tmp_width = a->width;
    int tmp_height = a->height;
    int tmp_bpp = a->bpp;
    int tmp_line_size = a->line_size;
    char *tmp_data = a->data;
    int tmp_do_not_free_data = a->do_not_free_data;

    a->width = b->width;
    a->height = b->height;
    a->bpp = b->bpp;
    a->line_size = b->line_size;
    a->data = b->data;
    a->do_not_free_data = b->do_not_free_data;

    b->width = tmp_width;
    b->height = tmp_height;
    b->bpp = tmp_bpp;
    b->line_size = tmp_line_size;
    b->data = tmp_data;
    b->do_not_free_data = tmp_do_not_free_data;
}

/**************************************************************************//**
 * Scales a bitmap image
 *
 * @param self Bitmap to scale
 * @param target_width target width
 * @param target_height target height
 * @return 0 for success
 */
static int
xrdp_bitmap_scale(struct xrdp_bitmap *self, int targ_width, int targ_height)
{
    int src_width = self->width;
    int src_height = self->height;

    if (src_width != targ_width || src_height != targ_height)
    {
        struct xrdp_bitmap *target =
            xrdp_bitmap_create(targ_width, targ_height,
                               self->bpp, WND_TYPE_BITMAP, 0);
        int targ_x, targ_y;

        if (target == NULL)
        {
            /* Error is logged */
            return 1;
        }

        /* For each pixel in the target pixmap, scale to one in the source */
        for (targ_x = 0 ; targ_x < targ_width; ++targ_x)
        {
            int src_x = targ_x * src_width / targ_width;
            for (targ_y = 0 ; targ_y < targ_height; ++targ_y)
            {
                int src_y = targ_y * src_height / targ_height;
                int pixel = xrdp_bitmap_get_pixel(self, src_x, src_y);
                xrdp_bitmap_set_pixel(target, targ_x, targ_y, pixel);
            }
        }
        swap_pixel_data(self, target);
        xrdp_bitmap_delete(target);
    }

    return 0;
}

/**************************************************************************//**
 * Zooms a bitmap image
 *
 * @param self Bitmap to zoom
 * @param target_width target width
 * @param target_height target height
 * @return 0 for success
 *
 * This works the same way as a scaled image, but the aspect ratio is
 * maintained by removing pixels from the top-and-bottom,
 * or the left-and-right before scaling.
 */
static int
xrdp_bitmap_zoom(struct xrdp_bitmap *self, int targ_width, int targ_height)
{
    int src_width = self->width;
    int src_height = self->height;
    double targ_ratio = (double)targ_width / targ_height;
    double src_ratio = (double)src_width / src_height;

    unsigned int chop_width;
    unsigned int chop_left_margin;
    unsigned int chop_height;
    unsigned int chop_top_margin;

    int result = 0;

    if (src_ratio > targ_ratio)
    {
        /* Source is relatively wider than source. Select a box
         * narrower than the source, but the same height */
        chop_width = (int)(targ_ratio * src_height + .5);
        chop_left_margin = (src_width - chop_width) / 2;
        chop_height = src_height;
        chop_top_margin = 0;
    }
    else
    {
        /* Source is relatively taller than source (or same shape) */
        chop_width = src_width;
        chop_left_margin = 0;
        chop_height = (int)(src_width / targ_ratio + .5);
        chop_top_margin = (src_height - chop_height) / 2;
    }

    /* Only chop the image if there's a need to */
    if (chop_top_margin != 0 || chop_left_margin != 0)
    {
        struct xrdp_bitmap *chopbox;
        chopbox = xrdp_bitmap_create(chop_width, chop_height, self->bpp,
                                     WND_TYPE_BITMAP, 0);
        if (chopbox == NULL)
        {
            LOG(LOG_LEVEL_ERROR, "xrdp_bitmap_zoom: no memory");
            result = 1;
        }
        else
        {
            result = xrdp_bitmap_copy_box(self, chopbox,
                                          chop_left_margin, chop_top_margin,
                                          chop_width, chop_height);
            if (result != 0)
            {
                LOG(LOG_LEVEL_ERROR, "xrdp_bitmap_zoom: can't copy box");
            }
            else
            {
                swap_pixel_data(self, chopbox);
            }
            xrdp_bitmap_delete(chopbox);
        }
    }

    if (result == 0)
    {
        result = xrdp_bitmap_scale(self, targ_width, targ_height);
    }

    return result;
}

/*****************************************************************************/
static int
xrdp_bitmap_get_index(struct xrdp_bitmap *self, const int *palette, int color)
{
    int r = 0;
    int g = 0;
    int b = 0;

    r = (color & 0xff0000) >> 16;
    g = (color & 0x00ff00) >> 8;
    b = (color & 0x0000ff) >> 0;
    r = (r >> 5) << 0;
    g = (g >> 5) << 3;
    b = (b >> 6) << 6;
    return (b | g | r);
}

/*****************************************************************************/
/* load a bmp file */
/* return 0 ok */
/* return 1 error */
static int
xrdp_bitmap_load_bmp(struct xrdp_bitmap *self, const char *filename,
                     const int *palette)
{
    int fd = 0;
    int len = 0;
    int i = 0;
    int j = 0;
    int k = 0;
    int color = 0;
    int size = 0;
    int palette1[256];
    char type1[4];
    struct xrdp_bmp_header header;
    struct stream *s = (struct stream *)NULL;

    g_memset(palette1, 0, sizeof(int) * 256);
    g_memset(type1, 0, sizeof(char) * 4);
    g_memset(&header, 0, sizeof(struct xrdp_bmp_header));

    if (!g_file_exist(filename))
    {
        LOG(LOG_LEVEL_ERROR, "%s: error bitmap file [%s] does not exist",
            __func__, filename);
        return 1;
    }

    fd = g_file_open(filename);

    if (fd == -1)
    {
        LOG(LOG_LEVEL_ERROR, "%s: error loading bitmap from file [%s]",
            __func__, filename);
        return 1;
    }

    /* read file type */
    if (g_file_read(fd, type1, 2) != 2)
    {
        LOG(LOG_LEVEL_ERROR, "%s: error bitmap file [%s] read error",
            __func__, filename);
        g_file_close(fd);
        return 1;
    }

    if ((type1[0] != 'B') || (type1[1] != 'M'))
    {
        LOG(LOG_LEVEL_ERROR, "%s: error bitmap file [%s] not BMP file",
            __func__, filename);
        g_file_close(fd);
        return 1;
    }

    /* read file size */
    make_stream(s);
    init_stream(s, 8192);
    if (g_file_read(fd, s->data, 4) != 4)
    {
        LOG(LOG_LEVEL_ERROR, "%s: missing length in file %s",
            __func__, filename);
        free_stream(s);
        g_file_close(fd);
        return 1;
    }
    s->end = s->data + 4;
    in_uint32_le(s, size);
    /* read bmp header */
    if (g_file_seek(fd, 14) < 0)
    {
        LOG(LOG_LEVEL_ERROR, "%s: seek error in file %s",
            __func__, filename);
        free_stream(s);
        g_file_close(fd);
        return 1;
    }
    init_stream(s, 8192);
    len = g_file_read(fd, s->data, 40); /* size better be 40 */
    if (len != 40)
    {
        LOG(LOG_LEVEL_ERROR, "%s: unexpected read length %d in file %s",
            __func__, len, filename);
        free_stream(s);
        g_file_close(fd);
        return 1;
    }
    s->end = s->data + len;
    in_uint32_le(s, header.size);
    in_uint32_le(s, header.image_width);
    in_uint32_le(s, header.image_height);
    in_uint16_le(s, header.planes);
    in_uint16_le(s, header.bit_count);
    in_uint32_le(s, header.compression);
    in_uint32_le(s, header.image_size);
    in_uint32_le(s, header.x_pels_per_meter);
    in_uint32_le(s, header.y_pels_per_meter);
    in_uint32_le(s, header.clr_used);
    in_uint32_le(s, header.clr_important);

    if ((header.bit_count != 4) && (header.bit_count != 8) &&
            (header.bit_count != 24))
    {
        LOG(LOG_LEVEL_ERROR, "%s: error bitmap file [%s] bad bpp %d",
            __func__, filename, header.bit_count);
        free_stream(s);
        g_file_close(fd);
        return 1;
    }

    if (header.bit_count == 24) /* 24 bit bitmap */
    {
        if (g_file_seek(fd, 14 + header.size) < 0)
        {
            LOG(LOG_LEVEL_WARNING, "%s: seek error in file %s",
                __func__, filename);
        }
        xrdp_bitmap_resize(self, header.image_width, header.image_height);
        size = header.image_width * header.image_height * 3;
        init_stream(s, size);
        /* Pre-fill the buffer, so if we get short reads we're
         * not working with uninitialised data */
        g_memset(s->data, 0, size);
        s->end = s->data + size;

        /* read data */
        for (i = header.image_height - 1; i >= 0; i--)
        {
            size = header.image_width * 3;
            k = g_file_read(fd, s->data + i * size, size);

            if (k != size)
            {
                LOG(LOG_LEVEL_WARNING, "%s: error bitmap file [%s] read",
                    __func__, filename);
            }
        }

        for (i = 0; i < self->height; i++)
        {
            for (j = 0; j < self->width; j++)
            {
                in_uint8(s, k);
                color = k;
                in_uint8(s, k);
                color |= k << 8;
                in_uint8(s, k);
                color |= k << 16;

                if (self->bpp == 8)
                {
                    color = xrdp_bitmap_get_index(self, palette, color);
                }
                else if (self->bpp == 15)
                {
                    color = COLOR15((color & 0xff0000) >> 16,
                                    (color & 0x00ff00) >> 8,
                                    (color & 0x0000ff) >> 0);
                }
                else if (self->bpp == 16)
                {
                    color = COLOR16((color & 0xff0000) >> 16,
                                    (color & 0x00ff00) >> 8,
                                    (color & 0x0000ff) >> 0);
                }

                xrdp_bitmap_set_pixel(self, j, i, color);
            }
        }
    }
    else if (header.bit_count == 8) /* 8 bit bitmap */
    {
        /* read palette */
        if (g_file_seek(fd, 14 + header.size) < 0)
        {
            LOG(LOG_LEVEL_WARNING, "%s: seek error in file %s",
                __func__, filename);
        }
        size = header.clr_used * sizeof(int);

        init_stream(s, size);
        /* Pre-fill the buffer, so if we get short reads we're
         * not working with uninitialised data */
        g_memset(s->data, 0, size);
        s->end = s->data + size;

        len = g_file_read(fd, s->data, size);
        if (len != size)
        {
            LOG(LOG_LEVEL_ERROR, "%s: unexpected read length in file %s",
                __func__, filename);
        }

        for (i = 0; i < header.clr_used; i++)
        {
            in_uint32_le(s, palette1[i]);
        }

        xrdp_bitmap_resize(self, header.image_width, header.image_height);
        size = header.image_width * header.image_height;
        init_stream(s, size);
        /* Pre-fill the buffer, so if we get short reads we're
         * not working with uninitialised data */
        g_memset(s->data, 0, size);
        s->end = s->data + size;

        /* read data */
        for (i = header.image_height - 1; i >= 0; i--)
        {
            size = header.image_width;
            k = g_file_read(fd, s->data + i * size, size);

            if (k != size)
            {
                LOG(LOG_LEVEL_WARNING, "%s: error bitmap file [%s] read",
                    __func__, filename);
            }
        }

        for (i = 0; i < self->height; i++)
        {
            for (j = 0; j < self->width; j++)
            {
                in_uint8(s, k);
                color = palette1[k];

                if (self->bpp == 8)
                {
                    color = xrdp_bitmap_get_index(self, palette, color);
                }
                else if (self->bpp == 15)
                {
                    color = COLOR15((color & 0xff0000) >> 16,
                                    (color & 0x00ff00) >> 8,
                                    (color & 0x0000ff) >> 0);
                }
                else if (self->bpp == 16)
                {
                    color = COLOR16((color & 0xff0000) >> 16,
                                    (color & 0x00ff00) >> 8,
                                    (color & 0x0000ff) >> 0);
                }

                xrdp_bitmap_set_pixel(self, j, i, color);
            }
        }
    }
    else if (header.bit_count == 4) /* 4 bit bitmap */
    {
        /* read palette */
        if (g_file_seek(fd, 14 + header.size) < 0)
        {
            LOG(LOG_LEVEL_WARNING, "%s: seek error in file %s",
                __func__, filename);
        }
        size = header.clr_used * sizeof(int);

        init_stream(s, size);
        /* Pre-fill the buffer, so if we get short reads we're
         * not working with uninitialised data */
        g_memset(s->data, 0, size);
        s->end = s->data + size;

        len = g_file_read(fd, s->data, size);
        if (len != size)
        {
            LOG(LOG_LEVEL_ERROR, "%s: unexpected read length in file %s",
                __func__, filename);
        }

        for (i = 0; i < header.clr_used; i++)
        {
            in_uint32_le(s, palette1[i]);
        }

        xrdp_bitmap_resize(self, header.image_width, header.image_height);
        size = (header.image_width * header.image_height) / 2;
        init_stream(s, size);
        /* Pre-fill the buffer, so if we get short reads we're
         * not working with uninitialised data */
        g_memset(s->data, 0, size);
        s->end = s->data + size;

        /* read data */
        for (i = header.image_height - 1; i >= 0; i--)
        {
            size = header.image_width / 2;
            k = g_file_read(fd, s->data + i * size, size);

            if (k != size)
            {
                LOG(LOG_LEVEL_WARNING, "%s: error bitmap file [%s] read",
                    __func__, filename);
            }
        }

        for (i = 0; i < self->height; i++)
        {
            for (j = 0; j < self->width; j++)
            {
                if ((j & 1) == 0)
                {
                    in_uint8(s, k);
                    color = (k >> 4) & 0xf;
                }
                else
                {
                    color = k & 0xf;
                }

                color = palette1[color];

                if (self->bpp == 8)
                {
                    color = xrdp_bitmap_get_index(self, palette, color);
                }
                else if (self->bpp == 15)
                {
                    color = COLOR15((color & 0xff0000) >> 16,
                                    (color & 0x00ff00) >> 8,
                                    (color & 0x0000ff) >> 0);
                }
                else if (self->bpp == 16)
                {
                    color = COLOR16((color & 0xff0000) >> 16,
                                    (color & 0x00ff00) >> 8,
                                    (color & 0x0000ff) >> 0);
                }

                xrdp_bitmap_set_pixel(self, j, i, color);
            }
        }
    }

    g_file_close(fd);
    free_stream(s);

    return 0;
}

/*****************************************************************************/
int
xrdp_bitmap_load(struct xrdp_bitmap *self, const char *filename,
                 const int *palette,
                 int background,
                 enum xrdp_bitmap_load_transform transform,
                 int twidth,
                 int theight)
{
    /* this is the default bmp-only implementation if a graphics library
     * isn't built in */

    int result = xrdp_bitmap_load_bmp(self, filename, palette);
    if (result == 0)
    {
        switch (transform)
        {
            case XBLT_NONE:
                break;

            case XBLT_SCALE:
                result = xrdp_bitmap_scale(self, twidth, theight);
                break;

            case XBLT_ZOOM:
                result = xrdp_bitmap_zoom(self, twidth, theight);
                break;

            default:
                LOG(LOG_LEVEL_WARNING, "Invalid bitmap transform %d specified",
                    transform);
        }
    }
    return result;
}
