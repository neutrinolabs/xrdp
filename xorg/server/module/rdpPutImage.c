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
rdpPutImagePre(rdpPtr dev, rdpClientCon *clientCon,
               int cd, RegionPtr clip_reg,
               DrawablePtr pDst, GCPtr pGC, int depth, int x, int y,
               int w, int h, int leftPad, int format, char *pBits)
{
}

/******************************************************************************/
static void
rdpPutImageOrg(DrawablePtr pDst, GCPtr pGC, int depth, int x, int y,
               int w, int h, int leftPad, int format, char *pBits)
{
    GC_OP_VARS;

    GC_OP_PROLOGUE(pGC);
    pGC->ops->PutImage(pDst, pGC, depth, x, y, w, h, leftPad,
                       format, pBits);
    GC_OP_EPILOGUE(pGC);
}

/******************************************************************************/
void
rdpPutImagePost(rdpPtr dev, rdpClientCon *clientCon,
                int cd, RegionPtr clip_reg,
                DrawablePtr pDst, GCPtr pGC, int depth, int x, int y,
                int w, int h, int leftPad, int format, char *pBits)
{
    WindowPtr pDstWnd;
    BoxRec box;
    RegionRec reg;

    if (cd == 0)
    {
        return;
    }
    if (pDst->type != DRAWABLE_WINDOW)
    {
        return;
    }
    pDstWnd = (WindowPtr) pDst;
    if (pDstWnd->viewable == FALSE)
    {
        return;
    }
    box.x1 = x + pDst->x;
    box.y1 = y + pDst->y;
    box.x2 = box.x1 + w;
    box.y2 = box.y1 + h;
    rdpRegionInit(&reg, &box, 0);
    if (cd == 2)
    {
        rdpRegionIntersect(&reg, clip_reg, &reg);
    }
    rdpClientConAddDirtyScreenReg(dev, clientCon, &reg);
    RegionUninit(&reg);
}

/******************************************************************************/
void
rdpPutImage(DrawablePtr pDst, GCPtr pGC, int depth, int x, int y,
            int w, int h, int leftPad, int format, char *pBits)
{
    rdpPtr dev;
    rdpClientCon *clientCon;
    RegionRec clip_reg;
    int cd;

    LLOGLN(10, ("rdpPutImage:"));
    dev = rdpGetDevFromScreen(pGC->pScreen);
    rdpRegionInit(&clip_reg, NullBox, 0);
    cd = rdpDrawGetClip(dev, &clip_reg, pDst, pGC);
    LLOGLN(10, ("rdpPutImage: cd %d", cd));
    clientCon = dev->clientConHead;
    while (clientCon != NULL)
    {
        rdpPutImagePre(dev, clientCon, cd, &clip_reg, pDst, pGC, depth, x, y,
                       w, h, leftPad, format, pBits);
        clientCon = clientCon->next;
    }

    /* do original call */
    rdpPutImageOrg(pDst, pGC, depth, x, y, w, h, leftPad, format, pBits);
    clientCon = dev->clientConHead;
    while (clientCon != NULL)
    {
        rdpPutImagePost(dev, clientCon, cd, &clip_reg, pDst, pGC, depth, x, y,
                        w, h, leftPad, format, pBits);
        clientCon = clientCon->next;
    }
    RegionUninit(&clip_reg);
}
