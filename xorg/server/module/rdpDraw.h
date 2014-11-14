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

misc draw calls

*/

#ifndef __RDPDRAW_H
#define __RDPDRAW_H

#include <xorg-server.h>
#include <xorgVersion.h>
#include <xf86.h>

#if XORG_VERSION_CURRENT < XORG_VERSION_NUMERIC(1, 13, 0, 0, 0)
/* 1.1, 1.2, 1.3, 1.4 1.5, 1.6, 1.7, 1.8, 1.9, 1.10, 1.11, 1.12 */
#define XRDP_CLOSESCR 1
#else
/* 1.13 */
#define XRDP_CLOSESCR 2
#endif

/* true if drawable is window or pixmap is screen */
#define XRDP_DRAWABLE_IS_VISIBLE(_dev, _drw) \
(((_drw)->type == DRAWABLE_WINDOW && ((WindowPtr)(_drw))->viewable) || \
 ((_drw)->type == DRAWABLE_PIXMAP && \
                   ((PixmapPtr)(_drw))->devPrivate.ptr == (_dev)->pfbMemory))

/******************************************************************************/
#define GC_OP_VARS rdpPtr dev; rdpGCPtr priv; GCFuncs *oldFuncs

/******************************************************************************/
#define GC_OP_PROLOGUE(_pGC) \
do { \
    dev = rdpGetDevFromScreen((_pGC)->pScreen); \
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

extern _X_EXPORT int
rdpDrawGetClip(rdpPtr dev, RegionPtr pRegion, DrawablePtr pDrawable, GCPtr pGC);
extern _X_EXPORT void
GetTextBoundingBox(DrawablePtr pDrawable, FontPtr font, int x, int y,
                   int n, BoxPtr pbox);
extern _X_EXPORT int
rdpDrawItemAdd(rdpPtr dev, rdpPixmapRec *priv, struct rdp_draw_item *di);
extern _X_EXPORT int
rdpDrawItemRemove(rdpPtr dev, rdpPixmapRec *priv, struct rdp_draw_item *di);
extern _X_EXPORT int
rdpDrawItemRemoveAll(rdpPtr dev, rdpPixmapRec *priv);
extern _X_EXPORT void
rdpCopyWindow(WindowPtr pWin, DDXPointRec ptOldOrg, RegionPtr pOldRegion);
#if XRDP_CLOSESCR == 1
extern _X_EXPORT Bool
rdpCloseScreen(int index, ScreenPtr pScreen);
#else
extern _X_EXPORT Bool
rdpCloseScreen(ScreenPtr pScreen);
#endif
extern _X_EXPORT WindowPtr
rdpGetRootWindowPtr(ScreenPtr pScreen);
extern _X_EXPORT rdpPtr
rdpGetDevFromScreen(ScreenPtr pScreen);

#endif
