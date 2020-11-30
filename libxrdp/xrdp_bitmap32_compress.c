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

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "libxrdp.h"

#define FLAGS_RLE     0x10
#define FLAGS_NOALPHA 0x20



/*****************************************************************************/
/* split RGB */
static int
fsplit3(char *in_data, int start_line, int width, int e,
        char *r_data, char *g_data, char *b_data)
{
#if defined(L_ENDIAN)
    int rp;
    int gp;
    int bp;
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
            rp  = (pixel >> 16) & 0x000000ff;
            gp  = (pixel >>  8) & 0x000000ff;
            bp  = (pixel >>  0) & 0x000000ff;
            pixel  = *ptr32;
            ptr32++;
            rp |= (pixel >>  8) & 0x0000ff00;
            gp |= (pixel <<  0) & 0x0000ff00;
            bp |= (pixel <<  8) & 0x0000ff00;
            pixel = *ptr32;
            ptr32++;
            rp |= (pixel >>  0) & 0x00ff0000;
            gp |= (pixel <<  8) & 0x00ff0000;
            bp |= (pixel << 16) & 0x00ff0000;
            pixel = *ptr32;
            ptr32++;
            rp |= (pixel <<  8) & 0xff000000;
            gp |= (pixel << 16) & 0xff000000;
            bp |= (pixel << 24) & 0xff000000;
            *((int *)(r_data + out_index)) = rp;
            *((int *)(g_data + out_index)) = gp;
            *((int *)(b_data + out_index)) = bp;
            out_index += 4;
            index += 4;
        }
#endif
        while (index < width)
        {
            pixel = *ptr32;
            ptr32++;
            r_data[out_index] = pixel >> 16;
            g_data[out_index] = pixel >> 8;
            b_data[out_index] = pixel >> 0;
            out_index++;
            index++;
        }
        for (index = 0; index < e; index++)
        {
            r_data[out_index] = r_data[out_index - 1];
            g_data[out_index] = g_data[out_index - 1];
            b_data[out_index] = b_data[out_index - 1];
            out_index++;
        }
        start_line--;
        cy++;
        if (out_index + width + e > 64 * 64)
        {
            break;
        }
    }
    return cy;
}

/*****************************************************************************/
/* split ARGB */
static int
fsplit4(char *in_data, int start_line, int width, int e,
        char *a_data, char *r_data, char *g_data, char *b_data)
{
#if defined(L_ENDIAN)
    int ap;
    int rp;
    int gp;
    int bp;
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
            ap  = (pixel >> 24) & 0x000000ff;
            rp  = (pixel >> 16) & 0x000000ff;
            gp  = (pixel >>  8) & 0x000000ff;
            bp  = (pixel >>  0) & 0x000000ff;
            pixel  = *ptr32;
            ptr32++;
            ap |= (pixel >> 16) & 0x0000ff00;
            rp |= (pixel >>  8) & 0x0000ff00;
            gp |= (pixel <<  0) & 0x0000ff00;
            bp |= (pixel <<  8) & 0x0000ff00;
            pixel = *ptr32;
            ptr32++;
            ap |= (pixel >>  8) & 0x00ff0000;
            rp |= (pixel >>  0) & 0x00ff0000;
            gp |= (pixel <<  8) & 0x00ff0000;
            bp |= (pixel << 16) & 0x00ff0000;
            pixel = *ptr32;
            ptr32++;
            ap |= (pixel <<  0) & 0xff000000;
            rp |= (pixel <<  8) & 0xff000000;
            gp |= (pixel << 16) & 0xff000000;
            bp |= (pixel << 24) & 0xff000000;
            *((int *)(a_data + out_index)) = ap;
            *((int *)(r_data + out_index)) = rp;
            *((int *)(g_data + out_index)) = gp;
            *((int *)(b_data + out_index)) = bp;
            out_index += 4;
            index += 4;
        }
