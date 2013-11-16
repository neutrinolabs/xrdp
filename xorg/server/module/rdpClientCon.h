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

Client connection to xrdp

*/

#ifndef _RDPCLIENTCON_H
#define _RDPCLIENTCON_H

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

    int rdp_bpp; /* client depth */
    int rdp_Bpp_mask;

    struct _rdpClientCon *next;
};

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


#endif
