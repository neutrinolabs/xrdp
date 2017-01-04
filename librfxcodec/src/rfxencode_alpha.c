/**
 * librfxcodec: A Remote Desktop Protocol client.
 * RemoteFX Codec Library
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

#include <rfxcodec_encode.h>

#include "rfxcommon.h"
#include "rfxencode.h"
#include "rfxconstants.h"
#include "rfxencode_tile.h"

#define LLOG_LEVEL 1
#define LLOGLN(_level, _args) \
    do { if (_level < LLOG_LEVEL) { printf _args ; printf("\n"); } } while (0)

#if 1
/*****************************************************************************/
static int
fdelta(char *in_plane, char *out_plane, int cx, int cy)
{
    char delta;
    char *src8;
    char *dst8;
    int index;
    int jndex;

    memcpy(out_plane, in_plane, cx);
    src8 = in_plane;
    dst8 = out_plane;
    for (jndex = 1; jndex < cy; jndex++)
    {
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
#endif

#if 0
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

    memcpy(out_plane, in_plane, cx);
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
#endif

/*****************************************************************************/
static int
fout(int collen, int replen, char *colptr, STREAM *s)
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
        stream_write_uint8(s, code);
        memcpy(s->p, colptr, lcollen);
        s->p += lcollen;
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
            stream_write_uint8(s, code);
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
            stream_write_uint8(s, code);
            memcpy(s->p, colptr, collen);
            s->p += collen;
            colptr += collen + lreplen;
            collen = 0;
        }
        cont = replen > 0;
    }
    return 0;
}

/*****************************************************************************/
static int
fpack(char *plane, int cx, int cy, STREAM *s)
{
    char *ptr8;
    char *colptr;
    char *lend;
    uint8 *holdp;
    int jndex;
    int collen;
    int replen;

    LLOGLN(10, ("fpack:"));
    holdp = s->p;
    for (jndex = 0; jndex < cy; jndex++)
    {
        LLOGLN(10, ("line start line %d cx %d cy %d", jndex, cx, cy));
        ptr8 = (char *) (plane + jndex * cx);
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
int
rfx_encode_plane(struct rfxencode *enc, uint8 *plane, int cx, int cy,
                 STREAM *s)
{
    char *org_plane;
    char *delta_plane;
    int bytes;
    uint8 *holdp;

    org_plane = (char *) plane;
    delta_plane = (char *) (enc->dwt_buffer1);
    fdelta(org_plane, delta_plane, cx, cy);
    holdp = s->p;
    stream_write_uint8(s, 0x10); /* flags, RLE */
    bytes = fpack(delta_plane, cx, cy, s);
    if (bytes > cx * cy)
    {
        LLOGLN(10, ("rfx_encode_plane: too big bytes %d", bytes));
        s->p = holdp;
        stream_write_uint8(s, 0); /* flags */
        memcpy(s->p, plane, cx * cy);
        s->p += cx * cy;
        stream_write_uint8(s, 0); /* pad if not RLE */
        bytes = cx * cy + 2;
    }
    else
    {
        LLOGLN(10, ("rfx_encode_plane: ok bytes %d", bytes));
    }
    return bytes;
}
