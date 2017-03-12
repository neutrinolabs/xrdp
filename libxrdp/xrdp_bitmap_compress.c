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
 * bitmap compressor
 * This is the original RDP bitmap compression algorithm.  Pixel based.
 * This does not do 32 bpp compression, nscodec, rfx, etc
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "libxrdp.h"

#define BC_MAX_BYTES (16 * 1024)

/*****************************************************************************/
#define IN_PIXEL8(in_ptr, in_x, in_y, in_w, in_last_pixel, in_pixel); \
    do { \
        if (in_ptr == 0) \
        { \
            in_pixel = 0; \
        } \
        else if (in_x < in_w) \
        { \
            in_pixel = GETPIXEL8(in_ptr, in_x, in_y, in_w); \
        } \
        else \
        { \
            in_pixel = in_last_pixel; \
        } \
    } while (0)

/*****************************************************************************/
#define IN_PIXEL16(in_ptr, in_x, in_y, in_w, in_last_pixel, in_pixel); \
    do { \
        if (in_ptr == 0) \
        { \
            in_pixel = 0; \
        } \
        else if (in_x < in_w) \
        { \
            in_pixel = GETPIXEL16(in_ptr, in_x, in_y, in_w); \
        } \
        else \
        { \
            in_pixel = in_last_pixel; \
        } \
    } while (0)

/*****************************************************************************/
#define IN_PIXEL32(in_ptr, in_x, in_y, in_w, in_last_pixel, in_pixel); \
    do { \
        if (in_ptr == 0) \
        { \
            in_pixel = 0; \
        } \
        else if (in_x < in_w) \
        { \
            in_pixel = GETPIXEL32(in_ptr, in_x, in_y, in_w); \
        } \
        else \
        { \
            in_pixel = in_last_pixel; \
        } \
    } while (0)

/*****************************************************************************/
/* color */
#define OUT_COLOR_COUNT1(in_count, in_s, in_data) \
    do { \
        if (in_count > 0) \
        { \
            if (in_count < 32) \
            { \
                temp = (0x3 << 5) | in_count; \
                out_uint8(in_s, temp); \
                out_uint8(in_s, in_data); \
            } \
            else if (in_count < 256 + 32) \
            { \
                out_uint8(in_s, 0x60); \
                temp = in_count - 32; \
                out_uint8(in_s, temp); \
                out_uint8(in_s, in_data); \
            } \
            else \
            { \
                out_uint8(in_s, 0xf3); \
                out_uint16_le(in_s, in_count); \
                out_uint8(in_s, in_data); \
            } \
        } \
        in_count = 0; \
    } while (0)

/*****************************************************************************/
/* color */
#define OUT_COLOR_COUNT2(in_count, in_s, in_data) \
    do { \
        if (in_count > 0) \
        { \
            if (in_count < 32) \
            { \
                temp = (0x3 << 5) | in_count; \
                out_uint8(in_s, temp); \
                out_uint16_le(in_s, in_data); \
            } \
            else if (in_count < 256 + 32) \
            { \
                out_uint8(in_s, 0x60); \
                temp = in_count - 32; \
                out_uint8(in_s, temp); \
                out_uint16_le(in_s, in_data); \
            } \
            else \
            { \
                out_uint8(in_s, 0xf3); \
                out_uint16_le(in_s, in_count); \
                out_uint16_le(in_s, in_data); \
            } \
        } \
        in_count = 0; \
    } while (0)

/*****************************************************************************/
/* color */
#define OUT_COLOR_COUNT3(in_count, in_s, in_data) \
    do { \
        if (in_count > 0) \
        { \
            if (in_count < 32) \
            { \
                temp = (0x3 << 5) | in_count; \
                out_uint8(in_s, temp); \
                out_uint8(in_s, in_data & 0xff); \
                out_uint8(in_s, (in_data >> 8) & 0xff); \
                out_uint8(in_s, (in_data >> 16) & 0xff); \
            } \
            else if (in_count < 256 + 32) \
            { \
                out_uint8(in_s, 0x60); \
                temp = in_count - 32; \
                out_uint8(in_s, temp); \
                out_uint8(in_s, in_data & 0xff); \
                out_uint8(in_s, (in_data >> 8) & 0xff); \
                out_uint8(in_s, (in_data >> 16) & 0xff); \
            } \
            else \
            { \
                out_uint8(in_s, 0xf3); \
                out_uint16_le(in_s, in_count); \
                out_uint8(in_s, in_data & 0xff); \
                out_uint8(in_s, (in_data >> 8) & 0xff); \
                out_uint8(in_s, (in_data >> 16) & 0xff); \
            } \
        } \
        in_count = 0; \
    } while (0)

/*****************************************************************************/
/* copy */
#define OUT_COPY_COUNT1(in_count, in_s, in_data) \
    do { \
        if (in_count > 0) \
        { \
            if (in_count < 32) \
            { \
                temp = (0x4 << 5) | in_count; \
                out_uint8(in_s, temp); \
                out_uint8a(in_s, in_data->data, in_count); \
            } \
            else if (in_count < 256 + 32) \
            { \
                out_uint8(in_s, 0x80); \
                temp = in_count - 32; \
                out_uint8(in_s, temp); \
                out_uint8a(in_s, in_data->data, in_count); \
            } \
            else \
            { \
                out_uint8(in_s, 0xf4); \
                out_uint16_le(in_s, in_count); \
                out_uint8a(in_s, in_data->data, in_count); \
            } \
        } \
        in_count = 0; \
        init_stream(in_data, 0); \
    } while (0)

