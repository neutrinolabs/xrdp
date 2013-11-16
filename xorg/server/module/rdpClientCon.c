/*
Copyright 2005-2013 Jay Sorg

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

Client connection to xrdp

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>

/* this should be before all X11 .h files */
#include <xorg-server.h>

/* all driver need this */
#include <xf86.h>
#include <xf86_OSproc.h>

#include "rdp.h"
#include "rdpDraw.h"
#include "rdpClientCon.h"
#include "rdpMisc.h"

#define LOG_LEVEL 1
#define LLOGLN(_level, _args) \
    do { if (_level < LOG_LEVEL) { ErrorF _args ; ErrorF("\n"); } } while (0)

#define LTOUI32(_in) ((unsigned int)(_in))

#define USE_MAX_OS_BYTES 1
#define MAX_OS_BYTES (16 * 1024 * 1024)

/*
0 GXclear,        0
1 GXnor,          DPon
2 GXandInverted,  DPna
3 GXcopyInverted, Pn
4 GXandReverse,   PDna
5 GXinvert,       Dn
6 GXxor,          DPx
7 GXnand,         DPan
8 GXand,          DPa
9 GXequiv,        DPxn
a GXnoop,         D
b GXorInverted,   DPno
c GXcopy,         P
d GXorReverse,   PDno
e GXor,          DPo
f GXset          1
*/

static int g_rdp_opcodes[16] =
{
    0x00, /* GXclear        0x0 0 */
    0x88, /* GXand          0x1 src AND dst */
    0x44, /* GXandReverse   0x2 src AND NOT dst */
    0xcc, /* GXcopy         0x3 src */
    0x22, /* GXandInverted  0x4 NOT src AND dst */
    0xaa, /* GXnoop         0x5 dst */
    0x66, /* GXxor          0x6 src XOR dst */
    0xee, /* GXor           0x7 src OR dst */
    0x11, /* GXnor          0x8 NOT src AND NOT dst */
    0x99, /* GXequiv        0x9 NOT src XOR dst */
    0x55, /* GXinvert       0xa NOT dst */
    0xdd, /* GXorReverse    0xb src OR NOT dst */
    0x33, /* GXcopyInverted 0xc NOT src */
    0xbb, /* GXorInverted   0xd NOT src OR dst */
    0x77, /* GXnand         0xe NOT src OR NOT dst */
    0xff  /* GXset          0xf 1 */
};

static int
rdpClientConSendPending(rdpPtr dev, rdpClientCon *clientCon);
static int
rdpClientConSendMsg(rdpPtr dev, rdpClientCon *clientCon);

/******************************************************************************/
static int
rdpClientConGotConnection(ScreenPtr pScreen, rdpPtr dev)
{
    rdpClientCon *clientCon;

    LLOGLN(0, ("rdpClientConGotConnection:"));
    clientCon = (rdpClientCon *) g_malloc(sizeof(rdpClientCon), 1);
    make_stream(clientCon->in_s);
    init_stream(clientCon->in_s, 8192);
    make_stream(clientCon->out_s);
    init_stream(clientCon->out_s, 8192 * 4 + 100);
    if (dev->clientConTail == NULL)
    {
        dev->clientConHead = clientCon;
        dev->clientConTail = clientCon;
    }
    else
    {
        dev->clientConTail->next = clientCon;
        dev->clientConTail = clientCon;
    }
    return 0;
}

/******************************************************************************/
static int
rdpClientConGotData(ScreenPtr pScreen, rdpPtr dev, rdpClientCon *clientCon)
{
    LLOGLN(0, ("rdpClientConGotData:"));
    return 0;
}

/******************************************************************************/
static int
rdpClientConGotControlConnection(ScreenPtr pScreen, rdpPtr dev,
                                 rdpClientCon *clientCon)
{
    LLOGLN(0, ("rdpClientConGotControlConnection:"));
    return 0;
}

/******************************************************************************/
static int
rdpClientConGotControlData(ScreenPtr pScreen, rdpPtr dev,
                           rdpClientCon *clientCon)
{
    LLOGLN(0, ("rdpClientConGotControlData:"));
    return 0;
}

