/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2012-2013
 * Copyright (C) Laxmikant Rashinkar 2012-2013
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

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#define LOG_LEVEL 1
#define LLOG(_level, _args) \
    do { if (_level < LOG_LEVEL) { ErrorF _args ; } } while (0)
#define LLOGLN(_level, _args) \
    do { if (_level < LOG_LEVEL) { printf _args ; printf("\n"); } } while (0)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "xrdp_sockets.h"
#include "xrdpapi.h"

struct wts_obj
{
    int      fd;
    int      status;
    char     name[9];
    char     dname[128];
    int      display_num;
    uint32_t flags;
};

/* helper functions used by WTSxxx API - do not invoke directly */
static int get_display_num_from_display(char *display_text);
static int send_init(struct wts_obj *wts);
static int can_send(int sck, int millis);
static int can_recv(int sck, int millis);

static const unsigned char g_xrdpapi_magic[12] =
{
    0x78, 0x32, 0x10, 0x67, 0x00, 0x92, 0x30, 0x56, 0xff, 0xd8, 0xa9, 0x1f
};

/*
 * Opens a handle to the server end of a specified virtual channel - this
 * call is deprecated - use WTSVirtualChannelOpenEx() instead
 *
 * @param  hServer
 * @param  SessionId     - current session ID; *must* be WTS_CURRENT_SERVER_HANDLE
 * @param  pVirtualName  - virtual channel name when using SVC
 *                       - name of endpoint listener when using DVC
 *
 * @return a valid pointer on success, NULL on error
 ******************************************************************************/
void *
WTSVirtualChannelOpen(void *hServer, unsigned int SessionId,
                      const char *pVirtualName)
{
    if (hServer != WTS_CURRENT_SERVER_HANDLE)
    {
        return 0;
    }

    return WTSVirtualChannelOpenEx(SessionId, pVirtualName, 0);
}

/*
 * Opens a handle to the server end of a specified virtual channel
 *
 * @param  SessionId     - current session ID; *must* be WTS_CURRENT_SERVER_HANDLE
 * @param  pVirtualName  - virtual channel name when using SVC
 *                       - name of endpoint listener when using DVC
 * @param  flags         - type of channel and channel priority if DVC
 *
 * @return a valid pointer on success, NULL on error
 ******************************************************************************/