/*****************************************************************************/
/* copy */
#define OUT_COPY_COUNT2(in_count, in_s, in_data) \
    do { \
        if (in_count > 0) \
        { \
            if (in_count < 32) \
            { \
                temp = (0x4 << 5) | in_count; \
                out_uint8(in_s, temp); \
                temp = in_count * 2; \
                out_uint8a(in_s, in_data->data, temp); \
            } \
            else if (in_count < 256 + 32) \
            { \
                out_uint8(in_s, 0x80); \
                temp = in_count - 32; \
                out_uint8(in_s, temp); \
                temp = in_count * 2; \
                out_uint8a(in_s, in_data->data, temp); \
            } \
            else \
            { \
                out_uint8(in_s, 0xf4); \
                out_uint16_le(in_s, in_count); \
                temp = in_count * 2; \
                out_uint8a(in_s, in_data->data, temp); \
            } \
        } \
        in_count = 0; \
        init_stream(in_data, 0); \
    } while (0)

/*****************************************************************************/
/* copy */
#define OUT_COPY_COUNT3(in_count, in_s, in_data) \
    do { \
        if (in_count > 0) \
        { \
            if (in_count < 32) \
            { \
                temp = (0x4 << 5) | in_count; \
                out_uint8(in_s, temp); \
                temp = in_count * 3; \
                out_uint8a(in_s, in_data->end, temp); \
            } \
            else if (in_count < 256 + 32) \
            { \
                out_uint8(in_s, 0x80); \
                temp = in_count - 32; \
                out_uint8(in_s, temp); \
                temp = in_count * 3; \
                out_uint8a(in_s, in_data->end, temp); \
            } \
            else \
            { \
                out_uint8(in_s, 0xf4); \
                out_uint16_le(in_s, in_count); \
                temp = in_count * 3; \
                out_uint8a(in_s, in_data->end, temp); \
            } \
        } \
        in_count = 0; \
        init_stream(in_data, 0); \
    } while (0)

/*****************************************************************************/
/* bicolor */
#define OUT_BICOLOR_COUNT1(in_count, in_s, in_color1, in_color2) \
    do { \
        if (in_count > 0) \
        { \
            if (in_count / 2 < 16) \
            { \
                temp = (0xe << 4) | (in_count / 2); \
                out_uint8(in_s, temp); \
                out_uint8(in_s, in_color1); \
                out_uint8(in_s, in_color2); \
            } \
            else if (in_count / 2 < 256 + 16) \
            { \
                out_uint8(in_s, 0xe0); \
                temp = in_count / 2 - 16; \
                out_uint8(in_s, temp); \
                out_uint8(in_s, in_color1); \
                out_uint8(in_s, in_color2); \
            } \
            else \
            { \
                out_uint8(in_s, 0xf8); \
                temp = in_count / 2; \
                out_uint16_le(in_s, temp); \
                out_uint8(in_s, in_color1); \
                out_uint8(in_s, in_color2); \
            } \
        } \
        in_count = 0; \
    } while (0)

/*****************************************************************************/
/* bicolor */
#define OUT_BICOLOR_COUNT2(in_count, in_s, in_color1, in_color2) \
    do { \
        if (in_count > 0) \
        { \
            if (in_count / 2 < 16) \
            { \
                temp = (0xe << 4) | (in_count / 2); \
                out_uint8(in_s, temp); \
                out_uint16_le(in_s, in_color1); \
                out_uint16_le(in_s, in_color2); \
            } \
            else if (in_count / 2 < 256 + 16) \
            { \
                out_uint8(in_s, 0xe0); \
                temp = in_count / 2 - 16; \
                out_uint8(in_s, temp); \
                out_uint16_le(in_s, in_color1); \
                out_uint16_le(in_s, in_color2); \
            } \
            else \
            { \
                out_uint8(in_s, 0xf8); \
                temp = in_count / 2; \
                out_uint16_le(in_s, temp); \
                out_uint16_le(in_s, in_color1); \
                out_uint16_le(in_s, in_color2); \
            } \
        } \
        in_count = 0; \
    } while (0)

/*****************************************************************************/
/* bicolor */
#define OUT_BICOLOR_COUNT3(in_count, in_s, in_color1, in_color2) \
    do { \
        if (in_count > 0) \
        { \
            if (in_count / 2 < 16) \
            { \
                temp = (0xe << 4) | (in_count / 2); \
                out_uint8(in_s, temp); \
                out_uint8(in_s, in_color1 & 0xff); \
                out_uint8(in_s, (in_color1 >> 8) & 0xff); \
                out_uint8(in_s, (in_color1 >> 16) & 0xff); \
                out_uint8(in_s, in_color2 & 0xff); \
                out_uint8(in_s, (in_color2 >> 8) & 0xff); \
                out_uint8(in_s, (in_color2 >> 16) & 0xff); \
            } \
            else if (in_count / 2 < 256 + 16) \
            { \
                out_uint8(in_s, 0xe0); \
                temp = in_count / 2 - 16; \
                out_uint8(in_s, temp); \
                out_uint8(in_s, in_color1 & 0xff); \
                out_uint8(in_s, (in_color1 >> 8) & 0xff); \
                out_uint8(in_s, (in_color1 >> 16) & 0xff); \
                out_uint8(in_s, in_color2 & 0xff); \
                out_uint8(in_s, (in_color2 >> 8) & 0xff); \
                out_uint8(in_s, (in_color2 >> 16) & 0xff); \
            } \
            else \
            { \
                out_uint8(in_s, 0xf8); \
                temp = in_count / 2; \
                out_uint16_le(in_s, temp); \
                out_uint8(in_s, in_color1 & 0xff); \
                out_uint8(in_s, (in_color1 >> 8) & 0xff); \
                out_uint8(in_s, (in_color1 >> 16) & 0xff); \
                out_uint8(in_s, in_color2 & 0xff); \
                out_uint8(in_s, (in_color2 >> 8) & 0xff); \
                out_uint8(in_s, (in_color2 >> 16) & 0xff); \
            } \
        } \
        in_count = 0; \
    } while (0)