/******************************************************************************/
int
rdpClientConCheck(ScreenPtr pScreen)
{
    rdpPtr dev;
    rdpClientCon *clientCon;
    fd_set rfds;
    struct timeval time;
    int max;
    int sel;
    int count;

    LLOGLN(10, ("rdpClientConCheck:"));
    dev = rdpGetDevFromScreen(pScreen);
    time.tv_sec = 0;
    time.tv_usec = 0;
    FD_ZERO(&rfds);
    count = 0;
    max = 0;
    if (dev->listen_sck > 0)
    {
        count++;
        FD_SET(LTOUI32(dev->listen_sck), &rfds);
        max = RDPMAX(dev->listen_sck, max);
    }
    clientCon = dev->clientConHead;
    while (clientCon != NULL)
    {
        if (clientCon->sck > 0)
        {
            count++;
            FD_SET(LTOUI32(clientCon->sck), &rfds);
            max = RDPMAX(clientCon->sck, max);
        }
        if (clientCon->sckControl > 0)
        {
            count++;
            FD_SET(LTOUI32(clientCon->sckControl), &rfds);
            max = RDPMAX(clientCon->sckControl, max);
        }
        if (clientCon->sckControlListener > 0)
        {
            count++;
            FD_SET(LTOUI32(clientCon->sckControlListener), &rfds);
            max = RDPMAX(clientCon->sckControlListener, max);
        }
        clientCon = clientCon->next;
    }
    if (count < 1)
    {
        sel = 0;
    }
    else
    {
        sel = select(max + 1, &rfds, 0, 0, &time);
    }
    if (sel < 1)
    {
        LLOGLN(10, ("rdpClientConCheck: no select"));
        return 0;
    }
    if (dev->listen_sck > 0)
    {
        if (FD_ISSET(LTOUI32(dev->listen_sck), &rfds))
        {
            rdpClientConGotConnection(pScreen, dev);
        }
    }
    clientCon = dev->clientConHead;
    while (clientCon != NULL)
    {
        if (clientCon->sck > 0)
        {
            if (FD_ISSET(LTOUI32(clientCon->sck), &rfds))
            {
                rdpClientConGotData(pScreen, dev, clientCon);
            }
        }
        if (clientCon->sckControlListener > 0)
        {
            if (FD_ISSET(LTOUI32(clientCon->sckControlListener), &rfds))
            {
                rdpClientConGotControlConnection(pScreen, dev, clientCon);
            }
        }
        if (clientCon->sckControl > 0)
        {
            if (FD_ISSET(LTOUI32(clientCon->sckControl), &rfds))
            {
                rdpClientConGotControlData(pScreen, dev, clientCon);
            }
        }
        clientCon = clientCon->next;
    }
    return 0;
}

/******************************************************************************/
int
rdpClientConInit(rdpPtr dev)
{
    int i;

    if (!g_directory_exist("/tmp/.xrdp"))
    {
        if (!g_create_dir("/tmp/.xrdp"))
        {
            if (!g_directory_exist("/tmp/.xrdp"))
            {
                LLOGLN(0, ("rdpup_init: g_create_dir failed"));
                return 0;
            }
        }
        g_chmod_hex("/tmp/.xrdp", 0x1777);
    }
    i = atoi(display);
    if (i < 1)
    {
        LLOGLN(0, ("rdpClientConInit: can not run at display < 1"));
        return 0;
    }
    g_sprintf(dev->uds_data, "/tmp/.xrdp/xrdp_display_%s", display);
    if (dev->listen_sck == 0)
    {
        unlink(dev->uds_data);
        dev->listen_sck = g_sck_local_socket_stream();
        if (g_sck_local_bind(dev->listen_sck, dev->uds_data) != 0)
        {
            LLOGLN(0, ("rdpClientConInit: g_tcp_local_bind failed"));
            return 1;
        }
        g_sck_listen(dev->listen_sck);
        AddEnabledDevice(dev->listen_sck);
    }
    return 0;
}

/******************************************************************************/
int
rdpClientConDeinit(rdpPtr dev)
{
    LLOGLN(0, ("rdpClientConDeinit:"));
    if (dev->listen_sck != 0)
    {
        close(dev->listen_sck);
        unlink(dev->uds_data);
    }
    return 0;
}

/******************************************************************************/
static CARD32
rdpClientConDeferredDisconnectCallback(OsTimerPtr timer, CARD32 now,
                                       pointer arg)
{
    CARD32 lnow_ms;
    rdpPtr dev;

    LLOGLN(10, ("rdpClientConDeferredDisconnectCallback"));
    dev = (rdpPtr) arg;
    if (dev->clientConHead != NULL) /* is there any connection ? */
    {
        LLOGLN(0, ("rdpClientConDeferredDisconnectCallback: one connected"));
        if (dev->disconnectTimer != NULL)
        {
            LLOGLN(0, ("rdpClientConDeferredDisconnectCallback: "
                   "canceling disconnectTimer"));
            TimerCancel(dev->disconnectTimer);
            TimerFree(dev->disconnectTimer);
            dev->disconnectTimer = NULL;
        }
        dev->disconnectScheduled = FALSE;
        return 0;
    }
    else
    {
        LLOGLN(10, ("rdpClientConDeferredDisconnectCallback: not connected"));
    }
    lnow_ms = GetTimeInMillis();
    if (lnow_ms - dev->disconnect_time_ms > dev->disconnect_timeout_s * 1000)
    {
        LLOGLN(0, ("rdpClientConDeferredDisconnectCallback: exit X11rdp"));
        kill(getpid(), SIGTERM);
        return 0;
    }
    dev->disconnectTimer = TimerSet(dev->disconnectTimer, 0, 1000 * 10,
                                    rdpClientConDeferredDisconnectCallback,
                                    dev);
    return 0;
}


