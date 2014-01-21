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

Client connection to xrdp

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

/* this should be before all X11 .h files */
#include <xorg-server.h>

/* all driver need this */
#include <xf86.h>
#include <xf86_OSproc.h>

#include "rdp.h"
#include "rdpDraw.h"
#include "rdpClientCon.h"
#include "rdpMisc.h"
#include "rdpInput.h"
#include "rdpReg.h"

#define LOG_LEVEL 1
#define LLOGLN(_level, _args) \
    do { if (_level < LOG_LEVEL) { ErrorF _args ; ErrorF("\n"); } } while (0)

#define LTOUI32(_in) ((unsigned int)(_in))

#define COLOR8(r, g, b) \
    ((((r) >> 5) << 0)  | (((g) >> 5) << 3) | (((b) >> 6) << 6))
#define COLOR15(r, g, b) \
    ((((r) >> 3) << 10) | (((g) >> 3) << 5) | (((b) >> 3) << 0))
#define COLOR16(r, g, b) \
    ((((r) >> 3) << 11) | (((g) >> 2) << 5) | (((b) >> 3) << 0))
#define COLOR24(r, g, b) \
    ((((r) >> 0) << 0)  | (((g) >> 0) << 8) | (((b) >> 0) << 16))
#define SPLITCOLOR32(r, g, b, c) \
    do { \
        r = ((c) >> 16) & 0xff; \
        g = ((c) >> 8) & 0xff; \
        b = (c) & 0xff; \
    } while (0)

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
d GXorReverse,    PDno
e GXor,           DPo
f GXset           1
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

/******************************************************************************/
static int
rdpClientConGotConnection(ScreenPtr pScreen, rdpPtr dev)
{
    rdpClientCon *clientCon;
    int new_sck;

    LLOGLN(0, ("rdpClientConGotConnection:"));
    clientCon = (rdpClientCon *) g_malloc(sizeof(rdpClientCon), 1);
    clientCon->dev = dev;
    make_stream(clientCon->in_s);
    init_stream(clientCon->in_s, 8192);
    make_stream(clientCon->out_s);
    init_stream(clientCon->out_s, 8192 * 4 + 100);

    new_sck = g_sck_accept(dev->listen_sck);
    if (new_sck == -1)
    {
        LLOGLN(0, ("rdpClientConGotConnection: g_sck_accept failed"));
    }
    else
    {
        LLOGLN(0, ("rdpClientConGotConnection: g_sck_accept ok new_sck %d",
               new_sck));
        clientCon->sck = new_sck;
        g_sck_set_non_blocking(clientCon->sck);
        g_sck_tcp_set_no_delay(clientCon->sck); /* only works if TCP */
        clientCon->connected = TRUE;
        clientCon->sckClosed = FALSE;
        clientCon->begin = FALSE;
        dev->conNumber++;
        clientCon->conNumber = dev->conNumber;
        AddEnabledDevice(clientCon->sck);
    }

    if (dev->clientConTail == NULL)
    {
        LLOGLN(0, ("rdpClientConGotConnection: adding only clientCon"));
        dev->clientConHead = clientCon;
        dev->clientConTail = clientCon;
    }
    else
    {
        LLOGLN(0, ("rdpClientConGotConnection: adding clientCon"));
        dev->clientConTail->next = clientCon;
        dev->clientConTail = clientCon;
    }

    clientCon->dirtyRegion = rdpRegionCreate(NullBox, 0);
    clientCon->shmRegion = rdpRegionCreate(NullBox, 0);

    return 0;
}

/******************************************************************************/
static CARD32
rdpDeferredDisconnectCallback(OsTimerPtr timer, CARD32 now, pointer arg)
{
    CARD32 lnow_ms;
    rdpPtr dev;

    dev = (rdpPtr) arg;
    LLOGLN(10, ("rdpDeferredDisconnectCallback"));
    if (dev->clientConHead != NULL)
    {
        /* this should not happen */
        LLOGLN(0, ("rdpDeferredDisconnectCallback: connected"));
        if (dev->disconnectTimer != NULL)
        {
            LLOGLN(0, ("rdpDeferredDisconnectCallback: canceling g_dis_timer"));
            TimerCancel(dev->disconnectTimer);
            TimerFree(dev->disconnectTimer);
            dev->disconnectTimer = NULL;
        }
        dev->disconnect_scheduled = FALSE;
        return 0;
    }
    else
    {
        LLOGLN(10, ("rdpDeferredDisconnectCallback: not connected"));
    }
    lnow_ms = GetTimeInMillis();
    if (lnow_ms - dev->disconnect_time_ms > dev->disconnect_timeout_s * 1000)
    {
        LLOGLN(0, ("rdpDeferredDisconnectCallback: exit X11rdp"));
        kill(getpid(), SIGTERM);
        return 0;
    }
    dev->disconnectTimer = TimerSet(dev->disconnectTimer, 0, 1000 * 10,
                                    rdpDeferredDisconnectCallback, dev);
    return 0;
}

