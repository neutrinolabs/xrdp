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
rdpPolyFillRectPre(rdpPtr dev, rdpClientCon *clientCon,
                   int cd, RegionPtr clip_reg,
                   DrawablePtr pDrawable, GCPtr pGC,
                   int nrectFill, xRectangle *prectInit,
                   RegionPtr reg)
{
}

/******************************************************************************/
static void
rdpPolyFillRectOrg(DrawablePtr pDrawable, GCPtr pGC, int nrectFill,
                   xRectangle *prectInit)
{
    GC_OP_VARS;

    GC_OP_PROLOGUE(pGC);
    pGC->ops->PolyFillRect(pDrawable, pGC, nrectFill, prectInit);
    GC_OP_EPILOGUE(pGC);
}

/******************************************************************************/
static void
rdpPolyFillRectPost(rdpPtr dev, rdpClientCon *clientCon,
                    int cd, RegionPtr clip_reg,
                    DrawablePtr pDrawable, GCPtr pGC,
                    int nrectFill, xRectangle *prectInit,
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
    rdpClientConAddDirtyScreenReg(dev, clientCon, reg);
}

/******************************************************************************/
void
rdpPolyFillRect(DrawablePtr pDrawable, GCPtr pGC, int nrectFill,
                xRectangle *prectInit)
{
    rdpPtr dev;
    rdpClientCon *clientCon;
    RegionRec clip_reg;
    RegionPtr reg;
    int cd;

    LLOGLN(10, ("rdpPolyFillRect:"));
    dev = rdpGetDevFromScreen(pGC->pScreen);
    dev->counts.rdpPolyFillRectCallCount++;
    /* make a copy of rects */
    reg = rdpRegionFromRects(nrectFill, prectInit, CT_NONE);
    rdpRegionTranslate(reg, pDrawable->x, pDrawable->y);
    rdpRegionInit(&clip_reg, NullBox, 0);
    cd = rdpDrawGetClip(dev, &clip_reg, pDrawable, pGC);
    if (cd == XRDP_CD_CLIP)
    {
        rdpRegionIntersect(reg, &clip_reg, reg);
    }
    LLOGLN(10, ("rdpPolyFillRect: cd %d", cd));
    clientCon = dev->clientConHead;
    while (clientCon != NULL)
    {
        rdpPolyFillRectPre(dev, clientCon, cd, &clip_reg, pDrawable,
                           pGC, nrectFill, prectInit, reg);
        clientCon = clientCon->next;
    }
    /* do original call */
    rdpPolyFillRectOrg(pDrawable, pGC, nrectFill, prectInit);
    clientCon = dev->clientConHead;
    while (clientCon != NULL)
    {
        rdpPolyFillRectPost(dev, clientCon, cd, &clip_reg, pDrawable,
                            pGC, nrectFill, prectInit, reg);
        clientCon = clientCon->next;
    }
    rdpRegionUninit(&clip_reg);
    rdpRegionDestroy(reg);
}