/*****************************************************************************/
static int
rdpClientConDisconnect(rdpPtr dev, rdpClientCon *clientCon)
{
    //int index;

    LLOGLN(0, ("rdpClientConDisconnect:"));
    if (dev->do_kill_disconnected)
    {
        if (dev->disconnect_scheduled == FALSE)
        {
            LLOGLN(0, ("rdpClientConDisconnect: starting g_dis_timer"));
            dev->disconnectTimer = TimerSet(dev->disconnectTimer, 0, 1000 * 10,
                         rdpClientConDeferredDisconnectCallback, dev);
            dev->disconnect_scheduled = TRUE;
        }
        dev->disconnect_time_ms = GetTimeInMillis();
    }

    //rdpClientConDelete(dev, clientCon);

#if 0

    // TODO

    RemoveEnabledDevice(clientCon->sck);
    clientCon->connected = FALSE;
    g_sck_close(clientCon->sck);
    clientCon->sck = 0;
    clientCon->sckClosed = TRUE;
    clientCon->osBitmapNumUsed = 0;
    clientCon->rdpIndex = -1;

    if (clientCon->maxOsBitmaps > 0)
    {
        for (index = 0; index < clientCon->maxOsBitmaps; index++)
        {
            if (clientCon->osBitmaps[index].used)
            {
                if (g_os_bitmaps[index].priv != 0)
                {
                    g_os_bitmaps[index].priv->status = 0;
                }
            }
        }
    }
    g_os_bitmap_alloc_size = 0;

    g_max_os_bitmaps = 0;
    g_free(g_os_bitmaps);
    g_os_bitmaps = 0;
    g_use_rail = 0;
    g_do_glyph_cache = 0;
    g_do_composite = 0;

#endif

    return 0;
}

/******************************************************************************/
int
rdpClientConBeginUpdate(rdpPtr dev, rdpClientCon *clientCon)
{
    LLOGLN(10, ("rdpClientConBeginUpdate:"));

    if (clientCon->connected)
    {
        if (clientCon->begin)
        {
            return 0;
        }
        init_stream(clientCon->out_s, 0);
        s_push_layer(clientCon->out_s, iso_hdr, 8);
        out_uint16_le(clientCon->out_s, 1); /* begin update */
        out_uint16_le(clientCon->out_s, 4); /* size */
        clientCon->begin = TRUE;
        clientCon->count = 1;
    }

    return 0;
}

/******************************************************************************/
int
rdpClientConEndUpdate(rdpPtr dev, rdpClientCon *clientCon)
{
    LLOGLN(10, ("rdpClientConEndUpdate"));

    if (clientCon->connected && clientCon->begin)
    {
        if (dev->do_dirty_ons)
        {
            /* in this mode, end update is only called in check dirty */
            rdpClientConSendPending(dev, clientCon);
        }
        else
        {
            rdpClientConScheduleDeferredUpdate(dev);
        }
    }

    return 0;
}

/******************************************************************************/
int
rdpClientConPreCheck(rdpPtr dev, rdpClientCon *clientCon, int in_size)
{
    int rv;

    rv = 0;
    if (clientCon->begin == FALSE)
    {
        rdpClientConBeginUpdate(dev, clientCon);
    }

    if ((clientCon->out_s->p - clientCon->out_s->data) >
        (clientCon->out_s->size - (in_size + 20)))
    {
        s_mark_end(clientCon->out_s);
        if (rdpClientConSendMsg(dev, clientCon) != 0)
        {
            LLOGLN(0, ("rdpClientConPreCheck: rdpup_send_msg failed"));
            rv = 1;
        }
        clientCon->count = 0;
        init_stream(clientCon->out_s, 0);
        s_push_layer(clientCon->out_s, iso_hdr, 8);
    }

    return rv;
}

/******************************************************************************/
int
rdpClientConFillRect(rdpPtr dev, rdpClientCon *clientCon,
                     short x, short y, int cx, int cy)
{
    if (clientCon->connected)
    {
        LLOGLN(10, ("rdpClientConFillRect:"));
        rdpClientConPreCheck(dev, clientCon, 12);
        out_uint16_le(clientCon->out_s, 3); /* fill rect */
        out_uint16_le(clientCon->out_s, 12); /* size */
        clientCon->count++;
        out_uint16_le(clientCon->out_s, x);
        out_uint16_le(clientCon->out_s, y);
        out_uint16_le(clientCon->out_s, cx);
        out_uint16_le(clientCon->out_s, cy);
    }

    return 0;
}

/******************************************************************************/
int
rdpClientConScreenBlt(rdpPtr dev, rdpClientCon *clientCon,
                      short x, short y, int cx, int cy, short srcx, short srcy)
{
    if (clientCon->connected)
    {
        LLOGLN(10, ("rdpClientConScreenBlt: x %d y %d cx %d cy %d "
               "srcx %d srcy %d",
               x, y, cx, cy, srcx, srcy));
        rdpClientConPreCheck(dev, clientCon, 16);
        out_uint16_le(clientCon->out_s, 4); /* screen blt */
        out_uint16_le(clientCon->out_s, 16); /* size */
        clientCon->count++;
        out_uint16_le(clientCon->out_s, x);
        out_uint16_le(clientCon->out_s, y);
        out_uint16_le(clientCon->out_s, cx);
        out_uint16_le(clientCon->out_s, cy);
        out_uint16_le(clientCon->out_s, srcx);
        out_uint16_le(clientCon->out_s, srcy);
    }

    return 0;
}