/*****************************************************************************/
static int
rdpClientConDisconnect(rdpPtr dev, rdpClientCon *clientCon)
{
    int index;
    rdpClientCon *pcli;
    rdpClientCon *plcli;

    LLOGLN(0, ("rdpClientConDisconnect:"));
    if (dev->do_kill_disconnected)
    {
        if (dev->disconnect_scheduled == FALSE)
        {
            LLOGLN(0, ("rdpClientConDisconnect: starting g_dis_timer"));
            dev->disconnectTimer = TimerSet(dev->disconnectTimer, 0, 1000 * 10,
                                            rdpDeferredDisconnectCallback, dev);
            dev->disconnect_scheduled = TRUE;
        }
        dev->disconnect_time_ms = GetTimeInMillis();
    }

    RemoveEnabledDevice(clientCon->sck);
    g_sck_close(clientCon->sck);
    if (clientCon->maxOsBitmaps > 0)
    {
        for (index = 0; index < clientCon->maxOsBitmaps; index++)
        {
            if (clientCon->osBitmaps[index].used)
            {
                if (clientCon->osBitmaps[index].priv != NULL)
                {
                    clientCon->osBitmaps[index].priv->status = 0;
                }
            }
        }
    }
    g_free(clientCon->osBitmaps);

    plcli = NULL;
    pcli = dev->clientConHead;
    while (pcli != NULL)
    {
        if (pcli == clientCon)
        {
            if (plcli == NULL)
            {
                /* removing first item */
                dev->clientConHead = pcli->next;
                if (dev->clientConHead == NULL)
                {
                    /* removed only */
                    dev->clientConTail = NULL;
                }
            }
            else
            {
                plcli->next = pcli->next;
                if (pcli == dev->clientConTail)
                {
                    /* removed last */
                    dev->clientConTail = plcli;
                }
            }
            break;
        }
        plcli = pcli;
        pcli = pcli->next;
    }
    rdpRegionDestroy(clientCon->dirtyRegion);
    rdpRegionDestroy(clientCon->shmRegion);
    g_free(clientCon);
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
/* returns error */
static int
rdpClientConRecv(rdpPtr dev, rdpClientCon *clientCon, char *data, int len)
{
    int rcvd;

    if (clientCon->sckClosed)
    {
        return 1;
    }

    while (len > 0)
    {
        rcvd = g_sck_recv(clientCon->sck, data, len, 0);

        if (rcvd == -1)
        {
            if (g_sck_last_error_would_block(clientCon->sck))
            {
                g_sleep(1);
            }
            else
            {
                LLOGLN(0, ("rdpClientConRecv: g_sck_recv failed(returned -1)"));
                rdpClientConDisconnect(dev, clientCon);
                return 1;
            }
        }
        else if (rcvd == 0)
        {
            LLOGLN(0, ("rdpClientConRecv: g_sck_recv failed(returned 0)"));
            rdpClientConDisconnect(dev, clientCon);
            return 1;
        }
        else
        {
            data += rcvd;
            len -= rcvd;
        }
    }

    return 0;
}

/******************************************************************************/
static int
rdpClientConRecvMsg(rdpPtr dev, rdpClientCon *clientCon)
{
    int len;
    int rv;
    struct stream *s;

    rv = 1;

    s = clientCon->in_s;
    if (s != 0)
    {
        init_stream(s, 4);
        rv = rdpClientConRecv(dev, clientCon, s->data, 4);

        if (rv == 0)
        {
            s->end = s->data + 4;
            in_uint32_le(s, len);

            if (len > 3)
            {
                init_stream(s, len);
                rv = rdpClientConRecv(dev, clientCon, s->data, len - 4);
                if (rv == 0)
                {
                    s->end = s->data + len;
                }
            }
        }
    }

    if (rv != 0)
    {
        LLOGLN(0, ("rdpClientConRecvMsg: error"));
    }

    return rv;
}

/******************************************************************************/
static int
rdpClientConSendCaps(rdpPtr dev, rdpClientCon *clientCon)
{
    struct stream *ls;
    int len;
    int rv;
    int cap_count;
    int cap_bytes;

    make_stream(ls);
    init_stream(ls, 8192);
    s_push_layer(ls, iso_hdr, 8);

    cap_count = 0;
    cap_bytes = 0;

#if 0
    out_uint16_le(ls, 0);
    out_uint16_le(ls, 4);
    cap_count++;
    cap_bytes += 4;

    out_uint16_le(ls, 1);
    out_uint16_le(ls, 4);
    cap_count++;
    cap_bytes += 4;
#endif

    s_mark_end(ls);
    len = (int)(ls->end - ls->data);
    s_pop_layer(ls, iso_hdr);
    out_uint16_le(ls, 2); /* caps */
    out_uint16_le(ls, cap_count); /* num caps */
    out_uint32_le(ls, cap_bytes); /* caps len after header */

    rv = rdpClientConSend(dev, clientCon, ls->data, len);

    if (rv != 0)
    {
        LLOGLN(0, ("rdpClientConSendCaps: rdpup_send failed"));
    }

    free_stream(ls);
    return rv;
}

/******************************************************************************/
static int
rdpClientConProcessMsgVersion(rdpPtr dev, rdpClientCon *clientCon,
                              int param1, int param2, int param3, int param4)
{
    LLOGLN(0, ("rdpClientConProcessMsgVersion: version %d %d %d %d",
           param1, param2, param3, param4));

    if ((param1 > 0) || (param2 > 0) || (param3 > 0) || (param4 > 0))
    {
        rdpClientConSendCaps(dev, clientCon);
    }

    return 0;
}

/******************************************************************************/
/*
    this from miScreenInit
    pScreen->mmWidth = (xsize * 254 + dpix * 5) / (dpix * 10);
    pScreen->mmHeight = (ysize * 254 + dpiy * 5) / (dpiy * 10);
*/
static int
rdpClientConProcessScreenSizeMsg(rdpPtr dev, rdpClientCon *clientCon,
                                 int width, int height, int bpp)
{
    RRScreenSizePtr pSize;
    int mmwidth;
    int mmheight;
    int bytes;
    Bool ok;

    LLOGLN(0, ("rdpClientConProcessScreenSizeMsg: set width %d height %d bpp %d",
               width, height, bpp));
    clientCon->rdp_width = width;
    clientCon->rdp_height = height;
    clientCon->rdp_bpp = bpp;

    if (bpp < 15)
    {
        clientCon->rdp_Bpp = 1;
        clientCon->rdp_Bpp_mask = 0xff;
    }
    else if (bpp == 15)
    {
        clientCon->rdp_Bpp = 2;
        clientCon->rdp_Bpp_mask = 0x7fff;
    }
    else if (bpp == 16)
    {
        clientCon->rdp_Bpp = 2;
        clientCon->rdp_Bpp_mask = 0xffff;
    }
    else if (bpp > 16)
    {
        clientCon->rdp_Bpp = 4;
        clientCon->rdp_Bpp_mask = 0xffffff;
    }

    if (clientCon->shmemptr != 0)
    {
        shmdt(clientCon->shmemptr);
    }
    bytes = clientCon->rdp_width * clientCon->rdp_height *
            clientCon->rdp_Bpp;
    clientCon->shmemid = shmget(IPC_PRIVATE, bytes, IPC_CREAT | 0777);
    clientCon->shmemptr = shmat(clientCon->shmemid, 0, 0);
    shmctl(clientCon->shmemid, IPC_RMID, NULL);
    LLOGLN(0, ("rdpClientConProcessScreenSizeMsg: shmemid %d shmemptr %p",
           clientCon->shmemid, clientCon->shmemptr));
    clientCon->shmem_lineBytes = clientCon->rdp_Bpp * clientCon->rdp_width;

    if (clientCon->shmRegion != 0)
    {
        rdpRegionDestroy(clientCon->shmRegion);
    }
    clientCon->shmRegion = rdpRegionCreate(NullBox, 0);

    mmwidth = PixelToMM(width);
    mmheight = PixelToMM(height);

    pSize = RRRegisterSize(dev->pScreen, width, height, mmwidth, mmheight);
    RRSetCurrentConfig(dev->pScreen, RR_Rotate_0, 0, pSize);

    if ((dev->width != width) || (dev->height != height))
    {
        ok = RRScreenSizeSet(dev->pScreen, width, height, mmwidth, mmheight);
        LLOGLN(0, ("rdpClientConProcessScreenSizeMsg: RRScreenSizeSet ok=[%d]", ok));
    }

    return 0;
}

/******************************************************************************/
static int
rdpClientConProcessMsgClientInput(rdpPtr dev, rdpClientCon *clientCon)
{
    struct stream *s;
    int msg;
    int param1;
    int param2;
    int param3;
    int param4;
    int x;
    int y;
    int cx;
    int cy;

    s = clientCon->in_s;
    in_uint32_le(s, msg);
    in_uint32_le(s, param1);
    in_uint32_le(s, param2);
    in_uint32_le(s, param3);
    in_uint32_le(s, param4);

    LLOGLN(10, ("rdpClientConProcessMsgClientInput: msg %d param1 %d param2 %d "
           "param3 %d param4 %d", msg, param1, param2, param3, param4));

    if (msg < 100)
    {
        rdpInputKeyboardEvent(dev, msg, param1, param2, param3, param4);
    }
    else if (msg < 200)
    {
        rdpInputMouseEvent(dev, msg, param1, param2, param3, param4);
    }
    else if (msg == 200) /* invalidate */
    {
        x = (param1 >> 16) & 0xffff;
        y = param1 & 0xffff;
        cx = (param2 >> 16) & 0xffff;
        cy = param2 & 0xffff;
        LLOGLN(0, ("rdpClientConProcessMsgClientInput: invalidate x %d y %d "
               "cx %d cy %d", x, y, cx, cy));
        rdpClientConAddDirtyScreen(dev, clientCon, x, y, cx, cy);
    }
    else if (msg == 300) /* resize desktop */
    {
        rdpClientConProcessScreenSizeMsg(dev, clientCon, param1, param2, param3);
    }
    else if (msg == 301) /* version */
    {
        rdpClientConProcessMsgVersion(dev, clientCon,
                                   param1, param2, param3, param4);
    }
    else
    {
        LLOGLN(0, ("rdpClientConProcessMsgClientInput: unknown msg %d", msg));
    }

    return 0;
}

/******************************************************************************/
static int
rdpClientConProcessMsgClientInfo(rdpPtr dev, rdpClientCon *clientCon)
{
    struct stream *s;
    int bytes;
    int i1;

    LLOGLN(0, ("rdpClientConProcessMsgClientInfo:"));
    s = clientCon->in_s;
    in_uint32_le(s, bytes);
    if (bytes > sizeof(clientCon->client_info))
    {
        bytes = sizeof(clientCon->client_info);
    }
    memcpy(&(clientCon->client_info), s->p - 4, bytes);
    clientCon->client_info.size = bytes;

    LLOGLN(0, ("  got client info bytes %d", bytes));
    LLOGLN(0, ("  jpeg support %d", clientCon->client_info.jpeg));
    i1 = clientCon->client_info.offscreen_support_level;
    LLOGLN(0, ("  offscreen support %d", i1));
    i1 = clientCon->client_info.offscreen_cache_size;
    LLOGLN(0, ("  offscreen size %d", i1));
    i1 = clientCon->client_info.offscreen_cache_entries;
    LLOGLN(0, ("  offscreen entries %d", i1));

    if (clientCon->client_info.offscreen_support_level > 0)
    {
        if (clientCon->client_info.offscreen_cache_entries > 0)
        {
            clientCon->maxOsBitmaps = clientCon->client_info.offscreen_cache_entries;
            g_free(clientCon->osBitmaps);
            clientCon->osBitmaps = (struct rdpup_os_bitmap *)
                        g_malloc(sizeof(struct rdpup_os_bitmap) * clientCon->maxOsBitmaps, 1);
        }
    }

    if (clientCon->client_info.orders[0x1b])   /* 27 NEG_GLYPH_INDEX_INDEX */
    {
        LLOGLN(0, ("  client supports glyph cache but server disabled"));
        //clientCon->doGlyphCache = 1;
    }
    if (clientCon->client_info.order_flags_ex & 0x100)
    {
        clientCon->doComposite = 1;
    }
    if (clientCon->doGlyphCache)
    {
        LLOGLN(0, ("  using glyph cache"));
    }
    if (clientCon->doComposite)
    {
        LLOGLN(0, ("  using client composite"));
    }
    LLOGLN(10, ("order_flags_ex 0x%x", clientCon->client_info.order_flags_ex));
    if (clientCon->client_info.offscreen_cache_entries == 2000)
    {
        LLOGLN(0, ("  client can do offscreen to offscreen blits"));
        clientCon->canDoPixToPix = 1;
    }
    else
    {
        LLOGLN(0, ("  client can not do offscreen to offscreen blits"));
        clientCon->canDoPixToPix = 0;
    }
    if (clientCon->client_info.pointer_flags & 1)
    {
        LLOGLN(0, ("  client can do new(color) cursor"));
    }
    else
    {
        LLOGLN(0, ("  client can not do new(color) cursor"));
    }
    if (clientCon->client_info.monitorCount > 0)
    {
        LLOGLN(0, ("  client can do multimon"));
        LLOGLN(0, ("  client monitor data, monitorCount= %d", clientCon->client_info.monitorCount));
        clientCon->doMultimon = 1;
    }
    else
    {
        LLOGLN(0, ("  client can not do multimon"));
        clientCon->doMultimon = 0;
    }

    //rdpLoadLayout(g_rdpScreen.client_info.keylayout);

    return 0;
}

/******************************************************************************/
static int
rdpClientConProcessMsgClientRegion(rdpPtr dev, rdpClientCon *clientCon)
{
    struct stream *s;
    int flags;
    int x;
    int y;
    int cx;
    int cy;
    RegionRec reg;
    BoxRec box;

    LLOGLN(0, ("rdpClientConProcessMsgClientRegion:"));
    s = clientCon->in_s;

    in_uint32_le(s, flags);
    in_uint32_le(s, clientCon->rect_id_ack);
    in_uint32_le(s, x);
    in_uint32_le(s, y);
    in_uint32_le(s, cx);
    in_uint32_le(s, cy);
    LLOGLN(0, ("rdpClientConProcessMsgClientRegion: %d %d %d %d flags 0x%8.8x",
           x, y, cx, cy, flags));
    LLOGLN(0, ("rdpClientConProcessMsgClientRegion: rect_id %d rect_id_ack %d",
           clientCon->rect_id, clientCon->rect_id_ack));

    box.x1 = x;
    box.y1 = y;
    box.x2 = box.x1 + cx;
    box.y2 = box.y1 + cy;

    rdpRegionInit(&reg, &box, 0);
    LLOGLN(0, ("rdpClientConProcessMsgClientRegion: %d %d %d %d",
           box.x1, box.y1, box.x2, box.y2));
    rdpRegionSubtract(clientCon->shmRegion, clientCon->shmRegion, &reg);
    rdpRegionUninit(&reg);

    return 0;
}

/******************************************************************************/
static int
rdpClientConProcessMsg(rdpPtr dev, rdpClientCon *clientCon)
{
    int msg_type;
    struct stream *s;

    LLOGLN(10, ("rdpClientConProcessMsg:"));
    s = clientCon->in_s;
    in_uint16_le(s, msg_type);
    LLOGLN(10, ("rdpClientConProcessMsg: msg_type %d", msg_type));
    switch (msg_type)
    {
        case 103: /* client input */
            rdpClientConProcessMsgClientInput(dev, clientCon);
            break;
        case 104: /* client info */
            rdpClientConProcessMsgClientInfo(dev, clientCon);
            break;
        case 105: /* client region */
            rdpClientConProcessMsgClientRegion(dev, clientCon);
            break;
        default:
            break;
    }

    return 0;
}

/******************************************************************************/
static int
rdpClientConGotData(ScreenPtr pScreen, rdpPtr dev, rdpClientCon *clientCon)
{
    int rv;

    LLOGLN(10, ("rdpClientConGotData:"));

    rv = rdpClientConRecvMsg(dev, clientCon);
    if (rv == 0)
    {
        rv = rdpClientConProcessMsg(dev, clientCon);
    }

    return rv;
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
                if (rdpClientConGotData(pScreen, dev, clientCon) != 0)
                {
                    LLOGLN(0, ("rdpClientConCheck: rdpClientConGotData failed"));
                    clientCon = dev->clientConHead;
                    continue;
                }
            }
        }
        if (clientCon->sckControlListener > 0)
        {
            if (FD_ISSET(LTOUI32(clientCon->sckControlListener), &rfds))
            {
                if (rdpClientConGotControlConnection(pScreen, dev, clientCon) != 0)
                {
                    LLOGLN(0, ("rdpClientConCheck: rdpClientConGotControlConnection failed"));
                    clientCon = dev->clientConHead;
                    continue;
                }
            }
        }
        if (clientCon->sckControl > 0)
        {
            if (FD_ISSET(LTOUI32(clientCon->sckControl), &rfds))
            {
                if (rdpClientConGotControlData(pScreen, dev, clientCon) != 0)
                {
                    LLOGLN(0, ("rdpClientConCheck: rdpClientConGotControlData failed"));
                    clientCon = dev->clientConHead;
                    continue;
                }
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
        dev->sendUpdateTimer =
                TimerSet(dev->sendUpdateTimer, 0, 40,
                         rdpClientConDeferredUpdateCallback, dev);
    }
}

