/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2014
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
 * planar bitmap compressor
 * 32 bpp compression
 */

/*
RDP 6.0 Bitmap Compression
http://msdn.microsoft.com/en-us/library/cc241877.aspx
*/

#include "libxrdp.h"

/*****************************************************************************/
/* returns the number of lines compressed */
int APP_CC
xrdp_bitmap32_compress(char *in_data, int width, int height,
                       struct stream *s, int bpp, int byte_limit,
                       int start_line, struct stream *temp_s,
                       int e)
{
    int pixel;
    int *ptr32;
    char *ptr8;
    char *alpha_data;
    char *red_data;
    char *green_data;
    char *blue_data;
    int alpha_bytes;
    int red_bytes;
    int green_bytes;
    int blue_bytes;
    int iindex;
    int jindex;
    int cx;
    int cy;

    alpha_data = g_malloc(width * height * 4, 0);
    red_data = alpha_data + width * height;
    green_data = red_data + width * height;
    blue_data = green_data + width * height;
    alpha_bytes = 0;
    red_bytes = 0;
    green_bytes = 0;
    blue_bytes = 0;
    cx = width;
    cy = 0;

    /* split planes */
    while (start_line >= 0)
    {
        ptr32 = (int *) (in_data + start_line * width * 4);
        for (iindex = 0; iindex < width; iindex++)
        {
            pixel = *ptr32;
            ptr32++;
            alpha_data[alpha_bytes] = pixel >> 24;
            alpha_bytes++;
            red_data[red_bytes] = pixel >> 16;
            red_bytes++;
            green_data[green_bytes] = pixel >> 8;
            green_bytes++;
            blue_data[blue_bytes] = pixel >> 0;
            blue_bytes++;
        }
        start_line--;
        cy++;
    }
    out_uint8(s, 0x20); /* no alpha */
    out_uint8a(s, red_data, red_bytes);
    out_uint8a(s, green_data, green_bytes);
    out_uint8a(s, blue_data, blue_bytes);
    out_uint8(s, 0x00);
    g_free(alpha_data);
    return cy;
}