/******************************************************************************/
int
rdpClientConSetClip(rdpPtr dev, rdpClientCon *clientCon,
                    short x, short y, int cx, int cy)
{
    if (clientCon->connected)
    {
        LLOGLN(10, ("rdpClientConSetClip:"));
        rdpClientConPreCheck(dev, clientCon, 12);
        out_uint16_le(clientCon->out_s, 10); /* set clip */
        out_uint16_le(clientCon->out_s, 12); /* size */
        clientCon->count++;
        out_uint16_le(clientCon->out_s, x);
        out_uint16_le(clientCon->out_s, y);
        out_uint16_le(clientCon->out_s, cx);
        out_uint16_le(clientCon->out_s, cy);
    }

    return 0;
}

/******************************************************************************/
int
rdpClientConResetClip(rdpPtr dev, rdpClientCon *clientCon)
{
    if (clientCon->connected)
    {
        LLOGLN(10, ("rdpClientConResetClip:"));
        rdpClientConPreCheck(dev, clientCon, 4);
        out_uint16_le(clientCon->out_s, 11); /* reset clip */
        out_uint16_le(clientCon->out_s, 4); /* size */
        clientCon->count++;
    }

    return 0;
}

#define COLOR8(r, g, b) \
    ((((r) >> 5) << 0)  | (((g) >> 5) << 3) | (((b) >> 6) << 6))
#define COLOR15(r, g, b) \
    ((((r) >> 3) << 10) | (((g) >> 3) << 5) | (((b) >> 3) << 0))
#define COLOR16(r, g, b) \
    ((((r) >> 3) << 11) | (((g) >> 2) << 5) | (((b) >> 3) << 0))
#define COLOR24(r, g, b) \
    ((((r) >> 0) << 0)  | (((g) >> 0) << 8) | (((b) >> 0) << 16))
#define SPLITCOLOR32(r, g, b, c) \
    { \
        r = ((c) >> 16) & 0xff; \
        g = ((c) >> 8) & 0xff; \
        b = (c) & 0xff; \
    }

/******************************************************************************/
int
rdpClientConConvertPixel(rdpPtr dev, rdpClientCon *clientCon, int in_pixel)
{
    int red;
    int green;
    int blue;
    int rv;

    rv = 0;

    if (dev->depth == 24)
    {
        if (clientCon->rdp_bpp == 24)
        {
            rv = in_pixel;
            SPLITCOLOR32(red, green, blue, rv);
            rv = COLOR24(red, green, blue);
        }
        else if (clientCon->rdp_bpp == 16)
        {
            rv = in_pixel;
            SPLITCOLOR32(red, green, blue, rv);
            rv = COLOR16(red, green, blue);
        }
        else if (clientCon->rdp_bpp == 15)
        {
            rv = in_pixel;
            SPLITCOLOR32(red, green, blue, rv);
            rv = COLOR15(red, green, blue);
        }
        else if (clientCon->rdp_bpp == 8)
        {
            rv = in_pixel;
            SPLITCOLOR32(red, green, blue, rv);
            rv = COLOR8(red, green, blue);
        }
    }
    else if (dev->depth == clientCon->rdp_bpp)
    {
        return in_pixel;
    }

    return rv;
}

/******************************************************************************/
int
rdpClientConConvertPixels(rdpPtr dev, rdpClientCon *clientCon,
                          void *src, void *dst, int num_pixels)
{
    unsigned int pixel;
    unsigned int red;
    unsigned int green;
    unsigned int blue;
    unsigned int *src32;
    unsigned int *dst32;
    unsigned short *dst16;
    unsigned char *dst8;
    int index;

    if (dev->depth == clientCon->rdp_bpp)
    {
        memcpy(dst, src, num_pixels * dev->Bpp);
        return 0;
    }

    if (dev->depth == 24)
    {
        src32 = (unsigned int *)src;

        if (clientCon->rdp_bpp == 24)
        {
            dst32 = (unsigned int *)dst;

            for (index = 0; index < num_pixels; index++)
            {
                pixel = *src32;
                *dst32 = pixel;
                dst32++;
                src32++;
            }
        }
        else if (clientCon->rdp_bpp == 16)
        {
            dst16 = (unsigned short *)dst;

            for (index = 0; index < num_pixels; index++)
            {
                pixel = *src32;
                SPLITCOLOR32(red, green, blue, pixel);
                pixel = COLOR16(red, green, blue);
                *dst16 = pixel;
                dst16++;
                src32++;
            }
        }
        else if (clientCon->rdp_bpp == 15)
        {
            dst16 = (unsigned short *)dst;

            for (index = 0; index < num_pixels; index++)
            {
                pixel = *src32;
                SPLITCOLOR32(red, green, blue, pixel);
                pixel = COLOR15(red, green, blue);
                *dst16 = pixel;
                dst16++;
                src32++;
            }
        }
        else if (clientCon->rdp_bpp == 8)
        {
            dst8 = (unsigned char *)dst;

            for (index = 0; index < num_pixels; index++)
            {
                pixel = *src32;
                SPLITCOLOR32(red, green, blue, pixel);
                pixel = COLOR8(red, green, blue);
                *dst8 = pixel;
                dst8++;
                src32++;
            }
        }
    }

    return 0;
}

/******************************************************************************/
int
rdpClientConAlphaPixels(void* src, void* dst, int num_pixels)
{
    unsigned int* src32;
    unsigned char* dst8;
    int index;

    src32 = (unsigned int*)src;
    dst8 = (unsigned char*)dst;
    for (index = 0; index < num_pixels; index++)
    {
        *dst8 = (*src32) >> 24;
        dst8++;
        src32++;
    }
    return 0;
}

