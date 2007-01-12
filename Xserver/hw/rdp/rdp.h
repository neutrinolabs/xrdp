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

*/

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "X11/X.h"
#define NEED_EVENTS
#include "X11/Xproto.h"
#include "X11/Xos.h"
#include "scrnintstr.h"
#include "servermd.h"
#define PSZ 8
#include "cfb.h"
#include "mibstore.h"
#include "colormapst.h"
#include "gcstruct.h"
#include "input.h"
#include "mipointer.h"
#include "dixstruct.h"
#include "propertyst.h"
#include <Xatom.h>
#include <errno.h>
#include <sys/param.h>
#include "dix.h"
#include <X11/keysym.h>
#include "dixfontstr.h"
#include "osdep.h"
#include "fontstruct.h"
#include <cursorstr.h>
#include "picturestr.h"
#include <netinet/tcp.h>

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

/* Per-screen (framebuffer) structure.  There is only one of these, since we
   don't allow the X server to have multiple screens. */
typedef struct
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
  /* Window Procedures */
  PaintWindowBackgroundProcPtr PaintWindowBackground;
  PaintWindowBorderProcPtr PaintWindowBorder;
  CopyWindowProcPtr CopyWindow;
  ClearToBackgroundProcPtr ClearToBackground;
  ScreenWakeupHandlerProcPtr WakeupHandler;
  CompositeProcPtr Composite;
  /* Backing store procedures */
  RestoreAreasProcPtr RestoreAreas;

  /*InstallColormapProcPtr InstallColormap;*/

} rdpScreenInfo;

typedef rdpScreenInfo* rdpScreenInfoPtr;

typedef struct
{
  GCFuncs* funcs;
  GCOps* ops;
} rdpGCRec;

typedef rdpGCRec* rdpGCPtr;

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
void
g_memcpy(void* d_ptr, const void* s_ptr, int size);
int
g_tcp_set_no_delay(int sck);
int
g_tcp_set_non_blocking(int sck);
int
g_tcp_accept(int sck);
int
g_tcp_select(int sck1, int sck2);
int
g_tcp_bind(int sck, char* port);
int
g_tcp_listen(int sck);

/* rdpdraw.c */
Bool
rdpCloseScreen(int i, ScreenPtr pScreen);
Bool
rdpCreateGC(GCPtr pGC);
void
rdpPaintWindowBackground(WindowPtr pWin, RegionPtr pRegion, int what);
void
rdpPaintWindowBorder(WindowPtr pWin, RegionPtr pRegion, int what);
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


/* rdpkbdptr.c */
int
rdpKeybdProc(DeviceIntPtr pDevice, int onoff);
int
rdpMouseProc(DeviceIntPtr pDevice, int onoff);
Bool
rdpCursorOffScreen(ScreenPtr* ppScreen, int* x, int* y);
void
rdpCrossScreen(ScreenPtr pScreen, Bool entering);
Bool
rdpSpriteRealizeCursor(ScreenPtr pScreen, CursorPtr pCursor);
Bool
rdpSpriteUnrealizeCursor(ScreenPtr pScreen, CursorPtr pCursor);
void
rdpSpriteSetCursor(ScreenPtr pScreen, CursorPtr pCursor, int x, int y);
void
rdpSpriteMoveCursor(ScreenPtr pScreen, int x, int y);
void
PtrAddEvent(int buttonMask, int x, int y);
void
KbdAddEvent(int down, int param1, int param2, int param3, int param4);

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

#if defined(__sparc__) || defined(__PPC__)
#define B_ENDIAN
#elif __BYTE_ORDER == __LITTLE_ENDIAN
#define L_ENDIAN
#elif __BYTE_ORDER == __BIG_ENDIAN
#define B_ENDIAN
#endif
/* check if we need to align data */
#if defined(__sparc__) || defined(__alpha__) || defined(__hppa__) || \
    defined(__AIX__) || defined(__PPC__) || defined(__mips__) || \
    defined(__ia64__)
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