/*****************************************************************************/
/* fill */
#define OUT_FILL_COUNT1(in_count, in_s) \
    do { \
        if (in_count > 0) \
        { \
            if (in_count < 32) \
            { \
                out_uint8(in_s, in_count); \
            } \
            else if (in_count < 256 + 32) \
            { \
                out_uint8(in_s, 0x0); \
                temp = in_count - 32; \
                out_uint8(in_s, temp); \
            } \
            else \
            { \
                out_uint8(in_s, 0xf0); \
                out_uint16_le(in_s, in_count); \
            } \
        } \
        in_count = 0; \
    } while (0)

/*****************************************************************************/
/* fill */
#define OUT_FILL_COUNT2(in_count, in_s) \
    do { \
        if (in_count > 0) \
        { \
            if (in_count < 32) \
            { \
                out_uint8(in_s, in_count); \
            } \
            else if (in_count < 256 + 32) \
            { \
                out_uint8(in_s, 0x0); \
                temp = in_count - 32; \
                out_uint8(in_s, temp); \
            } \
            else \
            { \
                out_uint8(in_s, 0xf0); \
                out_uint16_le(in_s, in_count); \
            } \
        } \
        in_count = 0; \
    } while (0)

/*****************************************************************************/
/* fill */
#define OUT_FILL_COUNT3(in_count, in_s) \
    do { \
        if (in_count > 0) \
        { \
            if (in_count < 32) \
            { \
                out_uint8(in_s, in_count); \
            } \
            else if (in_count < 256 + 32) \
            { \
                out_uint8(in_s, 0x0); \
                temp = in_count - 32; \
                out_uint8(in_s, temp); \
            } \
            else \
            { \
                out_uint8(in_s, 0xf0); \
                out_uint16_le(in_s, in_count); \
            } \
        } \
        in_count = 0; \
    } while (0)

/*****************************************************************************/
/* mix */
#define OUT_MIX_COUNT1(in_count, in_s) \
    do { \
        if (in_count > 0) \
        { \
            if (in_count < 32) \
            { \
                temp = (0x1 << 5) | in_count; \
                out_uint8(in_s, temp); \
            } \
            else if (in_count < 256 + 32) \
            { \
                out_uint8(in_s, 0x20); \
                temp = in_count - 32; \
                out_uint8(in_s, temp); \
            } \
            else \
            { \
                out_uint8(in_s, 0xf1); \
                out_uint16_le(in_s, in_count); \
            } \
        } \
        in_count = 0; \
    } while (0)

/*****************************************************************************/
/* mix */
#define OUT_MIX_COUNT2(in_count, in_s) \
    do { \
        if (in_count > 0) \
        { \
            if (in_count < 32) \
            { \
                temp = (0x1 << 5) | in_count; \
                out_uint8(in_s, temp); \
            } \
            else if (in_count < 256 + 32) \
            { \
                out_uint8(in_s, 0x20); \
                temp = in_count - 32; \
                out_uint8(in_s, temp); \
            } \
            else \
            { \
                out_uint8(in_s, 0xf1); \
                out_uint16_le(in_s, in_count); \
            } \
        } \
        in_count = 0; \
    } while (0)

/*****************************************************************************/
/* mix */
#define OUT_MIX_COUNT3(in_count, in_s) \
    do { \
        if (in_count > 0) \
        { \
            if (in_count < 32) \
            { \
                temp = (0x1 << 5) | in_count; \
                out_uint8(in_s, temp); \
            } \
            else if (in_count < 256 + 32) \
            { \
                out_uint8(in_s, 0x20); \
                temp = in_count - 32; \
                out_uint8(in_s, temp); \
            } \
            else \
            { \
                out_uint8(in_s, 0xf1); \
                out_uint16_le(in_s, in_count); \
            } \
        } \
        in_count = 0; \
    } while (0)

/*****************************************************************************/
/* fom */
#define OUT_FOM_COUNT1(in_count, in_s, in_mask, in_mask_len) \
    do { \
        if (in_count > 0) \
        { \
            if ((in_count % 8) == 0 && in_count < 249) \
            { \
                temp = (0x2 << 5) | (in_count / 8); \
                out_uint8(in_s, temp); \
                out_uint8a(in_s, in_mask, in_mask_len); \
            } \
            else if (in_count < 256) \
            { \
                out_uint8(in_s, 0x40); \
                temp = in_count - 1; \
                out_uint8(in_s, temp); \
                out_uint8a(in_s, in_mask, in_mask_len); \
            } \
            else \
            { \
                out_uint8(in_s, 0xf2); \
                out_uint16_le(in_s, in_count); \
                out_uint8a(in_s, in_mask, in_mask_len); \
            } \
        } \
        in_count = 0; \
    } while (0)

/*****************************************************************************/
/* fom */
#define OUT_FOM_COUNT2(in_count, in_s, in_mask, in_mask_len) \
    do { \
        if (in_count > 0) \
        { \
            if ((in_count % 8) == 0 && in_count < 249) \
            { \
                temp = (0x2 << 5) | (in_count / 8); \
                out_uint8(in_s, temp); \
                out_uint8a(in_s, in_mask, in_mask_len); \
            } \
            else if (in_count < 256) \
            { \
                out_uint8(in_s, 0x40); \
                temp = in_count - 1; \
                out_uint8(in_s, temp); \
                out_uint8a(in_s, in_mask, in_mask_len); \
            } \
            else \
            { \
                out_uint8(in_s, 0xf2); \
                out_uint16_le(in_s, in_count); \
                out_uint8a(in_s, in_mask, in_mask_len); \
            } \
        } \
        in_count = 0; \
    } while (0)

