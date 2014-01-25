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

composite(alpha blending) calls

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* this should be before all X11 .h files */
#include <xorg-server.h>

/* all driver need this */
#include <xf86.h>
#include <xf86_OSproc.h>

#include <picture.h>

#include "rdp.h"
#include "rdpDraw.h"
#include "rdpClientCon.h"
#include "rdpReg.h"
#include "rdpComposite.h"

/******************************************************************************/
#define LOG_LEVEL 1
#define LLOGLN(_level, _args) \
    do { if (_level < LOG_LEVEL) { ErrorF _args ; ErrorF("\n"); } } while (0)

/******************************************************************************/
static void
rdpCompositePre(rdpPtr dev, rdpClientCon *clientCon,
                PictureScreenPtr ps, CARD8 op, PicturePtr pSrc,
                PicturePtr pMask, PicturePtr pDst,
                INT16 xSrc, INT16 ySrc, INT16 xMask, INT16 yMask,
                INT16 xDst, INT16 yDst, CARD16 width, CARD16 height,
                BoxPtr box)
{
}

/******************************************************************************/
static void
rdpCompositeOrg(PictureScreenPtr ps, rdpPtr dev,
                CARD8 op, PicturePtr pSrc, PicturePtr pMask, PicturePtr pDst,
                INT16 xSrc, INT16 ySrc, INT16 xMask, INT16 yMask,
                INT16 xDst, INT16 yDst, CARD16 width, CARD16 height)
{
    ps->Composite = dev->Composite;
    ps->Composite(op, pSrc, pMask, pDst, xSrc, ySrc, xMask, yMask,
                  xDst, yDst, width, height);
    ps->Composite = rdpComposite;
}

/******************************************************************************/
static void
rdpCompositePost(rdpPtr dev, rdpClientCon *clientCon,
                 PictureScreenPtr ps, CARD8 op, PicturePtr pSrc,
                 PicturePtr pMask, PicturePtr pDst,
                 INT16 xSrc, INT16 ySrc, INT16 xMask, INT16 yMask,
                 INT16 xDst, INT16 yDst, CARD16 width, CARD16 height,
                 BoxPtr box)
{
    RegionRec reg;

    if (!XRDP_DRAWABLE_IS_VISIBLE(dev, pDst->pDrawable))
    {
        return;
    }
    rdpRegionInit(&reg, box, 0);
    if (pDst->pCompositeClip != 0)
    {
        rdpRegionIntersect(&reg, pDst->pCompositeClip, &reg);
    }
    rdpClientConAddDirtyScreenReg(dev, clientCon, &reg);
    rdpRegionUninit(&reg);
}

/******************************************************************************/
void
rdpComposite(CARD8 op, PicturePtr pSrc, PicturePtr pMask, PicturePtr pDst,
             INT16 xSrc, INT16 ySrc, INT16 xMask, INT16 yMask, INT16 xDst,
             INT16 yDst, CARD16 width, CARD16 height)
{
    ScreenPtr pScreen;
    rdpPtr dev;
    rdpClientCon *clientCon;
    PictureScreenPtr ps;
    BoxRec box;

    LLOGLN(0, ("rdpComposite:"));
    pScreen = pDst->pDrawable->pScreen;
    dev = rdpGetDevFromScreen(pScreen);
    dev->counts.rdpCompositeCallCount++;
    box.x1 = xDst + pDst->pDrawable->x;
    box.y1 = yDst + pDst->pDrawable->y;
    box.x2 = box.x1 + width;
    box.y2 = box.y1 + height;
    ps = GetPictureScreen(pScreen);
    clientCon = dev->clientConHead;
    while (clientCon != NULL)
    {
        rdpCompositePre(dev, clientCon, ps, op, pSrc, pMask, pDst,
                        xSrc, ySrc, xMask, yMask, xDst, yDst,
                        width, height, &box);
        clientCon = clientCon->next;
    }
    /* do original call */
    rdpCompositeOrg(ps, dev, op, pSrc, pMask, pDst, xSrc, ySrc,
                    xMask, yMask, xDst, yDst, width, height);
    clientCon = dev->clientConHead;
    while (clientCon != NULL)
    {
        rdpCompositePost(dev, clientCon, ps, op, pSrc, pMask, pDst,
                         xSrc, ySrc, xMask, yMask, xDst, yDst,
                         width, height, &box);
        clientCon = clientCon->next;
    }
}

