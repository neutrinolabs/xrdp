/*
Copyright 2005-2012 Jay Sorg

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

the rest

*/

#include "rdp.h"

#include <sys/un.h>

Bool noFontCacheExtension = 1;

/******************************************************************************/
/* print a time-stamped message to the log file (stderr). */
void
rdpLog(char *format, ...)
{
  va_list args;
  char buf[256];
  time_t clock;

  va_start(args, format);
  time(&clock);
  strftime(buf, 255, "%d/%m/%y %T ", localtime(&clock));
  fprintf(stderr, "%s", buf);
  vfprintf(stderr, format, args);
  fflush(stderr);
  va_end(args);
}

/******************************************************************************/
int
rdpBitsPerPixel(int depth)
{
  if (depth == 1)
  {
    return 1;
  }
  else if (depth <= 8)
  {
    return 8;
  }
  else if (depth <= 16)
  {
    return 16;
  }
  else
  {
    return 32;
  }
}

/******************************************************************************/
void
rdpClientStateChange(CallbackListPtr* cbl, pointer myData, pointer clt)
{
  dispatchException &= ~DE_RESET; /* hack - force server not to reset */
}

/******************************************************************************/
int
DPMSSupported(void)
{
  return 0;
}

/******************************************************************************/
int
DPSMGet(int* level)
{
  return -1;
}

/******************************************************************************/
void
DPMSSet(int level)
{
}

/******************************************************************************/
void
AddOtherInputDevices(void)
{
}

/******************************************************************************/
void
OpenInputDevice(DeviceIntPtr dev, ClientPtr client, int* status)
{
}

/******************************************************************************/
int
SetDeviceValuators(register ClientPtr client, DeviceIntPtr dev,
                   int* valuators, int first_valuator, int num_valuators)
{
  return BadMatch;
}

/******************************************************************************/
int
SetDeviceMode(register ClientPtr client, DeviceIntPtr dev, int mode)
{
  return BadMatch;
}

/******************************************************************************/
int
ChangeKeyboardDevice(DeviceIntPtr old_dev, DeviceIntPtr new_dev)
{
  return BadMatch;
}

/******************************************************************************/
int
ChangeDeviceControl(register ClientPtr client, DeviceIntPtr dev,
                    void* control)
{
  return BadMatch;
}

/******************************************************************************/
int
ChangePointerDevice(DeviceIntPtr old_dev, DeviceIntPtr new_dev,
                    unsigned char x, unsigned char y)
{
  return BadMatch;
}

/******************************************************************************/
void
CloseInputDevice(DeviceIntPtr d, ClientPtr client)
{
}

/* the g_ functions from os_calls.c */

/*****************************************************************************/
int
g_tcp_recv(int sck, void* ptr, int len, int flags)
{
  return recv(sck, ptr, len, flags);
}

/*****************************************************************************/
void
g_tcp_close(int sck)
{
  if (sck == 0)
  {
    return;
  }
  shutdown(sck, 2);
  close(sck);
}

/*****************************************************************************/
int
g_tcp_last_error_would_block(int sck)
{
  return (errno == EWOULDBLOCK) || (errno == EINPROGRESS);
}

/*****************************************************************************/
void
g_sleep(int msecs)
{
  usleep(msecs * 1000);
}

/*****************************************************************************/
int
g_tcp_send(int sck, void* ptr, int len, int flags)
{
  return send(sck, ptr, len, flags);
}

/*****************************************************************************/
void*
g_malloc(int size, int zero)
{
  char* rv;

//#ifdef _XSERVER64
#if 1
  /* I thought xalloc whould work here but I guess not, why, todo */
  rv = (char*)malloc(size);
#else
  rv = (char*)Xalloc(size);
#endif
  if (zero)
  {
    if (rv != 0)
    {
      memset(rv, 0, size);
    }
  }
  return rv;
}

/*****************************************************************************/
void
g_free(void* ptr)
{
  if (ptr != 0)
  {
//#ifdef _XSERVER64
#if 1
    /* I thought xfree whould work here but I guess not, why, todo */
    free(ptr);
#else
    Xfree(ptr);
#endif
  }
}

