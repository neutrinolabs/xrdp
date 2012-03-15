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

*/

#if defined(__arm__) && !defined(__arm32__)
#define __arm32__
#endif

#include "xorg-server.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <netdb.h>
#include "X.h"
#define NEED_EVENTS
#include "Xproto.h"
#include "Xos.h"
#include "scrnintstr.h"
#include "servermd.h"
#define PSZ 8

//#include "cfb.h"

#include "mibstore.h"
#include "colormapst.h"
#include "gcstruct.h"
#include "input.h"
#include "mipointer.h"
#include "dixstruct.h"
#include "propertyst.h"
#include "Xatom.h"
#include "dix.h"
#include "X11/keysym.h"
#include "dixfontstr.h"
#include "fontstruct.h"
#include "cursorstr.h"
#include "picturestr.h"
#include "XKBstr.h"
#include "inputstr.h"
#include "randrstr.h"
#include "mi.h"
#include "fb.h"
#include "micmap.h"
#include "events.h"
#include "exevents.h"
#include "xserver-properties.h"
#include "xkbsrv.h"

//#include "colormapst.h"

/* test to see if this is xorg source or xfree86 */
#ifdef XORGSERVER
#  define RDP_IS_XORG
#else
#  include <xf86Version.h>
#  if (XF86_VERSION_MAJOR == 4 && XF86_VERSION_MINOR > 3)
#    define RDP_IS_XFREE86
#  elif (XF86_VERSION_MAJOR > 4)
#    define RDP_IS_XFREE86
#  else
#    define RDP_IS_XORG
#  endif
#endif

#define X11RDPVER "0.7.0"

#define PixelDPI 100
#define PixelToMM(_size) (((_size) * 254 + (PixelDPI) * 5) / ((PixelDPI) * 10))

/* Per-screen (framebuffer) structure.  There is only one of these, since we
   don't allow the X server to have multiple screens. */
struct _rdpScreenInfoRec
{
  int width;
  int paddedWidthInBytes;
  int height;
  int depth;
  int bitsPerPixel;
  int sizeInBytes;
  char* pfbMemory;
  Pixel blackPixel;
  Pixel whitePixel;
  /* wrapped screen functions */
  /* Random screen procedures */
  CloseScreenProcPtr CloseScreen;
  /* GC procedures */
  CreateGCProcPtr CreateGC;
  /* Pixmap procedures */
  CreatePixmapProcPtr CreatePixmap;
  DestroyPixmapProcPtr DestroyPixmap;

  /* Window Procedures */
  CreateWindowProcPtr CreateWindow;
  DestroyWindowProcPtr DestroyWindow;

  CreateColormapProcPtr CreateColormap;
  DestroyColormapProcPtr DestroyColormap;

  CopyWindowProcPtr CopyWindow;
  ClearToBackgroundProcPtr ClearToBackground;
  ScreenWakeupHandlerProcPtr WakeupHandler;
  CompositeProcPtr Composite;
  /* Backing store procedures */
  RestoreAreasProcPtr RestoreAreas;

  int rdp_width;
  int rdp_height;
  int rdp_bpp;
  int rdp_Bpp;
  int rdp_Bpp_mask;
};
typedef struct _rdpScreenInfoRec rdpScreenInfoRec;
typedef rdpScreenInfoRec* rdpScreenInfoPtr;

struct _rdpGCRec
{
  GCFuncs* funcs;
  GCOps* ops;
};
typedef struct _rdpGCRec rdpGCRec;
typedef rdpGCRec* rdpGCPtr;
#define GETGCPRIV(_pGC) \
(rdpGCPtr)dixGetPrivateAddr(&(_pGC->devPrivates), &g_rdpGCIndex)

struct _rdpWindowRec
{
  int status;
};
typedef struct _rdpWindowRec rdpWindowRec;
typedef rdpWindowRec* rdpWindowPtr;
#define GETWINPRIV(_pWindow) \
(rdpWindowPtr)dixGetPrivateAddr(&(_pWindow->devPrivates), &g_rdpWindowIndex)

struct _rdpPixmapRec
{
  int status;
};
typedef struct _rdpPixmapRec rdpPixmapRec;
typedef rdpPixmapRec* rdpPixmapPtr;
#define GETPIXPRIV(_pPixmap) \
(rdpPixmapPtr)dixGetPrivateAddr(&(_pPixmap->devPrivates), &g_rdpPixmapIndex)

/* rdpmisc.c */
void
rdpLog(char *format, ...);
int
rdpBitsPerPixel(int depth);
void
rdpClientStateChange(CallbackListPtr* cbl, pointer myData, pointer clt);
int
g_tcp_recv(int sck, void* ptr, int len, int flags);
void
g_tcp_close(int sck);
int
g_tcp_last_error_would_block(int sck);
void
g_sleep(int msecs);
int
g_tcp_send(int sck, void* ptr, int len, int flags);
void*
g_malloc(int size, int zero);
void
g_free(void* ptr);
void
g_sprintf(char* dest, char* format, ...);
int
g_tcp_socket(void);
int
g_tcp_local_socket_dgram(void);
int
g_tcp_local_socket_stream(void);
void
g_memcpy(void* d_ptr, const void* s_ptr, int size);
int
g_tcp_set_no_delay(int sck);
int
g_tcp_set_non_blocking(int sck);
int
g_tcp_accept(int sck);
int
g_tcp_select(int sck1, int sck2, int sck3);
int
g_tcp_bind(int sck, char* port);
int
g_tcp_local_bind(int sck, char* port);
int
g_tcp_listen(int sck);
int
g_create_dir(const char* dirname);
int
g_directory_exist(const char* dirname);
int
g_chmod_hex(const char* filename, int flags);
void
hexdump(unsigned char *p, unsigned int len);

