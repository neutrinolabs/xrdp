/*
Copyright 2005-2014 Jay Sorg

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

cursor

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* this should be before all X11 .h files */
#include <xorg-server.h>
#include <xorgVersion.h>

/* all driver need this */
#include <xf86.h>
#include <xf86_OSproc.h>

#include <mipointer.h>
#include <fb.h>
#include <micmap.h>
#include <mi.h>
#include <cursor.h>
#include <cursorstr.h>

#include <X11/Xarch.h>

#include "rdp.h"
#include "rdpMain.h"
#include "rdpDraw.h"
#include "rdpClientCon.h"
#include "rdpCursor.h"

#ifndef X_BYTE_ORDER
#warning X_BYTE_ORDER not defined
#endif

#if (X_BYTE_ORDER == X_LITTLE_ENDIAN)
/* Copied from Xvnc/lib/font/util/utilbitmap.c */
static unsigned char g_reverse_byte[0x100] =
{
    0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
    0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
    0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
    0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
    0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
    0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
    0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
    0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
    0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
    0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
    0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
    0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
    0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
    0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
    0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
    0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
    0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
    0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
    0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
    0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
    0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
    0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
    0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
    0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
    0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
    0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
    0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
    0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
    0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
    0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
    0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
    0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
};
#endif

/******************************************************************************/
#define LOG_LEVEL 1
#define LLOGLN(_level, _args) \
    do { if (_level < LOG_LEVEL) { ErrorF _args ; ErrorF("\n"); } } while (0)

/******************************************************************************/
Bool
rdpSpriteRealizeCursor(DeviceIntPtr pDev, ScreenPtr pScr, CursorPtr pCurs)
{
    LLOGLN(10, ("rdpSpriteRealizeCursor:"));
    return TRUE;
}

/******************************************************************************/
Bool
rdpSpriteUnrealizeCursor(DeviceIntPtr pDev, ScreenPtr pScr, CursorPtr pCurs)
{
    LLOGLN(10, ("rdpSpriteUnrealizeCursor:"));
    return TRUE;
}

/******************************************************************************/
static int
get_pixel_safe(char *data, int x, int y, int width, int height, int bpp)
{
    int start;
    int shift;
    int c;
    unsigned int *src32;

    if (x < 0)
    {
        return 0;
    }

    if (y < 0)
    {
        return 0;
    }

    if (x >= width)
    {
        return 0;
    }

    if (y >= height)
    {
        return 0;
    }

    if (bpp == 1)
    {
        width = (width + 7) / 8;
        start = (y * width) + x / 8;
        shift = x % 8;
        c = (unsigned char)(data[start]);
#if (X_BYTE_ORDER == X_LITTLE_ENDIAN)
        return (g_reverse_byte[c] & (0x80 >> shift)) != 0;
#else
        return (c & (0x80 >> shift)) != 0;
#endif
    }
    else if (bpp == 32)
    {
        src32 = (unsigned int*)data;
        return src32[y * width + x];
    }

    return 0;
}

/******************************************************************************/
static void
set_pixel_safe(char *data, int x, int y, int width, int height, int bpp,
               int pixel)
{
    int start;
    int shift;
    unsigned int *dst32;

    if (x < 0)
    {
        return;
    }

    if (y < 0)
    {
        return;
    }

    if (x >= width)
    {
        return;
    }

    if (y >= height)
    {
        return;
    }

    if (bpp == 1)
    {
        width = (width + 7) / 8;
        start = (y * width) + x / 8;
        shift = x % 8;

        if (pixel & 1)
        {
            data[start] = data[start] | (0x80 >> shift);
        }
        else
        {
            data[start] = data[start] & ~(0x80 >> shift);
        }
    }
    else if (bpp == 24)
    {
        *(data + (3 * (y * width + x)) + 0) = pixel >> 0;
        *(data + (3 * (y * width + x)) + 1) = pixel >> 8;
        *(data + (3 * (y * width + x)) + 2) = pixel >> 16;
    }
    else if (bpp == 32)
    {
        dst32 = (unsigned int*)data;
        dst32[y * width + x] = pixel;
    }
}

