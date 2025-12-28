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
#include <poll.h>

#include "log.h"
#include "xrdp_sockets.h"
#include "string_calls.h"
#include "channel_defs.h"
#include "xrdpapi.h"

struct wts_obj
{
    int fd;
    int display_num;
};

/**
 * Data we store for each server
 */
struct wts_server
{
    struct wts_obj *info_obj; // Object to get session notifications
    struct xrdp_chan_session_state session_state; // session state
};

static struct wts_server wts_current_server;

/* helper functions used by WTSxxx API - do not invoke directly */
static int
can_send(int sck, int millis, int restart);
static int
can_recv(int sck, int millis, int restart);
static int
mysend(int sck, const void *adata, int bytes);
static int
myrecv(int sck, void *adata, int bytes);
static int
mypeek(int sck, void *adata, int bytes);

static void
free_wts(struct wts_obj *wts)
{
    if (wts != NULL)
    {
        if (wts->fd >= 0)
        {
            close(wts->fd);
        }
        free(wts);
    }
}

/*
 * Opens a handle to the server end of a specified virtual channel
 *
 * @param  SessionId     - current session ID; *must* be WTS_CURRENT_SESSION
 * @param  pVirtualName  - virtual channel name when using SVC
 *                       - name of endpoint listener when using DVC
 * @param  flags         - type of channel and channel priority if DVC
 * @param  private_chan  - If != 0, this is a private channel defined
 *                          in channel_defs.h
 * @param[out] errcode   - Indication for the user of a possible
 *                         error. Cannot be defaulted. Is only set on error.
 *
 * @return a valid pointer on success, NULL on error
 ******************************************************************************/
static void *
VirtualChannelOpen(unsigned int SessionId, const char *pVirtualName,
                   unsigned int flags,
                   unsigned int private_chan,
                   enum wts_errcode *errcode)
{
    struct wts_obj *wts;
    int bytes;
    unsigned long long1;
    struct sockaddr_un s;
    // Pad the connect data out to a larger size to allow for
    // changes to struct xrdp_chan_connect
    union
    {
        char pad[XRDPAPI_CONNECT_PDU_LEN];
        struct xrdp_chan_connect connect_data;
    } cd = {0};

    uint32_t connect_result;
    int lerrno;

    if (SessionId != WTS_CURRENT_SESSION)
    {
        LOG(LOG_LEVEL_ERROR, "WTSVirtualChannelOpenEx: bad SessionId");
        *errcode = WTS_E_BAD_SESSION_ID;
        return 0;
    }

