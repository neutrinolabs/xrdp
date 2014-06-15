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

#define FLAGS_RLE     0x10
#define FLAGS_NOALPHA 0x20

#define LLOG_LEVEL 1
#define LLOGLN(_level, _args) \
  do { if (_level < LLOG_LEVEL) { g_writeln _args ; } } while (0)
#define LHEXDUMP(_level, _args) \
  do { if (_level < LLOG_LEVEL) { g_hexdump _args ; } } while (0)

/*****************************************************************************/
static int APP_CC
fsplit(char *in_data, int start_line, int width, int e,
       char *alpha_data, char *red_data, char *green_data, char *blue_data)
{
    int index;
    int pixel;
    int cy;
    int alpha_bytes;
    int red_bytes;
    int green_bytes;
    int blue_bytes;
    int *ptr32;

    cy = 0;
    alpha_bytes = 0;
    red_bytes = 0;
    green_bytes = 0;
    blue_bytes = 0;
    while (start_line >= 0)
    {
        ptr32 = (int *) (in_data + start_line * width * 4);
        for (index = 0; index < width; index++)
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
        for (index = 0; index < e; index++)
        {
            alpha_data[alpha_bytes] = 0;
            alpha_bytes++;
            red_data[red_bytes] = 0;
            red_bytes++;
            green_data[green_bytes] = 0;
            green_bytes++;
            blue_data[blue_bytes] = 0;
            blue_bytes++;
        }
        start_line--;
        cy++;
    }
    return cy;
}

/*****************************************************************************/
static int APP_CC
fdelta(char *plane, int cx, int cy)
{
    char delta;
    char *ptr8;
    int index;
    int jndex;

    for (jndex = cy - 2; jndex >= 0; jndex--)
    {
        ptr8 = plane + jndex * cx;
        for (index = 0; index < cx; index++)
        {
            delta = ptr8[cx] - ptr8[0];
            if (delta & 0x80)
            {
                delta = (((~delta) + 1) << 1) - 1;
            }
            else
            {
                delta = delta << 1;
            }
            ptr8[cx] = delta;
            ptr8++;
        }
    }
    return 0;
}

/*****************************************************************************/
static int APP_CC
fout(int collen, int replen, char *colptr, struct stream *s)
{
    int code;
    int lcollen;
    int lreplen;
    int cont;

    LLOGLN(10, ("fout: collen %d replen %d", collen, replen));
    cont = collen > 13;
    while (cont)
    {
        lcollen = collen;
        if (lcollen > 15)
        {
            lcollen = 15;
        }
        code = lcollen << 4;
        out_uint8(s, code);
        out_uint8a(s, colptr, lcollen);
        colptr += lcollen;
        collen -= lcollen;
        cont = collen > 13;
    }
    cont = (collen > 0) || (replen > 0);
    while (cont)
    {
        lreplen = replen;
        if ((collen == 0) && (lreplen > 15))
        {
            /* big run */
            if (lreplen > 47)
            {
                lreplen = 47;
            }
            LLOGLN(10, ("fout: big run lreplen %d", lreplen));
            replen -= lreplen;
            code = ((lreplen & 0xF) << 4) | ((lreplen & 0xF0) >> 4);
        }
        else
        {
            if (lreplen > 15)
            {
                lreplen = 15;
            }
            replen -= lreplen;
            if (lreplen < 3)
            {
                collen += lreplen;
                lreplen = 0;
            }
            code = (collen << 4) | lreplen;
        }
        out_uint8(s, code);
        out_uint8a(s, colptr, collen);
        colptr += collen + lreplen;
        collen = 0;
        cont = replen > 0;
    }
    return 0;
}

