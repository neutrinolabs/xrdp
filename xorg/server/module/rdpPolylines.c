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
rdpPolylinesPre(rdpPtr dev, rdpClientCon *clientCon,
                int cd,  RegionPtr clip_reg,
                DrawablePtr pDrawable, GCPtr pGC, int mode,
                int npt, DDXPointPtr pptInit, RegionPtr reg)
{
}

/******************************************************************************/
static void
rdpPolylinesOrg(DrawablePtr pDrawable, GCPtr pGC, int mode,
                int npt, DDXPointPtr pptInit)
{
    GC_OP_VARS;

    GC_OP_PROLOGUE(pGC);
    pGC->ops->Polylines(pDrawable, pGC, mode, npt, pptInit);
    GC_OP_EPILOGUE(pGC);
}

/******************************************************************************/
void
rdpPolylinesPost(rdpPtr dev, rdpClientCon *clientCon,
                 int cd,  RegionPtr clip_reg,
                 DrawablePtr pDrawable, GCPtr pGC, int mode,
                 int npt, DDXPointPtr pptInit, RegionPtr reg)
{
    RegionRec lreg;

    if (cd == XRDP_CD_NODRAW)
    {
        return;
    }
    if (!XRDP_DRAWABLE_IS_VISIBLE(dev, pDrawable))
    {
        return;
    }
    rdpRegionInit(&lreg, NullBox, 0);
    if (cd == XRDP_CD_CLIP)
    {
        rdpRegionIntersect(&lreg, clip_reg, reg);
    }
    rdpClientConAddDirtyScreenReg(dev, clientCon, &lreg);
    rdpRegionUninit(&lreg);
}

/******************************************************************************/
void
rdpPolylines(DrawablePtr pDrawable, GCPtr pGC, int mode,
             int npt, DDXPointPtr pptInit)
{
    rdpPtr dev;
    rdpClientCon *clientCon;
    RegionRec clip_reg;
    RegionRec reg;
    int cd;
    int index;
    int x1;
    int y1;
    int x2;
    int y2;
    BoxRec box;

    LLOGLN(10, ("rdpPolylines:"));
    dev = rdpGetDevFromScreen(pGC->pScreen);
    dev->counts.rdpPolylinesCallCount++;
    rdpRegionInit(&reg, NullBox, 0);
    for (index = 1; index < npt; index++)
    {
        x1 = pptInit[index - 1].x + pDrawable->x;
        y1 = pptInit[index - 1].y + pDrawable->y;
        x2 = pptInit[index].x + pDrawable->x;
        y2 = pptInit[index].y + pDrawable->y;
        box.x1 = RDPMIN(x1, x2);
        box.y1 = RDPMIN(y1, y2);
        box.x2 = RDPMAX(x1, x2) + 1;
        box.y2 = RDPMAX(y1, y2) + 1;
        rdpRegionUnionRect(&reg, &box);
    }
    rdpRegionInit(&clip_reg, NullBox, 0);
    cd = rdpDrawGetClip(dev, &clip_reg, pDrawable, pGC);
    LLOGLN(10, ("rdpPolylines: cd %d", cd));
    clientCon = dev->clientConHead;
    while (clientCon != NULL)
    {
        rdpPolylinesPre(dev, clientCon, cd, &clip_reg, pDrawable,
                        pGC, mode, npt, pptInit, &reg);
        clientCon = clientCon->next;
    }
    /* do original call */
    rdpPolylinesOrg(pDrawable, pGC, mode, npt, pptInit);
    clientCon = dev->clientConHead;
    while (clientCon != NULL)
    {
        rdpPolylinesPost(dev, clientCon, cd, &clip_reg, pDrawable,
                         pGC, mode, npt, pptInit, &reg);
        clientCon = clientCon->next;
    }
    rdpRegionUninit(&clip_reg);
    rdpRegionUninit(&reg);
}
