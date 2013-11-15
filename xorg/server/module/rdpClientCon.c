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

/*****************************************************************************/
static int
rdpClientConDisconnect(rdpPtr dev, rdpClientCon *clientCon)
{
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
