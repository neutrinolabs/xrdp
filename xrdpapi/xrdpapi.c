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

#include "log.h"
#include "xrdp_sockets.h"
#include "string_calls.h"
#include "xrdpapi.h"

struct wts_obj
{
    int fd;
    int display_num;
};

/* helper functions used by WTSxxx API - do not invoke directly */
static int
can_send(int sck, int millis);
static int
can_recv(int sck, int millis);
static int
mysend(int sck, const void *adata, int bytes);
static int
myrecv(int sck, void *adata, int bytes);

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
    struct wts_obj *wts;
    int bytes;
    unsigned long long1;
    struct sockaddr_un s;
    char *connect_data;
    int chan_name_bytes;
    int lerrno;

    if (SessionId != WTS_CURRENT_SESSION)
    {
        LOG(LOG_LEVEL_ERROR, "WTSVirtualChannelOpenEx: bad SessionId");
        return 0;
    }
    wts = (struct wts_obj *) calloc(1, sizeof(struct wts_obj));
    if (wts == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "WTSVirtualChannelOpenEx: calloc failed");
        return 0;
    }
    wts->fd = -1;
    wts->display_num = g_get_display_num_from_display(getenv("DISPLAY"));
    if (wts->display_num < 0)
    {
        LOG(LOG_LEVEL_ERROR, "WTSVirtualChannelOpenEx: fatal error; invalid DISPLAY");
        free(wts);
        return NULL;
    }

    /* we use unix domain socket to communicate with chansrv */
    if ((wts->fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
    {
        LOG(LOG_LEVEL_ERROR, "WTSVirtualChannelOpenEx: socket failed");
        free(wts);
        return NULL;
    }

    /* set non blocking */
    long1 = fcntl(wts->fd, F_GETFL);
    long1 = long1 | O_NONBLOCK;
    if (fcntl(wts->fd, F_SETFL, long1) < 0)
    {
        LOG(LOG_LEVEL_WARNING, "WTSVirtualChannelOpenEx: set non-block mode failed");
    }

    /* connect to chansrv session */
    memset(&s, 0, sizeof(struct sockaddr_un));
    s.sun_family = AF_UNIX;
    bytes = sizeof(s.sun_path);
    snprintf(s.sun_path, bytes - 1, CHANSRV_API_STR, wts->display_num);
    s.sun_path[bytes - 1] = 0;
    bytes = sizeof(struct sockaddr_un);

    if (connect(wts->fd, (struct sockaddr *) &s, bytes) < 0)
    {
        lerrno = errno;
        if ((lerrno == EWOULDBLOCK) || (lerrno == EAGAIN) ||
                (lerrno == EINPROGRESS))
        {
            /* ok */
        }
        else
        {
            LOG(LOG_LEVEL_ERROR, "WTSVirtualChannelOpenEx: connect failed");
            free(wts);
            return NULL;
        }
    }

    /* wait for connection to complete */
    if (!can_send(wts->fd, 500))
    {
        LOG(LOG_LEVEL_ERROR, "WTSVirtualChannelOpenEx: can_send failed");
        free(wts);
        return NULL;
    }

    chan_name_bytes = strlen(pVirtualName);
    bytes = 4 + 4 + 4 + chan_name_bytes + 4;

    LOG_DEVEL(LOG_LEVEL_DEBUG,
              "WTSVirtualChannelOpenEx: chan_name_bytes %d bytes %d pVirtualName %s",
              chan_name_bytes, bytes, pVirtualName);

    connect_data = (char *) calloc(bytes, 1);
    if (connect_data == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "WTSVirtualChannelOpenEx: calloc failed");
        free(wts);
        return NULL;
    }

    connect_data[0] = (bytes >> 0) & 0xFF;
    connect_data[1] = (bytes >> 8) & 0xFF;
    connect_data[2] = (bytes >> 16) & 0xFF;
    connect_data[3] = (bytes >> 24) & 0xFF;

    /* version here(4-7), just leave 0 */

    connect_data[8] = (chan_name_bytes >> 0) & 0xFF;
    connect_data[9] = (chan_name_bytes >> 8) & 0xFF;
    connect_data[10] = (chan_name_bytes >> 16) & 0xFF;
    connect_data[11] = (chan_name_bytes >> 24) & 0xFF;

    memcpy(connect_data + 12, pVirtualName, chan_name_bytes);

    connect_data[4 + 4 + 4 + chan_name_bytes + 0] = (flags >> 0) & 0xFF;
    connect_data[4 + 4 + 4 + chan_name_bytes + 1] = (flags >> 8) & 0xFF;
    connect_data[4 + 4 + 4 + chan_name_bytes + 2] = (flags >> 16) & 0xFF;
    connect_data[4 + 4 + 4 + chan_name_bytes + 3] = (flags >> 24) & 0xFF;

    LOG_DEVEL(LOG_LEVEL_DEBUG,
              "WTSVirtualChannelOpenEx: calling mysend with %d bytes", bytes);

    if (mysend(wts->fd, connect_data, bytes) != bytes)
    {
        LOG(LOG_LEVEL_ERROR, "WTSVirtualChannelOpenEx: mysend failed");
        free(wts);
        return NULL;
    }
    LOG_DEVEL(LOG_LEVEL_DEBUG, "WTSVirtualChannelOpenEx: sent ok");

    if (!can_recv(wts->fd, 500))
    {
        LOG(LOG_LEVEL_ERROR, "WTSVirtualChannelOpenEx: can_recv failed");
        free(wts);
        return NULL;
    }

    /* get response */
    if (myrecv(wts->fd, connect_data, 4) != 4)
    {
        LOG(LOG_LEVEL_ERROR, "WTSVirtualChannelOpenEx: myrecv failed");
        free(wts);
        return NULL;
    }

    if ((connect_data[0] != 0) || (connect_data[1] != 0) ||
            (connect_data[2] != 0) || (connect_data[3] != 0))
    {
        LOG(LOG_LEVEL_ERROR, "WTSVirtualChannelOpenEx: connect_data not ok");
        free(wts);
        return NULL;
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
mysend(int sck, const void *adata, int bytes)
{
    int sent;
    int error;
    const char *data;

#if defined(SO_NOSIGPIPE)
    const int on = 1;
    setsockopt(sck, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
#endif

    data = (const char *) adata;
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

/*****************************************************************************/
static int
myrecv(int sck, void *adata, int bytes)
{
    int recd;
    int error;
    char *data;

#if defined(SO_NOSIGPIPE)
    const int on = 1;
    setsockopt(sck, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
#endif

    data = (char *) adata;
    recd = 0;
    while (recd < bytes)
    {
        if (can_recv(sck, 100))
        {
            error = recv(sck, data + recd, bytes - recd, MSG_NOSIGNAL);
            if (error < 1)
            {
                return -1;
            }
            recd += error;
        }
    }
    return recd;
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
    int rv;

    wts = (struct wts_obj *) hChannelHandle;

    *pBytesWritten = 0;

    if (wts == 0)
    {
        LOG(LOG_LEVEL_ERROR, "WTSVirtualChannelWrite: wts is NULL");
        return 0;
    }

    if (!can_send(wts->fd, 0))
    {
        return 1;    /* can't write now, ok to try again */
    }

    rv = mysend(wts->fd, Buffer, Length);

    LOG_DEVEL(LOG_LEVEL_DEBUG, "WTSVirtualChannelWrite: mysend() returned %d", rv);

    if (rv >= 0)
    {
        /* success, but zero bytes may have been written */
        *pBytesWritten = rv;
        return 1;
    }

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

    if (wts == NULL)
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

    if (wts == NULL)
    {
        return 0;
    }

    if (WtsVirtualClass == WTSVirtualFileHandle)
    {
        *pBytesReturned = 4;
        *ppBuffer = malloc(4);
        if (*ppBuffer == NULL)
        {
            return 0;
        }
        memcpy(*ppBuffer, &(wts->fd), 4);
    }

    return 1;
}

/*****************************************************************************/
void
WTSFreeMemory(void *pMemory)
{
    if (pMemory != NULL)
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
