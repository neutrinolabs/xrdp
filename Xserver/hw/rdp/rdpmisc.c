/*
Copyright 2005-2007 Jay Sorg

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
  fprintf(stderr, buf);
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
#if defined(_WIN32)
  return recv(sck, (char*)ptr, len, flags);
#else
  return recv(sck, ptr, len, flags);
#endif
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
#if defined(_WIN32)
  closesocket(sck);
#else
  close(sck);
#endif
}

/*****************************************************************************/
int
g_tcp_last_error_would_block(int sck)
{
#if defined(_WIN32)
  return WSAGetLastError() == WSAEWOULDBLOCK;
#else
  return errno == EWOULDBLOCK;
#endif
}

/*****************************************************************************/
void
g_sleep(int msecs)
{
#if defined(_WIN32)
  Sleep(msecs);
#else
  usleep(msecs * 1000);
#endif
}

/*****************************************************************************/
int
g_tcp_send(int sck, void* ptr, int len, int flags)
{
#if defined(_WIN32)
  return send(sck, (char*)ptr, len, flags);
#else
  return send(sck, ptr, len, flags);
#endif
}

/*****************************************************************************/
void*
g_malloc(int size, int zero)
{
  char* rv;

  rv = (char*)malloc(size);
  if (zero)
  {
    memset(rv, 0, size);
  }
  return rv;
}