/*****************************************************************************/
/* fill or mix (fom) */
#define OUT_FOM_COUNT3(in_count, in_s, in_mask, in_mask_len) \
    do { \
        if (in_count > 0) \
        { \
            if ((in_count % 8) == 0 && in_count < 249) \
            { \
                temp = (0x2 << 5) | (in_count / 8); \
                out_uint8(in_s, temp); \
                out_uint8a(in_s, in_mask, in_mask_len); \
            } \
            else if (in_count < 256) \
            { \
                out_uint8(in_s, 0x40); \
                temp = in_count - 1; \
                out_uint8(in_s, temp); \
                out_uint8a(in_s, in_mask, in_mask_len); \
            } \
            else \
            { \
                out_uint8(in_s, 0xf2); \
                out_uint16_le(in_s, in_count); \
                out_uint8a(in_s, in_mask, in_mask_len); \
            } \
        } \
        in_count = 0; \
    } while (0)

/*****************************************************************************/
#define TEST_FILL \
    ((last_line == 0 && pixel == 0) || \
     (last_line != 0 && pixel == ypixel))
#define TEST_MIX \
    ((last_line == 0 && pixel == mix) || \
     (last_line != 0 && pixel == (ypixel ^ mix)))
#define TEST_FOM (TEST_FILL || TEST_MIX)
#define TEST_COLOR (pixel == last_pixel)
#define TEST_BICOLOR \
    ( \
      (pixel != last_pixel) && \
      ( \
        (!bicolor_spin && pixel == bicolor1 && last_pixel == bicolor2) || \
        (bicolor_spin && pixel == bicolor2 && last_pixel == bicolor1) \
      ) \
    )
#define RESET_COUNTS \
    do { \
        bicolor_count = 0; \
        fill_count = 0; \
        color_count = 0; \
        mix_count = 0; \
        fom_count = 0; \
        fom_mask_len = 0; \
        bicolor_spin = 0; \
    } while (0)

