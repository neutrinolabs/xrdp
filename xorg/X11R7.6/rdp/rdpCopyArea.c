/*
Copyright 2005-2012 Jay Sorg

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

#include "rdp.h"
#include "rdpdraw.h"

#define LDEBUG 0

#define LOG_LEVEL 1
#define LLOG(_level, _args) \
    do { if (_level < LOG_LEVEL) { ErrorF _args ; } } while (0)
#define LLOGLN(_level, _args) \
    do { if (_level < LOG_LEVEL) { ErrorF _args ; ErrorF("\n"); } } while (0)

extern rdpScreenInfoRec g_rdpScreen; /* from rdpmain.c */
extern DevPrivateKeyRec g_rdpGCIndex; /* from rdpmain.c */
extern DevPrivateKeyRec g_rdpWindowIndex; /* from rdpmain.c */
extern DevPrivateKeyRec g_rdpPixmapIndex; /* from rdpmain.c */
extern int g_Bpp; /* from rdpmain.c */
extern ScreenPtr g_pScreen; /* from rdpmain.c */
extern Bool g_wrapPixmap; /* from rdpmain.c */
extern int g_can_do_pix_to_pix; /* from rdpmain.c */
extern int g_do_dirty_os; /* in rdpmain.c */
extern int g_do_dirty_ons; /* in rdpmain.c */
extern rdpPixmapRec g_screenPriv; /* in rdpmain.c */

extern GCOps g_rdpGCOps; /* from rdpdraw.c */

extern int g_con_number; /* in rdpup.c */

/******************************************************************************/
static RegionPtr
rdpCopyAreaOrg(DrawablePtr pSrc, DrawablePtr pDst, GCPtr pGC,
               int srcx, int srcy, int w, int h, int dstx, int dsty)
{
    rdpGCPtr priv;
    GCFuncs *oldFuncs;
    RegionPtr rv;

    GC_OP_PROLOGUE(pGC);
    rv = pGC->ops->CopyArea(pSrc, pDst, pGC, srcx, srcy, w, h, dstx, dsty);
    GC_OP_EPILOGUE(pGC);
    return rv;
}

