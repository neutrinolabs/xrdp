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
rdpFillPolygonOrg(DrawablePtr pDrawable, GCPtr pGC,
                  int shape, int mode, int count,
                  DDXPointPtr pPts)
{
    GC_OP_VARS;

    GC_OP_PROLOGUE(pGC);
    pGC->ops->FillPolygon(pDrawable, pGC, shape, mode, count, pPts);
    GC_OP_EPILOGUE(pGC);
}

/******************************************************************************/
void
rdpFillPolygon(DrawablePtr pDrawable, GCPtr pGC,
               int shape, int mode, int count,
               DDXPointPtr pPts)
{
    rdpPtr dev;
    RegionRec clip_reg;
    RegionRec reg;
    int cd;
    int maxx;
    int maxy;
    int minx;
    int miny;
    int index;
    int x;
    int y;
    BoxRec box;

    LLOGLN(10, ("rdpFillPolygon:"));
    dev = rdpGetDevFromScreen(pGC->pScreen);
    dev->counts.rdpFillPolygonCallCount++;
    box.x1 = 0;
    box.y1 = 0;
    box.x2 = 0;
    box.y2 = 0;
    if (count > 0)
    {
        maxx = pPts[0].x;
        maxy = pPts[0].y;
        minx = maxx;
        miny = maxy;
        for (index = 1; index < count; index++)
        {
            x = pPts[index].x;
            y = pPts[index].y;
            maxx = RDPMAX(x, maxx);
            minx = RDPMIN(x, minx);
            maxy = RDPMAX(y, maxy);
            miny = RDPMIN(y, miny);
        }
        box.x1 = pDrawable->x + minx;
        box.y1 = pDrawable->y + miny;
        box.x2 = pDrawable->x + maxx + 1;
        box.y2 = pDrawable->y + maxy + 1;
    }
    rdpRegionInit(&reg, &box, 0);
    rdpRegionInit(&clip_reg, NullBox, 0);
    cd = rdpDrawGetClip(dev, &clip_reg, pDrawable, pGC);
    LLOGLN(10, ("rdpFillPolygon: cd %d", cd));
    if (cd == XRDP_CD_CLIP)
    {
        rdpRegionIntersect(&reg, &clip_reg, &reg);
    }
    /* do original call */
    rdpFillPolygonOrg(pDrawable, pGC, shape, mode, count, pPts);
    if (cd != XRDP_CD_NODRAW)
    {
        rdpClientConAddAllReg(dev, &reg, pDrawable);
    }
    rdpRegionUninit(&clip_reg);
    rdpRegionUninit(&reg);
}