/******************************************************************************/
int
rdpClientConSetFgcolor(rdpPtr dev, rdpClientCon *clientCon, int fgcolor)
{
    if (clientCon->connected)
    {
        LLOGLN(10, ("rdpClientConSetFgcolor:"));
        rdpClientConPreCheck(dev, clientCon, 8);
        out_uint16_le(clientCon->out_s, 12); /* set fgcolor */
        out_uint16_le(clientCon->out_s, 8); /* size */
        clientCon->count++;
        fgcolor = fgcolor & dev->Bpp_mask;
        fgcolor = rdpClientConConvertPixel(dev, clientCon, fgcolor) &
                  clientCon->rdp_Bpp_mask;
        out_uint32_le(clientCon->out_s, fgcolor);
    }

    return 0;
}

/******************************************************************************/
int
rdpClientConSetBgcolor(rdpPtr dev, rdpClientCon *clientCon, int bgcolor)
{
    if (clientCon->connected)
    {
        LLOGLN(10, ("rdpClientConSetBgcolor:"));
        rdpClientConPreCheck(dev, clientCon, 8);
        out_uint16_le(clientCon->out_s, 13); /* set bg color */
        out_uint16_le(clientCon->out_s, 8); /* size */
        clientCon->count++;
        bgcolor = bgcolor & dev->Bpp_mask;
        bgcolor = rdpClientConConvertPixel(dev, clientCon, bgcolor) &
                  clientCon->rdp_Bpp_mask;
        out_uint32_le(clientCon->out_s, bgcolor);
    }

    return 0;
}

/******************************************************************************/
int
rdpClientConSetOpcode(rdpPtr dev, rdpClientCon *clientCon, int opcode)
{
    if (clientCon->connected)
    {
        LLOGLN(10, ("rdpClientConSetOpcode:"));
        rdpClientConPreCheck(dev, clientCon, 6);
        out_uint16_le(clientCon->out_s, 14); /* set opcode */
        out_uint16_le(clientCon->out_s, 6); /* size */
        clientCon->count++;
        out_uint16_le(clientCon->out_s, g_rdp_opcodes[opcode & 0xf]);
    }

    return 0;
}

/******************************************************************************/
int
rdpClientConSetPen(rdpPtr dev, rdpClientCon *clientCon, int style, int width)
{
    if (clientCon->connected)
    {
        LLOGLN(10, ("rdpClientConSetPen:"));
        rdpClientConPreCheck(dev, clientCon, 8);
        out_uint16_le(clientCon->out_s, 17); /* set pen */
        out_uint16_le(clientCon->out_s, 8); /* size */
        clientCon->count++;
        out_uint16_le(clientCon->out_s, style);
        out_uint16_le(clientCon->out_s, width);
    }

    return 0;
}

/******************************************************************************/
int
rdpClientConDrawLine(rdpPtr dev, rdpClientCon *clientCon,
                     short x1, short y1, short x2, short y2)
{
    if (clientCon->connected)
    {
        LLOGLN(10, ("rdpClientConDrawLine:"));
        rdpClientConPreCheck(dev, clientCon, 12);
        out_uint16_le(clientCon->out_s, 18); /* draw line */
        out_uint16_le(clientCon->out_s, 12); /* size */
        clientCon->count++;
        out_uint16_le(clientCon->out_s, x1);
        out_uint16_le(clientCon->out_s, y1);
        out_uint16_le(clientCon->out_s, x2);
        out_uint16_le(clientCon->out_s, y2);
    }

    return 0;
}

/******************************************************************************/
int
rdpClientConSetCursor(rdpPtr dev, rdpClientCon *clientCon,
                      short x, short y, char *cur_data, char *cur_mask)
{
    int size;

    if (clientCon->connected)
    {
        LLOGLN(10, ("rdpClientConSetCursor:"));
        size = 8 + 32 * (32 * 3) + 32 * (32 / 8);
        rdpClientConPreCheck(dev, clientCon, size);
        out_uint16_le(clientCon->out_s, 19); /* set cursor */
        out_uint16_le(clientCon->out_s, size); /* size */
        clientCon->count++;
        x = RDPMAX(0, x);
        x = RDPMIN(31, x);
        y = RDPMAX(0, y);
        y = RDPMIN(31, y);
        out_uint16_le(clientCon->out_s, x);
        out_uint16_le(clientCon->out_s, y);
        out_uint8a(clientCon->out_s, cur_data, 32 * (32 * 3));
        out_uint8a(clientCon->out_s, cur_mask, 32 * (32 / 8));
    }

    return 0;
}

