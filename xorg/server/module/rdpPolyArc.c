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
rdpPolyArcPre(rdpPtr dev, rdpClientCon *clientCon,
              int cd, RegionPtr clip_reg,
              DrawablePtr pDrawable, GCPtr pGC, int narcs, xArc *parcs,
              RegionPtr reg)
{
}

/******************************************************************************/
static void
rdpPolyArcOrg(DrawablePtr pDrawable, GCPtr pGC, int narcs, xArc *parcs)
{
    GC_OP_VARS;

    GC_OP_PROLOGUE(pGC);
    pGC->ops->PolyArc(pDrawable, pGC, narcs, parcs);
    GC_OP_EPILOGUE(pGC);
}

/******************************************************************************/
static void
rdpPolyArcPost(rdpPtr dev, rdpClientCon *clientCon,
               int cd, RegionPtr clip_reg,
               DrawablePtr pDrawable, GCPtr pGC, int narcs, xArc *parcs,
               RegionPtr reg)
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
rdpPolyArc(DrawablePtr pDrawable, GCPtr pGC, int narcs, xArc *parcs)
{
    rdpPtr dev;
    rdpClientCon *clientCon;
    BoxRec box;
    int index;
    int cd;
    int lw;
    int extra;
    RegionRec clip_reg;
    RegionRec reg;

    LLOGLN(10, ("rdpPolyArc:"));
    dev = rdpGetDevFromScreen(pGC->pScreen);
    dev->counts.rdpPolyArcCallCount++;
    rdpRegionInit(&reg, NullBox, 0);
    if (narcs > 0)
    {
        lw = pGC->lineWidth;
        if (lw == 0)
        {
            lw = 1;
        }
        extra = lw / 2;
        for (index = 0; index < narcs; index++)
        {
            box.x1 = (parcs[index].x - extra) + pDrawable->x;
            box.y1 = (parcs[index].y - extra) + pDrawable->y;
            box.x2 = box.x1 + parcs[index].width + lw;
            box.y2 = box.y1 + parcs[index].height + lw;
            rdpRegionUnionRect(&reg, &box);
        }
    }
    rdpRegionInit(&clip_reg, NullBox, 0);
    cd = rdpDrawGetClip(dev, &clip_reg, pDrawable, pGC);
    LLOGLN(10, ("rdpPolyArc: cd %d", cd));
    clientCon = dev->clientConHead;
    while (clientCon != NULL)
    {
        rdpPolyArcPre(dev, clientCon, cd, &clip_reg, pDrawable, pGC,
                      narcs, parcs, &reg);
        clientCon = clientCon->next;
    }
    /* do original call */
    rdpPolyArcOrg(pDrawable, pGC, narcs, parcs);
    clientCon = dev->clientConHead;
    while (clientCon != NULL)
    {
        rdpPolyArcPost(dev, clientCon, cd, &clip_reg, pDrawable, pGC,
                       narcs, parcs, &reg);
        clientCon = clientCon->next;
    }
    rdpRegionUninit(&reg);
    rdpRegionUninit(&clip_reg);
}
