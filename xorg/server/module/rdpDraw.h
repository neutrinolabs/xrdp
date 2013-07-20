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

misc draw calls

*/

#ifndef __RDPDRAW_H
#define __RDPDRAW_H

#include <xorg-server.h>
#include <xf86.h>

/******************************************************************************/
#define GC_OP_PROLOGUE(_pGC) \
do { \
    rdpPtr dev; \
    ScreenPtr pScreen; \
    ScrnInfoPtr pScrn; \
    pScreen = (_pGC)->pScreen; \
    pScrn = xf86Screens[pScreen->myNum]; \
    dev = XRDPPTR(pScrn); \
    priv = (rdpGCPtr)rdpGetGCPrivate(_pGC, dev->privateKeyRecGC); \
    oldFuncs = (_pGC)->funcs; \
    (_pGC)->funcs = priv->funcs; \
    (_pGC)->ops = priv->ops; \
} while (0)

/******************************************************************************/
#define GC_OP_EPILOGUE(_pGC) \
do { \
    priv->ops = (_pGC)->ops; \
    (_pGC)->funcs = oldFuncs; \
    (_pGC)->ops = &g_rdpGCOps; \
} while (0)

extern GCOps g_rdpGCOps; /* in rdpGC.c */

PixmapPtr
rdpCreatePixmap(ScreenPtr pScreen, int width, int height, int depth,
                unsigned usage_hint);
Bool
rdpDestroyPixmap(PixmapPtr pPixmap);
Bool
rdpModifyPixmapHeader(PixmapPtr pPixmap, int width, int height, int depth,
                      int bitsPerPixel, int devKind, pointer pPixData);
void
rdpCopyWindow(WindowPtr pWin, DDXPointRec ptOldOrg, RegionPtr pOldRegion);
Bool
rdpCloseScreen(int index, ScreenPtr pScreen);
WindowPtr
rdpGetRootWindowPtr(ScreenPtr pScreen);

#endif
