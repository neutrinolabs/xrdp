/*
Copyright 2014 Jay Sorg

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

#include "mipict.h"
#include <picture.h>

#include "rdp.h"
#include "rdpDraw.h"
#include "rdpClientCon.h"
#include "rdpReg.h"
#include "rdpTrapezoids.h"

/******************************************************************************/
#define LOG_LEVEL 1
#define LLOGLN(_level, _args) \
    do { if (_level < LOG_LEVEL) { ErrorF _args ; ErrorF("\n"); } } while (0)

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
    int dstx;
    int dsty;

    LLOGLN(10, ("rdpTrapezoids:"));
    pScreen = pDst->pDrawable->pScreen;
    dev = rdpGetDevFromScreen(pScreen);
    dev->counts.rdpTrapezoidsCallCount++;
    dstx = traps[0].left.p1.x >> 16;
    dsty = traps[0].left.p1.y >> 16;
    miTrapezoidBounds(ntrap, traps, &box);
    box.x1 += pDst->pDrawable->x;
    box.y1 += pDst->pDrawable->y;
    box.x2 += pDst->pDrawable->x;
    box.y2 += pDst->pDrawable->y;
    LLOGLN(10, ("%d %d %d %d %d %d", dstx, dsty, box.x1, box.y1,
           box.x2, box.y2));
    ps = GetPictureScreen(pScreen);
    clientCon = dev->clientConHead;
    while (clientCon != NULL)
    {
        rdpTrapezoidsPre(dev, clientCon, ps, op, pSrc, pDst,
                         maskFormat, xSrc, ySrc, ntrap, traps, &box);
        clientCon = clientCon->next;
    }
    /* do original call */
    rdpTrapezoidsOrg(ps, dev, op, pSrc, pDst, maskFormat, xSrc, ySrc,
                     ntrap, traps);
    clientCon = dev->clientConHead;
    while (clientCon != NULL)
    {
        rdpTrapezoidsPost(dev, clientCon, ps, op, pSrc, pDst,
                          maskFormat, xSrc, ySrc, ntrap, traps, &box);
        clientCon = clientCon->next;
    }
}