/******************************************************************************/
int
rdpClientConSetCursorEx(rdpPtr dev, rdpClientCon *clientCon,
                        short x, short y, char *cur_data,
                        char *cur_mask, int bpp)
{
    int size;
    int Bpp;

    if (clientCon->connected)
    {
        LLOGLN(10, ("rdpClientConSetCursorEx:"));
        Bpp = (bpp == 0) ? 3 : (bpp + 7) / 8;
        size = 10 + 32 * (32 * Bpp) + 32 * (32 / 8);
        rdpClientConPreCheck(dev, clientCon, size);
        out_uint16_le(clientCon->out_s, 51); /* set cursor ex */
        out_uint16_le(clientCon->out_s, size); /* size */
        clientCon->count++;
        x = RDPMAX(0, x);
        x = RDPMIN(31, x);
        y = RDPMAX(0, y);
        y = RDPMIN(31, y);
        out_uint16_le(clientCon->out_s, x);
        out_uint16_le(clientCon->out_s, y);
        out_uint16_le(clientCon->out_s, bpp);
        out_uint8a(clientCon->out_s, cur_data, 32 * (32 * Bpp));
        out_uint8a(clientCon->out_s, cur_mask, 32 * (32 / 8));
    }

    return 0;
}

/******************************************************************************/
int
rdpClientConCreateOsSurface(rdpPtr dev, rdpClientCon *clientCon,
                            int rdpindex, int width, int height)
{
    LLOGLN(10, ("rdpClientConCreateOsSurface:"));

    if (clientCon->connected)
    {
        LLOGLN(10, ("rdpClientConCreateOsSurface: width %d height %d", width, height));
        rdpClientConPreCheck(dev, clientCon, 12);
        out_uint16_le(clientCon->out_s, 20);
        out_uint16_le(clientCon->out_s, 12);
        clientCon->count++;
        out_uint32_le(clientCon->out_s, rdpindex);
        out_uint16_le(clientCon->out_s, width);
        out_uint16_le(clientCon->out_s, height);
    }

    return 0;
}

/******************************************************************************/
int
rdpClientConCreateOsSurfaceBpp(rdpPtr dev, rdpClientCon *clientCon,
                               int rdpindex, int width, int height, int bpp)
{
    LLOGLN(10, ("rdpClientConCreateOsSurfaceBpp:"));
    if (clientCon->connected)
    {
        LLOGLN(10, ("rdpClientConCreateOsSurfaceBpp: width %d height %d "
               "bpp %d", width, height, bpp));
        rdpClientConPreCheck(dev, clientCon, 13);
        out_uint16_le(clientCon->out_s, 31);
        out_uint16_le(clientCon->out_s, 13);
        clientCon->count++;
        out_uint32_le(clientCon->out_s, rdpindex);
        out_uint16_le(clientCon->out_s, width);
        out_uint16_le(clientCon->out_s, height);
        out_uint8(clientCon->out_s, bpp);
    }
    return 0;
}

/******************************************************************************/
int
rdpClientConSwitchOsSurface(rdpPtr dev, rdpClientCon *clientCon, int rdpindex)
{
    LLOGLN(10, ("rdpClientConSwitchOsSurface:"));

    if (clientCon->connected)
    {
        if (clientCon->rdpIndex == rdpindex)
        {
            return 0;
        }

        clientCon->rdpIndex = rdpindex;
        LLOGLN(10, ("rdpClientConSwitchOsSurface: rdpindex %d", rdpindex));
        /* switch surface */
        rdpClientConPreCheck(dev, clientCon, 8);
        out_uint16_le(clientCon->out_s, 21);
        out_uint16_le(clientCon->out_s, 8);
        out_uint32_le(clientCon->out_s, rdpindex);
        clientCon->count++;
    }

    return 0;
}

/******************************************************************************/
int
rdpClientConDeleteOsSurface(rdpPtr dev, rdpClientCon *clientCon, int rdpindex)
{
    LLOGLN(10, ("rdpClientConDeleteOsSurface: rdpindex %d", rdpindex));

    if (clientCon->connected)
    {
        LLOGLN(10, ("rdpClientConDeleteOsSurface: rdpindex %d", rdpindex));
        rdpClientConPreCheck(dev, clientCon, 8);
        out_uint16_le(clientCon->out_s, 22);
        out_uint16_le(clientCon->out_s, 8);
        clientCon->count++;
        out_uint32_le(clientCon->out_s, rdpindex);
    }

    return 0;
}

