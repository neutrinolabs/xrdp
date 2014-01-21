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

misc draw calls

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* this should be before all X11 .h files */
#include <xorg-server.h>

/* all driver need this */
#include <xf86.h>
#include <xf86_OSproc.h>

#include <mipointer.h>
#include <fb.h>
#include <micmap.h>
#include <mi.h>
#include <dixfontstr.h>

#include "rdp.h"
#include "rdpDraw.h"
#include "rdpClientCon.h"
#include "rdpMisc.h"
#include "rdpGlyphs.h"
#include "rdpReg.h"

#define LOG_LEVEL 1
#define LLOGLN(_level, _args) \
    do { if (_level < LOG_LEVEL) { ErrorF _args ; ErrorF("\n"); } } while (0)

/******************************************************************************/
/* return 0, draw nothing */
/* return 1, draw with no clip */
/* return 2, draw using clip */
int
rdpDrawGetClip(rdpPtr dev, RegionPtr pRegion, DrawablePtr pDrawable, GCPtr pGC)
{
    WindowPtr pWindow;
    RegionPtr temp;
    BoxRec box;
    int rv;

    rv = 0;

    if (pDrawable->type == DRAWABLE_PIXMAP)
    {
        switch (pGC->clientClipType)
        {
            case CT_NONE:
                rv = 1;
                break;
            case CT_REGION:
                rv = 2;
                rdpRegionCopy(pRegion, pGC->clientClip);
                break;
            default:
                LLOGLN(0, ("rdpDrawGetClip: unimp clip type %d",
                       pGC->clientClipType));
                break;
        }

        if (rv == 2) /* check if the clip is the entire pixmap */
        {
            box.x1 = 0;
            box.y1 = 0;
            box.x2 = pDrawable->width;
            box.y2 = pDrawable->height;

            if (rdpRegionContainsRect(pRegion, &box) == rgnIN)
            {
                rv = 1;
            }
        }
    }
    else if (pDrawable->type == DRAWABLE_WINDOW)
    {
        pWindow = (WindowPtr)pDrawable;

        if (pWindow->viewable)
        {
            if (pGC->subWindowMode == IncludeInferiors)
            {
                temp = &pWindow->borderClip;
            }
            else
            {
                temp = &pWindow->clipList;
            }

            if (RegionNotEmpty(temp))
            {
                switch (pGC->clientClipType)
                {
                    case CT_NONE:
                        rv = 2;
                        rdpRegionCopy(pRegion, temp);
                        break;
                    case CT_REGION:
                        rv = 2;
                        rdpRegionCopy(pRegion, pGC->clientClip);
                        rdpRegionTranslate(pRegion,
                                           pDrawable->x + pGC->clipOrg.x,
                                           pDrawable->y + pGC->clipOrg.y);
                        rdpRegionIntersect(pRegion, pRegion, temp);
                        break;
                    default:
                        LLOGLN(0, ("rdpDrawGetClip: unimp clip type %d",
                               pGC->clientClipType));
                        break;
                }

                if (rv == 2) /* check if the clip is the entire screen */
                {
                    box.x1 = 0;
                    box.y1 = 0;
                    box.x2 = dev->width;
                    box.y2 = dev->height;

                    if (rdpRegionContainsRect(pRegion, &box) == rgnIN)
                    {
                        rv = 1;
                    }
                }
            }
        }
    }

    return rv;
}

/******************************************************************************/
void
GetTextBoundingBox(DrawablePtr pDrawable, FontPtr font, int x, int y,
                   int n, BoxPtr pbox)
{
    int maxAscent;
    int maxDescent;
    int maxCharWidth;

    if (FONTASCENT(font) > FONTMAXBOUNDS(font, ascent))
    {
        maxAscent = FONTASCENT(font);
    }
    else
    {
        maxAscent = FONTMAXBOUNDS(font, ascent);
    }

    if (FONTDESCENT(font) > FONTMAXBOUNDS(font, descent))
    {
        maxDescent = FONTDESCENT(font);
    }
    else
    {
        maxDescent = FONTMAXBOUNDS(font, descent);
    }

    if (FONTMAXBOUNDS(font, rightSideBearing) >
            FONTMAXBOUNDS(font, characterWidth))
    {
        maxCharWidth = FONTMAXBOUNDS(font, rightSideBearing);
    }
    else
    {
        maxCharWidth = FONTMAXBOUNDS(font, characterWidth);
    }

    pbox->x1 = pDrawable->x + x;
    pbox->y1 = pDrawable->y + y - maxAscent;
    pbox->x2 = pbox->x1 + maxCharWidth * n;
    pbox->y2 = pbox->y1 + maxAscent + maxDescent;

    if (FONTMINBOUNDS(font, leftSideBearing) < 0)
    {
        pbox->x1 += FONTMINBOUNDS(font, leftSideBearing);
    }
}

