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

#ifdef USE_IMLIB2
#   include <Imlib2.h>
#endif

#include "xrdp.h"
#include "log.h"

/* Rounds up to the nearest multiple of 4 */
#define ROUND4(x) (((x) + 3) / 4 * 4)

/* Are we using the builtin BMP format-only loader */

#ifdef USE_BUILTIN_LOADER
#   undef USE_BUILTIN_LOADER
#endif

#ifndef USE_IMLIB2
#   define USE_BUILTIN_LOADER
#endif

/**
 * Describes a box within an image
 */
struct box
{
    int left_margin;
    int width;
    int top_margin;
    int height;
};

/**************************************************************************//**
 * Calculates a zoom box, from source and destination image dimensions
 *
 * The zoom box is the largest centred part of the source image which
 * preserves the aspect ratio of the destination image. We find it
 * by cutting off the left and right sides of the source, or the top
 * and bottom.
 *
 * @param src_width    Width of source image
 * @param src_height   Height of source image
 * @param dst_width    Width of destination image
 * @param dst_height   Height of destination image
 * @param[out] zb_return Zoom box
 * @return 0 for success
 */
static int
calculate_zoom_box(int src_width, int src_height,
                   int dst_width, int dst_height,
                   struct box *zb_return)
{
    int result = 1;
    struct box zb;