/*****************************************************************************/
/* returns -1 on error */
int
rdpClientConAddOsBitmap(rdpPtr dev, rdpClientCon *clientCon,
                        PixmapPtr pixmap, rdpPixmapPtr priv)
{
    int index;
    int rv;
    int oldest;
    int oldest_index;
    int this_bytes;

    LLOGLN(10, ("rdpClientConAddOsBitmap:"));
    if (clientCon->connected == FALSE)
    {
        LLOGLN(10, ("rdpClientConAddOsBitmap: test error 1"));
        return -1;
    }

    if (clientCon->osBitmaps == NULL)
    {
        LLOGLN(10, ("rdpClientConAddOsBitmap: test error 2"));
        return -1;
    }

    this_bytes = pixmap->devKind * pixmap->drawable.height;
    if (this_bytes > MAX_OS_BYTES)
    {
        LLOGLN(10, ("rdpClientConAddOsBitmap: error, too big this_bytes %d "
               "width %d height %d", this_bytes,
               pixmap->drawable.height, pixmap->drawable.height));
        return -1;
    }

    oldest = 0x7fffffff;
    oldest_index = -1;
    rv = -1;
    index = 0;

    while (index < clientCon->maxOsBitmaps)
    {
        if (clientCon->osBitmaps[index].used == FALSE)
        {
            clientCon->osBitmaps[index].used = TRUE;
            clientCon->osBitmaps[index].pixmap = pixmap;
            clientCon->osBitmaps[index].priv = priv;
            clientCon->osBitmaps[index].stamp = clientCon->osBitmapStamp;
            clientCon->osBitmapStamp++;
            clientCon->osBitmapNumUsed++;
            rv = index;
            break;
        }
        else
        {
            if (clientCon->osBitmaps[index].stamp < oldest)
            {
                oldest = clientCon->osBitmaps[index].stamp;
                oldest_index = index;
            }
        }
        index++;
    }

    if (rv == -1)
    {
        if (oldest_index == -1)
        {
            LLOGLN(0, ("rdpClientConAddOsBitmap: error"));
        }
        else
        {
            LLOGLN(10, ("rdpClientConAddOsBitmap: too many pixmaps removing "
                   "oldest_index %d", oldest_index));
            rdpClientConRemoveOsBitmap(dev, clientCon, oldest_index);
            rdpClientConDeleteOsSurface(dev, clientCon, oldest_index);
            clientCon->osBitmaps[oldest_index].used = TRUE;
            clientCon->osBitmaps[oldest_index].pixmap = pixmap;
            clientCon->osBitmaps[oldest_index].priv = priv;
            clientCon->osBitmaps[oldest_index].stamp = clientCon->osBitmapStamp;
            clientCon->osBitmapStamp++;
            clientCon->osBitmapNumUsed++;
            rv = oldest_index;
        }
    }

    if (rv < 0)
    {
        LLOGLN(10, ("rdpClientConAddOsBitmap: test error 3"));
        return rv;
    }

    clientCon->osBitmapAllocSize += this_bytes;
    LLOGLN(10, ("rdpClientConAddOsBitmap: this_bytes %d "
           "clientCon->osBitmapAllocSize %d",
           this_bytes, clientCon->osBitmapAllocSize));
#if USE_MAX_OS_BYTES
    while (clientCon->osBitmapAllocSize > MAX_OS_BYTES)
    {
        LLOGLN(10, ("rdpClientConAddOsBitmap: must delete "
               "clientCon->osBitmapNumUsed %d",
               clientCon->osBitmapNumUsed));
        /* find oldest */
        oldest = 0x7fffffff;
        oldest_index = -1;
        index = 0;
        while (index < clientCon->maxOsBitmaps)
        {
            if (clientCon->osBitmaps[index].used &&
                (clientCon->osBitmaps[index].stamp < oldest))
            {
                oldest = clientCon->osBitmaps[index].stamp;
                oldest_index = index;
            }
            index++;
        }
        if (oldest_index == -1)
        {
            LLOGLN(0, ("rdpClientConAddOsBitmap: error 1"));
            break;
        }
        if (oldest_index == rv)
        {
            LLOGLN(0, ("rdpClientConAddOsBitmap: error 2"));
            break;
        }
        rdpClientConRemoveOsBitmap(dev, clientCon, oldest_index);
        rdpClientConDeleteOsSurface(dev, clientCon, oldest_index);
    }
#endif
    LLOGLN(10, ("rdpClientConAddOsBitmap: new bitmap index %d", rv));
    LLOGLN(10, ("rdpClientConAddOsBitmap: clientCon->osBitmapNumUsed %d "
           "clientCon->osBitmapStamp 0x%8.8x",
           clientCon->osBitmapNumUsed, clientCon->osBitmapStamp));
    return rv;
}

/*****************************************************************************/
int
rdpClientConRemoveOsBitmap(rdpPtr dev, rdpClientCon *clientCon, int rdpindex)
{
    PixmapPtr pixmap;
    rdpPixmapPtr priv;
    int this_bytes;

    if (clientCon->osBitmaps == NULL)
    {
        LLOGLN(10, ("rdpClientConRemoveOsBitmap: test error 1"));
        return 1;
    }

    LLOGLN(10, ("rdpClientConRemoveOsBitmap: index %d stamp %d",
           rdpindex, clientCon->osBitmaps[rdpindex].stamp));

    if ((rdpindex < 0) && (rdpindex >= clientCon->maxOsBitmaps))
    {
        LLOGLN(10, ("rdpClientConRemoveOsBitmap: test error 2"));
        return 1;
    }

    if (clientCon->osBitmaps[rdpindex].used)
    {
        pixmap = clientCon->osBitmaps[rdpindex].pixmap;
        priv = clientCon->osBitmaps[rdpindex].priv;
        rdpDrawItemRemoveAll(dev, priv);
        this_bytes = pixmap->devKind * pixmap->drawable.height;
        clientCon->osBitmapAllocSize -= this_bytes;
        LLOGLN(10, ("rdpClientConRemoveOsBitmap: this_bytes %d "
               "clientCon->osBitmapAllocSize %d", this_bytes,
               clientCon->osBitmapAllocSize));
        clientCon->osBitmaps[rdpindex].used = 0;
        clientCon->osBitmaps[rdpindex].pixmap = 0;
        clientCon->osBitmaps[rdpindex].priv = 0;
        clientCon->osBitmapNumUsed--;
        priv->status = 0;
        priv->con_number = 0;
        priv->use_count = 0;
    }
    else
    {
        LLOGLN(0, ("rdpup_remove_os_bitmap: error"));
    }

    LLOGLN(10, ("rdpup_remove_os_bitmap: clientCon->osBitmapNumUsed %d",
           clientCon->osBitmapNumUsed));
    return 0;
}