/******************************************************************************/
int
rdpClientConCheckDirtyScreen(rdpPtr dev, rdpClientCon *clientCon)
{
    return 0;
}

/******************************************************************************/
static CARD32
rdpDeferredUpdateCallback(OsTimerPtr timer, CARD32 now, pointer arg)
{
    rdpClientCon *clientCon;
    int num_rects;
    int index;
    BoxRec box;

    LLOGLN(0, ("rdpDeferredUpdateCallback:"));
    clientCon = (rdpClientCon *) arg;

    if (clientCon->rect_id != clientCon->rect_id_ack)
    {
        LLOGLN(0, ("rdpDeferredUpdateCallback: reschedual"));
        clientCon->updateTimer = TimerSet(clientCon->updateTimer, 0, 40,
                                          rdpDeferredUpdateCallback, clientCon);
        return 0;
    }

    clientCon->updateSchedualed = FALSE;
    rdpClientConBeginUpdate(clientCon->dev, clientCon);
    num_rects = REGION_NUM_RECTS(clientCon->dirtyRegion);
    for (index = 0; index < num_rects; index++)
    {
        box = REGION_RECTS(clientCon->dirtyRegion)[index];
        LLOGLN(0, ("  x1 %d y1 %d x2 %d y2 %d cx %d cy %d", box.x1, box.y1,
               box.x2, box.y2, box.x2 - box.x1, box.y2 - box.y1));
        rdpClientConSendArea(clientCon->dev, clientCon, NULL, box.x1, box.y1,
                             box.x2 - box.x1, box.y2 - box.y1);
    }
    rdpClientConEndUpdate(clientCon->dev, clientCon);
    rdpRegionDestroy(clientCon->dirtyRegion);
    clientCon->dirtyRegion = RegionCreate(NullBox, 0);
    return 0;
}