/*****************************************************************************/
void
g_sprintf(char* dest, char* format, ...)
{
  va_list ap;

  va_start(ap, format);
  vsprintf(dest, format, ap);
  va_end(ap);
}

/*****************************************************************************/
int
g_tcp_socket(void)
{
  int rv;
  int i;

  i = 1;
  rv = socket(PF_INET, SOCK_STREAM, 0);
  setsockopt(rv, IPPROTO_TCP, TCP_NODELAY, (void*)&i, sizeof(i));
  setsockopt(rv, SOL_SOCKET, SO_REUSEADDR, (void*)&i, sizeof(i));
  return rv;
}

/*****************************************************************************/
int
g_tcp_local_socket_dgram(void)
{
  return socket(AF_UNIX, SOCK_DGRAM, 0);
}

/*****************************************************************************/
int
g_tcp_local_socket_stream(void)
{
  return socket(AF_UNIX, SOCK_STREAM, 0);
}

/*****************************************************************************/
void
g_memcpy(void* d_ptr, const void* s_ptr, int size)
{
  memcpy(d_ptr, s_ptr, size);
}

/*****************************************************************************/
int
g_tcp_set_no_delay(int sck)
{
  int i;

  i = 1;
  setsockopt(sck, IPPROTO_TCP, TCP_NODELAY, (void*)&i, sizeof(i));
  return 0;
}

/*****************************************************************************/
int
g_tcp_set_non_blocking(int sck)
{
  unsigned long i;

  i = fcntl(sck, F_GETFL);
  i = i | O_NONBLOCK;
  fcntl(sck, F_SETFL, i);
  return 0;
}

/*****************************************************************************/
int
g_tcp_accept(int sck)
{
  struct sockaddr_in s;
  unsigned int i;

  i = sizeof(struct sockaddr_in);
  memset(&s, 0, i);
  return accept(sck, (struct sockaddr*)&s, &i);
}

/*****************************************************************************/
int
g_tcp_select(int sck1, int sck2, int sck3)
{
  fd_set rfds;
  struct timeval time;
  int max;
  int rv;

  time.tv_sec = 0;
  time.tv_usec = 0;
  FD_ZERO(&rfds);
  if (sck1 > 0)
  {
    FD_SET(((unsigned int)sck1), &rfds);
  }
  if (sck2 > 0)
  {
    FD_SET(((unsigned int)sck2), &rfds);
  }
  if (sck3 > 0)
  {
    FD_SET(((unsigned int)sck3), &rfds);
  }
  max = sck1;
  if (sck2 > max)
  {
    max = sck2;
  }
  if (sck3 > max)
  {
    max = sck3;
  }
  rv = select(max + 1, &rfds, 0, 0, &time);
  if (rv > 0)
  {
    rv = 0;
    if (FD_ISSET(((unsigned int)sck1), &rfds))
    {
      rv = rv | 1;
    }
    if (FD_ISSET(((unsigned int)sck2), &rfds))
    {
      rv = rv | 2;
    }
    if (FD_ISSET(((unsigned int)sck3), &rfds))
    {
      rv = rv | 4;
    }
  }
  else
  {
    rv = 0;
  }
  return rv;
}

/*****************************************************************************/
int
g_tcp_bind(int sck, char* port)
{
  struct sockaddr_in s;

  memset(&s, 0, sizeof(struct sockaddr_in));
  s.sin_family = AF_INET;
  s.sin_port = htons(atoi(port));
  s.sin_addr.s_addr = INADDR_ANY;
  return bind(sck, (struct sockaddr*)&s, sizeof(struct sockaddr_in));
}

/*****************************************************************************/
int
g_tcp_local_bind(int sck, char* port)
{
  struct sockaddr_un s;

  memset(&s, 0, sizeof(struct sockaddr_un));
  s.sun_family = AF_UNIX;
  strcpy(s.sun_path, port);
  return bind(sck, (struct sockaddr*)&s, sizeof(struct sockaddr_un));
}

