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
/* split ARGB */
static int APP_CC
fsplit4(char *in_data, int start_line, int width, int e,
        char *alpha_data, char *red_data, char *green_data, char *blue_data)
{
#if defined(L_ENDIAN)
    int alpha;
    int red;
    int green;
    int blue;
#endif
    int index;
    int out_index;
    int pixel;
    int cy;
    int *ptr32;

    cy = 0;
    out_index = 0;
    while (start_line >= 0)
    {
        ptr32 = (int *) (in_data + start_line * width * 4);
        index = 0;
#if defined(L_ENDIAN)
        while (index + 4 <= width)
        {
            pixel = *ptr32;
            ptr32++;
            alpha  = (pixel >> 24) & 0x000000ff;
            red    = (pixel >> 16) & 0x000000ff;
            green  = (pixel >>  8) & 0x000000ff;
            blue   = (pixel >>  0) & 0x000000ff;
            pixel  = *ptr32;
            ptr32++;
            alpha |= (pixel >> 16) & 0x0000ff00;
            red   |= (pixel >>  8) & 0x0000ff00;
            green |= (pixel <<  0) & 0x0000ff00;
            blue  |= (pixel <<  8) & 0x0000ff00;
            pixel = *ptr32;
            ptr32++;
            alpha |= (pixel >>  8) & 0x00ff0000;
            red   |= (pixel >>  0) & 0x00ff0000;
            green |= (pixel <<  8) & 0x00ff0000;
            blue  |= (pixel << 16) & 0x00ff0000;
            pixel = *ptr32;
            ptr32++;
            alpha |= (pixel <<  0) & 0xff000000;
            red   |= (pixel <<  8) & 0xff000000;
            green |= (pixel << 16) & 0xff000000;
            blue  |= (pixel << 24) & 0xff000000;
            *((int*)(alpha_data + out_index)) = alpha;
            *((int*)(red_data + out_index)) = red;
            *((int*)(green_data + out_index)) = green;
            *((int*)(blue_data + out_index)) = blue;
            out_index += 4;
            index += 4;
        }
#endif
        while (index < width)
        {
            pixel = *ptr32;
            ptr32++;
            alpha_data[out_index] = pixel >> 24;
            red_data[out_index] = pixel >> 16;
            green_data[out_index] = pixel >> 8;
            blue_data[out_index] = pixel >> 0;
            out_index++;
            index++;
        }
        for (index = 0; index < e; index++)
        {
            alpha_data[out_index] = alpha_data[out_index - 1];
            red_data[out_index] = red_data[out_index - 1];
            green_data[out_index] = green_data[out_index - 1];
            blue_data[out_index] = blue_data[out_index - 1];
            out_index++;
        }
        start_line--;
        cy++;
    }
    return cy;
}

/*****************************************************************************/
/* split RGB */
static int APP_CC
fsplit3(char *in_data, int start_line, int width, int e,
        char *red_data, char *green_data, char *blue_data)
{
#if defined(L_ENDIAN)
    int red;
    int green;
    int blue;
#endif
    int index;
    int out_index;
    int pixel;
    int cy;
    int *ptr32;

    cy = 0;
    out_index = 0;
    while (start_line >= 0)
    {
        ptr32 = (int *) (in_data + start_line * width * 4);
        index = 0;
#if defined(L_ENDIAN)
        while (index + 4 <= width)
        {
            pixel = *ptr32;
            ptr32++;
            red    = (pixel >> 16) & 0x000000ff;
            green  = (pixel >>  8) & 0x000000ff;
            blue   = (pixel >>  0) & 0x000000ff;
            pixel  = *ptr32;
            ptr32++;
            red   |= (pixel >>  8) & 0x0000ff00;
            green |= (pixel <<  0) & 0x0000ff00;
            blue  |= (pixel <<  8) & 0x0000ff00;
            pixel = *ptr32;
            ptr32++;
            red   |= (pixel >>  0) & 0x00ff0000;
            green |= (pixel <<  8) & 0x00ff0000;
            blue  |= (pixel << 16) & 0x00ff0000;
            pixel = *ptr32;
            ptr32++;
            red   |= (pixel <<  8) & 0xff000000;
            green |= (pixel << 16) & 0xff000000;
            blue  |= (pixel << 24) & 0xff000000;
            *((int*)(red_data + out_index)) = red;
            *((int*)(green_data + out_index)) = green;
            *((int*)(blue_data + out_index)) = blue;
            out_index += 4;
            index += 4;
        }
#endif
        while (index < width)
        {
            pixel = *ptr32;
            ptr32++;
            red_data[out_index] = pixel >> 16;
            green_data[out_index] = pixel >> 8;
            blue_data[out_index] = pixel >> 0;
            out_index++;
            index++;
        }
        for (index = 0; index < e; index++)
        {
            red_data[out_index] = red_data[out_index - 1];
            green_data[out_index] = green_data[out_index - 1];
            blue_data[out_index] = blue_data[out_index - 1];
            out_index++;
        }
        start_line--;
        cy++;
    }
    return cy;
}