/******************************************************************************/
int
rdpDrawItemAdd(rdpPtr dev, rdpPixmapRec *priv, struct rdp_draw_item *di)
{
    priv->is_alpha_dirty_not = FALSE;

    if (priv->draw_item_tail == NULL)
    {
        priv->draw_item_tail = di;
        priv->draw_item_head = di;
    }
    else
    {
        di->prev = priv->draw_item_tail;
        priv->draw_item_tail->next = di;
        priv->draw_item_tail = di;
    }

    if (priv == &(dev->screenPriv))
    {
        rdpClientConScheduleDeferredUpdate(dev);
    }

    return 0;
}

/******************************************************************************/
int
rdpDrawItemRemove(rdpPtr dev, rdpPixmapRec *priv, struct rdp_draw_item *di)
{
    if (di->prev != NULL)
    {
        di->prev->next = di->next;
    }

    if (di->next != NULL)
    {
        di->next->prev = di->prev;
    }

    if (priv->draw_item_head == di)
    {
        priv->draw_item_head = di->next;
    }

    if (priv->draw_item_tail == di)
    {
        priv->draw_item_tail = di->prev;
    }

    if (di->type == RDI_LINE)
    {
        if (di->u.line.segs != NULL)
        {
            g_free(di->u.line.segs);
        }
    }

    if (di->type == RDI_TEXT)
    {
        rdpGlyphDeleteRdpText(di->u.text.rtext);
    }

    rdpRegionDestroy(di->reg);
    g_free(di);
    return 0;
}

/******************************************************************************/
int
rdpDrawItemRemoveAll(rdpPtr dev, rdpPixmapRec *priv)
{
    struct rdp_draw_item *di;

    di = priv->draw_item_head;

    while (di != NULL)
    {
        rdpDrawItemRemove(dev, priv, di);
        di = priv->draw_item_head;
    }

    return 0;
}

/*****************************************************************************/
void
rdpCopyWindow(WindowPtr pWin, DDXPointRec ptOldOrg, RegionPtr pOldRegion)
{
    ScreenPtr pScreen;
    rdpPtr dev;
    rdpClientCon *clientCon;
    RegionRec reg;
    RegionRec clip;
    int dx;
    int dy;
    int num_clip_rects;
    int num_reg_rects;
    BoxPtr box;

    pScreen = pWin->drawable.pScreen;
    dev = rdpGetDevFromScreen(pScreen);

    rdpRegionInit(&reg, NullBox, 0);
    rdpRegionCopy(&reg, pOldRegion);
    rdpRegionInit(&clip, NullBox, 0);
    rdpRegionCopy(&clip, &pWin->borderClip);
    dx = pWin->drawable.x - ptOldOrg.x;
    dy = pWin->drawable.y - ptOldOrg.y;

    dev->pScreen->CopyWindow = dev->CopyWindow;
    dev->pScreen->CopyWindow(pWin, ptOldOrg, pOldRegion);
    dev->pScreen->CopyWindow = rdpCopyWindow;

    num_clip_rects = REGION_NUM_RECTS(&clip);
    num_reg_rects = REGION_NUM_RECTS(&reg);

    if ((num_clip_rects == 0) || (num_reg_rects == 0))
    {
        rdpRegionUninit(&reg);
        rdpRegionUninit(&clip);
        return;
    }

    if ((num_clip_rects > 16) && (num_reg_rects > 16))
    {
        box = rdpRegionExtents(&reg);
        clientCon = dev->clientConHead;
        while (clientCon != NULL)
        {
            rdpClientConAddDirtyScreenBox(dev, clientCon, box);
            clientCon = clientCon->next;
        }
    }
    else
    {
        rdpRegionTranslate(&reg, dx, dy);
        rdpRegionIntersect(&reg, &reg, &clip);
        clientCon = dev->clientConHead;
        while (clientCon != NULL)
        {
            rdpClientConAddDirtyScreenReg(dev, clientCon, &reg);
            clientCon = clientCon->next;
        }
    }
    rdpRegionUninit(&reg);
    rdpRegionUninit(&clip);
}

/*****************************************************************************/
Bool
rdpCloseScreen(int index, ScreenPtr pScreen)
{
    rdpPtr dev;
    Bool rv;

    LLOGLN(0, ("rdpCloseScreen:"));
    dev = rdpGetDevFromScreen(pScreen);
    dev->pScreen->CloseScreen = dev->CloseScreen;
    rv = dev->pScreen->CloseScreen(index, pScreen);
    dev->pScreen->CloseScreen = rdpCloseScreen;
    return rv;
}

/******************************************************************************/
WindowPtr
rdpGetRootWindowPtr(ScreenPtr pScreen)
{
#if XORG_VERSION_CURRENT < (((1) * 10000000) + ((9) * 100000) + ((0) * 1000) + 0)
    return WindowTable[pScreen->myNum]; /* in globals.c */
#else
    return pScreen->root;
#endif
}

/******************************************************************************/
rdpPtr
rdpGetDevFromScreen(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn;
    rdpPtr dev;

    if (pScreen == NULL)
    {
        pScrn = xf86Screens[0];
    }
    else
    {
        pScrn = xf86Screens[pScreen->myNum];
    }
    dev = XRDPPTR(pScrn);
    return dev;
}