void *
WTSVirtualChannelOpenEx(unsigned int SessionId, const char *pVirtualName,
                        unsigned int flags)
{
    struct wts_obj     *wts;
    char               *display_text;
    int                 bytes;
    unsigned long       llong;
    struct sockaddr_un  s;

    if (SessionId != WTS_CURRENT_SESSION)
    {
        LLOGLN(0, ("WTSVirtualChannelOpenEx: bad SessionId"));
        return 0;
    }

    wts = (struct wts_obj *) calloc(1, sizeof(struct wts_obj));

    wts->fd = -1;
    wts->flags = flags;
    display_text = getenv("DISPLAY");

    if (display_text != 0)
    {
        wts->display_num = get_display_num_from_display(display_text);
    }

    if (wts->display_num <= 0)
    {
        LLOGLN(0, ("WTSVirtualChannelOpenEx: fatal error; display is 0"));
        free(wts);
        return NULL;
    }

    /* we use unix domain socket to communicate with chansrv */
    if ((wts->fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
    {
        free(wts);
        return NULL;
    }

    /* set non blocking */
    llong = fcntl(wts->fd, F_GETFL);
    llong = llong | O_NONBLOCK;
    if (fcntl(wts->fd, F_SETFL, llong) < 0)
    {
        LLOGLN(10, ("WTSVirtualChannelOpenEx: set non-block mode failed"));
    }

    /* connect to chansrv session */
    memset(&s, 0, sizeof(struct sockaddr_un));
    s.sun_family = AF_UNIX;
    bytes = sizeof(s.sun_path);
    snprintf(s.sun_path, bytes - 1, CHANSRV_API_STR, wts->display_num);
    s.sun_path[bytes - 1] = 0;
    bytes = sizeof(struct sockaddr_un);

    if (connect(wts->fd, (struct sockaddr *) &s, bytes) == 0)
    {
        LLOGLN(10, ("WTSVirtualChannelOpenEx: connected ok, name %s", pVirtualName));
        strncpy(wts->name, pVirtualName, 8);

        /* wait for connection to complete and send init */
        if (send_init(wts) == 0)
        {
            /* all ok */
            wts->status = 1;
        }
    }

    return wts;
}

/*
 * Prevent receiving SIGPIPE on disconnect using either MSG_NOSIGNAL (Linux)
 * or SO_NOSIGPIPE (Mac OS X)
 */
#if !defined(MSG_NOSIGNAL)
#define MSG_NOSIGNAL 0
#endif

/*****************************************************************************/
static int
mysend(int sck, const void* adata, int bytes)
{
    int sent;
    int error;
    const char* data;

#if defined(SO_NOSIGPIPE)
    const int on = 1;
    setsockopt(sck, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
#endif

    data = (const char*)adata;
    sent = 0;
    while (sent < bytes)
    {
        if (can_send(sck, 100))
        {
            error = send(sck, data + sent, bytes - sent, MSG_NOSIGNAL);
            if (error < 1)
            {
                return -1;
            }
            sent += error;
        }
    }
    return sent;
}

/*
 * write data to client connection
 *
 * @return 1 on success, 0 on error
 *****************************************************************************/
int
WTSVirtualChannelWrite(void *hChannelHandle, const char *Buffer,
                       unsigned int Length, unsigned int *pBytesWritten)
{
    struct wts_obj *wts;
    int             rv;
    int             header[4];

    wts = (struct wts_obj *) hChannelHandle;

    *pBytesWritten = 0;

    if (wts == 0)
    {
        LLOGLN(10, ("WTSVirtualChannelWrite: wts is NULL"));
        return 0;
    }

    if (wts->status != 1)
    {
        LLOGLN(10, ("WTSVirtualChannelWrite: wts->status != 1"));
        return 0;
    }

    if (!can_send(wts->fd, 0))
    {
        return 1;    /* can't write now, ok to try again */
    }

    rv = 0;
    memcpy(header, g_xrdpapi_magic, 12);
    header[3] = Length;
    if (mysend(wts->fd, header, 16) == 16)
    {
        rv = mysend(wts->fd, Buffer, Length);
    }
    else
    {
        LLOGLN(0, ("WTSVirtualChannelWrite: header write failed"));
        return 0;
    }

    LLOGLN(10, ("WTSVirtualChannelWrite: mysend() returned %d", rv));

    if (rv >= 0)
    {
        /* success, but zero bytes may have been written */
        *pBytesWritten = rv;
        return 1;
    }

#if 0 /* coverity: this is dead code */
    /* error, but is it ok to try again? */
    if ((rv == EWOULDBLOCK) || (rv == EAGAIN) || (rv == EINPROGRESS))
    {
        return 0;    /* failed to send, but should try again */
    }
#endif

    /* fatal error */
    return 0;
}

/*
 * read data from a client connection
 *
 * @return 1 on success, 0 on error
 *****************************************************************************/
int
WTSVirtualChannelRead(void *hChannelHandle, unsigned int TimeOut,
                      char *Buffer, unsigned int BufferSize,
                      unsigned int *pBytesRead)
{
    struct wts_obj *wts;
    int rv;
    int lerrno;

    wts = (struct wts_obj *)hChannelHandle;

    if (wts == 0)
    {
        return 0;
    }

    if (wts->status != 1)
    {
        return 0;
    }

    if (can_recv(wts->fd, TimeOut))
    {
        rv = recv(wts->fd, Buffer, BufferSize, 0);

        if (rv == -1)
        {
            lerrno = errno;

            if ((lerrno == EWOULDBLOCK) || (lerrno == EAGAIN) ||
                    (lerrno == EINPROGRESS))
            {
                *pBytesRead = 0;
                return 1;
            }

            return 0;
        }
        else if (rv == 0)
        {
            return 0;
        }
        else if (rv > 0)
        {
            *pBytesRead = rv;
            return 1;
        }
    }

    *pBytesRead = 0;
    return 1;
}

/*****************************************************************************/
int
WTSVirtualChannelClose(void *hChannelHandle)
{
    struct wts_obj *wts;

    wts = (struct wts_obj *)hChannelHandle;

    if (wts == 0)
    {
        return 0;
    }

    if (wts->fd != -1)
    {
        close(wts->fd);
    }

    free(wts);
    return 1;
}

/*****************************************************************************/
int
WTSVirtualChannelQuery(void *hChannelHandle, WTS_VIRTUAL_CLASS WtsVirtualClass,
                       void **ppBuffer, unsigned int *pBytesReturned)
{
    struct wts_obj *wts;

    wts = (struct wts_obj *)hChannelHandle;

    if (wts == 0)
    {
        return 0;
    }

    if (wts->status != 1)
    {
        return 0;
    }

    if (WtsVirtualClass == WTSVirtualFileHandle)
    {
        *pBytesReturned = 4;
        *ppBuffer = malloc(4);
        memcpy(*ppBuffer, &(wts->fd), 4);
    }

    return 1;
}

/*****************************************************************************/
void
WTSFreeMemory(void *pMemory)
{
    if (pMemory != 0)
    {
        free(pMemory);
    }
}

/*****************************************************************************
**                                                                          **
**                                                                          **
**      Helper functions used by WTSxxx API - do not invoke directly        **
**                                                                          **
**                                                                          **
*****************************************************************************/

/*
 * check if socket is in a writable state - i.e will not block on write
 *
 * @param  sck    socket to check
 * @param  millis timeout value in milliseconds
 *
 * @return 0 if write will block
 * @return 1 if write will not block
 ******************************************************************************/
static int
can_send(int sck, int millis)
{
    struct timeval time;
    fd_set         wfds;
    int            select_rv;

    /* setup for a select call */
    FD_ZERO(&wfds);
    FD_SET(sck, &wfds);
    time.tv_sec = millis / 1000;
    time.tv_usec = (millis * 1000) % 1000000;

    /* check if it is ok to write to specified socket */
    select_rv = select(sck + 1, 0, &wfds, 0, &time);

    return (select_rv > 0) ? 1 : 0;
}

/*****************************************************************************/
static int
can_recv(int sck, int millis)
{
    struct timeval time;
    fd_set rfds;
    int select_rv;

    FD_ZERO(&rfds);
    FD_SET(sck, &rfds);
    time.tv_sec = millis / 1000;
    time.tv_usec = (millis * 1000) % 1000000;
    select_rv = select(sck + 1, &rfds, 0, 0, &time);

    if (select_rv > 0)
    {
        return 1;
    }

    return 0;
}

/*****************************************************************************/
static int
send_init(struct wts_obj *wts)
{
    char initmsg[64];

    memset(initmsg, 0, 64);

    /* insert channel name */
    strncpy(initmsg, wts->name, 8);

    /* insert open mode flags */
    initmsg[16] = (wts->flags >>  0) & 0xff;
    initmsg[17] = (wts->flags >>  8) & 0xff;
    initmsg[18] = (wts->flags >> 16) & 0xff;
    initmsg[19] = (wts->flags >> 24) & 0xff;

    if (!can_send(wts->fd, 500))
    {
        LLOGLN(10, ("send_init: send() will block!"));
        return 1;
    }

    if (send(wts->fd, initmsg, 64, 0) != 64)
    {
        LLOGLN(10, ("send_init: send() failed!"));
        return 1;
    }

    LLOGLN(10, ("send_init: sent ok!"));
    return 0;
}

/*****************************************************************************/
static int
get_display_num_from_display(char *display_text)
{
    int index;
    int mode;
    int disp_index;
    char disp[256];

    index = 0;
    disp_index = 0;
    mode = 0;

    while (display_text[index] != 0)
    {
        if (display_text[index] == ':')
        {
            mode = 1;
        }
        else if (display_text[index] == '.')
        {
            mode = 2;
        }
        else if (mode == 1)
        {
            disp[disp_index] = display_text[index];
            disp_index++;
        }

        index++;
    }

    disp[disp_index] = 0;
    return atoi(disp);
}