/*****************************************************************************/
static int APP_CC
fdelta(char *in_plane, char *out_plane, int cx, int cy)
{
    char delta;
    char *src8;
    char *dst8;
    int index;
    int jndex;

    g_memcpy(out_plane, in_plane, cx);
    for (jndex = cy - 2; jndex >= 0; jndex--)
    {
        src8 = in_plane + jndex * cx;
        dst8 = out_plane + jndex * cx;
        for (index = 0; index < cx; index++)
        {
            delta = src8[cx] - src8[0];
            if (delta & 0x80)
            {
                delta = (((~delta) + 1) << 1) - 1;
            }
            else
            {
                delta = delta << 1;
            }
            dst8[cx] = delta;
            src8++;
            dst8++;
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
            out_uint8(s, code);
            colptr += lreplen;
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
            out_uint8(s, code);
            out_uint8a(s, colptr, collen);
            colptr += collen + lreplen;
            collen = 0;
        }
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
static int APP_CC
foutraw3(struct stream *s, int bytes, int header,
         char *red_data, char *green_data, char *blue_data)
{
    out_uint8(s, header);
    out_uint8a(s, red_data, bytes);
    out_uint8a(s, green_data, bytes);
    out_uint8a(s, blue_data, bytes);
    /* pad if no RLE */
    out_uint8(s, 0x00);
    return 0;
}

/*****************************************************************************/
static int APP_CC
foutraw4(struct stream *s, int bytes, int header,
         char *alpha_data, char *red_data, char *green_data, char *blue_data)
{
    out_uint8(s, header);
    out_uint8a(s, alpha_data, bytes);
    out_uint8a(s, red_data, bytes);
    out_uint8a(s, green_data, bytes);
    out_uint8a(s, blue_data, bytes);
    /* pad if no RLE */
    out_uint8(s, 0x00);
    return 0;
}

/*****************************************************************************/
/* returns the number of lines compressed */
int APP_CC
xrdp_bitmap32_compress(char *in_data, int width, int height,
                       struct stream *s, int bpp, int byte_limit,
                       int start_line, struct stream *temp_s,
                       int e, int flags)
{
    char *alpha_data;
    char *red_data;
    char *green_data;
    char *blue_data;
    char *salpha_data;
    char *sred_data;
    char *sgreen_data;
    char *sblue_data;
    int alpha_bytes;
    int red_bytes;
    int green_bytes;
    int blue_bytes;
    int cx;
    int cy;
    int max_bytes;
    int total_bytes;
    int header;

    LLOGLN(10, ("xrdp_bitmap32_compress:"));

    header = flags & 0xFF;
    cx = width + e;
    salpha_data = temp_s->data;
    sred_data = salpha_data + cx * height;
    sgreen_data = sred_data + cx * height;
    sblue_data = sgreen_data + cx * height;
    alpha_data = sblue_data + cx * height;
    red_data = alpha_data + cx * height;
    green_data = red_data + cx * height;
    blue_data = green_data + cx * height;

    if (header & FLAGS_NOALPHA)
    {
        cy = fsplit3(in_data, start_line, width, e,
                     sred_data, sgreen_data, sblue_data);
        if (header & FLAGS_RLE)
        {
            fdelta(sred_data, red_data, cx, cy);
            fdelta(sgreen_data, green_data, cx, cy);
            fdelta(sblue_data, blue_data, cx, cy);
            out_uint8(s, header);
            red_bytes = fpack(red_data, cx, cy, s);
            green_bytes = fpack(green_data, cx, cy, s);
            blue_bytes = fpack(blue_data, cx, cy, s);
            total_bytes = red_bytes + green_bytes + blue_bytes;
            if (1 + total_bytes > byte_limit)
            {
                /* failed */
                LLOGLN(0, ("xrdp_bitmap32_compress: too big, rgb "
                       "bytes %d %d %d total_bytes %d cx %d cy %d byte_limit %d",
                       red_bytes, green_bytes, blue_bytes,
                       total_bytes, cx, cy, byte_limit));
                return 0;
            }
            max_bytes = cx * cy * 3;
            if (total_bytes > max_bytes)
            {
                /* raw is better */
                LLOGLN(10, ("xrdp_bitmap32_compress: too big, rgb "
                       "bytes %d %d %d total_bytes %d cx %d cy %d max_bytes %d",
                       red_bytes, green_bytes, blue_bytes,
                       total_bytes, cx, cy, max_bytes));
                init_stream(s, 0);
                foutraw3(s, cx * cy, FLAGS_NOALPHA, sred_data,
                         sgreen_data, sblue_data);
            }
        }
        else
        {
            foutraw3(s, cx * cy, FLAGS_NOALPHA, sred_data,
                     sgreen_data, sblue_data);
        }
    }
    else
    {
        cy = fsplit4(in_data, start_line, width, e,
                     salpha_data, sred_data, sgreen_data, sblue_data);
        if (header & FLAGS_RLE)
        {
            fdelta(salpha_data, alpha_data, cx, cy);
            fdelta(sred_data, red_data, cx, cy);
            fdelta(sgreen_data, green_data, cx, cy);
            fdelta(sblue_data, blue_data, cx, cy);
            out_uint8(s, header);
            alpha_bytes = fpack(alpha_data, cx, cy, s);
            red_bytes = fpack(red_data, cx, cy, s);
            green_bytes = fpack(green_data, cx, cy, s);
            blue_bytes = fpack(blue_data, cx, cy, s);
            max_bytes = cx * cy * 4;
            total_bytes = alpha_bytes + red_bytes + green_bytes + blue_bytes;
            if (1 + total_bytes > byte_limit)
            {
                /* failed */
                LLOGLN(0, ("xrdp_bitmap32_compress: too big, argb "
                       "bytes %d %d %d %d total_bytes %d cx %d cy %d byte_limit %d",
                       alpha_bytes, red_bytes, green_bytes, blue_bytes,
                       total_bytes, cx, cy, byte_limit));
                return 0;
            }
            if (total_bytes > max_bytes)
            {
                /* raw is better */
                LLOGLN(10, ("xrdp_bitmap32_compress: too big, argb "
                       "bytes %d %d %d %d total_bytes %d cx %d cy %d max_bytes %d",
                       alpha_bytes, red_bytes, green_bytes, blue_bytes,
                       total_bytes, cx, cy, max_bytes));
                init_stream(s, 0);
                foutraw4(s, cx * cy, 0, salpha_data, sred_data,
                         sgreen_data, sblue_data);
            }
        }
        else
        {
            foutraw4(s, cx * cy, 0, salpha_data, sred_data,
                     sgreen_data, sblue_data);
        }
    }
    return cy;
}