    wts = (struct wts_obj *) calloc(1, sizeof(struct wts_obj));
    if (wts == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "WTSVirtualChannelOpenEx: calloc failed");
        *errcode = WTS_E_RESOURCE_ERROR;
        return 0;
    }
    wts->fd = -1;
    wts->display_num = g_get_display_num_from_display(getenv("DISPLAY"));
    if (wts->display_num < 0)
    {
        LOG(LOG_LEVEL_ERROR, "WTSVirtualChannelOpenEx: fatal error; invalid DISPLAY");
        *errcode = WTS_E_RESOURCE_ERROR;
        free_wts(wts);
        return NULL;
    }

    /* we use unix domain socket to communicate with chansrv */
    if ((wts->fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
    {
        LOG(LOG_LEVEL_ERROR, "WTSVirtualChannelOpenEx: socket failed");
        *errcode = WTS_E_RESOURCE_ERROR;
        free_wts(wts);
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
    snprintf(s.sun_path, bytes - 1, CHANSRV_API_STR, getuid(), wts->display_num);
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
            *errcode = WTS_E_CHANSRV_NOT_UP;
            free_wts(wts);
            return NULL;
        }
    }

    /* wait for connection to complete */
    if (!can_send(wts->fd, 500, 1))
    {
        LOG(LOG_LEVEL_ERROR, "WTSVirtualChannelOpenEx: can_send failed");
        *errcode = WTS_E_CHANSRV_NOT_UP;
        free_wts(wts);
        return NULL;
    }

    cd.connect_data.version = XRDPAPI_CONNECT_PDU_VERSION;
    cd.connect_data.private_chan = private_chan;
    cd.connect_data.flags = flags;
    snprintf(cd.connect_data.name, sizeof(cd.connect_data.name), "%s", pVirtualName);

    if (mysend(wts->fd, &cd, sizeof(cd)) != sizeof(cd))
    {
        LOG(LOG_LEVEL_ERROR, "WTSVirtualChannelOpenEx: mysend failed");
        *errcode = WTS_E_RESOURCE_ERROR;
        free_wts(wts);
        return NULL;
    }
    LOG_DEVEL(LOG_LEVEL_DEBUG, "WTSVirtualChannelOpenEx: sent ok");

    if (!can_recv(wts->fd, 500, 1))
    {
        LOG(LOG_LEVEL_ERROR, "WTSVirtualChannelOpenEx: can_recv failed");
        *errcode = WTS_E_RESOURCE_ERROR;
        free_wts(wts);
        return NULL;
    }

    /* get response */
    if (myrecv(wts->fd, &connect_result, sizeof(connect_result)) !=
            sizeof(connect_result))
    {
        LOG(LOG_LEVEL_ERROR, "WTSVirtualChannelOpenEx: myrecv failed");
        *errcode = WTS_E_RESOURCE_ERROR;
        free_wts(wts);
        return NULL;
    }

    if (connect_result != 0)
    {
        LOG(LOG_LEVEL_ERROR, "WTSVirtualChannelOpenEx: connect_data not ok");
        *errcode = WTS_E_RESOURCE_ERROR;
        free_wts(wts);
        return NULL;
    }

    return wts;
}

/*
 * Opens a handle to the server end of a specified virtual channel - this
 * call is deprecated - use WTSVirtualChannelOpenEx() instead
 *
 * @param  hServer       - *must* be WTS_CURRENT_SERVER_HANDLE
 * @param  SessionId     - current session ID; *must* be WTS_CURRENT_SESSION
 * @param  pVirtualName  - virtual channel name when using SVC
 *                       - name of endpoint listener when using DVC
 *
 * @return a valid pointer on success, NULL on error
 ******************************************************************************/
void *
WTSVirtualChannelOpen(void *hServer, unsigned int SessionId,
                      const char *pVirtualName)
{
    enum wts_errcode errcode_dummy = WTS_E_NO_ERROR;

    if (hServer != WTS_CURRENT_SERVER_HANDLE)
    {
        return 0;
    }
    return VirtualChannelOpen(SessionId, pVirtualName, 0, 0, &errcode_dummy);
}

