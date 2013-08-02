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
        dev->listen_sck = g_tcp_local_socket_stream();
        if (g_tcp_local_bind(dev->listen_sck, dev->uds_data) != 0)
        {
            LLOGLN(0, ("rdpClientConInit: g_tcp_local_bind failed"));
            return 1;
        }
        g_tcp_listen(dev->listen_sck);
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