    if (dst_height == 0 || src_height == 0)
    {
        LOG(LOG_LEVEL_ERROR, "Can't zoom to or from zero-width images");
    }
    else
    {
        double dst_ratio = (double)dst_width / dst_height;
        double src_ratio = (double)src_width / src_height;

        if (src_ratio > dst_ratio)
        {
            /* Source is relatively wider than source. Select a box
             * narrower than the source, but the same height */
            zb.width = (int)(dst_ratio * src_height + .5);
            zb.left_margin = (src_width - zb.width) / 2;
            zb.height = src_height;
            zb.top_margin = 0;
        }
        else
        {
            /* Source is relatively taller than source (or same shape) */
            zb.width = src_width;
            zb.left_margin = 0;
            zb.height = (int)(src_width / dst_ratio + .5);
            zb.top_margin = (src_height - zb.height) / 2;
        }

        /* Only allow meaningful zoom boxes */
        if (zb.width < 1 || zb.height < 1)
        {
            LOG(LOG_LEVEL_WARNING, "Ignoring pathological zoom"
                " request (%dx%d) -> (%dx%d)", src_width, src_height,
                dst_width, dst_height);
        }
        else
        {
            *zb_return = zb;
            result = 0;
        }
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

#ifdef USE_BUILTIN_LOADER
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
#endif /* USE_BUILTIN_LOADER */

#ifdef USE_BUILTIN_LOADER
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
#endif /* USE_BUILTIN_LOADER */


#ifdef USE_BUILTIN_LOADER
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
    struct box zb;

    int result = 0;

    if (calculate_zoom_box(self->width, self->height,
                           targ_width, targ_height, &zb) == 0)
    {
        /* Need to chop anything? */
        if (zb.top_margin != 0 || zb.left_margin != 0)
        {
            struct xrdp_bitmap *zbitmap;
            zbitmap = xrdp_bitmap_create(zb.width, zb.height, self->bpp,
                                         WND_TYPE_BITMAP, 0);
            if (zbitmap == NULL)
            {
                LOG(LOG_LEVEL_ERROR, "xrdp_bitmap_zoom: no memory");
                result = 1;
            }
            else
            {
                result = xrdp_bitmap_copy_box(self, zbitmap,
                                              zb.left_margin, zb.top_margin,
                                              zb.width, zb.height);
                if (result != 0)
                {
                    LOG(LOG_LEVEL_ERROR, "xrdp_bitmap_zoom: can't copy box");
                }
                else
                {
                    swap_pixel_data(self, zbitmap);
                }
                xrdp_bitmap_delete(zbitmap);
            }
        }
    }

    if (result == 0)
    {
        result = xrdp_bitmap_scale(self, targ_width, targ_height);
    }

    return result;
}
#endif /* USE_BUILTIN_LOADER */

#ifdef USE_BUILTIN_LOADER
/**************************************************************************//**
 * reads the palette from a bmp file with a palette embedded in it
 *
 * @pre The read position in the file is at the end of the bmp DIB header.
 *
 * @param filename  Name of file
 * @param fd   File descriptor for file
 * @param header Pointer to BMP header info struct
 * @param palette output. Must be at least 256 elements
 */
static void
read_palette(const char *filename, int fd,
             const struct xrdp_bmp_header *header, int *palette)
{
    struct stream *s;
    int clr_used;
    int palette_size;
    int len;
    int i;

    /* Find the number of colors used in the bitmap */
    if (header->clr_used != 0)
    {
        clr_used = header->clr_used;
        /* Is the header value sane? */
        if (clr_used < 0 || clr_used > 256)
        {
            clr_used = 256;
            LOG(LOG_LEVEL_WARNING, "%s : Invalid palette size %d in file %s",
                __func__, header->clr_used, filename);
        }
    }
    else if (header->bit_count == 4)
    {
        clr_used = 16;
    }
    else
    {
        clr_used = 256;
    }

    palette_size = clr_used * 4;

    make_stream(s);
    init_stream(s, palette_size);

    /* Pre-fill the buffer, so if we get short reads we're
     * not working with uninitialised data */
    g_memset(s->data, 0, palette_size);
    s->end = s->data + palette_size;

    len = g_file_read(fd, s->data, palette_size);
    if (len != palette_size)
    {
        LOG(LOG_LEVEL_WARNING, "%s: unexpected read length in file %s",
            __func__, filename);
    }

    for (i = 0; i < clr_used; ++i)
    {
        in_uint32_le(s, palette[i]);
    }

    free_stream(s);
}
#endif /* USE_BUILTIN_LOADER */

#ifdef USE_BUILTIN_LOADER
/**************************************************************************//**
 * Process a row of data from a 24-bit bmp file
 *
 * @param self Bitmap we're filling in
 * @param s Stream containing bitmap data
 * @param in_palette Palette from bmp file (unused)
 * @param out_palette Palette for output bitmap
 * @param row Row number
 */
static void
process_row_data_24bit(struct xrdp_bitmap *self,
                       struct stream *s,
                       const int *in_palette,
                       const int *out_palette,
                       int row)
{
    int j;
    int k;
    int color;

    for (j = 0; j < self->width; ++j)
    {
        in_uint8(s, k);
        color = k;
        in_uint8(s, k);
        color |= k << 8;
        in_uint8(s, k);
        color |= k << 16;

        if (self->bpp == 8)
        {
            color = xrdp_bitmap_get_index(self, out_palette, color);
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

        xrdp_bitmap_set_pixel(self, j, row, color);
    }
}
#endif /* USE_BUILTIN_LOADER */

#ifdef USE_BUILTIN_LOADER
/**************************************************************************//**
 * Process a row of data from an 8-bit bmp file
 *
 * @param self Bitmap we're filling in
 * @param s Stream containing bitmap data
 * @param in_palette Palette from bmp file
 * @param out_palette Palette for output bitmap
 * @param row Row number
 */
static void
process_row_data_8bit(struct xrdp_bitmap *self,
                      struct stream *s,
                      const int *in_palette,
                      const int *out_palette,
                      int row)
{
    int j;
    int k;
    int color;

    for (j = 0; j < self->width; ++j)
    {
        in_uint8(s, k);
        color = in_palette[k];

        if (self->bpp == 8)
        {
            color = xrdp_bitmap_get_index(self, out_palette, color);
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

        xrdp_bitmap_set_pixel(self, j, row, color);
    }
}
#endif /* USE_BUILTIN_LOADER */

#ifdef USE_BUILTIN_LOADER
/**************************************************************************//**
 * Process a row of data from an 4-bit bmp file
 *
 * @param self Bitmap we're filling in
 * @param s Stream containing bitmap data
 * @param in_palette Palette from bmp file
 * @param out_palette Palette for output bitmap
 * @param row Row number
 */
static void
process_row_data_4bit(struct xrdp_bitmap *self,
                      struct stream *s,
                      const int *in_palette,
                      const int *out_palette,
                      int row)
{
    int j;
    int k = 0;
    int color;

    for (j = 0; j < self->width; ++j)
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

        color = in_palette[color];

        if (self->bpp == 8)
        {
            color = xrdp_bitmap_get_index(self, out_palette, color);
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

        xrdp_bitmap_set_pixel(self, j, row, color);
    }
}
#endif /* USE_BUILTIN_LOADER */

#ifdef USE_BUILTIN_LOADER
/*****************************************************************************/
/* load a bmp file */
/* return 0 ok */
/* return 1 error */
static int
xrdp_bitmap_load_bmp(struct xrdp_bitmap *self, const char *filename,
                     const int *out_palette)
{
    int fd = 0;
    int len = 0;
    int row = 0;
    int row_size = 0;
    int bmp_palette[256] = {0};
    char fixed_header[14] = {0};
    struct xrdp_bmp_header header = {0};
    struct stream *s = (struct stream *)NULL;
    /* Pointer to row data processing function */
    void (*process_row_data)(struct xrdp_bitmap * self,
                             struct stream * s,
                             const int *in_palette,
                             const int *out_palette,
                             int row);

    if (!g_file_exist(filename))
    {
        LOG(LOG_LEVEL_ERROR, "%s: error bitmap file [%s] does not exist",
            __func__, filename);
        return 1;
    }

    fd = g_file_open_ro(filename);

    if (fd == -1)
    {
        LOG(LOG_LEVEL_ERROR, "%s: error loading bitmap from file [%s]",
            __func__, filename);
        return 1;
    }

    /* read the fixed file header */
    if (g_file_read(fd, fixed_header, 14) != 14)
    {
        LOG(LOG_LEVEL_ERROR, "%s: error bitmap file [%s] read error",
            __func__, filename);
        g_file_close(fd);
        return 1;
    }

    if ((fixed_header[0] != 'B') || (fixed_header[1] != 'M'))
    {
        LOG(LOG_LEVEL_ERROR, "%s: error bitmap file [%s] not BMP file",
            __func__, filename);
        g_file_close(fd);
        return 1;
    }

    /* read information header */
    make_stream(s);
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

    if (header.compression != 0)
    {
        LOG(LOG_LEVEL_WARNING, "%s: error bitmap file [%s]"
            " unsupported compression value %d",
            __func__, filename, header.compression);
    }

    if (g_file_seek(fd, 14 + header.size) < 0)
    {
        LOG(LOG_LEVEL_WARNING, "%s: seek error in file %s",
            __func__, filename);
    }

    /* Validate the bit count for the file, read any palette, and
     * determine the row size and row processing function */
    switch (header.bit_count)
    {
        case 24:
            row_size = ROUND4(header.image_width * 3);
            process_row_data = process_row_data_24bit;
            break;

        case 8:
            read_palette(filename, fd, &header, bmp_palette);
            row_size = ROUND4(header.image_width);
            process_row_data = process_row_data_8bit;
            break;

        case 4:
            read_palette(filename, fd, &header, bmp_palette);
            /* The storage for a row is a complex calculation for 4 bit pixels.
             * a width of 1-8 pixels takes 4 bytes
             * a width of 9-16 pixels takes 8 bytes, etc
             * So bytes = (width + 7) / 8 * 4
             */
            row_size = ((header.image_width + 7) / 8 * 4);
            process_row_data = process_row_data_4bit;
            break;

        default:
            LOG(LOG_LEVEL_ERROR, "%s: error bitmap file [%s] bad bpp %d",
                __func__, filename, header.bit_count);
            free_stream(s);
            g_file_close(fd);
            return 1;
    }

    xrdp_bitmap_resize(self, header.image_width, header.image_height);

    /* Set up the row data buffer. Pre fill it, so if we get short reads
     * we're not working with uninitialised data */
    init_stream(s, row_size);
    g_memset(s->data, 0, row_size);
    s->end = s->data + row_size;

    /* read and process the pixel data a row at a time */
    for (row = header.image_height - 1; row >= 0; row--)
    {
        len = g_file_read(fd, s->data, row_size);

        if (len != row_size)
        {
            LOG(LOG_LEVEL_WARNING, "%s: error bitmap file [%s] read",
                __func__, filename);
        }

        s->p = s->data;
        (*process_row_data)(self, s, bmp_palette, out_palette, row);
    }

    g_file_close(fd);
    free_stream(s);

    return 0;
}
#endif /* USE_BUILTIN_LOADER */

#ifdef USE_BUILTIN_LOADER
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
#endif /* USE_BUILTIN_LOADER */

#ifdef USE_IMLIB2
/**************************************************************************//**
 * Log an error from the Imlib2 library
 *
 * @param level Log level to use
 * @param filename file we're trying to load
 * @param lerr Error return from imlib2
 */
static void
log_imlib2_error(enum logLevels level, const char *filename,
                 Imlib_Load_Error lerr)
{
    const char *msg;
    char buff[256];

    switch (lerr)
    {
        case IMLIB_LOAD_ERROR_NONE:
            msg = "No error";
            break;
        case IMLIB_LOAD_ERROR_FILE_DOES_NOT_EXIST:
            msg = "No such file";
            break;
        case IMLIB_LOAD_ERROR_PERMISSION_DENIED_TO_READ:
            msg = "Permission denied";
            break;
        case IMLIB_LOAD_ERROR_NO_LOADER_FOR_FILE_FORMAT:
            msg = "Unrecognised file format";
            break;
        case IMLIB_LOAD_ERROR_FILE_IS_DIRECTORY:
        case IMLIB_LOAD_ERROR_PATH_TOO_LONG:
        case IMLIB_LOAD_ERROR_PATH_COMPONENT_NON_EXISTANT:
        case IMLIB_LOAD_ERROR_PATH_POINTS_OUTSIDE_ADDRESS_SPACE:
        case IMLIB_LOAD_ERROR_TOO_MANY_SYMBOLIC_LINKS:
            msg = "Bad filename";
            break;
        case IMLIB_LOAD_ERROR_OUT_OF_MEMORY:
            msg = " No memory";
            break;
        case IMLIB_LOAD_ERROR_OUT_OF_FILE_DESCRIPTORS:
            msg = "No file descriptors";
            break;
        case IMLIB_LOAD_ERROR_OUT_OF_DISK_SPACE:
            msg = "No disk space";
            break;
        case IMLIB_LOAD_ERROR_UNKNOWN:
            msg = "Unknown error";
            break;
        default:
            g_snprintf(buff, sizeof(buff), "Unrecognised code %d", lerr);
            msg = buff;
    }

    LOG(LOG_LEVEL_ERROR, "Error loading %s [%s]", filename, msg);
}
#endif /* USE_IMLIB2 */

#ifdef USE_IMLIB2
/**************************************************************************//**
 * Blend an imlib2 image onto a background of the specified color
 *
 * The current context image is merged. On return the new image is the
 * current context image, and the old image is deleted.
 *
 * @param filename Filename we're working on (for error reporting)
 * @param r Background red
 * @param g Background green
 * @param g Background blue
 *
 * @return 0 for success. On failure the current context image is unchanged.
 */
static int
blend_imlib_image_onto_background(const char *filename, int r, int g, int b)
{
    int result = 0;
    Imlib_Image img = imlib_context_get_image();

    if (img == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "No context for blending image");
        result = 1;
    }
    else
    {
        int width = imlib_image_get_width();
        int height = imlib_image_get_height();

        /* Create a suitable image to merge this one onto */
        Imlib_Image bg = imlib_create_image(width, height);
        if (bg == NULL)
        {
            log_imlib2_error(LOG_LEVEL_ERROR, filename,
                             IMLIB_LOAD_ERROR_OUT_OF_MEMORY);
            result = 1;
        }
        else
        {
            imlib_context_set_image(bg);
            imlib_context_set_color(r, g, b, 0xff);
            imlib_image_fill_rectangle(0, 0, width, height);
            imlib_blend_image_onto_image(img, 0,
                                         0, 0, width, height,
                                         0, 0, width, height);
            imlib_context_set_image(img);
            imlib_free_image();
            imlib_context_set_image(bg);
        }
    }
    return result;
}
#endif /* USE_IMLIB2 */

#ifdef USE_IMLIB2
/**************************************************************************//**
 * Scales an imlib2 image
 *
 * The current context image is scaled. On return the new image is the
 * current context image, and the old image is deleted.
 *
 * @param filename Filename we're working on (for error reporting)
 * @param twidth target width
 * @param theight target height
 * @return 0 for success
 */
static int
scale_imlib_image(const char *filename, int twidth, int theight)
{
    int result = 0;
    Imlib_Image img = imlib_context_get_image();

    if (img == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "No context for scaling image");
        result = 1;
    }
    else
    {
        int width = imlib_image_get_width();
        int height = imlib_image_get_height();

        Imlib_Image newimg = imlib_create_cropped_scaled_image(
                                 0, 0, width, height, twidth, theight);
        if (newimg == NULL)
        {
            log_imlib2_error(LOG_LEVEL_ERROR, filename,
                             IMLIB_LOAD_ERROR_OUT_OF_MEMORY);
            result = 1;
        }
        else
        {
            imlib_free_image();
            imlib_context_set_image(newimg);
        }
    }
    return result;
}
#endif /* USE_IMLIB2 */

#ifdef USE_IMLIB2
/**************************************************************************//**
 * Zooms an imlib2 image
 *
 * @param filename Filename we're working on (for error reporting)
 * @param twidth target width
 * @param theight target height
 * @return 0 for success
 */
static int
zoom_imlib_image(const char *filename, int twidth, int theight)
{
    int result = 0;
    Imlib_Image img = imlib_context_get_image();

    if (img == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "No context for zooming image");
        result = 1;
    }
    else
    {
        struct box zb;
        Imlib_Image newimg = NULL;
        int width = imlib_image_get_width();
        int height = imlib_image_get_height();

        if (calculate_zoom_box(width, height,
                               twidth, theight, &zb) == 0)
        {
            newimg = imlib_create_cropped_scaled_image(
                         zb.left_margin, zb.top_margin,
                         zb.width, zb.height, twidth, theight);
        }
        else
        {
            /* Can't zoom - scale the image instead */
            newimg = imlib_create_cropped_scaled_image(
                         0, 0, width, height, twidth, theight);
        }
        if (newimg == NULL)

        {
            log_imlib2_error(LOG_LEVEL_ERROR, filename,
                             IMLIB_LOAD_ERROR_OUT_OF_MEMORY);
            result = 1;
        }
        else
        {
            imlib_free_image();
            imlib_context_set_image(newimg);
        }
    }
    return result;
}
#endif /* USE_IMLIB2 */