/* rdpdraw.c */
Bool
rdpCloseScreen(int i, ScreenPtr pScreen);

PixmapPtr
rdpCreatePixmap(ScreenPtr pScreen, int width, int height, int depth,
                unsigned usage_hint);
Bool
rdpDestroyPixmap(PixmapPtr pPixmap);

Bool
rdpCreateWindow(WindowPtr pWindow);
Bool
rdpDestroyWindow(WindowPtr pWindow);

Bool
rdpCreateGC(GCPtr pGC);
void
rdpCopyWindow(WindowPtr pWin, DDXPointRec ptOldOrg, RegionPtr pOldRegion);
void
rdpClearToBackground(WindowPtr pWin, int x, int y, int w, int h,
                     Bool generateExposures);
RegionPtr
rdpRestoreAreas(WindowPtr pWin, RegionPtr prgnExposed);
void
rdpInstallColormap(ColormapPtr pmap);
void
rdpUninstallColormap(ColormapPtr pmap);
int
rdpListInstalledColormaps(ScreenPtr pScreen, Colormap* pmaps);
void
rdpStoreColors(ColormapPtr pmap, int ndef, xColorItem* pdefs);
Bool
rdpSaveScreen(ScreenPtr pScreen, int on);
Bool
rdpRealizeCursor(ScreenPtr pScreen, CursorPtr pCursor);
Bool
rdpUnrealizeCursor(ScreenPtr pScreen, CursorPtr pCursor);
void
rdpCursorLimits(ScreenPtr pScreen, CursorPtr pCursor,
                BoxPtr pHotBox, BoxPtr pTopLeftBox);
void
rdpConstrainCursor(ScreenPtr pScreen, BoxPtr pBox);
Bool
rdpSetCursorPosition(ScreenPtr pScreen, int x, int y, Bool generateEvent);
Bool
rdpDisplayCursor(ScreenPtr pScreen, CursorPtr pCursor);
void
rdpRecolorCursor(ScreenPtr pScreen, CursorPtr pCursor,
                 Bool displayed);
void
rdpComposite(CARD8 op, PicturePtr pSrc, PicturePtr pMask, PicturePtr pDst,
             INT16 xSrc, INT16 ySrc, INT16 xMask, INT16 yMask, INT16 xDst,
             INT16 yDst, CARD16 width, CARD16 height);


/* rdpinput.c */
int
rdpKeybdProc(DeviceIntPtr pDevice, int onoff);
int
rdpMouseProc(DeviceIntPtr pDevice, int onoff);
Bool
rdpCursorOffScreen(ScreenPtr* ppScreen, int* x, int* y);
void
rdpCrossScreen(ScreenPtr pScreen, Bool entering);
void
rdpPointerWarpCursor(DeviceIntPtr pDev, ScreenPtr pScr, int x, int y);
void
rdpPointerEnqueueEvent(DeviceIntPtr pDev, InternalEvent* event);
void
rdpPointerNewEventScreen(DeviceIntPtr pDev, ScreenPtr pScr, Bool fromDIX);
Bool
rdpSpriteRealizeCursor(DeviceIntPtr pDev, ScreenPtr pScr, CursorPtr pCurs);
Bool
rdpSpriteUnrealizeCursor(DeviceIntPtr pDev, ScreenPtr pScr, CursorPtr pCurs);
void
rdpSpriteSetCursor(DeviceIntPtr pDev, ScreenPtr pScr, CursorPtr pCurs,
                   int x, int y);
void
rdpSpriteMoveCursor(DeviceIntPtr pDev, ScreenPtr pScr, int x, int y);
Bool
rdpSpriteDeviceCursorInitialize(DeviceIntPtr pDev, ScreenPtr pScr);
void
rdpSpriteDeviceCursorCleanup(DeviceIntPtr pDev, ScreenPtr pScr);
void
PtrAddEvent(int buttonMask, int x, int y);
void
KbdAddEvent(int down, int param1, int param2, int param3, int param4);
void
KbdSync(int param1);

/* rdpup.c */
int
rdpup_init(void);
int
rdpup_check(void);
int
rdpup_begin_update(void);
int
rdpup_end_update(void);
int
rdpup_fill_rect(short x, short y, int cx, int cy);
int
rdpup_screen_blt(short x, short y, int cx, int cy, short srcx, short srcy);
int
rdpup_set_clip(short x, short y, int cx, int cy);
int
rdpup_reset_clip(void);
int
rdpup_set_fgcolor(int fgcolor);
int
rdpup_set_bgcolor(int bgcolor);
int
rdpup_set_opcode(int opcode);
int
rdpup_paint_rect(short x, short y, int cx, int cy,
                 char* bmpdata, int width, int height,
                 short srcx, short srcy);