/*****************************************************************************/
int
xrdp_bitmap_compress(char *in_data, int width, int height,
                     struct stream *s, int bpp, int byte_limit,
                     int start_line, struct stream *temp_s,
                     int e)
{
    char *line;
    char *last_line;
    char fom_mask[8192]; /* good for up to 64K bitmap */
    int lines_sent;
    int pixel;
    int count;
    int color_count;
    int last_pixel;
    int bicolor_count;
    int bicolor1;
    int bicolor2;
    int bicolor_spin;
    int end;
    int i;
    int out_count;
    int ypixel;
    int last_ypixel;
    int fill_count;
    int mix_count;
    int mix;
    int fom_count;
    int fom_mask_len;
    int temp; /* used in macros */

    init_stream(temp_s, 0);
    fom_mask_len = 0;
    last_line = 0;
    lines_sent = 0;
    end = width + e;
    count = 0;
    color_count = 0;
    last_pixel = 0;
    last_ypixel = 0;
    bicolor_count = 0;
    bicolor1 = 0;
    bicolor2 = 0;
    bicolor_spin = 0;
    fill_count = 0;
    mix_count = 0;
    fom_count = 0;

    if (bpp == 8)
    {
        mix = 0xff;
        out_count = end;
        line = in_data + width * start_line;

        while (start_line >= 0 && out_count <= BC_MAX_BYTES)
        {
            i = (s->p - s->data) + count;

            if (i - color_count >= byte_limit &&
                    i - bicolor_count >= byte_limit &&
                    i - fill_count >= byte_limit &&
                    i - mix_count >= byte_limit &&
                    i - fom_count >= byte_limit)
            {
                break;
            }

            out_count += end;

            for (i = 0; i < end; i++)
            {
                /* read next pixel */
                IN_PIXEL8(line, i, 0, width, last_pixel, pixel);
                IN_PIXEL8(last_line, i, 0, width, last_ypixel, ypixel);

                if (!TEST_FILL)
                {
                    if (fill_count > 3 &&
                            fill_count >= color_count &&
                            fill_count >= bicolor_count &&
                            fill_count >= mix_count &&
                            fill_count >= fom_count)
                    {
                        count -= fill_count;
                        OUT_COPY_COUNT1(count, s, temp_s);
                        OUT_FILL_COUNT1(fill_count, s);
                        RESET_COUNTS;
                    }

                    fill_count = 0;
                }

                if (!TEST_MIX)
                {
                    if (mix_count > 3 &&
                            mix_count >= fill_count &&
                            mix_count >= bicolor_count &&
                            mix_count >= color_count &&
                            mix_count >= fom_count)
                    {
                        count -= mix_count;
                        OUT_COPY_COUNT1(count, s, temp_s);
                        OUT_MIX_COUNT1(mix_count, s);
                        RESET_COUNTS;
                    }

                    mix_count = 0;
                }

                if (!TEST_COLOR)
                {
                    if (color_count > 3 &&
                            color_count >= fill_count &&
                            color_count >= bicolor_count &&
                            color_count >= mix_count &&
                            color_count >= fom_count)
                    {
                        count -= color_count;
                        OUT_COPY_COUNT1(count, s, temp_s);
                        OUT_COLOR_COUNT1(color_count, s, last_pixel);
                        RESET_COUNTS;
                    }

                    color_count = 0;
                }

                if (!TEST_BICOLOR)
                {
                    if (bicolor_count > 3 &&
                            bicolor_count >= fill_count &&
                            bicolor_count >= color_count &&
                            bicolor_count >= mix_count &&
                            bicolor_count >= fom_count)
                    {
                        if ((bicolor_count % 2) == 0)
                        {
                            count -= bicolor_count;
                            OUT_COPY_COUNT1(count, s, temp_s);
                            OUT_BICOLOR_COUNT1(bicolor_count, s, bicolor1, bicolor2);
                        }
                        else
                        {
                            bicolor_count--;
                            count -= bicolor_count;
                            OUT_COPY_COUNT1(count, s, temp_s);
                            OUT_BICOLOR_COUNT1(bicolor_count, s, bicolor2, bicolor1);
                        }

                        RESET_COUNTS;
                    }

                    bicolor_count = 0;
                    bicolor1 = last_pixel;
                    bicolor2 = pixel;
                    bicolor_spin = 0;
                }

                if (!TEST_FOM)
                {
                    if (fom_count > 3 &&
                            fom_count >= fill_count &&
                            fom_count >= color_count &&
                            fom_count >= mix_count &&
                            fom_count >= bicolor_count)
                    {
                        count -= fom_count;
                        OUT_COPY_COUNT1(count, s, temp_s);
                        OUT_FOM_COUNT1(fom_count, s, fom_mask, fom_mask_len);
                        RESET_COUNTS;
                    }

                    fom_count = 0;
                    fom_mask_len = 0;
                }

                if (TEST_FILL)
                {
                    fill_count++;
                }

                if (TEST_MIX)
                {
                    mix_count++;
                }

                if (TEST_COLOR)
                {
                    color_count++;
                }

                if (TEST_BICOLOR)
                {
                    bicolor_spin = !bicolor_spin;
                    bicolor_count++;
                }

                if (TEST_FOM)
                {
                    if ((fom_count % 8) == 0)
                    {
                        fom_mask[fom_mask_len] = 0;
                        fom_mask_len++;
                    }

                    if (pixel == (ypixel ^ mix))
                    {
                        fom_mask[fom_mask_len - 1] |= (1 << (fom_count % 8));
                    }

                    fom_count++;
                }

                out_uint8(temp_s, pixel);
                count++;
                last_pixel = pixel;
                last_ypixel = ypixel;
            }

            /* can't take fix, mix, or fom past first line */
            if (last_line == 0)
            {
                if (fill_count > 3 &&
                        fill_count >= color_count &&
                        fill_count >= bicolor_count &&
                        fill_count >= mix_count &&
                        fill_count >= fom_count)
                {
                    count -= fill_count;
                    OUT_COPY_COUNT1(count, s, temp_s);
                    OUT_FILL_COUNT1(fill_count, s);
                    RESET_COUNTS;
                }

                fill_count = 0;

                if (mix_count > 3 &&
                        mix_count >= fill_count &&
                        mix_count >= bicolor_count &&
                        mix_count >= color_count &&
                        mix_count >= fom_count)
                {
                    count -= mix_count;
                    OUT_COPY_COUNT1(count, s, temp_s);
                    OUT_MIX_COUNT1(mix_count, s);
                    RESET_COUNTS;
                }

                mix_count = 0;

                if (fom_count > 3 &&
                        fom_count >= fill_count &&
                        fom_count >= color_count &&
                        fom_count >= mix_count &&
                        fom_count >= bicolor_count)
                {
                    count -= fom_count;
                    OUT_COPY_COUNT1(count, s, temp_s);
                    OUT_FOM_COUNT1(fom_count, s, fom_mask, fom_mask_len);
                    RESET_COUNTS;
                }

                fom_count = 0;
                fom_mask_len = 0;
            }

            last_line = line;
            line = line - width;
            start_line--;
            lines_sent++;
        }

        if (fill_count > 3 &&
                fill_count >= color_count &&
                fill_count >= bicolor_count &&
                fill_count >= mix_count &&
                fill_count >= fom_count)
        {
            count -= fill_count;
            OUT_COPY_COUNT1(count, s, temp_s);
            OUT_FILL_COUNT1(fill_count, s);
        }
        else if (mix_count > 3 &&
                 mix_count >= color_count &&
                 mix_count >= bicolor_count &&
                 mix_count >= fill_count &&
                 mix_count >= fom_count)
        {
            count -= mix_count;
            OUT_COPY_COUNT1(count, s, temp_s);
            OUT_MIX_COUNT1(mix_count, s);
        }
        else if (color_count > 3 &&
                 color_count >= mix_count &&
                 color_count >= bicolor_count &&
                 color_count >= fill_count &&
                 color_count >= fom_count)
        {
            count -= color_count;
            OUT_COPY_COUNT1(count, s, temp_s);
            OUT_COLOR_COUNT1(color_count, s, last_pixel);
        }
        else if (bicolor_count > 3 &&
                 bicolor_count >= mix_count &&
                 bicolor_count >= color_count &&
                 bicolor_count >= fill_count &&
                 bicolor_count >= fom_count)
        {
            if ((bicolor_count % 2) == 0)
            {
                count -= bicolor_count;
                OUT_COPY_COUNT1(count, s, temp_s);
                OUT_BICOLOR_COUNT1(bicolor_count, s, bicolor1, bicolor2);
            }
            else
            {
                bicolor_count--;
                count -= bicolor_count;
                OUT_COPY_COUNT1(count, s, temp_s);
                OUT_BICOLOR_COUNT1(bicolor_count, s, bicolor2, bicolor1);
            }

            count -= bicolor_count;
            OUT_COPY_COUNT1(count, s, temp_s);
            OUT_BICOLOR_COUNT1(bicolor_count, s, bicolor1, bicolor2);
        }
        else if (fom_count > 3 &&
                 fom_count >= mix_count &&
                 fom_count >= color_count &&
                 fom_count >= fill_count &&
                 fom_count >= bicolor_count)
        {
            count -= fom_count;
            OUT_COPY_COUNT1(count, s, temp_s);
            OUT_FOM_COUNT1(fom_count, s, fom_mask, fom_mask_len);
        }
        else
        {
            OUT_COPY_COUNT1(count, s, temp_s);
        }
    }
    else if ((bpp == 15) || (bpp == 16))
    {
        mix = (bpp == 15) ? 0xba1f : 0xffff;
        out_count = end * 2;
        line = in_data + width * start_line * 2;

        while (start_line >= 0 && out_count <= BC_MAX_BYTES)
        {
            i = (s->p - s->data) + count * 2;

            if (i - (color_count * 2) >= byte_limit &&
                    i - (bicolor_count * 2) >= byte_limit &&
                    i - (fill_count * 2) >= byte_limit &&
                    i - (mix_count * 2) >= byte_limit &&
                    i - (fom_count * 2) >= byte_limit)
            {
                break;
            }

            out_count += end * 2;

            for (i = 0; i < end; i++)
            {
                /* read next pixel */
                IN_PIXEL16(line, i, 0, width, last_pixel, pixel);
                IN_PIXEL16(last_line, i, 0, width, last_ypixel, ypixel);

                if (!TEST_FILL)
                {
                    if (fill_count > 3 &&
                            fill_count >= color_count &&
                            fill_count >= bicolor_count &&
                            fill_count >= mix_count &&
                            fill_count >= fom_count)
                    {
                        count -= fill_count;
                        OUT_COPY_COUNT2(count, s, temp_s);
                        OUT_FILL_COUNT2(fill_count, s);
                        RESET_COUNTS;
                    }

                    fill_count = 0;
                }

                if (!TEST_MIX)
                {
                    if (mix_count > 3 &&
                            mix_count >= fill_count &&
                            mix_count >= bicolor_count &&
                            mix_count >= color_count &&
                            mix_count >= fom_count)
                    {
                        count -= mix_count;
                        OUT_COPY_COUNT2(count, s, temp_s);
                        OUT_MIX_COUNT2(mix_count, s);
                        RESET_COUNTS;
                    }

                    mix_count = 0;
                }

                if (!TEST_COLOR)
                {
                    if (color_count > 3 &&
                            color_count >= fill_count &&
                            color_count >= bicolor_count &&
                            color_count >= mix_count &&
                            color_count >= fom_count)
                    {
                        count -= color_count;
                        OUT_COPY_COUNT2(count, s, temp_s);
                        OUT_COLOR_COUNT2(color_count, s, last_pixel);
                        RESET_COUNTS;
                    }

                    color_count = 0;
                }

                if (!TEST_BICOLOR)
                {
                    if (bicolor_count > 3 &&
                            bicolor_count >= fill_count &&
                            bicolor_count >= color_count &&
                            bicolor_count >= mix_count &&
                            bicolor_count >= fom_count)
                    {
                        if ((bicolor_count % 2) == 0)
                        {
                            count -= bicolor_count;
                            OUT_COPY_COUNT2(count, s, temp_s);
                            OUT_BICOLOR_COUNT2(bicolor_count, s, bicolor1, bicolor2);
                        }
                        else
                        {
                            bicolor_count--;
                            count -= bicolor_count;
                            OUT_COPY_COUNT2(count, s, temp_s);
                            OUT_BICOLOR_COUNT2(bicolor_count, s, bicolor2, bicolor1);
                        }

                        RESET_COUNTS;
                    }

                    bicolor_count = 0;
                    bicolor1 = last_pixel;
                    bicolor2 = pixel;
                    bicolor_spin = 0;
                }

                if (!TEST_FOM)
                {
                    if (fom_count > 3 &&
                            fom_count >= fill_count &&
                            fom_count >= color_count &&
                            fom_count >= mix_count &&
                            fom_count >= bicolor_count)
                    {
                        count -= fom_count;
                        OUT_COPY_COUNT2(count, s, temp_s);
                        OUT_FOM_COUNT2(fom_count, s, fom_mask, fom_mask_len);
                        RESET_COUNTS;
                    }

                    fom_count = 0;
                    fom_mask_len = 0;
                }

                if (TEST_FILL)
                {
                    fill_count++;
                }

                if (TEST_MIX)
                {
                    mix_count++;
                }

                if (TEST_COLOR)
                {
                    color_count++;
                }

                if (TEST_BICOLOR)
                {
                    bicolor_spin = !bicolor_spin;
                    bicolor_count++;
                }

                if (TEST_FOM)
                {
                    if ((fom_count % 8) == 0)
                    {
                        fom_mask[fom_mask_len] = 0;
                        fom_mask_len++;
                    }

                    if (pixel == (ypixel ^ mix))
                    {
                        fom_mask[fom_mask_len - 1] |= (1 << (fom_count % 8));
                    }

                    fom_count++;
                }

                out_uint16_le(temp_s, pixel);
                count++;
                last_pixel = pixel;
                last_ypixel = ypixel;
            }

            /* can't take fix, mix, or fom past first line */
            if (last_line == 0)
            {
                if (fill_count > 3 &&
                        fill_count >= color_count &&
                        fill_count >= bicolor_count &&
                        fill_count >= mix_count &&
                        fill_count >= fom_count)
                {
                    count -= fill_count;
                    OUT_COPY_COUNT2(count, s, temp_s);
                    OUT_FILL_COUNT2(fill_count, s);
                    RESET_COUNTS;
                }

                fill_count = 0;

                if (mix_count > 3 &&
                        mix_count >= fill_count &&
                        mix_count >= bicolor_count &&
                        mix_count >= color_count &&
                        mix_count >= fom_count)
                {
                    count -= mix_count;
                    OUT_COPY_COUNT2(count, s, temp_s);
                    OUT_MIX_COUNT2(mix_count, s);
                    RESET_COUNTS;
                }

                mix_count = 0;

                if (fom_count > 3 &&
                        fom_count >= fill_count &&
                        fom_count >= color_count &&
                        fom_count >= mix_count &&
                        fom_count >= bicolor_count)
                {
                    count -= fom_count;
                    OUT_COPY_COUNT2(count, s, temp_s);
                    OUT_FOM_COUNT2(fom_count, s, fom_mask, fom_mask_len);
                    RESET_COUNTS;
                }

                fom_count = 0;
                fom_mask_len = 0;
            }

            last_line = line;
            line = line - width * 2;
            start_line--;
            lines_sent++;
        }

        if (fill_count > 3 &&
                fill_count >= color_count &&
                fill_count >= bicolor_count &&
                fill_count >= mix_count &&
                fill_count >= fom_count)
        {
            count -= fill_count;
            OUT_COPY_COUNT2(count, s, temp_s);
            OUT_FILL_COUNT2(fill_count, s);
        }
        else if (mix_count > 3 &&
                 mix_count >= color_count &&
                 mix_count >= bicolor_count &&
                 mix_count >= fill_count &&
                 mix_count >= fom_count)
        {
            count -= mix_count;
            OUT_COPY_COUNT2(count, s, temp_s);
            OUT_MIX_COUNT2(mix_count, s);
        }
        else if (color_count > 3 &&
                 color_count >= mix_count &&
                 color_count >= bicolor_count &&
                 color_count >= fill_count &&
                 color_count >= fom_count)
        {
            count -= color_count;
            OUT_COPY_COUNT2(count, s, temp_s);
            OUT_COLOR_COUNT2(color_count, s, last_pixel);
        }
        else if (bicolor_count > 3 &&
                 bicolor_count >= mix_count &&
                 bicolor_count >= color_count &&
                 bicolor_count >= fill_count &&
                 bicolor_count >= fom_count)
        {
            if ((bicolor_count % 2) == 0)
            {
                count -= bicolor_count;
                OUT_COPY_COUNT2(count, s, temp_s);
                OUT_BICOLOR_COUNT2(bicolor_count, s, bicolor1, bicolor2);
            }
            else
            {
                bicolor_count--;
                count -= bicolor_count;
                OUT_COPY_COUNT2(count, s, temp_s);
                OUT_BICOLOR_COUNT2(bicolor_count, s, bicolor2, bicolor1);
            }

            count -= bicolor_count;
            OUT_COPY_COUNT2(count, s, temp_s);
            OUT_BICOLOR_COUNT2(bicolor_count, s, bicolor1, bicolor2);
        }
        else if (fom_count > 3 &&
                 fom_count >= mix_count &&
                 fom_count >= color_count &&
                 fom_count >= fill_count &&
                 fom_count >= bicolor_count)
        {
            count -= fom_count;
            OUT_COPY_COUNT2(count, s, temp_s);
            OUT_FOM_COUNT2(fom_count, s, fom_mask, fom_mask_len);
        }
        else
        {
            OUT_COPY_COUNT2(count, s, temp_s);
        }
    }
    else if (bpp == 24)
    {
        mix = 0xffffff;
        out_count = end * 3;
        line = in_data + width * start_line * 4;

        while (start_line >= 0 && out_count <= BC_MAX_BYTES)
        {
            i = (s->p - s->data) + count * 3;

            if (i - (color_count * 3) >= byte_limit &&
                    i - (bicolor_count * 3) >= byte_limit &&
                    i - (fill_count * 3) >= byte_limit &&
                    i - (mix_count * 3) >= byte_limit &&
                    i - (fom_count * 3) >= byte_limit)
            {
                break;
            }

            out_count += end * 3;

            for (i = 0; i < end; i++)
            {
                /* read next pixel */
                IN_PIXEL32(line, i, 0, width, last_pixel, pixel);
                IN_PIXEL32(last_line, i, 0, width, last_ypixel, ypixel);

                if (!TEST_FILL)
                {
                    if (fill_count > 3 &&
                            fill_count >= color_count &&
                            fill_count >= bicolor_count &&
                            fill_count >= mix_count &&
                            fill_count >= fom_count)
                    {
                        count -= fill_count;
                        OUT_COPY_COUNT3(count, s, temp_s);
                        OUT_FILL_COUNT3(fill_count, s);
                        RESET_COUNTS;
                    }

                    fill_count = 0;
                }

                if (!TEST_MIX)
                {
                    if (mix_count > 3 &&
                            mix_count >= fill_count &&
                            mix_count >= bicolor_count &&
                            mix_count >= color_count &&
                            mix_count >= fom_count)
                    {
                        count -= mix_count;
                        OUT_COPY_COUNT3(count, s, temp_s);
                        OUT_MIX_COUNT3(mix_count, s);
                        RESET_COUNTS;
                    }

                    mix_count = 0;
                }

                if (!TEST_COLOR)
                {
                    if (color_count > 3 &&
                            color_count >= fill_count &&
                            color_count >= bicolor_count &&
                            color_count >= mix_count &&
                            color_count >= fom_count)
                    {
                        count -= color_count;
                        OUT_COPY_COUNT3(count, s, temp_s);
                        OUT_COLOR_COUNT3(color_count, s, last_pixel);
                        RESET_COUNTS;
                    }

                    color_count = 0;
                }

                if (!TEST_BICOLOR)
                {
                    if (bicolor_count > 3 &&
                            bicolor_count >= fill_count &&
                            bicolor_count >= color_count &&
                            bicolor_count >= mix_count &&
                            bicolor_count >= fom_count)
                    {
                        if ((bicolor_count % 2) == 0)
                        {
                            count -= bicolor_count;
                            OUT_COPY_COUNT3(count, s, temp_s);
                            OUT_BICOLOR_COUNT3(bicolor_count, s, bicolor1, bicolor2);
                        }
                        else
                        {
                            bicolor_count--;
                            count -= bicolor_count;
                            OUT_COPY_COUNT3(count, s, temp_s);
                            OUT_BICOLOR_COUNT3(bicolor_count, s, bicolor2, bicolor1);
                        }

                        RESET_COUNTS;
                    }

                    bicolor_count = 0;
                    bicolor1 = last_pixel;
                    bicolor2 = pixel;
                    bicolor_spin = 0;
                }

                if (!TEST_FOM)
                {
                    if (fom_count > 3 &&
                            fom_count >= fill_count &&
                            fom_count >= color_count &&
                            fom_count >= mix_count &&
                            fom_count >= bicolor_count)
                    {
                        count -= fom_count;
                        OUT_COPY_COUNT3(count, s, temp_s);
                        OUT_FOM_COUNT3(fom_count, s, fom_mask, fom_mask_len);
                        RESET_COUNTS;
                    }

                    fom_count = 0;
                    fom_mask_len = 0;
                }

                if (TEST_FILL)
                {
                    fill_count++;
                }

                if (TEST_MIX)
                {
                    mix_count++;
                }

                if (TEST_COLOR)
                {
                    color_count++;
                }

                if (TEST_BICOLOR)
                {
                    bicolor_spin = !bicolor_spin;
                    bicolor_count++;
                }

                if (TEST_FOM)
                {
                    if ((fom_count % 8) == 0)
                    {
                        fom_mask[fom_mask_len] = 0;
                        fom_mask_len++;
                    }

                    if (pixel == (ypixel ^ mix))
                    {
                        fom_mask[fom_mask_len - 1] |= (1 << (fom_count % 8));
                    }

                    fom_count++;
                }

                out_uint8(temp_s, pixel & 0xff);
                out_uint8(temp_s, (pixel >> 8) & 0xff);
                out_uint8(temp_s, (pixel >> 16) & 0xff);
                count++;
                last_pixel = pixel;
                last_ypixel = ypixel;
            }

            /* can't take fix, mix, or fom past first line */
            if (last_line == 0)
            {
                if (fill_count > 3 &&
                        fill_count >= color_count &&
                        fill_count >= bicolor_count &&
                        fill_count >= mix_count &&
                        fill_count >= fom_count)
                {
                    count -= fill_count;
                    OUT_COPY_COUNT3(count, s, temp_s);
                    OUT_FILL_COUNT3(fill_count, s);
                    RESET_COUNTS;
                }

                fill_count = 0;

                if (mix_count > 3 &&
                        mix_count >= fill_count &&
                        mix_count >= bicolor_count &&
                        mix_count >= color_count &&
                        mix_count >= fom_count)
                {
                    count -= mix_count;
                    OUT_COPY_COUNT3(count, s, temp_s);
                    OUT_MIX_COUNT3(mix_count, s);
                    RESET_COUNTS;
                }

                mix_count = 0;

                if (fom_count > 3 &&
                        fom_count >= fill_count &&
                        fom_count >= color_count &&
                        fom_count >= mix_count &&
                        fom_count >= bicolor_count)
                {
                    count -= fom_count;
                    OUT_COPY_COUNT3(count, s, temp_s);
                    OUT_FOM_COUNT3(fom_count, s, fom_mask, fom_mask_len);
                    RESET_COUNTS;
                }

                fom_count = 0;
                fom_mask_len = 0;
            }

            last_line = line;
            line = line - width * 4;
            start_line--;
            lines_sent++;
        }

        if (fill_count > 3 &&
                fill_count >= color_count &&
                fill_count >= bicolor_count &&
                fill_count >= mix_count &&
                fill_count >= fom_count)
        {
            count -= fill_count;
            OUT_COPY_COUNT3(count, s, temp_s);
            OUT_FILL_COUNT3(fill_count, s);
        }
        else if (mix_count > 3 &&
                 mix_count >= color_count &&
                 mix_count >= bicolor_count &&
                 mix_count >= fill_count &&
                 mix_count >= fom_count)
        {
            count -= mix_count;
            OUT_COPY_COUNT3(count, s, temp_s);
            OUT_MIX_COUNT3(mix_count, s);
        }
        else if (color_count > 3 &&
                 color_count >= mix_count &&
                 color_count >= bicolor_count &&
                 color_count >= fill_count &&
                 color_count >= fom_count)
        {
            count -= color_count;
            OUT_COPY_COUNT3(count, s, temp_s);
            OUT_COLOR_COUNT3(color_count, s, last_pixel);
        }
        else if (bicolor_count > 3 &&
                 bicolor_count >= mix_count &&
                 bicolor_count >= color_count &&
                 bicolor_count >= fill_count &&
                 bicolor_count >= fom_count)
        {
            if ((bicolor_count % 2) == 0)
            {
                count -= bicolor_count;
                OUT_COPY_COUNT3(count, s, temp_s);
                OUT_BICOLOR_COUNT3(bicolor_count, s, bicolor1, bicolor2);
            }
            else
            {
                bicolor_count--;
                count -= bicolor_count;
                OUT_COPY_COUNT3(count, s, temp_s);
                OUT_BICOLOR_COUNT3(bicolor_count, s, bicolor2, bicolor1);
            }

            count -= bicolor_count;
            OUT_COPY_COUNT3(count, s, temp_s);
            OUT_BICOLOR_COUNT3(bicolor_count, s, bicolor1, bicolor2);
        }
        else if (fom_count > 3 &&
                 fom_count >= mix_count &&
                 fom_count >= color_count &&
                 fom_count >= fill_count &&
                 fom_count >= bicolor_count)
        {
            count -= fom_count;
            OUT_COPY_COUNT3(count, s, temp_s);
            OUT_FOM_COUNT3(fom_count, s, fom_mask, fom_mask_len);
        }
        else
        {
            OUT_COPY_COUNT3(count, s, temp_s);
        }
    }

    return lines_sent;
}