#endif
        while (index < width)
        {
            pixel = *ptr32;
            ptr32++;
            a_data[out_index] = pixel >> 24;
            r_data[out_index] = pixel >> 16;
            g_data[out_index] = pixel >> 8;
            b_data[out_index] = pixel >> 0;
            out_index++;
            index++;
        }
        for (index = 0; index < e; index++)
        {
            a_data[out_index] = a_data[out_index - 1];
            r_data[out_index] = r_data[out_index - 1];
            g_data[out_index] = g_data[out_index - 1];
            b_data[out_index] = b_data[out_index - 1];
            out_index++;
        }
        start_line--;
        cy++;
        if (out_index + width + e > 64 * 64)
        {
            break;
        }
    }
    return cy;
}

/*****************************************************************************/
#define DELTA_ONE \
    do { \
        delta = src8[cx] - src8[0]; \
        is_neg = (delta >> 7) & 1; \
        dst8[cx] = (((delta ^ -is_neg) + is_neg) << 1) - is_neg; \
        src8++; \
        dst8++; \
    } while (0)

/*****************************************************************************/
static int
fdelta(char *in_plane, char *out_plane, int cx, int cy)
{
    char delta;
    char is_neg;
    char *src8;
    char *dst8;
    char *src8_end;

    g_memcpy(out_plane, in_plane, cx);
    src8 = in_plane;
    dst8 = out_plane;
    src8_end = src8 + (cx * cy - cx);
    while (src8 + 8 <= src8_end)
    {
        DELTA_ONE;
        DELTA_ONE;
        DELTA_ONE;
        DELTA_ONE;
        DELTA_ONE;
        DELTA_ONE;
        DELTA_ONE;
        DELTA_ONE;
    }
    while (src8 < src8_end)
    {
        DELTA_ONE;
    }
    return 0;
}

/*****************************************************************************/
static int
fout(int collen, int replen, char *colptr, struct stream *s)
{
    int code;
    int lcollen;
    int lreplen;
    int cont;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "fout: collen %d replen %d", collen, replen);
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
            LOG_DEVEL(LOG_LEVEL_DEBUG, "fout: big run lreplen %d", lreplen);
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
static int
fpack(char *plane, int cx, int cy, struct stream *s)
{
    char *ptr8;
    char *colptr;
    char *lend;
    char *holdp;
    int jndex;
    int collen;
    int replen;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "fpack:");
    holdp = s->p;
    for (jndex = 0; jndex < cy; jndex++)
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "line start line %d cx %d cy %d", jndex, cx, cy);
        ptr8 = plane + jndex * cx;
        LOG_DEVEL_HEXDUMP(LOG_LEVEL_TRACE, "line content", ptr8, cx);
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
static int
foutraw3(struct stream *s, int bytes, int header,
         char *r_data, char *g_data, char *b_data)
{
    out_uint8(s, header);
    out_uint8a(s, r_data, bytes);
    out_uint8a(s, g_data, bytes);
    out_uint8a(s, b_data, bytes);
    /* pad if no RLE */
    out_uint8(s, 0x00);
    return 0;
}

/*****************************************************************************/
static int
foutraw4(struct stream *s, int bytes, int header,
         char *a_data, char *r_data, char *g_data, char *b_data)
{
    out_uint8(s, header);
    out_uint8a(s, a_data, bytes);
    out_uint8a(s, r_data, bytes);
    out_uint8a(s, g_data, bytes);
    out_uint8a(s, b_data, bytes);
    /* pad if no RLE */
    out_uint8(s, 0x00);
    return 0;
}