#ifdef USE_IMLIB2
/**************************************************************************//**
 * Copies imlib2 image data to a bitmap
 *
 * @param self bitmap to copy data to
 * @param out_palette Palette for output bitmap
 * @return 0 for success
 */
static int
copy_imlib_data_to_bitmap(struct xrdp_bitmap *self,
                          const int *out_palette)
{
    int result = 0;
    Imlib_Image img = imlib_context_get_image();

    if (img == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "No context for zooming image");
        result = 1;
    }
    else
    {
        int width = imlib_image_get_width();
        int height = imlib_image_get_height();
        int i;
        int j;
        DATA32 *bdata;
        int color;
        xrdp_bitmap_resize(self, width, height);

        bdata = imlib_image_get_data_for_reading_only();
        for (j = 0 ; j < height; ++j)
        {
            for (i = 0 ; i < width ; ++i)
            {
                color = (*bdata++ & 0xffffff);

                if (self->bpp == 8)
                {
                    color = xrdp_bitmap_get_index(self, out_palette, color);
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

                xrdp_bitmap_set_pixel(self, i, j, color);
            }
        }
    }

    return result;
}
#endif /* USE_IMLIB2 */

#ifdef USE_IMLIB2
/**
 * Converts an xrdp HCOLOR into RGB values used by imlib2
 *
 * @param hcolor Color to convert
 * @param bpp    Bits-per-pixel for the hcolor
 * @param[out] r Red value
 * @param[out] g Green value
 * @param[out] b Blue value
 */