/*****************************************************************************/
static int APP_CC
fpack(char *plane, int cx, int cy, struct stream *s)
{
    char *ptr8;
    char *colptr;
    char *lend;
    char *holdp;
    int jndex;
    int collen;
    int replen;

    LLOGLN(10, ("fpack:"));
    holdp = s->p;
    for (jndex = 0; jndex < cy; jndex++)
    {
        LLOGLN(10, ("line start line %d cx %d cy %d", jndex, cx, cy));
        ptr8 = plane + jndex * cx;
        LHEXDUMP(10, (ptr8, cx));
        lend = ptr8 + (cx - 1);
        colptr = ptr8;
        if (colptr[0] == 0)
        {
            collen = 0;
            replen = 1;
        }
        else
        {
            collen = 1;
            replen = 0;
        }
        while (ptr8 < lend)
        {
            if (ptr8[0] == ptr8[1])
            {
                replen++;
            }
            else
            {
                if (replen > 0)
                {
                    if (replen < 3)
                    {
                        collen += replen + 1;
                        replen = 0;
                    }
                    else
                    {
                        fout(collen, replen, colptr, s);
                        colptr = ptr8 + 1;
                        replen = 0;
                        collen = 1;
                    }
                }
                else
                {
                    collen++;
                }
            }
            ptr8++;
        }
        /* end of line */
        fout(collen, replen, colptr, s);
    }
    return (int) (s->p - holdp);
}

/*****************************************************************************/
/* returns the number of lines compressed */
int APP_CC
xrdp_bitmap32_compress(char *in_data, int width, int height,
                       struct stream *s, int bpp, int byte_limit,
                       int start_line, struct stream *temp_s,
                       int e)
{
    char *alpha_data;
    char *red_data;
    char *green_data;
    char *blue_data;
    int alpha_bytes;
    int red_bytes;
    int green_bytes;
    int blue_bytes;
    int cx;
    int cy;
    int header;
    int max_bytes;

    LLOGLN(10, ("xrdp_bitmap32_compress:"));

    //header = FLAGS_NOALPHA | FLAGS_RLE;
    //header = FLAGS_NOALPHA;
    header = FLAGS_RLE;

    cx = width + e;
    alpha_data = temp_s->data;
    red_data = alpha_data + cx * height;
    green_data = red_data + cx * height;
    blue_data = green_data + cx * height;

    /* split planes */
    cy = fsplit(in_data, start_line, width, e,
                alpha_data, red_data, green_data, blue_data);

    if (header & FLAGS_RLE)
    {
        out_uint8(s, header);
        if (header & FLAGS_NOALPHA)
        {
            fdelta(red_data, cx, cy);
            fdelta(green_data, cx, cy);
            fdelta(blue_data, cx, cy);
            red_bytes = fpack(red_data, cx, cy, s);
            green_bytes = fpack(green_data, cx, cy, s);
            blue_bytes = fpack(blue_data, cx, cy, s);
            max_bytes = cx * cy * 3;
        }
        else
        {
            fdelta(alpha_data, cx, cy);
            fdelta(red_data, cx, cy);
            fdelta(green_data, cx, cy);
            fdelta(blue_data, cx, cy);
            alpha_bytes = fpack(alpha_data, cx, cy, s);
            red_bytes = fpack(red_data, cx, cy, s);
            green_bytes = fpack(green_data, cx, cy, s);
            blue_bytes = fpack(blue_data, cx, cy, s);
            max_bytes = cx * cy * 4;
        }
        if (alpha_bytes + red_bytes + green_bytes + blue_bytes > max_bytes)
        {
            LLOGLN(10, ("xrdp_bitmap32_compress: too big, argb "
                   "bytes %d %d %d %d cx %d cy %d", alpha_bytes, red_bytes,
                   green_bytes, blue_bytes, cx, cy));
        }
    }
    else
    {
        out_uint8(s, header);
        red_bytes = cx * cy;
        green_bytes = cx * cy;
        blue_bytes = cx * cy;
        if (header & FLAGS_NOALPHA)
        {
            out_uint8a(s, red_data, red_bytes);
            out_uint8a(s, green_data, green_bytes);
            out_uint8a(s, blue_data, blue_bytes);
        }
        else
        {
            alpha_bytes = cx * cy;
            out_uint8a(s, alpha_data, alpha_bytes);
            out_uint8a(s, red_data, red_bytes);
            out_uint8a(s, green_data, green_bytes);
            out_uint8a(s, blue_data, blue_bytes);
        }
        /* pad if no RLE */
        out_uint8(s, 0x00);
    }

    return cy;
}