/******************************************************************************/
int
rdpClientConAddDirtyScreenReg(rdpPtr dev, rdpClientCon *clientCon,
                              RegionPtr reg)
{
    rdpRegionUnion(clientCon->dirtyRegion, clientCon->dirtyRegion, reg);
    if (clientCon->updateSchedualed == FALSE)
    {
        clientCon->updateTimer = TimerSet(clientCon->updateTimer, 0, 40,
                                          rdpDeferredUpdateCallback, clientCon);
        clientCon->updateSchedualed = TRUE;
    }
    return 0;
}

/******************************************************************************/
int
rdpClientConAddDirtyScreenBox(rdpPtr dev, rdpClientCon *clientCon,
                              BoxPtr box)
{
    RegionPtr reg;

    reg = rdpRegionCreate(box, 0);
    rdpClientConAddDirtyScreenReg(dev, clientCon, reg);
    rdpRegionDestroy(reg);
    return 0;
}

/******************************************************************************/
int
rdpClientConAddDirtyScreen(rdpPtr dev, rdpClientCon *clientCon,
                           int x, int y, int cx, int cy)
{
    BoxRec box;

    box.x1 = x;
    box.y1 = y;
    box.x2 = box.x1 + cx;
    box.y2 = box.y1 + cy;
    rdpClientConAddDirtyScreenBox(dev, clientCon, &box);
    return 0;
}