static void hcolor_to_rgb(int hcolor, int bpp, int *r, int *g, int *b)
{
    switch (bpp)
    {
        case 8:
            *r = (hcolor & 0x7) << 5;
            *g = (hcolor & 0x38) << 2;
            *b = (hcolor & 0xc0);
            break;

        case 15:
            SPLITCOLOR15(*r, *g, *b, hcolor);
            break;

        case 16:
            SPLITCOLOR16(*r, *g, *b, hcolor);
            break;

        default:
            /* Beware : HCOLOR is BGR, not RGB */
            *r = hcolor & 0xff;
            *g = (hcolor >> 8) & 0xff;
            *b = (hcolor >> 16) & 0xff;
            break;
    }
}
#endif

#ifdef USE_IMLIB2
/*****************************************************************************/
int
xrdp_bitmap_load(struct xrdp_bitmap *self, const char *filename,
                 const int *palette,
                 int background,
                 enum xrdp_bitmap_load_transform transform,
                 int twidth,
                 int theight)
{
    int result = 0;
    Imlib_Load_Error lerr;
    int free_context_image = 0; /* Set if we've got an image loaded */
    Imlib_Image img = imlib_load_image_with_error_return(filename, &lerr);

    /* Load the image */
    if (img == NULL)
    {
        log_imlib2_error(LOG_LEVEL_ERROR, filename, lerr);
        result = 1;
    }
    else
    {
        imlib_context_set_image(img);
        free_context_image = 1;
    }

    /* Sort out the background */
    if (result == 0 && imlib_image_has_alpha())
    {
        int r;
        int g;
        int b;
        hcolor_to_rgb(background, self->bpp, &r, &g, &b);

        result = blend_imlib_image_onto_background(filename, r, g, b);
    }

    if (result == 0)
    {
        switch (transform)
        {
            case XBLT_NONE:
                break;

            case XBLT_SCALE:
                result = scale_imlib_image(filename, twidth, theight);
                break;

            case XBLT_ZOOM:
                result = zoom_imlib_image(filename, twidth, theight);
                break;

            default:
                LOG(LOG_LEVEL_WARNING, "Invalid bitmap transform %d specified",
                    transform);
        }
    }

    if (result == 0)
    {
        result = copy_imlib_data_to_bitmap(self, palette);
    }

    if (free_context_image)
    {
        imlib_free_image();
    }

    return result;
}
#endif /* USE_IMLIB2 */