int
rdpup_set_pen(int style, int width);
int
rdpup_draw_line(short x1, short y1, short x2, short y2);
void
rdpup_send_area(int x, int y, int w, int h);
int
rdpup_set_cursor(short x, short y, char* cur_data, char* cur_mask);

#if defined(X_BYTE_ORDER)
#  if X_BYTE_ORDER == X_LITTLE_ENDIAN
#    define L_ENDIAN
#  else
#    define B_ENDIAN
#  endif
#else
#  error Unknown endianness in rdp.h
#endif
/* check if we need to align data */
/* check if we need to align data */
#if defined(__sparc__) || defined(__alpha__) || defined(__hppa__) || \
    defined(__AIX__) || defined(__PPC__) || defined(__mips__) || \
    defined(__ia64__) || defined(__ppc__) || defined(__arm__)
#define NEED_ALIGN
#endif

/* parser state */
struct stream
{
  char* p;
  char* end;
  char* data;
  int size;
  /* offsets of various headers */
  char* iso_hdr;
  char* mcs_hdr;
  char* sec_hdr;
  char* rdp_hdr;
  char* channel_hdr;
  char* next_packet;
};

/******************************************************************************/
#define s_push_layer(s, h, n) \
{ \
  (s)->h = (s)->p; \
  (s)->p += (n); \
}

/******************************************************************************/
#define s_pop_layer(s, h) \
{ \
  (s)->p = (s)->h; \
}

/******************************************************************************/
#if defined(B_ENDIAN) || defined(NEED_ALIGN)
#define out_uint16_le(s, v) \
{ \
  *((s)->p) = (unsigned char)((v) >> 0); \
  (s)->p++; \
  *((s)->p) = (unsigned char)((v) >> 8); \
  (s)->p++; \
}
#else
#define out_uint16_le(s, v) \
{ \
  *((unsigned short*)((s)->p)) = (unsigned short)(v); \
  (s)->p += 2; \
}
#endif

/******************************************************************************/
#define init_stream(s, v) \
{ \
  if ((v) > (s)->size) \
  { \
    g_free((s)->data); \
    (s)->data = (char*)g_malloc((v), 0); \
    (s)->size = (v); \
  } \
  (s)->p = (s)->data; \
  (s)->end = (s)->data; \
  (s)->next_packet = 0; \
}

/******************************************************************************/
#define out_uint8p(s, v, n) \
{ \
  g_memcpy((s)->p, (v), (n)); \
  (s)->p += (n); \
}

/******************************************************************************/
#define out_uint8a(s, v, n) \
{ \
  out_uint8p((s), (v), (n)); \
}

/******************************************************************************/
#if defined(B_ENDIAN) || defined(NEED_ALIGN)
#define out_uint32_le(s, v) \
{ \
  *((s)->p) = (unsigned char)((v) >> 0); \
  (s)->p++; \
  *((s)->p) = (unsigned char)((v) >> 8); \
  (s)->p++; \
  *((s)->p) = (unsigned char)((v) >> 16); \
  (s)->p++; \
  *((s)->p) = (unsigned char)((v) >> 24); \
  (s)->p++; \
}
#else
#define out_uint32_le(s, v) \
{ \
  *((unsigned int*)((s)->p)) = (v); \
  (s)->p += 4; \
}
#endif

/******************************************************************************/
#if defined(B_ENDIAN) || defined(NEED_ALIGN)
#define in_uint32_le(s, v) \
{ \
  (v) = (unsigned int) \
    ( \
      (*((unsigned char*)((s)->p + 0)) << 0) | \
      (*((unsigned char*)((s)->p + 1)) << 8) | \
      (*((unsigned char*)((s)->p + 2)) << 16) | \
      (*((unsigned char*)((s)->p + 3)) << 24) \
    ); \
  (s)->p += 4; \
}
#else
#define in_uint32_le(s, v) \
{ \
  (v) = *((unsigned int*)((s)->p)); \
  (s)->p += 4; \
}
#endif

/******************************************************************************/
#if defined(B_ENDIAN) || defined(NEED_ALIGN)
#define in_uint16_le(s, v) \
{ \
  (v) = (unsigned short) \
    ( \
      (*((unsigned char*)((s)->p + 0)) << 0) | \
      (*((unsigned char*)((s)->p + 1)) << 8) \
    ); \
  (s)->p += 2; \
}
#else
#define in_uint16_le(s, v) \
{ \
  (v) = *((unsigned short*)((s)->p)); \
  (s)->p += 2; \
}
#endif

/******************************************************************************/
#define s_mark_end(s) \
{ \
  (s)->end = (s)->p; \
}

/******************************************************************************/
#define make_stream(s) \
{ \
  (s) = (struct stream*)g_malloc(sizeof(struct stream), 1); \
}