/******************************************************************************/
void
rdpClientConGetScreenImageRect(rdpPtr dev, rdpClientCon *clientCon,
                               struct image_data *id)
{
    id->width = dev->width;
    id->height = dev->height;
    id->bpp = clientCon->rdp_bpp;
    id->Bpp = clientCon->rdp_Bpp;
    id->lineBytes = dev->paddedWidthInBytes;
    id->pixels = dev->pfbMemory;
    id->shmem_pixels = clientCon->shmemptr;
    id->shmem_id = clientCon->shmemid;
    id->shmem_offset = 0;
    id->shmem_lineBytes = clientCon->shmem_lineBytes;
}

/******************************************************************************/
void
rdpClientConGetPixmapImageRect(rdpPtr dev, rdpClientCon *clientCon,
                               PixmapPtr pPixmap, struct image_data *id)
{
    id->width = pPixmap->drawable.width;
    id->height = pPixmap->drawable.height;
    id->bpp = clientCon->rdp_bpp;
    id->Bpp = clientCon->rdp_Bpp;
    id->lineBytes = pPixmap->devKind;
    id->pixels = (char *)(pPixmap->devPrivate.ptr);
    id->shmem_pixels = 0;
    id->shmem_id = 0;
    id->shmem_offset = 0;
    id->shmem_lineBytes = 0;
}