/******************************************************************************/
static RegionPtr
rdpCopyAreaWndToWnd(WindowPtr pSrcWnd, WindowPtr pDstWnd, GCPtr pGC,
                    int srcx, int srcy, int w, int h,
                    int dstx, int dsty)
{
    int cd;
    int lsrcx;
    int lsrcy;
    int ldstx;
    int ldsty;
    int num_clips;
    int dx;
    int dy;
    int j;
    BoxRec box;
    RegionPtr rv;
    RegionRec clip_reg;

    LLOGLN(10, ("rdpCopyAreaWndToWnd:"));
    RegionInit(&clip_reg, NullBox, 0);
    cd = rdp_get_clip(&clip_reg, &(pDstWnd->drawable), pGC);
    lsrcx = pSrcWnd->drawable.x + srcx;
    lsrcy = pSrcWnd->drawable.y + srcy;
    ldstx = pDstWnd->drawable.x + dstx;
    ldsty = pDstWnd->drawable.y + dsty;

    if (cd == 1)
    {
        rdpup_begin_update();
        rdpup_screen_blt(ldstx, ldsty, w, h, lsrcx, lsrcy);
        rdpup_end_update();
    }
    else if (cd == 2)
    {
        num_clips = REGION_NUM_RECTS(&clip_reg);

        if (num_clips > 0)
        {
            rdpup_begin_update();
            dx = ldstx - lsrcx;
            dy = ldsty - lsrcy;

            if ((dy < 0) || ((dy == 0) && (dx < 0)))
            {
                for (j = 0; j < num_clips; j++)
                {
                    box = REGION_RECTS(&clip_reg)[j];
                    LLOGLN(10, ("  index %d x1 %d y1 %d x2 %d y2 %d", j,
                           box.x1, box.y1, box.x2, box.y2));
                    rdpup_set_clip(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
                    rdpup_screen_blt(ldstx, ldsty, w, h, lsrcx, lsrcy);
                }
            }
            else
            {
                for (j = num_clips - 1; j >= 0; j--)
                {
                    box = REGION_RECTS(&clip_reg)[j];
                    LLOGLN(10, ("  index %d x1 %d y1 %d x2 %d y2 %d", j,
                           box.x1, box.y1, box.x2, box.y2));
                    rdpup_set_clip(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
                    rdpup_screen_blt(ldstx, ldsty, w, h, lsrcx, lsrcy);
                }
            }

            rdpup_reset_clip();
            rdpup_end_update();
        }
    }

    RegionUninit(&clip_reg);
    rv = rdpCopyAreaOrg(&(pSrcWnd->drawable), &(pDstWnd->drawable),
                        pGC, srcx, srcy, w, h, dstx, dsty);
    return rv;
}

/******************************************************************************/
static RegionPtr
rdpCopyAreaWndToPixmap(WindowPtr pSrcWnd,
                       PixmapPtr pDstPixmap, rdpPixmapRec *pDstPriv,
                       GCPtr pGC, int srcx, int srcy, int w, int h,
                       int dstx, int dsty)
{
    int cd;
    int lsrcx;
    int lsrcy;
    int ldstx;
    int ldsty;
    int num_clips;
    int j;
    BoxRec box;
    RegionPtr rv;
    RegionRec clip_reg;

    LLOGLN(10, ("rdpCopyAreaWndToPixmap:"));
    RegionInit(&clip_reg, NullBox, 0);
    cd = rdp_get_clip(&clip_reg, &(pDstPixmap->drawable), pGC);
    lsrcx = pSrcWnd->drawable.x + srcx;
    lsrcy = pSrcWnd->drawable.y + srcy;
    ldstx = pDstPixmap->drawable.x + dstx;
    ldsty = pDstPixmap->drawable.y + dsty;

    if (cd == 1)
    {
        rdpup_switch_os_surface(pDstPriv->rdpindex);
        rdpup_begin_update();
        rdpup_screen_blt(ldstx, ldsty, w, h, lsrcx, lsrcy);
        rdpup_end_update();
        rdpup_switch_os_surface(-1);
    }
    else if (cd == 2)
    {
        num_clips = REGION_NUM_RECTS(&clip_reg);

        if (num_clips > 0)
        {
            rdpup_switch_os_surface(pDstPriv->rdpindex);
            rdpup_begin_update();

            for (j = 0; j < num_clips; j++)
            {
                box = REGION_RECTS(&clip_reg)[j];
                rdpup_set_clip(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
                rdpup_screen_blt(ldstx, ldsty, w, h, lsrcx, lsrcy);
            }

            rdpup_reset_clip();
            rdpup_end_update();
            rdpup_switch_os_surface(-1);
        }
    }

    RegionUninit(&clip_reg);
    rv = rdpCopyAreaOrg(&(pSrcWnd->drawable), &(pDstPixmap->drawable),
                        pGC, srcx, srcy, w, h, dstx, dsty);
    return rv;
}

/******************************************************************************/
/* draw from an off screen pixmap to a visible window */
static RegionPtr
rdpCopyAreaPixmapToWnd(PixmapPtr pSrcPixmap, rdpPixmapRec *pSrcPriv,
                       WindowPtr pDstWnd, GCPtr pGC,
                       int srcx, int srcy, int w, int h,
                       int dstx, int dsty)
{
    int lsrcx;
    int lsrcy;
    int ldstx;
    int ldsty;
    int cd;
    int j;
    int num_clips;
    RegionPtr rv;
    RegionRec clip_reg;
    BoxRec box;

    LLOGLN(10, ("rdpCopyAreaPixmapToWnd:"));
    RegionInit(&clip_reg, NullBox, 0);
    cd = rdp_get_clip(&clip_reg, &(pDstWnd->drawable), pGC);
    ldstx = pDstWnd->drawable.x + dstx;
    ldsty = pDstWnd->drawable.y + dsty;
    lsrcx = pSrcPixmap->drawable.x + srcx;
    lsrcy = pSrcPixmap->drawable.y + srcy;

    if (cd == 1)
    {
        rdpup_begin_update();
        rdpup_paint_rect_os(ldstx, ldsty, w, h, pSrcPriv->rdpindex, lsrcx, lsrcy);
        rdpup_end_update();
    }
    else if (cd == 2)
    {
        num_clips = REGION_NUM_RECTS(&clip_reg);

        if (num_clips > 0)
        {
            rdpup_begin_update();
            LLOGLN(10, ("rdpCopyAreaPixmapToWnd: num_clips %d", num_clips));

            for (j = 0; j < num_clips; j++)
            {
                box = REGION_RECTS(&clip_reg)[j];
                LLOGLN(10, ("rdpCopyAreaPixmapToWnd: %d %d %d %d", box.x1, box.y1, box.x2, box.y2));
                rdpup_set_clip(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
                LLOGLN(10, ("rdpCopyAreaPixmapToWnd: %d %d", w, h));
                rdpup_paint_rect_os(ldstx, ldsty, w, h, pSrcPriv->rdpindex, lsrcx, lsrcy);
            }

            rdpup_reset_clip();
            rdpup_end_update();
        }
    }

    RegionUninit(&clip_reg);
    rv = rdpCopyAreaOrg(&(pSrcPixmap->drawable), &(pDstWnd->drawable),
                        pGC, srcx, srcy, w, h, dstx, dsty);
    return rv;
}

/******************************************************************************/
/* draw from an off screen pixmap to an off screen pixmap */
static RegionPtr
rdpCopyAreaPixmapToPixmap(PixmapPtr pSrcPixmap, rdpPixmapRec *pSrcPriv,
                          PixmapPtr pDstPixmap, rdpPixmapRec *pDstPriv,
                          GCPtr pGC, int srcx, int srcy, int w, int h,
                          int dstx, int dsty)
{
    int lsrcx;
    int lsrcy;
    int ldstx;
    int ldsty;
    int cd;
    int j;
    int num_clips;
    int dx;
    int dy;
    RegionPtr rv;
    RegionRec clip_reg;
    BoxRec box;

    LLOGLN(10, ("rdpCopyAreaPixmapToPixmap:"));
    RegionInit(&clip_reg, NullBox, 0);
    cd = rdp_get_clip(&clip_reg, &(pDstPixmap->drawable), pGC);
    LLOGLN(10, ("rdpCopyAreaPixmapToPixmap: cd %d", cd));
    ldstx = pDstPixmap->drawable.x + dstx;
    ldsty = pDstPixmap->drawable.y + dsty;
    lsrcx = pSrcPixmap->drawable.x + srcx;
    lsrcy = pSrcPixmap->drawable.y + srcy;

    if (cd == 1)
    {
        rdpup_switch_os_surface(pDstPriv->rdpindex);
        rdpup_begin_update();
        rdpup_paint_rect_os(ldstx, ldsty, w, h, pSrcPriv->rdpindex, lsrcx, lsrcy);
        LLOGLN(10, ("%d %d %d %d %d %d", ldstx, ldsty, w, h, lsrcx, lsrcy));
        rdpup_end_update();
        rdpup_switch_os_surface(-1);
    }
    else if (cd == 2)
    {
        num_clips = REGION_NUM_RECTS(&clip_reg);

        if (num_clips > 0)
        {
            rdpup_switch_os_surface(pDstPriv->rdpindex);
            rdpup_begin_update();
            dx = ldstx - lsrcx;
            dy = ldsty - lsrcy;

            LLOGLN(10, ("rdpCopyAreaPixmapToPixmap: num_clips %d", num_clips));

            if ((dy < 0) || ((dy == 0) && (dx < 0)))
            {
                for (j = 0; j < num_clips; j++)
                {
                    box = REGION_RECTS(&clip_reg)[j];
                    rdpup_set_clip(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
                    rdpup_paint_rect_os(ldstx, ldsty, w, h, pSrcPriv->rdpindex, lsrcx, lsrcy);
                    LLOGLN(10, ("%d %d %d %d %d %d", ldstx, ldsty, w, h, lsrcx, lsrcy));
                }
            }
            else
            {
                for (j = num_clips - 1; j >= 0; j--)
                {
                    box = REGION_RECTS(&clip_reg)[j];
                    rdpup_set_clip(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
                    rdpup_paint_rect_os(ldstx, ldsty, w, h, pSrcPriv->rdpindex, lsrcx, lsrcy);
                    LLOGLN(10, ("%d %d %d %d %d %d", ldstx, ldsty, w, h, lsrcx, lsrcy));
                }
            }

            rdpup_reset_clip();
            rdpup_end_update();
            rdpup_switch_os_surface(-1);
        }
    }

    RegionUninit(&clip_reg);
    rv = rdpCopyAreaOrg(&(pSrcPixmap->drawable), &(pDstPixmap->drawable),
                        pGC, srcx, srcy, w, h, dstx, dsty);
    return rv;
}

/******************************************************************************/
RegionPtr
rdpCopyArea(DrawablePtr pSrc, DrawablePtr pDst, GCPtr pGC,
            int srcx, int srcy, int w, int h, int dstx, int dsty)
{
    RegionPtr rv;
    RegionRec clip_reg;
    RegionRec box_reg;
    RegionRec reg1;
    int num_clips;
    int cd;
    int j;
    int can_do_screen_blt;
    int got_id;
    int dirty_type;
    int post_process;
    int reset_surface;
    struct image_data id;
    BoxRec box;
    BoxPtr pbox;
    PixmapPtr pSrcPixmap;
    PixmapPtr pDstPixmap;
    rdpPixmapRec *pSrcPriv;
    rdpPixmapRec *pDstPriv;
    rdpPixmapRec *pDirtyPriv;
    WindowPtr pDstWnd;
    WindowPtr pSrcWnd;

    LLOGLN(10, ("rdpCopyArea:"));

    if (pSrc->type == DRAWABLE_WINDOW)
    {
        pSrcWnd = (WindowPtr)pSrc;

        if (pSrcWnd->viewable)
        {
            rdpup_check_dirty_screen(&g_screenPriv);

            if (pDst->type == DRAWABLE_WINDOW)
            {
                pDstWnd = (WindowPtr)pDst;

                if (pDstWnd->viewable)
                {
                    can_do_screen_blt = pGC->alu == GXcopy;

                    if (can_do_screen_blt)
                    {
                        return rdpCopyAreaWndToWnd(pSrcWnd, pDstWnd, pGC,
                                                   srcx, srcy, w, h, dstx, dsty);
                    }
                }
            }
            else if (pDst->type == DRAWABLE_PIXMAP)
            {
                pDstPixmap = (PixmapPtr)pDst;
                pDstPriv = GETPIXPRIV(pDstPixmap);

                if (xrdp_is_os(pDstPixmap, pDstPriv))
                {
                    can_do_screen_blt = pGC->alu == GXcopy;

                    if (can_do_screen_blt)
                    {
                        rdpup_check_dirty(pDstPixmap, pDstPriv);
                        return rdpCopyAreaWndToPixmap(pSrcWnd, pDstPixmap, pDstPriv, pGC,
                                                      srcx, srcy, w, h, dstx, dsty);
                    }
                }
            }
        }
    }

    if (pSrc->type == DRAWABLE_PIXMAP)
    {
        pSrcPixmap = (PixmapPtr)pSrc;
        pSrcPriv = GETPIXPRIV(pSrcPixmap);

        if (xrdp_is_os(pSrcPixmap, pSrcPriv))
        {
            if (pDst->type == DRAWABLE_WINDOW)
            {
                pDstWnd = (WindowPtr)pDst;

                if (pDstWnd->viewable)
                {
                    rdpup_check_dirty_screen(&g_screenPriv);
                    rdpup_check_dirty(pSrcPixmap, pSrcPriv);
                    return rdpCopyAreaPixmapToWnd(pSrcPixmap, pSrcPriv, pDstWnd, pGC,
                                                  srcx, srcy, w, h, dstx, dsty);
                }
            }
            else if (pDst->type == DRAWABLE_PIXMAP)
            {
                pDstPixmap = (PixmapPtr)pDst;
                pDstPriv = GETPIXPRIV(pDstPixmap);

                if (xrdp_is_os(pDstPixmap, pDstPriv))
                {
                    if (g_can_do_pix_to_pix)
                    {
                        rdpup_check_dirty(pSrcPixmap, pSrcPriv);
                        rdpup_check_dirty(pDstPixmap, pDstPriv);
                        return rdpCopyAreaPixmapToPixmap(pSrcPixmap, pSrcPriv,
                                                         pDstPixmap, pDstPriv,
                                                         pGC, srcx, srcy, w, h,
                                                         dstx, dsty);
                    }
                }
            }
        }
    }

    /* do original call */
    rv = rdpCopyAreaOrg(pSrc, pDst, pGC, srcx, srcy, w, h, dstx, dsty);

    dirty_type = 0;
    pDirtyPriv = 0;
    post_process = 0;
    reset_surface = 0;
    got_id = 0;

    if (pDst->type == DRAWABLE_PIXMAP)
    {
        pDstPixmap = (PixmapPtr)pDst;
        pDstPriv = GETPIXPRIV(pDstPixmap);

        if (xrdp_is_os(pDstPixmap, pDstPriv))
        {
            post_process = 1;

            if (g_do_dirty_os)
            {
                LLOGLN(10, ("rdpCopyArea: gettig dirty"));
                pDstPriv->is_dirty = 1;
                pDirtyPriv = pDstPriv;
                dirty_type = RDI_IMGLL;
            }
            else
            {
                rdpup_switch_os_surface(pDstPriv->rdpindex);
                reset_surface = 1;
                rdpup_get_pixmap_image_rect(pDstPixmap, &id);
                got_id = 1;
            }
        }
    }
    else
    {
        if (pDst->type == DRAWABLE_WINDOW)
        {
            pDstWnd = (WindowPtr)pDst;

            if (pDstWnd->viewable)
            {
                post_process = 1;

                if (g_do_dirty_ons)
                {
                    LLOGLN(0, ("rdpCopyArea: gettig dirty"));
                    g_screenPriv.is_dirty = 1;
                    pDirtyPriv = &g_screenPriv;
                    dirty_type = RDI_IMGLL;
                }
                else
                {
                    rdpup_get_screen_image_rect(&id);
                    got_id = 1;
                }
            }
        }
    }

    if (!post_process)
    {
        return rv;
    }

    RegionInit(&clip_reg, NullBox, 0);
    cd = rdp_get_clip(&clip_reg, pDst, pGC);

    if (cd == 1)
    {
        if (dirty_type != 0)
        {
            box.x1 = pDst->x + dstx;
            box.y1 = pDst->y + dsty;
            box.x2 = box.x1 + w;
            box.y2 = box.y1 + h;
            RegionInit(&reg1, &box, 0);
            draw_item_add_img_region(pDirtyPriv, &reg1, GXcopy, dirty_type);
            RegionUninit(&reg1);
        }
        else if (got_id)
        {
            rdpup_begin_update();
            rdpup_send_area(&id, pDst->x + dstx, pDst->y + dsty, w, h);
            rdpup_end_update();
        }
    }
    else if (cd == 2)
    {
        num_clips = REGION_NUM_RECTS(&clip_reg);

        if (num_clips > 0)
        {
            if (dirty_type != 0)
            {
                box.x1 = pDst->x + dstx;
                box.y1 = pDst->y + dsty;
                box.x2 = box.x1 + w;
                box.y2 = box.y1 + h;
                RegionInit(&box_reg, &box, 0);
                RegionIntersect(&clip_reg, &clip_reg, &box_reg);
                draw_item_add_img_region(pDirtyPriv, &clip_reg, GXcopy, dirty_type);
                RegionUninit(&box_reg);
            }
            else if (got_id)
            {
                rdpup_begin_update();
                box.x1 = pDst->x + dstx;
                box.y1 = pDst->y + dsty;
                box.x2 = box.x1 + w;
                box.y2 = box.y1 + h;
                RegionInit(&box_reg, &box, 0);
                RegionIntersect(&clip_reg, &clip_reg, &box_reg);
                num_clips = REGION_NUM_RECTS(&clip_reg);

                if (num_clips < 10)
                {
                    for (j = num_clips - 1; j >= 0; j--)
                    {
                        box = REGION_RECTS(&clip_reg)[j];
                        rdpup_send_area(&id, box.x1, box.y1, box.x2 - box.x1,
                                        box.y2 - box.y1);
                    }
                }
                else
                {
                    pbox = RegionExtents(&clip_reg);
                    rdpup_send_area(&id, pbox->x1, pbox->y1, pbox->x2 - pbox->x1,
                                    pbox->y2 - pbox->y1);
                }

                RegionUninit(&box_reg);
                rdpup_end_update();
            }
        }
    }

    RegionUninit(&clip_reg);

    if (reset_surface)
    {
        rdpup_switch_os_surface(-1);
    }

    return rv;
}
