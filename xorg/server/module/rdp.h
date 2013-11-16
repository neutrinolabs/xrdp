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

*/

#ifndef _RDP_H
#define _RDP_H

#include <xorg-server.h>
#include <scrnintstr.h>
#include <gcstruct.h>
#include <mipointer.h>
#include <randrstr.h>

#include "rdpPri.h"

#define PixelDPI 100
#define PixelToMM(_size) (((_size) * 254 + (PixelDPI) * 5) / ((PixelDPI) * 10))

#define RDPMIN(_val1, _val2) ((_val1) < (_val2) ? (_val1) : (_val2))
#define RDPMAX(_val1, _val2) ((_val1) < (_val2) ? (_val2) : (_val1))
#define RDPCLAMP(_val, _lo, _hi) \
  (_val) < (_lo) ? (_lo) : (_val) > (_hi) ? (_hi) : (_val)

/* defined in rdpClientCon.h */
typedef struct _rdpClientCon rdpClientCon;

struct _rdpPointer
{
    int cursor_x;
    int cursor_y;
    int old_button_mask;
    int button_mask;
    DeviceIntPtr device;
};
typedef struct _rdpPointer rdpPointer;

struct _rdpKeyboard
{
    int pause_spe;
    int ctrl_down;
    int alt_down;
    int shift_down;
    int tab_down;
    /* this is toggled every time num lock key is released, not like the
       above *_down vars */
    int scroll_lock_down;
    DeviceIntPtr device;
};
typedef struct _rdpKeyboard rdpKeyboard;


struct _rdpPixmapRec
{
    int status;
    int rdpindex;
    int con_number;
    int is_dirty;
    int is_scratch;
    int is_alpha_dirty_not;
    /* number of times used in a remote operation
       if this gets above XRDP_USE_COUNT_THRESHOLD
       then we force remote the pixmap */
    int use_count;
    int kind_width;
    struct rdp_draw_item *draw_item_head;
    struct rdp_draw_item *draw_item_tail;
};
typedef struct _rdpPixmapRec rdpPixmapRec;
typedef struct _rdpPixmapRec * rdpPixmapPtr;

/* move this to common header */
struct _rdpRec
{
    int width;
    int height;
    int depth;
    int paddedWidthInBytes;
    int sizeInBytes;
    int num_modes;
    int bitsPerPixel;
    int Bpp;
    int Bpp_mask;
    char *pfbMemory;
    ScreenPtr pScreen;
    rdpDevPrivateKey privateKeyRecGC;
    rdpDevPrivateKey privateKeyRecPixmap;

    CopyWindowProcPtr CopyWindow;
    CreateGCProcPtr CreateGC;
    CreatePixmapProcPtr CreatePixmap;
    DestroyPixmapProcPtr DestroyPixmap;
    ModifyPixmapHeaderProcPtr ModifyPixmapHeader;
    CloseScreenProcPtr CloseScreen;
    CompositeProcPtr Composite;
    GlyphsProcPtr Glyphs;

    /* keyboard and mouse */
    miPointerScreenFuncPtr pCursorFuncs;
    /* mouse */
    rdpPointer pointer;
    /* keyboard */
    rdpKeyboard keyboard;

    /* RandR */
    RRSetConfigProcPtr rrSetConfig;
    RRGetInfoProcPtr rrGetInfo;
    RRScreenSetSizeProcPtr rrScreenSetSize;
    RRCrtcSetProcPtr rrCrtcSet;
    RRCrtcSetGammaProcPtr rrCrtcSetGamma;
    RRCrtcGetGammaProcPtr rrCrtcGetGamma;
    RROutputSetPropertyProcPtr rrOutputSetProperty;
    RROutputValidateModeProcPtr rrOutputValidateMode;
    RRModeDestroyProcPtr rrModeDestroy;
    RROutputGetPropertyProcPtr rrOutputGetProperty;
    RRGetPanningProcPtr rrGetPanning;
    RRSetPanningProcPtr rrSetPanning;

    int listen_sck;
    char uds_data[256];
    rdpClientCon *clientConHead;
    rdpClientCon *clientConTail;

    rdpPixmapRec screenPriv;
    int sendUpdateScheduled; /* boolean */
    OsTimerPtr sendUpdateTimer;

    int do_dirty_ons; /* boolean */
    int disconnect_scheduled; /* boolean */
    int do_kill_disconnected; /* boolean */

    OsTimerPtr disconnectTimer;
    int disconnectScheduled; /* boolean */
    int disconnect_timeout_s;
    int disconnect_time_ms;

};
typedef struct _rdpRec rdpRec;
typedef struct _rdpRec * rdpPtr;
#define XRDPPTR(_p) ((rdpPtr)((_p)->driverPrivate))

struct _rdpGCRec
{
    GCFuncs *funcs;
    GCOps *ops;
};
typedef struct _rdpGCRec rdpGCRec;
typedef struct _rdpGCRec * rdpGCPtr;

#define RDI_FILL 1
#define RDI_IMGLL 2 /* lossless */
#define RDI_IMGLY 3 /* lossy */
#define RDI_LINE 4
#define RDI_SCRBLT 5
#define RDI_TEXT 6

struct urdp_draw_item_fill
{
  int opcode;
  int fg_color;
  int bg_color;
  int pad0;
};

struct urdp_draw_item_img
{
  int opcode;
  int pad0;
};

struct urdp_draw_item_line
{
  int opcode;
  int fg_color;
  int bg_color;
  int width;
  xSegment* segs;
  int nseg;
  int flags;
};

struct urdp_draw_item_scrblt
{
  int srcx;
  int srcy;
  int dstx;
  int dsty;
  int cx;
  int cy;
};

struct urdp_draw_item_text
{
  int opcode;
  int fg_color;
  struct rdp_text* rtext; /* in rdpglyph.h */
};

union urdp_draw_item
{
  struct urdp_draw_item_fill fill;
  struct urdp_draw_item_img img;
  struct urdp_draw_item_line line;
  struct urdp_draw_item_scrblt scrblt;
  struct urdp_draw_item_text text;
};

struct rdp_draw_item
{
  int type; /* RDI_FILL, RDI_IMGLL, ... */
  int flags;
  struct rdp_draw_item* prev;
  struct rdp_draw_item* next;
  RegionPtr reg;
  union urdp_draw_item u;
};

#define XRDP_USE_COUNT_THRESHOLD 1
#endif