/******************************************************************************/
void
rdpClientConSendArea(rdpPtr dev, rdpClientCon *clientCon,
                     struct image_data *id, int x, int y, int w, int h)
{
    struct image_data lid;
    BoxRec box;
    RegionRec reg;
    int ly;
    int size;
    char *src;
    char *dst;
    struct stream *s;

    LLOGLN(10, ("rdpClientConSendArea: id %p x %d y %d w %d h %d", id, x, y, w, h));

    if (id == NULL)
    {
        rdpClientConGetScreenImageRect(dev, clientCon, &lid);
        id = &lid;
    }

    if (x >= id->width)
    {
        return;
    }

    if (y >= id->height)
    {
        return;
    }

    if (x < 0)
    {
        w += x;
        x = 0;
    }

    if (y < 0)
    {
        h += y;
        y = 0;
    }

    if (w <= 0)
    {
        return;
    }

    if (h <= 0)
    {
        return;
    }

    if (x + w > id->width)
    {
        w = id->width - x;
    }

    if (y + h > id->height)
    {
        h = id->height - y;
    }

    if (clientCon->connected && clientCon->begin)
    {
        if (id->shmem_pixels != 0)
        {
            LLOGLN(0, ("rdpClientConSendArea: using shmem"));
            box.x1 = x;
            box.y1 = y;
            box.x2 = box.x1 + w;
            box.y2 = box.y1 + h;
            src = id->pixels;
            src += y * id->lineBytes;
            src += x * dev->Bpp;
            dst = id->shmem_pixels + id->shmem_offset;
            dst += y * id->shmem_lineBytes;
            dst += x * clientCon->rdp_Bpp;
            ly = y;
            while (ly < y + h)
            {
                rdpClientConConvertPixels(dev, clientCon, src, dst, w);
                src += id->lineBytes;
                dst += id->shmem_lineBytes;
                ly += 1;
            }
            size = 36;
            rdpClientConPreCheck(dev, clientCon, size);
            s = clientCon->out_s;
            out_uint16_le(s, 60);
            out_uint16_le(s, size);
            clientCon->count++;
            LLOGLN(0, ("rdpClientConSendArea: 2 x %d y %d w %d h %d", x, y, w, h));
            out_uint16_le(s, x);
            out_uint16_le(s, y);
            out_uint16_le(s, w);
            out_uint16_le(s, h);
            out_uint32_le(s, 0);
            clientCon->rect_id++;
            out_uint32_le(s, clientCon->rect_id);
            out_uint32_le(s, id->shmem_id);
            out_uint32_le(s, id->shmem_offset);
            out_uint16_le(s, id->width);
            out_uint16_le(s, id->height);
            out_uint16_le(s, x);
            out_uint16_le(s, y);
            rdpRegionInit(&reg, &box, 0);
            rdpRegionUnion(clientCon->shmRegion, clientCon->shmRegion, &reg);
            rdpRegionUninit(&reg);
            return;
        }
    }
}