/*
 * Opens a handle to the server end of a specified virtual channel
 *
 * @param  SessionId     - current session ID; *must* be WTS_CURRENT_SESSION
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
    enum wts_errcode errcode_dummy = WTS_E_NO_ERROR;
    return VirtualChannelOpen(SessionId, pVirtualName,
                              flags, 0, &errcode_dummy);
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
        if (can_send(sck, 100, 0))
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
        if (can_recv(sck, 100, 0))
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

/*****************************************************************************/
static int
mypeek(int sck, void *adata, int bytes)
{
    int error;

#if defined(SO_NOSIGPIPE)
    const int on = 1;
    setsockopt(sck, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
#endif

    error = 0;
    if (can_recv(sck, 100, 0))
    {
        error = recv(sck, adata, bytes, MSG_NOSIGNAL | MSG_PEEK);
    }
    return error;
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

    if (!can_send(wts->fd, 0, 0))
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

    if (can_recv(wts->fd, TimeOut, 0))
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
    struct wts_obj *wts = (struct wts_obj *)hChannelHandle;

    if (wts == NULL)
    {
        return 0;
    }

    free_wts(wts);
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
 * @param  restart Try again if interrupted, even if this exceeds the timeout
 *
 * @return 0 if write will block
 * @return 1 if write will not block
 ******************************************************************************/
static int
can_send(int sck, int millis, int restart)
{
    int rv = 0;
    struct pollfd pollfd;
    int status;

    pollfd.fd = sck;
    pollfd.events = POLLOUT;
    pollfd.revents = 0;

    do
    {
        status = poll(&pollfd, 1, millis);
    }
    while (status < 0 && errno == EINTR && restart);

    if (status > 0)
    {
        if ((pollfd.revents & POLLOUT) != 0)
        {
            rv = 1;
        }
    }

    return rv;
}

/*****************************************************************************/
static int
can_recv(int sck, int millis, int restart)
{
    int rv = 0;
    struct pollfd pollfd;
    int status;

    pollfd.fd = sck;
    pollfd.events = POLLIN;
    pollfd.revents = 0;
    do
    {
        status = poll(&pollfd, 1, millis);
    }
    while (status < 0 && errno == EINTR && restart);

    if (status > 0)
    {
        if ((pollfd.revents & (POLLIN | POLLHUP)) != 0)
        {
            rv = 1;
        }
    }

    return rv;
}

/*****************************************************************************/
int WTSQuerySessionInformationA(void *hServer,
                                unsigned int SessionId,
                                WTS_INFO_CLASS WTSInfoClass,
                                void           *ppBuffer,
                                DWORD          *pBytesReturned,
                                enum wts_errcode *errcode)
{
    int rv = 0;
    enum wts_errcode errcode_dummy;
    if (errcode == NULL)
    {
        errcode = &errcode_dummy;
    }
    *errcode = WTS_E_NO_ERROR;

    if (hServer != WTS_CURRENT_SERVER_HANDLE)
    {
        LOG(LOG_LEVEL_ERROR, "WTSQuerySessionInformationA: bad hServer");
        *errcode = WTS_E_BAD_SERVER;
    }
    else if (SessionId != WTS_CURRENT_SESSION)
    {
        LOG(LOG_LEVEL_ERROR, "WTSQuerySessionInformationA: bad SessionId");
        *errcode = WTS_E_BAD_SESSION_ID;
    }
    else if (WTSInfoClass != WTSConnectState)
    {
        LOG(LOG_LEVEL_ERROR,
            "WTSQuerySessionInformationA: unsupported WTSInfoClass");
        *errcode = WTS_E_BAD_INFO_CLASS;
    }
    else
    {
        rv = 1; // Assume success
        if (wts_current_server.info_obj == NULL)
        {
            // We don't have a current connection for server state events.
            // Set one up to update our cached values, then tear it down again.
            int fd;

            rv = WTSRegisterSessionNotificationEx(hServer, &fd, 0, errcode) &&
                 WTSUnRegisterSessionNotificationEx(hServer, fd, errcode);
        }

        if (rv == 1)
        {
            *(WTS_CONNECTSTATE_CLASS *)ppBuffer =
                (wts_current_server.session_state.is_connected)
                ? WTSConnected
                : WTSDisconnected;
            if (pBytesReturned != NULL)
            {
                *pBytesReturned = sizeof(WTS_CONNECTSTATE_CLASS);
            }
        }
    }

    return rv;
}

/*****************************************************************************/
int WTSRegisterSessionNotificationEx(void *hServer,
                                     int *fd_ptr,
                                     int dwFlags,
                                     enum wts_errcode *errcode)
{
    (void)dwFlags;  // Unused parameter
    enum wts_errcode errcode_dummy;
    if (errcode == NULL)
    {
        errcode = &errcode_dummy;
    }
    *errcode = WTS_E_NO_ERROR;

    if (hServer != WTS_CURRENT_SERVER_HANDLE)
    {
        LOG(LOG_LEVEL_ERROR, "WTSRegisterSessionNotificationEx: bad hServer");
        *errcode = WTS_E_BAD_SERVER;
        return 0;
    }

    if (wts_current_server.info_obj == NULL)
    {
        // Open a private channel for session info messages
        // The name and flags args are ignored for xrdp private channels
        struct wts_obj *wts = (struct wts_obj *)
                              VirtualChannelOpen(WTS_CURRENT_SESSION,
                                  "", 0,
                                  CHAN_ID_XRDP_SESSION_INFO,
                                  errcode);
        if (wts != NULL)
        {
            // Server will pass the current session state now
            struct xrdp_chan_session_state sess_state;
            if (!can_recv(wts->fd, 1000, 1))
            {
                LOG(LOG_LEVEL_ERROR,
                    "WTSRegisterSessionNotificationEx: can_recv failed");
                *errcode = WTS_E_RESOURCE_ERROR;
                free_wts(wts);
            }
            /* get server_status */
            else if (myrecv(wts->fd, &sess_state, sizeof(sess_state)) !=
                     sizeof(sess_state))
            {
                LOG(LOG_LEVEL_ERROR,
                    "WTSRegisterSessionNotificationEx: myrecv failed");
                *errcode = WTS_E_RESOURCE_ERROR;
                free_wts(wts);
            }
            else
            {
                wts_current_server.info_obj = wts;
                wts_current_server.session_state = sess_state;
            }
        }
    }
    if (wts_current_server.info_obj == NULL)
    {
        *fd_ptr = -1;
        return 0;
    }
    *fd_ptr = wts_current_server.info_obj->fd;
    return 1;
}

/*****************************************************************************/
int WTSUnRegisterSessionNotificationEx(void *hServer,
                                       int fd,
                                       enum wts_errcode *errcode)
{
    (void)fd;  // Unused parameter
    enum wts_errcode errcode_dummy;
    if (errcode == NULL)
    {
        errcode = &errcode_dummy;
    }
    *errcode = WTS_E_NO_ERROR;

    if (hServer != WTS_CURRENT_SERVER_HANDLE)
    {
        LOG(LOG_LEVEL_ERROR, "WTSUnRegisterSessionNotificationEx: bad hServer");
        *errcode = WTS_E_BAD_SERVER;
        return 0;
    }

    free_wts(wts_current_server.info_obj);
    wts_current_server.info_obj = NULL;
    return 1;
}

/*****************************************************************************/
int
WTSGetDispatchMessage(void *cbdata, WNDPROC wndproc, LRESULT *lResult)
{
    LRESULT result = 0;
    struct xrdp_chan_session_state new_state;

    /* get response */
    if (wts_current_server.info_obj == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "WTSGetDispatchMessage: No notification channel was opened");
    }
    else if (!can_recv(wts_current_server.info_obj->fd, 0, 1))
    {
        // No message available - nothing to log
    }
    else if (mypeek(wts_current_server.info_obj->fd,
                    &new_state, sizeof(new_state)) != sizeof(new_state))
    {
        LOG(LOG_LEVEL_ERROR, "WTSGetDispatchMessage: An incomplete message was received");
    }
    else
    {
        /* We've peeked a message. Find a SINGLE difference with our
         * own state, update our state, and issue a callback. If there
         * are more differences, the application will call us back and
         * we can process the next one. When all the differences are
         * accounted for, we can clear the message from the queue, and
         * the fd will no longer be readable */
        UINT msgno = 0; // 0 means 'no message'
        WPARAM wParam = 0;
        LPARAM lParam = 0;
        // Look for a single difference in the state we have, and the
        // current state from the server
        struct xrdp_chan_session_state *curr_state;
        curr_state = &wts_current_server.session_state;

        if (curr_state->is_connected != new_state.is_connected)
        {
            curr_state->is_connected = new_state.is_connected;
            msgno = WM_WTSSESSION_CHANGE;
            wParam = (new_state.is_connected)
                     ? WTS_REMOTE_CONNECT : WTS_REMOTE_DISCONNECT;
        }

        // If we found a difference, activate the callback
        if (msgno != 0)
        {
            *lResult = wndproc(cbdata, msgno, wParam, lParam);
            result = 1;
        }

        // If we've exhausted all the differences between the old and
        // the new state, purge the message from the queue
        if (curr_state->is_connected == new_state.is_connected &&
                /* Add further checks here in the future */
                1)
        {
            /* Sanity check on the fd */
            if (wts_current_server.info_obj->fd >= 0)
            {
                (void)myrecv(wts_current_server.info_obj->fd,
                             &new_state, sizeof(new_state));
            }
        }
    }

    return result;
}
