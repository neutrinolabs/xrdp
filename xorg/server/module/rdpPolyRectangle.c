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

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* this should be before all X11 .h files */
#include <xorg-server.h>

/* all driver need this */
#include <xf86.h>
#include <xf86_OSproc.h>

#include "rdp.h"
#include "rdpDraw.h"
#include "rdpClientCon.h"
#include "rdpReg.h"

#define LOG_LEVEL 1
#define LLOGLN(_level, _args) \
    do { if (_level < LOG_LEVEL) { ErrorF _args ; ErrorF("\n"); } } while (0)

/******************************************************************************/
static void
rdpPolyRectangleOrg(DrawablePtr pDrawable, GCPtr pGC, int nrects,
                    xRectangle *rects)
{
    GC_OP_VARS;

    GC_OP_PROLOGUE(pGC);
    pGC->ops->PolyRectangle(pDrawable, pGC, nrects, rects);
    GC_OP_EPILOGUE(pGC);
}

/******************************************************************************/
void
rdpPolyRectangle(DrawablePtr pDrawable, GCPtr pGC, int nrects,
                 xRectangle *rects)
{
    rdpPtr dev;
    BoxRec box;
    int index;
    int up;
    int down;
    int lw;
    int cd;
    int x1;
    int y1;
    int x2;
    int y2;
    RegionRec clip_reg;
    RegionRec reg;

    LLOGLN(10, ("rdpPolyRectangle:"));
    dev = rdpGetDevFromScreen(pGC->pScreen);
    dev->counts.rdpPolyRectangleCallCount++;
    rdpRegionInit(&reg, NullBox, 0);
    lw = pGC->lineWidth;
    if (lw < 1)
    {
        lw = 1;
    }
    up = lw / 2;
    down = 1 + (lw - 1) / 2;
    index = 0;
    while (index < nrects)
    {
        x1 = rects[index].x + pDrawable->x;
        y1 = rects[index].y + pDrawable->y;
        x2 = x1 + rects[index].width;
        y2 = y1 + rects[index].height;
        /* top */
        box.x1 = x1 - up;
        box.y1 = y1 - up;
        box.x2 = x2 + down;
        box.y2 = y1 + down;
        rdpRegionUnionRect(&reg, &box);
        /* left */
        box.x1 = x1 - up;
        box.y1 = y1 - up;
        box.x2 = x1 + down;
        box.y2 = y2 + down;
        rdpRegionUnionRect(&reg, &box);
        /* right */
        box.x1 = x2 - up;
        box.y1 = y1 - up;
        box.x2 = x2 + down;
        box.y2 = y2 + down;
        rdpRegionUnionRect(&reg, &box);
        /* bottom */
        box.x1 = x1 - up;
        box.y1 = y2 - up;
        box.x2 = x2 + down;
        box.y2 = y2 + down;
        rdpRegionUnionRect(&reg, &box);
        index++;
    }
    rdpRegionInit(&clip_reg, NullBox, 0);
    cd = rdpDrawGetClip(dev, &clip_reg, pDrawable, pGC);
    LLOGLN(10, ("rdpPolyRectangle: cd %d", cd));
    if (cd == XRDP_CD_CLIP)
    {
        rdpRegionIntersect(&reg, &clip_reg, &reg);
    }
    /* do original call */
    rdpPolyRectangleOrg(pDrawable, pGC, nrects, rects);
    if (cd != XRDP_CD_NODRAW)
    {
        rdpClientConAddAllReg(dev, &reg, pDrawable);
    }
    rdpRegionUninit(&clip_reg);
    rdpRegionUninit(&reg);
}