/******************************************************************************/
static void
rdpTrapezoidsPre(rdpPtr dev, rdpClientCon *clientCon, PictureScreenPtr ps,
                 CARD8 op, PicturePtr pSrc, PicturePtr pDst,
                 PictFormatPtr maskFormat, INT16 xSrc, INT16 ySrc,
                 int ntrap, xTrapezoid *traps, BoxPtr box)
{
}

/******************************************************************************/
static void
rdpTrapezoidsOrg(PictureScreenPtr ps, rdpPtr dev,
                 CARD8 op, PicturePtr pSrc, PicturePtr pDst,
                 PictFormatPtr maskFormat, INT16 xSrc, INT16 ySrc,
                 int ntrap, xTrapezoid *traps)
{
    ps->Trapezoids = dev->Trapezoids;
    ps->Trapezoids(op, pSrc, pDst, maskFormat, xSrc, ySrc, ntrap, traps);
    ps->Trapezoids = rdpTrapezoids;
}

/******************************************************************************/
static void
rdpTrapezoidsPost(rdpPtr dev, rdpClientCon *clientCon, PictureScreenPtr ps,
                  CARD8 op, PicturePtr pSrc, PicturePtr pDst,
                  PictFormatPtr maskFormat, INT16 xSrc, INT16 ySrc,
                  int ntrap, xTrapezoid *traps, BoxPtr box)
{
    RegionRec reg;

    if (!XRDP_DRAWABLE_IS_VISIBLE(dev, pDst->pDrawable))
    {
        return;
    }
    rdpRegionInit(&reg, box, 0);
    if (pDst->pCompositeClip != 0)
    {
        rdpRegionIntersect(&reg, pDst->pCompositeClip, &reg);
    }
    rdpClientConAddDirtyScreenReg(dev, clientCon, &reg);
    rdpRegionUninit(&reg);
}

/******************************************************************************/
void
rdpTrapezoids(CARD8 op, PicturePtr pSrc, PicturePtr pDst,
              PictFormatPtr maskFormat, INT16 xSrc, INT16 ySrc,
              int ntrap, xTrapezoid *traps)
{
    ScreenPtr pScreen;
    rdpPtr dev;
    rdpClientCon *clientCon;
    PictureScreenPtr ps;
    BoxRec box;
    RegionRec reg;
    int index;

    LLOGLN(0, ("rdpTrapezoids:"));
    pScreen = pDst->pDrawable->pScreen;
    dev = rdpGetDevFromScreen(pScreen);
    dev->counts.rdpTrapezoidsCallCount++;

    rdpRegionInit(&reg, NullBox, 0);
    for (index = 0; index < ntrap; index++)
    {
        LLOGLN(0, ("  top %d bottom %d left p1 %d %d",
               traps[index].top, traps[index].bottom,
               traps[index].left.p1.x, traps[index].left.p1.y));
        //box.x1 = traps[index].left + pDst->pDrawable->x;
        //box.y1 = traps[index].top + pDst->pDrawable->y;
        //box.x2 = traps[index].right + pDst->pDrawable->x;
        //box.y2 = traps[index].bottom + pDst->pDrawable->y;
    }
    ps = GetPictureScreen(pScreen);
    clientCon = dev->clientConHead;
    while (clientCon != NULL)
    {
        //rdpTrapezoidsPre(dev, clientCon, ps, op, pSrc, pDst,
        //                 maskFormat, xSrc, ySrc, ntrap, traps, &box);
        clientCon = clientCon->next;
    }
    /* do original call */
    rdpTrapezoidsOrg(ps, dev, op, pSrc, pDst, maskFormat, xSrc, ySrc,
                     ntrap, traps);
    clientCon = dev->clientConHead;
    while (clientCon != NULL)
    {
        //rdpTrapezoidsPost(dev, clientCon, ps, op, pSrc, pDst,
        //                  maskFormat, xSrc, ySrc, ntrap, traps, &box);
        clientCon = clientCon->next;
    }
}
