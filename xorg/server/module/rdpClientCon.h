/*
Copyright 2005-2014 Jay Sorg

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

#include <xorg-server.h>
#include <xorgVersion.h>
#include <xf86.h>

/* in xrdp/common */
#include "xrdp_client_info.h"
#include "xrdp_constants.h"

#ifndef _RDPCLIENTCON_H
#define _RDPCLIENTCON_H

/* used in rdpGlyphs.c */
struct font_cache
{
    int offset;
    int baseline;
    int width;
    int height;
    int crc;
    int stamp;
};

struct rdpup_os_bitmap
{
    int used;
    PixmapPtr pixmap;
    rdpPixmapPtr priv;
    int stamp;
};

/* one of these for each client */
struct _rdpClientCon
{
    rdpPtr dev;

    int sck;
    int sckControlListener;
    int sckControl;
    struct stream *out_s;
    struct stream *in_s;

    int rectIdAck;
    int rectId;
    int connected; /* boolean */
    int begin; /* boolean */
    int count;
    int sckClosed; /* boolean */
    struct rdpup_os_bitmap *osBitmaps;
    int maxOsBitmaps;
    int osBitmapStamp;
    int osBitmapAllocSize;
    int osBitmapNumUsed;
    int doComposite;
    int doGlyphCache;
    int canDoPixToPix;
    int doMultimon;

    int rdp_bpp; /* client depth */
    int rdp_Bpp;
    int rdp_Bpp_mask;
    int rdp_width;
    int rdp_height;
    int rdp_format; /* XRDP_a8r8g8b8, XRDP_r5g6b5, ... */
    int cap_width;
    int cap_height;

    int rdpIndex; /* current os target */

    int conNumber;

    /* rdpGlyphs.c */
    struct font_cache font_cache[12][256];
    int font_stamp;

    struct xrdp_client_info client_info;

    char *shmemptr;
    int shmemid;
    int shmem_lineBytes;
    RegionPtr shmRegion;
    int rect_id;
    int rect_id_ack;

    OsTimerPtr updateTimer;
    int updateSchedualed; /* boolean */

    RegionPtr dirtyRegion;

    struct _rdpClientCon *next;
};

extern _X_EXPORT int
rdpClientConBeginUpdate(rdpPtr dev, rdpClientCon *clientCon);
extern _X_EXPORT int
rdpClientConEndUpdate(rdpPtr dev, rdpClientCon *clientCon);
extern _X_EXPORT int
rdpClientConSetFgcolor(rdpPtr dev, rdpClientCon *clientCon, int fgcolor);
extern _X_EXPORT void
rdpClientConSendArea(rdpPtr dev, rdpClientCon *clientCon,
                     struct image_data *id, int x, int y, int w, int h);
extern _X_EXPORT int
rdpClientConFillRect(rdpPtr dev, rdpClientCon *clientCon,
                     short x, short y, int cx, int cy);
extern _X_EXPORT int
rdpClientConCheck(ScreenPtr pScreen);
extern _X_EXPORT int
rdpClientConInit(rdpPtr dev);
extern _X_EXPORT int
rdpClientConDeinit(rdpPtr dev);

extern _X_EXPORT int
rdpClientConDeleteOsSurface(rdpPtr dev, rdpClientCon *clientCon, int rdpindex);

extern _X_EXPORT int
rdpClientConRemoveOsBitmap(rdpPtr dev, rdpClientCon *clientCon, int rdpindex);

extern _X_EXPORT void
rdpClientConScheduleDeferredUpdate(rdpPtr dev);
extern _X_EXPORT int
rdpClientConCheckDirtyScreen(rdpPtr dev, rdpClientCon *clientCon);
extern _X_EXPORT int
rdpClientConAddDirtyScreenReg(rdpPtr dev, rdpClientCon *clientCon,
                              RegionPtr reg);
extern _X_EXPORT int
rdpClientConAddDirtyScreenBox(rdpPtr dev, rdpClientCon *clientCon,
                              BoxPtr box);
extern _X_EXPORT int
rdpClientConAddDirtyScreen(rdpPtr dev, rdpClientCon *clientCon,
                           int x, int y, int cx, int cy);
extern _X_EXPORT void
rdpClientConGetScreenImageRect(rdpPtr dev, rdpClientCon *clientCon,
                               struct image_data *id);
extern _X_EXPORT int
rdpClientConAddAllReg(rdpPtr dev, RegionPtr reg, DrawablePtr pDrawable);
extern _X_EXPORT int
rdpClientConAddAllBox(rdpPtr dev, BoxPtr box, DrawablePtr pDrawable);
extern _X_EXPORT int
rdpClientConSetCursor(rdpPtr dev, rdpClientCon *clientCon,
                      short x, short y, char *cur_data, char *cur_mask);
extern _X_EXPORT int
rdpClientConSetCursorEx(rdpPtr dev, rdpClientCon *clientCon,
                        short x, short y, char *cur_data,
                        char *cur_mask, int bpp);

#endif
