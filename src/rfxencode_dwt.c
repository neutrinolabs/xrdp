/**
 * FreeRDP: A Remote Desktop Protocol client.
 * RemoteFX Codec Library - DWT
 *
 * Copyright 2011 Vic Lee
 * Copyright 2014 Jay Sorg <jay.sorg@gmail.com>
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

#include "rfxcommon.h"

/******************************************************************************/
static int
rfx_dwt_2d_encode_horz(sint16 *buffer, sint16 *dwt, int subband_width)
{
    sint16 *l_src, *h_src;
    sint16 *hl, *lh, *hh, *ll;
    int x, y;
    int n;

    /* DWT in horizontal direction, results in 4 sub-bands in
     * HL(0), LH(1), HH(2), LL(3) order, stored in original buffer. */
    /* The lower part L generates LL(3) and HL(0). */
    /* The higher part H generates LH(1) and HH(2). */

    ll = buffer + subband_width * subband_width * 3;
    hl = buffer;
    l_src = dwt;

    lh = buffer + subband_width * subband_width;
    hh = buffer + subband_width * subband_width * 2;
    h_src = dwt + subband_width * subband_width * 2;

    for (y = 0; y < subband_width; y++)
    {

        /* pre */
        hl[0] = (l_src[1] - ((l_src[0] + l_src[2]) >> 1)) >> 1;
        ll[0] = l_src[0] + hl[0];

        /* loop */
        for (n = 1; n < subband_width - 1; n++)
        {
            x = n << 1;
            hl[n] = (l_src[x + 1] - ((l_src[x] + l_src[x + 2]) >> 1)) >> 1;
            ll[n] = l_src[x] + ((hl[n - 1] + hl[n]) >> 1);
        }

        /* post */
        n = subband_width - 1;
        x = n << 1;
        hl[n] = (l_src[x + 1] - ((l_src[x] + l_src[x]) >> 1)) >> 1;
        ll[n] = l_src[x] + ((hl[n - 1] + hl[n]) >> 1);

        /* pre */
        hh[0] = (h_src[1] - ((h_src[0] + h_src[2]) >> 1)) >> 1;
        lh[0] = h_src[0] + hh[0];

        /* loop */
        for (n = 1; n < subband_width - 1; n++)
        {
            x = n << 1;
            hh[n] = (h_src[x + 1] - ((h_src[x] + h_src[x + 2]) >> 1)) >> 1;
            lh[n] = h_src[x] + ((hh[n - 1] + hh[n]) >> 1);
        }

        /* post */
        n = subband_width - 1;
        x = n << 1;
        hh[n] = (h_src[x + 1] - ((h_src[x] + h_src[x]) >> 1)) >> 1;
        lh[n] = h_src[x] + ((hh[n - 1] + hh[n]) >> 1);

        ll += subband_width;
        hl += subband_width;
        l_src += subband_width << 1;

        lh += subband_width;
        hh += subband_width;
        h_src += subband_width << 1;
    }
    return 0;
}

/******************************************************************************/
static int
rfx_dwt_2d_encode_block(sint16 *buffer, sint16 *dwt, int subband_width)
{
    sint16 *src, *l, *h;
    int total_width;
    int x, y;
    int n;

    total_width = subband_width << 1;

    /* DWT in vertical direction, results in 2 sub-bands in L, H order in
     * tmp buffer dwt. */
    for (x = 0; x < total_width; x++)
    {

        /* pre */
        l = dwt + x;
        h = l + subband_width * total_width;
        src = buffer + x;
        *h = (src[total_width] - ((src[0] + src[2 * total_width]) >> 1)) >> 1;
        *l = src[0] + (*h);

        /* loop */
        for (n = 1; n < subband_width - 1; n++)
        {
            y = n << 1;
            l = dwt + n * total_width + x;
            h = l + subband_width * total_width;
            src = buffer + y * total_width + x;
            *h = (src[total_width] - ((src[0] + src[2 * total_width]) >> 1)) >> 1;
            *l = src[0] + ((*(h - total_width) + *h) >> 1);
        }

        /* post */
        n = subband_width - 1;
        y = n << 1;
        l = dwt + n * total_width + x;
        h = l + subband_width * total_width;
        src = buffer + y * total_width + x;
        *h = (src[total_width] - ((src[0] + src[0]) >> 1)) >> 1;
        *l = src[0] + ((*(h - total_width) + *h) >> 1);

    }

    return rfx_dwt_2d_encode_horz(buffer, dwt, subband_width);
}

/******************************************************************************/
static int
rfx_dwt_2d_encode_block8(sint8 *in_buffer,
                         sint16 *buffer, sint16 *dwt, int subband_width)
{
    sint8 *src;
    sint16 *l, *h;
    int total_width;
    int x, y;
    int n;

    total_width = subband_width << 1;

    /* DWT in vertical direction, results in 2 sub-bands in L, H order in
     * tmp buffer dwt. */
    for (x = 0; x < total_width; x++)
    {

        /* pre */
        l = dwt + x;
        h = l + subband_width * total_width;
        src = in_buffer + x;
        *h = (src[total_width] - ((src[0] + src[2 * total_width]) >> 1)) >> 1;
        *l = src[0] + *h;

        /* loop */
        for (n = 1; n < subband_width - 1; n++)
        {
            y = n << 1;
            l = dwt + n * total_width + x;
            h = l + subband_width * total_width;
            src = in_buffer + y * total_width + x;
            *h = (src[total_width] - ((src[0] + src[2 * total_width]) >> 1)) >> 1;
            *l = src[0] + ((*(h - total_width) + *h) >> 1);
        }

        /* post */
        n = subband_width - 1;
        y = n << 1;
        l = dwt + n * total_width + x;
        h = l + subband_width * total_width;
        src = in_buffer + y * total_width + x;
        *h = (src[total_width] - ((src[0] + src[0]) >> 1)) >> 1;
        *l = src[0] + ((*(h - total_width) + *h) >> 1);

    }

    return rfx_dwt_2d_encode_horz(buffer, dwt, subband_width);
}

/******************************************************************************/
int
rfx_dwt_2d_encode(sint8 *in_buffer, sint16 *buffer, sint16 *dwt_buffer)
{
    rfx_dwt_2d_encode_block8(in_buffer, buffer, dwt_buffer, 32);
    rfx_dwt_2d_encode_block(buffer + 3072, dwt_buffer, 16);
    rfx_dwt_2d_encode_block(buffer + 3840, dwt_buffer, 8);
    return 0;
}
