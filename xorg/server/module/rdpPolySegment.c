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
rdpPolySegmentOrg(DrawablePtr pDrawable, GCPtr pGC, int nseg, xSegment *pSegs)
{
    GC_OP_VARS;

    GC_OP_PROLOGUE(pGC);
    pGC->ops->PolySegment(pDrawable, pGC, nseg, pSegs);
    GC_OP_EPILOGUE(pGC);
}

/******************************************************************************/
void
rdpPolySegment(DrawablePtr pDrawable, GCPtr pGC, int nseg, xSegment *pSegs)
{
    rdpPtr dev;
    RegionRec clip_reg;
    RegionRec reg;
    int cd;
    int index;
    int x1;
    int y1;
    int x2;
    int y2;
    BoxRec box;

    LLOGLN(10, ("rdpPolySegment:"));
    dev = rdpGetDevFromScreen(pGC->pScreen);
    dev->counts.rdpPolySegmentCallCount++;
    rdpRegionInit(&reg, NullBox, 0);
    for (index = 0; index < nseg; index++)
    {
        x1 = pSegs[index].x1 + pDrawable->x;
        y1 = pSegs[index].y1 + pDrawable->y;
        x2 = pSegs[index].x2 + pDrawable->x;
        y2 = pSegs[index].y2 + pDrawable->y;
        box.x1 = RDPMIN(x1, x2);
        box.y1 = RDPMIN(y1, y2);
        box.x2 = RDPMAX(x1, x2) + 1;
        box.y2 = RDPMAX(y1, y2) + 1;
        rdpRegionUnionRect(&reg, &box);
    }
    rdpRegionInit(&clip_reg, NullBox, 0);
    cd = rdpDrawGetClip(dev, &clip_reg, pDrawable, pGC);
    LLOGLN(10, ("rdpPolySegment: cd %d", cd));
    if (cd == XRDP_CD_CLIP)
    {
        rdpRegionIntersect(&reg, &clip_reg, &reg);
    }
    /* do original call */
    rdpPolySegmentOrg(pDrawable, pGC, nseg, pSegs);
    if (cd != XRDP_CD_NODRAW)
    {
        rdpClientConAddAllReg(dev, &reg, pDrawable);
    }
    rdpRegionUninit(&clip_reg);
    rdpRegionUninit(&reg);
}
