/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Laxmikant Rashinkar 2013 LK.Rashinkar@gmail.com
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
#include <stdint.h>
#include <string.h>

/**
 * An optimized hexdump function
 *
 * @param  address  address to display in address column
 * @param  buf      data to hexdump
 * @param  len      number of bytes to dump
 *****************************************************************************/

void hexdump(int address, char *buf, int len)
{
    uint32_t addr;
    char     outbuf[80];
    int      blocks;
    int      residual;
    int      i;
    int      j;
    int      buf_index;
    int      index2;     /* data column    */
    int      index3;     /* ascii column   */

    unsigned char c;

    char cvt[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                  '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
                 };

    if ((buf == NULL) || (len <= 0))
    {
        return;
    }

    addr = (address < 0) ? 0 : address;
    blocks = len / 16;
    residual = len - blocks * 16;
    buf_index = 0;

    for (i = 0; i < blocks; i++)
    {
        index2 = 10;
        index3 = 60;

        outbuf[9] = ' ';
        outbuf[8] = ' ';
        outbuf[7] = cvt[(addr >>  0) & 0x0000000f];
        outbuf[6] = cvt[(addr >>  4) & 0x0000000f];
        outbuf[5] = cvt[(addr >>  8) & 0x0000000f];
        outbuf[4] = cvt[(addr >> 12) & 0x0000000f];
        outbuf[3] = cvt[(addr >> 16) & 0x0000000f];
        outbuf[2] = cvt[(addr >> 20) & 0x0000000f];
        outbuf[1] = cvt[(addr >> 24) & 0x0000000f];
        outbuf[0] = cvt[(addr >> 28) & 0x0000000f];
        addr += 16;

        /* insert spaces */
        outbuf[8] = ' ';
        outbuf[9] = ' ';
        outbuf[58] = ' ';
        outbuf[59] = ' ';

        for (j = 0; j < 16; j++)
        {
            c = buf[buf_index++];

            outbuf[index2++] = cvt[(c >> 4) & 0x0f];
            outbuf[index2++] = cvt[(c >> 0) & 0x0f];
            outbuf[index2++] = ' ';

            if ((c >= 0x20) && (c <= 0x7e))
            {
                outbuf[index3++] = c;
            }
            else
            {
                outbuf[index3++] = '.';
            }
        }

        outbuf[index3] = 0;
        puts(outbuf);
    }

    if (!residual)
    {
        return;
    }

    outbuf[7] = cvt[(addr >>  0) & 0x0000000f];
    outbuf[6] = cvt[(addr >>  4) & 0x0000000f];
    outbuf[5] = cvt[(addr >>  8) & 0x0000000f];
    outbuf[4] = cvt[(addr >> 12) & 0x0000000f];
    outbuf[3] = cvt[(addr >> 16) & 0x0000000f];
    outbuf[2] = cvt[(addr >> 20) & 0x0000000f];
    outbuf[1] = cvt[(addr >> 24) & 0x0000000f];
    outbuf[0] = cvt[(addr >> 28) & 0x0000000f];
    addr += 16;

    index2 = 10;
    index3 = 60;
    memset(&outbuf[8], ' ', 68);

    for (j = 0; j < residual; j++)
    {
        c = buf[buf_index++];

        outbuf[index2++] = cvt[(c >> 4) & 0x0f];
        outbuf[index2++] = cvt[(c >> 0) & 0x0f];
        outbuf[index2++] = ' ';

        if ((c >= 0x20) && (c <= 0x7e))
        {
            outbuf[index3++] = c;
        }
        else
        {
            outbuf[index3++] = '.';
        }
    }

    outbuf[index3] = 0;
    puts(outbuf);
}