/******************************************************************************/
void
rdpSpriteSetCursorCon(rdpClientCon *clientCon,
                      DeviceIntPtr pDev, ScreenPtr pScr, CursorPtr pCurs,
                      int x, int y)
{
    char cur_data[32 * (32 * 4)];
    char cur_mask[32 * (32 / 8)];
    char *mask;
    char *data;
    int i;
    int j;
    int w;
    int h;
    int p;
    int xhot;
    int yhot;
    int paddedRowBytes;
    int fgcolor;
    int bgcolor;
    int bpp;

    LLOGLN(10, ("rdpSpriteSetCursorCon:"));

    w = pCurs->bits->width;
    h = pCurs->bits->height;
    if ((pCurs->bits->argb != 0) &&
        (clientCon->client_info.pointer_flags & 1))
    {
        bpp = 32;
        paddedRowBytes = PixmapBytePad(w, 32);
        xhot = pCurs->bits->xhot;
        yhot = pCurs->bits->yhot;
        data = (char *)(pCurs->bits->argb);
        memset(cur_data, 0, sizeof(cur_data));
        memset(cur_mask, 0, sizeof(cur_mask));

        for (j = 0; j < 32; j++)
        {
            for (i = 0; i < 32; i++)
            {
                p = get_pixel_safe(data, i, j, paddedRowBytes / 4, h, 32);
                set_pixel_safe(cur_data, i, 31 - j, 32, 32, 32, p);
            }
        }
    }
    else
    {
        bpp = 0;
        paddedRowBytes = PixmapBytePad(w, 1);
        xhot = pCurs->bits->xhot;
        yhot = pCurs->bits->yhot;
        data = (char *)(pCurs->bits->source);
        mask = (char *)(pCurs->bits->mask);
        fgcolor = (((pCurs->foreRed >> 8) & 0xff) << 16) |
                  (((pCurs->foreGreen >> 8) & 0xff) << 8) |
                  ((pCurs->foreBlue >> 8) & 0xff);
        bgcolor = (((pCurs->backRed >> 8) & 0xff) << 16) |
                  (((pCurs->backGreen >> 8) & 0xff) << 8) |
                  ((pCurs->backBlue >> 8) & 0xff);
        memset(cur_data, 0, sizeof(cur_data));
        memset(cur_mask, 0, sizeof(cur_mask));

        for (j = 0; j < 32; j++)
        {
            for (i = 0; i < 32; i++)
            {
                p = get_pixel_safe(mask, i, j, paddedRowBytes * 8, h, 1);
                set_pixel_safe(cur_mask, i, 31 - j, 32, 32, 1, !p);

                if (p != 0)
                {
                    p = get_pixel_safe(data, i, j, paddedRowBytes * 8, h, 1);
                    p = p ? fgcolor : bgcolor;
                    set_pixel_safe(cur_data, i, 31 - j, 32, 32, 24, p);
                }
            }
        }
    }

    rdpClientConBeginUpdate(clientCon->dev, clientCon);
    rdpClientConSetCursorEx(clientCon->dev, clientCon, xhot, yhot,
                            cur_data, cur_mask, bpp);
    rdpClientConEndUpdate(clientCon->dev, clientCon);

}

/******************************************************************************/
void
rdpSpriteSetCursor(DeviceIntPtr pDev, ScreenPtr pScr, CursorPtr pCurs,
                   int x, int y)
{
    rdpPtr dev;
    rdpClientCon *clientCon;

    LLOGLN(10, ("rdpSpriteSetCursor:"));
    if (pCurs == 0)
    {
        return;
    }

    if (pCurs->bits == 0)
    {
        return;
    }

    dev = rdpGetDevFromScreen(pScr);
    clientCon = dev->clientConHead;
    while (clientCon != NULL)
    {
        rdpSpriteSetCursorCon(clientCon, pDev, pScr, pCurs, x, y);
        clientCon = clientCon->next;
    }
}

/******************************************************************************/
void
rdpSpriteMoveCursor(DeviceIntPtr pDev, ScreenPtr pScr, int x, int y)
{
    LLOGLN(10, ("rdpSpriteMoveCursor:"));
}

/******************************************************************************/
Bool
rdpSpriteDeviceCursorInitialize(DeviceIntPtr pDev, ScreenPtr pScr)
{
    LLOGLN(10, ("rdpSpriteDeviceCursorInitialize:"));
    return TRUE;
}

/******************************************************************************/
void
rdpSpriteDeviceCursorCleanup(DeviceIntPtr pDev, ScreenPtr pScr)
{
    LLOGLN(10, ("rdpSpriteDeviceCursorCleanup:"));
    xorgxrdpDownDown(pScr);
}
