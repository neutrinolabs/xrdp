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
rdpCopyAreaPre(rdpPtr dev, rdpClientCon *clientCon,
               int cd, RegionPtr clip_reg,
               DrawablePtr pSrc, DrawablePtr pDst, GCPtr pGC,
               int srcx, int srcy, int w, int h, int dstx, int dsty)
{
}

/******************************************************************************/
static RegionPtr
rdpCopyAreaOrg(DrawablePtr pSrc, DrawablePtr pDst, GCPtr pGC,
               int srcx, int srcy, int w, int h, int dstx, int dsty)
{
    GC_OP_VARS;
    RegionPtr rv;

    GC_OP_PROLOGUE(pGC);
    rv = pGC->ops->CopyArea(pSrc, pDst, pGC, srcx, srcy, w, h, dstx, dsty);
    GC_OP_EPILOGUE(pGC);
    return rv;
}

/******************************************************************************/
static void
rdpCopyAreaPost(rdpPtr dev, rdpClientCon *clientCon,
                int cd, RegionPtr clip_reg,
                DrawablePtr pSrc, DrawablePtr pDst, GCPtr pGC,
                int srcx, int srcy, int w, int h, int dstx, int dsty)
{
    BoxRec box;
    RegionRec reg;

    if (cd == XRDP_CD_NODRAW)
    {
        return;
    }
    if (!XRDP_DRAWABLE_IS_VISIBLE(dev, pDst))
    {
        return;
    }
    box.x1 = dstx + pDst->x;
    box.y1 = dsty + pDst->y;
    box.x2 = box.x1 + w;
    box.y2 = box.y1 + h;
    rdpRegionInit(&reg, &box, 0);
    if (cd == XRDP_CD_CLIP)
    {
        rdpRegionIntersect(&reg, clip_reg, &reg);
    }
    rdpClientConAddDirtyScreenReg(dev, clientCon, &reg);
    rdpRegionUninit(&reg);
}

/******************************************************************************/
RegionPtr
rdpCopyArea(DrawablePtr pSrc, DrawablePtr pDst, GCPtr pGC,
            int srcx, int srcy, int w, int h, int dstx, int dsty)
{
    rdpPtr dev;
    rdpClientCon *clientCon;
    RegionPtr rv;
    RegionRec clip_reg;
    int cd;

    LLOGLN(10, ("rdpCopyArea:"));
    dev = rdpGetDevFromScreen(pGC->pScreen);
    dev->counts.rdpCopyAreaCallCount++;
    rdpRegionInit(&clip_reg, NullBox, 0);
    cd = rdpDrawGetClip(dev, &clip_reg, pDst, pGC);
    LLOGLN(10, ("rdpCopyArea: cd %d", cd));
    clientCon = dev->clientConHead;
    while (clientCon != NULL)
    {
        rdpCopyAreaPre(dev, clientCon, cd, &clip_reg, pSrc, pDst, pGC,
                       srcx, srcy, w, h, dstx, dsty);
        clientCon = clientCon->next;
    }
    /* do original call */
    rv = rdpCopyAreaOrg(pSrc, pDst, pGC, srcx, srcy, w, h, dstx, dsty);
    clientCon = dev->clientConHead;
    while (clientCon != NULL)
    {
        rdpCopyAreaPost(dev, clientCon, cd, &clip_reg, pSrc, pDst, pGC,
                        srcx, srcy, w, h, dstx, dsty);
        clientCon = clientCon->next;
    }
    rdpRegionUninit(&clip_reg);
    return rv;
}