/*****************************************************************************/
/* returns the number of lines compressed */
int
xrdp_bitmap32_compress(char *in_data, int width, int height,
                       struct stream *s, int bpp, int byte_limit,
                       int start_line, struct stream *temp_s,
                       int e, int flags)
{
    char *a_data;
    char *r_data;
    char *g_data;
    char *b_data;
    char *sa_data;
    char *sr_data;
    char *sg_data;
    char *sb_data;
    char *hold_p;
    int a_bytes;
    int r_bytes;
    int g_bytes;
    int b_bytes;
    int cx;
    int cy;
    int max_bytes;
    int total_bytes;
    int header;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "xrdp_bitmap32_compress:");
    max_bytes = 4 * 1024;
    /* need max 8, 4K planes for work */
    if (max_bytes * 8 > temp_s->size)
    {
        return 0;
    }
    header = flags & 0xFF;
    cx = width + e;
    sa_data = temp_s->data;
    sr_data = sa_data + max_bytes;
    sg_data = sr_data + max_bytes;
    sb_data = sg_data + max_bytes;
    a_data = sb_data + max_bytes;
    r_data = a_data + max_bytes;
    g_data = r_data + max_bytes;
    b_data = g_data + max_bytes;
    hold_p = s->p;

    if (header & FLAGS_NOALPHA)
    {
        cy = fsplit3(in_data, start_line, width, e,
                     sr_data, sg_data, sb_data);
        if (header & FLAGS_RLE)
        {
            fdelta(sr_data, r_data, cx, cy);
            fdelta(sg_data, g_data, cx, cy);
            fdelta(sb_data, b_data, cx, cy);
            while (cy > 0)
            {
                s->p = hold_p;
                out_uint8(s, header);
                r_bytes = fpack(r_data, cx, cy, s);
                g_bytes = fpack(g_data, cx, cy, s);
                b_bytes = fpack(b_data, cx, cy, s);
                max_bytes = cx * cy * 3;
                total_bytes = r_bytes + g_bytes + b_bytes;
                if (total_bytes > max_bytes)
                {
                    if (2 + max_bytes <= byte_limit)
                    {
                        s->p = hold_p;
                        foutraw3(s, cx * cy, FLAGS_NOALPHA, sr_data, sg_data, sb_data);
                        break;
                    }
                }
                if (1 + total_bytes <= byte_limit)
                {
                    break;
                }
                cy--;
            }
        }
        else
        {
            while (cy > 0)
            {
                max_bytes = cx * cy * 3;
                if (2 + max_bytes <= byte_limit)
                {
                    s->p = hold_p;
                    foutraw3(s, cx * cy, FLAGS_NOALPHA, sr_data, sg_data, sb_data);
                    break;
                }
                cy--;
            }
        }
    }
    else
    {
        cy = fsplit4(in_data, start_line, width, e,
                     sa_data, sr_data, sg_data, sb_data);
        if (header & FLAGS_RLE)
        {
            fdelta(sa_data, a_data, cx, cy);
            fdelta(sr_data, r_data, cx, cy);
            fdelta(sg_data, g_data, cx, cy);
            fdelta(sb_data, b_data, cx, cy);
            while (cy > 0)
            {
                s->p = hold_p;
                out_uint8(s, header);
                a_bytes = fpack(a_data, cx, cy, s);
                r_bytes = fpack(r_data, cx, cy, s);
                g_bytes = fpack(g_data, cx, cy, s);
                b_bytes = fpack(b_data, cx, cy, s);
                max_bytes = cx * cy * 4;
                total_bytes = a_bytes + r_bytes + g_bytes + b_bytes;
                if (total_bytes > max_bytes)
                {
                    if (2 + max_bytes <= byte_limit)
                    {
                        s->p = hold_p;
                        foutraw4(s, cx * cy, 0, sa_data, sr_data, sg_data, sb_data);
                        break;
                    }
                }
                if (1 + total_bytes <= byte_limit)
                {
                    break;
                }
                cy--;
            }
        }
        else
        {
            while (cy > 0)
            {
                max_bytes = cx * cy * 4;
                if (2 + max_bytes <= byte_limit)
                {
                    s->p = hold_p;
                    foutraw4(s, cx * cy, 0, sa_data, sr_data, sg_data, sb_data);
                    break;
                }
                cy--;
            }
        }
    }
    return cy;
}
