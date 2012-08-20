/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Thomas Goddard 2012
 * Copyright (C) Jay Sorg 2012
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

/* do not use os_calls in here */

#define LOG_LEVEL 1
#define LLOG(_level, _args) \
  do { if (_level < LOG_LEVEL) { ErrorF _args ; } } while (0)
#define LLOGLN(_level, _args) \
  do { if (_level < LOG_LEVEL) { printf _args ; printf("\n"); } } while (0)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "xrdpapi.h"

struct wts_obj
{
  int fd;
  int status;
  char name[8];
  char dname[128];
  int display_num;
  int flags;
};

/*****************************************************************************/
static int
get_display_num_from_display(char* display_text)
{
  int index;
  int mode;
  int host_index;
  int disp_index;
  int scre_index;
  char host[256];
  char disp[256];
  char scre[256];

  index = 0;
  host_index = 0;
  disp_index = 0;
  scre_index = 0;
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
    else if (mode == 0)
    {
      host[host_index] = display_text[index];
      host_index++;
    }
    else if (mode == 1)
    {
      disp[disp_index] = display_text[index];
      disp_index++;
    }
    else if (mode == 2)
    {
      scre[scre_index] = display_text[index];
      scre_index++;
    }
    index++;
  }
  host[host_index] = 0;
  disp[disp_index] = 0;
  scre[scre_index] = 0;
  return atoi(disp);
}

/*****************************************************************************/
void*
WTSVirtualChannelOpen(void* hServer, unsigned int SessionId,
                      const char* pVirtualName)
{
  if (hServer != WTS_CURRENT_SERVER_HANDLE)
  {
    return 0;
  }
  return WTSVirtualChannelOpenEx(SessionId, pVirtualName, 0);
}

/*****************************************************************************/
static int
can_send(int sck, int millis)
{
  struct timeval time;
  fd_set wfds;
  int select_rv;

  FD_ZERO(&wfds);
  FD_SET(sck, &wfds);
  time.tv_sec = millis / 1000;
  time.tv_usec = (millis * 1000) % 1000000;
  select_rv = select(sck + 1, 0, &wfds, 0, &time);
  if (select_rv > 0)
  {
    return 1;
  }
  return 0;
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
send_init(struct wts_obj* wts)
{
  char initmsg[64];

  memset(initmsg, 0, 64);
  strncpy(initmsg, wts->name, 8);
  initmsg[16] = (wts->flags >> 0) & 0xff;
  initmsg[17] = (wts->flags >> 8) & 0xff;
  initmsg[18] = (wts->flags >> 16) & 0xff;
  initmsg[19] = (wts->flags >> 24) & 0xff;
  LLOGLN(10, ("send_init: sending %s", initmsg));
  if (!can_send(wts->fd, 500))
  {
    return 1;
  }
  if (send(wts->fd, initmsg, 64, 0) != 64)
  {
    return 1;
  }
  LLOGLN(10, ("send_init: send ok!"));
  return 0;
}

/*****************************************************************************/
void*
WTSVirtualChannelOpenEx(unsigned int SessionId,
                        const char* pVirtualName,
                        unsigned int flags)
{
  struct wts_obj* wts;
  char* display_text;
  struct sockaddr_un s;
  int bytes;
  unsigned long llong;

  if (SessionId != WTS_CURRENT_SESSION)
  {
    LLOGLN(0, ("WTSVirtualChannelOpenEx: SessionId bad"));
    return 0;
  }
  wts = (struct wts_obj*)malloc(sizeof(struct wts_obj));
  memset(wts, 0, sizeof(struct wts_obj));
  wts->fd = -1;
  wts->flags = flags;
  display_text = getenv("DISPLAY");
  if (display_text != 0)
  {
    wts->display_num = get_display_num_from_display(display_text);
  }
  if (wts->display_num > 0)
  {
    wts->fd = socket(AF_UNIX, SOCK_STREAM, 0);
    /* set non blocking */
    llong = fcntl(wts->fd, F_GETFL);
    llong = llong | O_NONBLOCK;
    fcntl(wts->fd, F_SETFL, llong);
    /* connect to session chansrv */
    memset(&s, 0, sizeof(struct sockaddr_un));
    s.sun_family = AF_UNIX;
    bytes = sizeof(s.sun_path);
    snprintf(s.sun_path, bytes - 1, "/tmp/.xrdp/xrdpapi_%d", wts->display_num);
    s.sun_path[bytes - 1] = 0;
    bytes = sizeof(struct sockaddr_un);
    if (connect(wts->fd, (struct sockaddr*)&s, bytes) == 0)
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
  }
  else
  {
    LLOGLN(0, ("WTSVirtualChannelOpenEx: display is 0"));
  }
  return wts;
}

/*****************************************************************************/
int
WTSVirtualChannelWrite(void* hChannelHandle, const char* Buffer,
                       unsigned int Length, unsigned int* pBytesWritten)
{
  struct wts_obj* wts;
  int error;
  int lerrno;

  wts = (struct wts_obj*)hChannelHandle;
  if (wts == 0)
  {
    return 0;
  }
  if (wts->status != 1)
  {
    return 0;
  }
  if (can_send(wts->fd, 0))
  {
    error = send(wts->fd, Buffer, Length, 0);
    if (error == -1)
    {
      lerrno = errno;
      if ((lerrno == EWOULDBLOCK) || (lerrno == EAGAIN) ||
          (lerrno == EINPROGRESS))
      {
        *pBytesWritten = 0;
        return 1;
      }
      return 0;
    }
    else if (error == 0)
    {
      return 0;
    }
    else if (error > 0)
    {
      *pBytesWritten = error;
      return 1;
    }
  }
  *pBytesWritten = 0;
  return 1;
}

/*****************************************************************************/
int
WTSVirtualChannelRead(void* hChannelHandle, unsigned int TimeOut,
                      char* Buffer, unsigned int BufferSize,
                      unsigned int* pBytesRead)
{
  struct wts_obj* wts;
  int error;
  int lerrno;

  wts = (struct wts_obj*)hChannelHandle;
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
    error = recv(wts->fd, Buffer, BufferSize, 0);
    if (error == -1)
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
    else if (error == 0)
    {
      return 0;
    }
    else if (error > 0)
    {
      *pBytesRead = error;
      return 1;
    }
  }
  *pBytesRead = 0;
  return 1;
}

/*****************************************************************************/
int
WTSVirtualChannelClose(void* hChannelHandle)
{
  struct wts_obj* wts;

  wts = (struct wts_obj*)hChannelHandle;
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
WTSVirtualChannelQuery(void* hChannelHandle, WTS_VIRTUAL_CLASS WtsVirtualClass,
                       void** ppBuffer, unsigned int* pBytesReturned)
{
  struct wts_obj* wts;

  wts = (struct wts_obj*)hChannelHandle;
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
WTSFreeMemory(void* pMemory)
{
  if (pMemory != 0)
  {
    free(pMemory);
  }
}
