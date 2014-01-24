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
void
rdpPolyRectanglePre(rdpPtr dev, rdpClientCon *clientCon,
                    int cd, RegionPtr clip_reg,
                    DrawablePtr pDrawable, GCPtr pGC, int nrects,
                    xRectangle *rects, RegionPtr reg)
{
}

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
rdpPolyRectanglePost(rdpPtr dev, rdpClientCon *clientCon,
                     int cd, RegionPtr clip_reg,
                     DrawablePtr pDrawable, GCPtr pGC, int nrects,
                     xRectangle *rects, RegionPtr reg)
{
    if (cd == XRDP_CD_NODRAW)
    {
        return;
    }
    if (!XRDP_DRAWABLE_IS_VISIBLE(dev, pDrawable))
    {
        return;
    }
    if (cd == XRDP_CD_CLIP)
    {
        rdpRegionIntersect(reg, clip_reg, reg);
    }
    rdpClientConAddDirtyScreenReg(dev, clientCon, reg);
}

/******************************************************************************/
void
rdpPolyRectangle(DrawablePtr pDrawable, GCPtr pGC, int nrects,
                 xRectangle *rects)
{
    rdpPtr dev;
    rdpClientCon *clientCon;
    BoxRec box;
    int index;
    int up;
    int down;
    int lw;
    int cd;
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

        box.x1 = (rects[index].x + pDrawable->x) - up;
        box.y1 = (rects[index].y + pDrawable->y) - up;
        box.x2 = box.x1 + rects[index].width + (up + down);
        box.y2 = box.y1 + lw;
        rdpRegionUnionRect(&reg, &box);

        box.x1 = (rects[index].x + pDrawable->x) - up;
        box.y1 = (rects[index].y + pDrawable->y) + down;
        box.x2 = box.x1 + lw;
        box.y2 = box.y1 + rects[index].height - (up + down);
        rdpRegionUnionRect(&reg, &box);

        box.x1 = ((rects[index].x + rects[index].width) + pDrawable->x) - up;
        box.y1 = (rects[index].y + pDrawable->y) + down;
        box.x2 = box.x1 + lw;
        box.y2 = box.y1 + rects[index].height - (up + down);
        rdpRegionUnionRect(&reg, &box);

        box.x1 = (rects[index].x + pDrawable->x) - up;
        box.y1 = ((rects[index].y + rects[index].height) + pDrawable->y) - up;
        box.x2 = box.x1 + rects[index].width + (up + down);
        box.y2 = box.y1 + lw;
        rdpRegionUnionRect(&reg, &box);

        index++;
    }

    rdpRegionInit(&clip_reg, NullBox, 0);
    cd = rdpDrawGetClip(dev, &clip_reg, pDrawable, pGC);
    LLOGLN(10, ("rdpPolyRectangle: cd %d", cd));

    clientCon = dev->clientConHead;
    while (clientCon != NULL)
    {
        rdpPolyRectanglePre(dev, clientCon, cd, &clip_reg, pDrawable, pGC,
                            nrects, rects, &reg);
        clientCon = clientCon->next;
    }
    /* do original call */
    rdpPolyRectangleOrg(pDrawable, pGC, nrects, rects);
    clientCon = dev->clientConHead;
    while (clientCon != NULL)
    {
        rdpPolyRectanglePost(dev, clientCon, cd, &clip_reg, pDrawable, pGC,
                             nrects, rects, &reg);
        clientCon = clientCon->next;
    }
    rdpRegionUninit(&reg);
    rdpRegionUninit(&clip_reg);
}
