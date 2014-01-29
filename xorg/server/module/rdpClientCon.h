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

    int rdpIndex; /* current os target */

    int conNumber;

    /* rdpGlyphs.c */
    struct font_cache font_cache[12][256];
    int font_stamp;

    RegionPtr dirtyRegion;

    struct xrdp_client_info client_info;

    char *shmemptr;
    int shmemid;
    int shmem_lineBytes;
    RegionPtr shmRegion;
    int rect_id;
    int rect_id_ack;

    OsTimerPtr updateTimer;
    int updateSchedualed; /* boolean */

    struct _rdpClientCon *next;
};

int
rdpClientConBeginUpdate(rdpPtr dev, rdpClientCon *clientCon);
int
rdpClientConEndUpdate(rdpPtr dev, rdpClientCon *clientCon);
int
rdpClientConSetFgcolor(rdpPtr dev, rdpClientCon *clientCon, int fgcolor);
void
rdpClientConSendArea(rdpPtr dev, rdpClientCon *clientCon,
                     struct image_data *id, int x, int y, int w, int h);
int
rdpClientConFillRect(rdpPtr dev, rdpClientCon *clientCon,
                     short x, short y, int cx, int cy);
int
rdpClientConCheck(ScreenPtr pScreen);
int
rdpClientConInit(rdpPtr dev);
int
rdpClientConDeinit(rdpPtr dev);

int
rdpClientConDeleteOsSurface(rdpPtr dev, rdpClientCon *clientCon, int rdpindex);

int
rdpClientConRemoveOsBitmap(rdpPtr dev, rdpClientCon *clientCon, int rdpindex);

void
rdpClientConScheduleDeferredUpdate(rdpPtr dev);
int
rdpClientConCheckDirtyScreen(rdpPtr dev, rdpClientCon *clientCon);
int
rdpClientConAddDirtyScreenReg(rdpPtr dev, rdpClientCon *clientCon,
                              RegionPtr reg);
int
rdpClientConAddDirtyScreenBox(rdpPtr dev, rdpClientCon *clientCon,
                              BoxPtr box);
int
rdpClientConAddDirtyScreen(rdpPtr dev, rdpClientCon *clientCon,
                           int x, int y, int cx, int cy);
int
rdpClientConAddAllReg(rdpPtr dev, RegionPtr reg, DrawablePtr pDrawable);
int
rdpClientConAddAllBox(rdpPtr dev, BoxPtr box, DrawablePtr pDrawable);

#endif