/*****************************************************************************/
int
g_tcp_listen(int sck)
{
  return listen(sck, 2);
}

/*****************************************************************************/
/* returns boolean */
int
g_create_dir(const char* dirname)
{
  return mkdir(dirname, (mode_t)-1) == 0;
}

/*****************************************************************************/
/* returns boolean, non zero if the directory exists */
int
g_directory_exist(const char* dirname)
{
  struct stat st;

  if (stat(dirname, &st) == 0)
  {
    return S_ISDIR(st.st_mode);
  }
  else
  {
    return 0;
  }
}

/*****************************************************************************/
/* returns error */
int
g_chmod_hex(const char* filename, int flags)
{
  int fl;

  fl = 0;
  fl |= (flags & 0x4000) ? S_ISUID : 0;
  fl |= (flags & 0x2000) ? S_ISGID : 0;
  fl |= (flags & 0x1000) ? S_ISVTX : 0;
  fl |= (flags & 0x0400) ? S_IRUSR : 0;
  fl |= (flags & 0x0200) ? S_IWUSR : 0;
  fl |= (flags & 0x0100) ? S_IXUSR : 0;
  fl |= (flags & 0x0040) ? S_IRGRP : 0;
  fl |= (flags & 0x0020) ? S_IWGRP : 0;
  fl |= (flags & 0x0010) ? S_IXGRP : 0;
  fl |= (flags & 0x0004) ? S_IROTH : 0;
  fl |= (flags & 0x0002) ? S_IWOTH : 0;
  fl |= (flags & 0x0001) ? S_IXOTH : 0;
  return chmod(filename, fl);
}

/* produce a hex dump */
void
hexdump(unsigned char* p, unsigned int len)
{
  unsigned char* line;
  int i;
  int thisline;
  int offset;

  offset = 0;
  line = p;
  while (offset < len)
  {
    ErrorF("%04x ", offset);
    thisline = len - offset;
    if (thisline > 16)
      thisline = 16;

    for (i = 0; i < thisline; i++)
      ErrorF("%02x ", line[i]);

    for (; i < 16; i++)
      ErrorF("   ");

    for (i = 0; i < thisline; i++)
      ErrorF("%c", (line[i] >= 0x20 && line[i] < 0x7f) ? line[i] : '.');

    ErrorF("\n");
    offset += thisline;
    line += thisline;
  }
}

/*
  stub for XpClient* functions.
*/

/*****************************************************************************/
Bool
XpClientIsBitmapClient(ClientPtr client)
{
  return 1;
}

/*****************************************************************************/
Bool
XpClientIsPrintClient(ClientPtr client, FontPathElementPtr fpe)
{
  return 0;
}

/*****************************************************************************/
int
PrinterOptions(int argc, char** argv, int i)
{
  return i;
}

/*****************************************************************************/
void
PrinterInitOutput(ScreenInfo* pScreenInfo, int argc, char** argv)
{
}

/*****************************************************************************/
void
PrinterUseMsg(void)
{
}

/*****************************************************************************/
void
PrinterInitGlobals(void)
{
}

/*****************************************************************************/
void
FontCacheExtensionInit(INITARGS)
{
}

/******************************************************************************/
void
RegionAroundSegs(RegionPtr reg, xSegment* segs, int nseg)
{
  int index;
  BoxRec box;
  RegionRec treg;

  index = 0;
  while (index < nseg)
  {
    if (segs[index].x1 < segs[index].x2)
    {
      box.x1 = segs[index].x1;
      box.x2 = segs[index].x2;
    }
    else
    {
      box.x1 = segs[index].x2;
      box.x2 = segs[index].x1;
    }
    box.x2++;
    if (segs[index].y1 < segs[index].y2)
    {
      box.y1 = segs[index].y1;
      box.y2 = segs[index].y2;
    }
    else
    {
      box.y1 = segs[index].y2;
      box.y2 = segs[index].y1;
    }
    box.y2++;
    RegionInit(&treg, &box, 0);
    RegionUnion(reg, reg, &treg);
    RegionUninit(&treg);
    index++;
  }
}