/*****************************************************************************/
void
g_free(void* ptr)
{
  if (ptr != 0)
  {
    free(ptr);
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
#if defined(_WIN32)
  setsockopt(rv, IPPROTO_TCP, TCP_NODELAY, (char*)&i, sizeof(i));
#else
  setsockopt(rv, IPPROTO_TCP, TCP_NODELAY, (void*)&i, sizeof(i));
#endif
  return rv;
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
#if defined(_WIN32)
  setsockopt(sck, IPPROTO_TCP, TCP_NODELAY, (char*)&i, sizeof(i));
#else
  setsockopt(sck, IPPROTO_TCP, TCP_NODELAY, (void*)&i, sizeof(i));
#endif
  return 0;
}

/*****************************************************************************/
int
g_tcp_set_non_blocking(int sck)
{
  unsigned long i;

#if defined(_WIN32)
  i = 1;
  ioctlsocket(sck, FIONBIO, &i);
#else
  i = fcntl(sck, F_GETFL);
  i = i | O_NONBLOCK;
  fcntl(sck, F_SETFL, i);
#endif
  return 0;
}

/*****************************************************************************/
int
g_tcp_accept(int sck)
{
  struct sockaddr_in s;
#if defined(_WIN32)
  signed int i;
#else
  unsigned int i;
#endif

  i = sizeof(struct sockaddr_in);
  memset(&s, 0, i);
  return accept(sck, (struct sockaddr*)&s, &i);
}

/*****************************************************************************/
int
g_tcp_select(int sck1, int sck2)
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
  max = sck1;
  if (sck2 > max)
  {
    max = sck2;
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
g_tcp_listen(int sck)
{
  return listen(sck, 2);
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
void PrinterUseMsg(void)
{
}

/*****************************************************************************/
void PrinterInitGlobals(void)
{
}

#ifdef RDP_IS_XORG

#define NEED_XF86_TYPES
#include <xf86_libc.h>

#define XF86FILE_magic 0x58464856 /* "XFHV" */

typedef struct _xf86_file_
{
  INT32 fileno;
  INT32 magic;
  FILE* filehnd;
  char* fname;
} XF86FILE_priv;

XF86FILE_priv stdhnd[3] =
{
  { 0, XF86FILE_magic, NULL, "$stdinp$" },
  { 0, XF86FILE_magic, NULL, "$stdout$" },
  { 0, XF86FILE_magic, NULL, "$stderr$" }
};

XF86FILE* xf86stdin = (XF86FILE*)&stdhnd[0];
XF86FILE* xf86stdout = (XF86FILE*)&stdhnd[1];
XF86FILE* xf86stderr = (XF86FILE*)&stdhnd[2];

double xf86HUGE_VAL;
int xf86errno;
Bool noFontCacheExtension = 1;

#define mapnum(e) case (xf86_##e): err = e; break;

/*****************************************************************************/
static int
xf86GetErrno(void)
{
  int err;

  switch (errno)
  {
    case 0:
      return 0;
    mapnum(EACCES);
    mapnum(EAGAIN);
    mapnum(EBADF);
    mapnum(EEXIST);
    mapnum(EFAULT);
    mapnum(EINTR);
    mapnum(EINVAL);
    mapnum(EISDIR);
    mapnum(ELOOP); /* not POSIX 1 */
    mapnum(EMFILE);
    mapnum(ENAMETOOLONG);
    mapnum(ENFILE);
    mapnum(ENOENT);
    mapnum(ENOMEM);
    mapnum(ENOSPC);
    mapnum(ENOTDIR);
    mapnum(EPIPE);
    mapnum(EROFS);
    mapnum(ETXTBSY); /* not POSIX 1 */
    mapnum(ENOTTY);
    mapnum(EBUSY);
    mapnum(ENODEV);
    mapnum(EIO);
    default:
      return xf86_UNKNOWN;
  }
  return (int)strerror(err);
}

/*****************************************************************************/
static void
_xf86checkhndl(XF86FILE_priv* f, const char* func)
{
  if (!f || f->magic != XF86FILE_magic ||
      !f->filehnd || !f->fname)
  {
    FatalError("libc_wrapper error: passed invalid FILE handle to %s", func);
    exit(42);
  }
}

/*****************************************************************************/
void
xf86WrapperInit(void)
{
  if (stdhnd[0].filehnd == NULL)
  {
    stdhnd[0].filehnd = stdin;
  }
  if (stdhnd[1].filehnd == NULL)
  {
    stdhnd[1].filehnd = stdout;
  }
  if (stdhnd[2].filehnd == NULL)
  {
    stdhnd[2].filehnd = stderr;
  }
  xf86HUGE_VAL = HUGE_VAL;
}

/*****************************************************************************/
int
xf86strcmp(const char* s1, const char* s2)
{
  return strcmp(s1, s2);
}

/*****************************************************************************/
char*
xf86strchr(const char* s, int c)
{
  return strchr(s, c);
}

/*****************************************************************************/
double
xf86fabs(double x)
{
  return fabs(x);
}

/*****************************************************************************/
double
xf86exp(double x)
{
  return exp(x);
}

/*****************************************************************************/
double
xf86log(double x)
{
  return log(x);
}

/*****************************************************************************/
double
xf86sin(double x)
{
  return sin(x);
}

/*****************************************************************************/
double
xf86cos(double x)
{
  return cos(x);
}

/*****************************************************************************/
double
xf86sqrt(double x)
{
  return sqrt(x);
}

/*****************************************************************************/
double
xf86floor(double x)
{
  return floor(x);
}

/*****************************************************************************/
void
xf86free(void* p)
{
  xfree(p);
}

/*****************************************************************************/
int
xf86fclose(XF86FILE* f)
{
  XF86FILE_priv* fp = (XF86FILE_priv*)f;
  int ret;

  _xf86checkhndl(fp, "xf86fclose");

  /* somewhat bad check */
  if (fp->fileno < 3 && fp->fname[0] == '$')
  {
    /* assume this is stdin/out/err, don't dispose */
    ret = fclose(fp->filehnd);
  }
  else
  {
    ret = fclose(fp->filehnd);
    fp->magic = 0; /* invalidate */
    xfree(fp->fname);
    xfree(fp);
  }
  return ret ? -1 : 0;
}

/*****************************************************************************/
int
xf86fflush(XF86FILE* f)
{
  XF86FILE_priv* fp = (XF86FILE_priv*)f;

  _xf86checkhndl(fp,"xf86fflush");
  return fflush(fp->filehnd);
}

/*****************************************************************************/
int
xf86fprintf(XF86FILE* f, const char *format, ...)
{
  XF86FILE_priv* fp = (XF86FILE_priv*)f;

  int ret;
  va_list args;
  va_start(args, format);

#ifdef DEBUG
  ErrorF("xf86fprintf for XF86FILE %p\n", fp);
#endif
  _xf86checkhndl(fp,"xf86fprintf");

  ret = vfprintf(fp->filehnd,format,args);
  va_end(args);
  return ret;
}

/*****************************************************************************/
char*
xf86strdup(const char* s)
{
  return xstrdup(s);
}

/*****************************************************************************/
XF86FILE*
xf86fopen(const char* fn, const char* mode)
{
  XF86FILE_priv* fp;
  FILE* f = fopen(fn, mode);

  xf86errno = xf86GetErrno();
  if (!f)
  {
    return 0;
  }
  fp = (XF86FILE_priv*)xalloc(sizeof(XF86FILE_priv));
  fp->magic = XF86FILE_magic;
  fp->filehnd = f;
  fp->fileno = fileno(f);
  fp->fname = (char*)xf86strdup(fn);
#ifdef DEBUG
  ErrorF("xf86fopen(%s,%s) yields FILE %p XF86FILE %p\n",
         fn,mode,f,fp);
#endif
  return (XF86FILE*)fp;
}

/*****************************************************************************/
int
xf86sprintf(char* s, const char* format, ...)
{
  int ret;
  va_list args;

  va_start(args, format);
  ret = vsprintf(s, format, args);
  va_end(args);
  return ret;
}

/*****************************************************************************/
double
xf86atof(const char* s)
{
  return atof(s);
}

/*****************************************************************************/
xf86size_t
xf86strlen(const char* s)
{
  return (xf86size_t)strlen(s);
}

/*****************************************************************************/
void
xf86exit(int ex)
{
  ErrorF("Module called exit() function with value=%d\n", ex);
  exit(ex);
}

/*****************************************************************************/
int
xf86vsprintf(char* s, const char* format, va_list ap)
{
  return vsprintf(s, format, ap);
}

/*****************************************************************************/
double
xf86frexp(double x, int* exp)
{
  return frexp(x, exp);
}

/*****************************************************************************/
void*
xf86memcpy(void* dest, const void* src, xf86size_t n)
{
  return memcpy(dest, src, (size_t)n);
}

/*****************************************************************************/
int
xf86memcmp(const void* s1, const void* s2, xf86size_t n)
{
  return memcmp(s1, s2, (size_t)n);
}

/*****************************************************************************/
int
xf86ffs(int mask)
{
  int n;

  if (mask == 0)
  {
    return 0;
  }
  for (n = 1; (mask & 1) == 0; n++)
  {
    mask >>= 1;
  }
  return n;
}

/*****************************************************************************/
void
xf86abort(void)
{
  ErrorF("Module called abort() function\n");
  abort();
}

/*****************************************************************************/
double
xf86ldexp(double x, int exp)
{
  return ldexp(x, exp);
}

/*****************************************************************************/
char*
xf86getenv(const char* a)
{
  /* Only allow this when the real and effective uids are the same */
  if (getuid() != geteuid())
  {
    return NULL;
  }
  else
  {
    return getenv(a);
  }
}

/*****************************************************************************/
void*
xf86memset(void* s, int c, xf86size_t n)
{
  return memset(s, c, (size_t)n);
}

/*****************************************************************************/
void*
xf86malloc(xf86size_t n)
{
  return (void*)xalloc((size_t)n);
}

/*****************************************************************************/
void*
xf86calloc(xf86size_t sz, xf86size_t n)
{
  return (void*)xcalloc((size_t)sz, (size_t)n);
}

/*****************************************************************************/
double
xf86pow(double x, double y)
{
  return pow(x, y);
}

/*****************************************************************************/
int
xf86vsnprintf(char* s, xf86size_t len, const char* format, va_list ap)
{
  return vsnprintf(s, (size_t)len, format, ap);
}

/*****************************************************************************/
char*
xf86strstr(const char* s1, const char* s2)
{
  return strstr(s1, s2);
}

/*****************************************************************************/
char*
xf86strncat(char* dest, const char* src, xf86size_t n)
{
  return strncat(dest, src, (size_t)n);
}

/*****************************************************************************/
char*
xf86strcpy(char* dest, const char* src)
{
  return strcpy(dest, src);
}

/*****************************************************************************/
char*
xf86strncpy(char* dest, const char* src, xf86size_t n)
{
  return strncpy(dest, src, (size_t)n);
}

/*****************************************************************************/
int
xf86strncmp(const char* s1, const char* s2, xf86size_t n)
{
  return strncmp(s1, s2, (size_t)n);
}

/*****************************************************************************/
double
xf86strtod(const char* s, char** end)
{
  return strtod(s, end);
}

/*****************************************************************************/
int
xf86printf(const char* format, ...)
{
  int ret;
  va_list args;

  va_start(args, format);
  ret = printf(format, args);
  va_end(args);
  return ret;
}

/*****************************************************************************/
void*
xf86realloc(void* p, xf86size_t n)
{
  return (void*)xrealloc(p, n);
}

/*****************************************************************************/
int
xf86atoi(const char* s)
{
  return atoi(s);
}

/*****************************************************************************/
int
xf86vfprintf(XF86FILE* f, const char *format, va_list ap)
{
  XF86FILE_priv* fp = (XF86FILE_priv*)f;

#ifdef DEBUG
  ErrorF("xf86vfprintf for XF86FILE %p\n", fp);
#endif
  _xf86checkhndl(fp,"xf86vfprintf");
  return vfprintf(fp->filehnd, format, ap);
}

/*****************************************************************************/
void*
xf86bsearch(const void* key, const void* base, xf86size_t nmemb,
            xf86size_t size, int (*compar)(const void*, const void*))
{
  return bsearch(key, base, (size_t)nmemb, (size_t)size, compar);
}

/*****************************************************************************/
int
xf86sscanf(char* s, const char* format, ...)
{
  int ret;
  va_list args;

  va_start(args, format);
  ret = vsscanf(s,format,args);
  va_end(args);
  return ret;
}

/*****************************************************************************/
char*
xf86strtok(char* s1, const char* s2)
{
  return strtok(s1, s2);
}

/*****************************************************************************/
void
xf86qsort(void* base, xf86size_t nmemb, xf86size_t size,
          int (*comp)(const void *, const void *))
{
  qsort(base, nmemb, size, comp);
}

/*****************************************************************************/
char*
xf86strcat(char* dest, const char* src)
{
  return strcat(dest, src);
}

/*****************************************************************************/
xf86size_t
xf86strcspn(const char* s1, const char* s2)
{
  return (xf86size_t)strcspn(s1, s2);
}

/*****************************************************************************/
int
xf86abs(int x)
{
  return abs(x);
}

/*****************************************************************************/
double
xf86atan2(double x, double y)
{
  return atan2(x, y);
}

/*****************************************************************************/
void*
xf86memmove(void* dest, const void* src, xf86size_t n)
{
  return memmove(dest, src, (size_t)n);
}

/*****************************************************************************/
void
xf86bzero(void* s, unsigned int n)
{
  memset(s, 0, n);
}

/* other, what is this? */

/*****************************************************************************/
void
FontCacheExtensionInit(INITARGS)
{
}

#endif