/*****************************************************************************/
int
rdpClientConUpdateOsUse(rdpPtr dev, rdpClientCon *clientCon, int rdpindex)
{
    if (clientCon->osBitmaps == NULL)
    {
        return 1;
    }

    LLOGLN(10, ("rdpClientConUpdateOsUse: index %d stamp %d",
           rdpindex, clientCon->osBitmaps[rdpindex].stamp));

    if ((rdpindex < 0) && (rdpindex >= clientCon->maxOsBitmaps))
    {
        return 1;
    }

    if (clientCon->osBitmaps[rdpindex].used)
    {
        clientCon->osBitmaps[rdpindex].stamp = clientCon->osBitmapStamp;
        clientCon->osBitmapStamp++;
    }
    else
    {
        LLOGLN(0, ("rdpClientConUpdateOsUse: error rdpindex %d", rdpindex));
    }

    return 0;
}

/*****************************************************************************/
/* returns error */
static int
rdpClientConSend(rdpPtr dev, rdpClientCon *clientCon, char *data, int len)
{
    int sent;

    LLOGLN(10, ("rdpClientConSend - sending %d bytes", len));

    if (clientCon->sckClosed)
    {
        return 1;
    }

    while (len > 0)
    {
        sent = g_sck_send(clientCon->sck, data, len, 0);

        if (sent == -1)
        {
            if (g_sck_last_error_would_block(clientCon->sck))
            {
                g_sleep(1);
            }
            else
            {
                LLOGLN(0, ("rdpClientConSend: g_tcp_send failed(returned -1)"));
                rdpClientConDisconnect(dev, clientCon);
                return 1;
            }
        }
        else if (sent == 0)
        {
            LLOGLN(0, ("rdpClientConSend: g_tcp_send failed(returned zero)"));
            rdpClientConDisconnect(dev, clientCon);
            return 1;
        }
        else
        {
            data += sent;
            len -= sent;
        }
    }

    return 0;
}

/******************************************************************************/
static int
rdpClientConSendMsg(rdpPtr dev, rdpClientCon *clientCon)
{
    int len;
    int rv;
    struct stream *s;

    rv = 1;
    s = clientCon->out_s;
    if (s != NULL)
    {
        len = (int) (s->end - s->data);

        if (len > s->size)
        {
            LLOGLN(0, ("rdpClientConSendMsg: overrun error len %d count %d",
                   len, clientCon->count));
        }

        s_pop_layer(s, iso_hdr);
        out_uint16_le(s, 3);
        out_uint16_le(s, clientCon->count);
        out_uint32_le(s, len - 8);
        rv = rdpClientConSend(dev, clientCon, s->data, len);
    }

    if (rv != 0)
    {
        LLOGLN(0, ("rdpClientConSendMsg: error in rdpup_send_msg"));
    }

    return rv;
}

/******************************************************************************/
static int
rdpClientConSendPending(rdpPtr dev, rdpClientCon *clientCon)
{
    int rv;

    rv = 0;
    if (clientCon->connected && clientCon->begin)
    {
        out_uint16_le(clientCon->out_s, 2); /* XR_SERVER_END_UPDATE */
        out_uint16_le(clientCon->out_s, 4); /* size */
        clientCon->count++;
        s_mark_end(clientCon->out_s);
        if (rdpClientConSendMsg(dev, clientCon) != 0)
        {
            LLOGLN(0, ("rdpClientConSendPending: rdpClientConSendMsg failed"));
            rv = 1;
        }
    }
    clientCon->count = 0;
    clientCon->begin = FALSE;
    return rv;
}

/******************************************************************************/
static CARD32
rdpClientConDeferredUpdateCallback(OsTimerPtr timer, CARD32 now, pointer arg)
{
    rdpPtr dev;
    rdpClientCon *clientCon;

    LLOGLN(10, ("rdpClientConDeferredUpdateCallback"));

    dev = (rdpPtr) arg;
    clientCon = dev->clientConHead;
    while (clientCon != NULL)
    {
        if (dev->do_dirty_ons)
        {
            if (clientCon->rectId == clientCon->rectIdAck)
            {
                rdpClientConCheckDirtyScreen(dev, clientCon);
            }
            else
            {
                LLOGLN(0, ("rdpClientConDeferredUpdateCallback: skipping"));
            }
        }
        else
        {
            rdpClientConSendPending(dev, clientCon);
        }
        clientCon = clientCon->next;
    }
    dev->sendUpdateScheduled = FALSE;
    return 0;
}

/******************************************************************************/
void
rdpClientConScheduleDeferredUpdate(rdpPtr dev)
{
    if (dev->sendUpdateScheduled == FALSE)
    {
        dev->sendUpdateScheduled = TRUE;
        dev->sendUpdateTimer = TimerSet(dev->sendUpdateTimer, 0, 40,
                                        rdpClientConDeferredUpdateCallback,
                                        dev);
    }
}

/******************************************************************************/
int
rdpClientConCheckDirtyScreen(rdpPtr dev, rdpClientCon *clientCon)
{
    return 0;
}
