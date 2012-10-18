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
extern int g_do_dirty_os; /* in rdpmain.c */
extern int g_do_dirty_ons; /* in rdpmain.c */
extern rdpPixmapRec g_screenPriv; /* in rdpmain.c */

extern GCOps g_rdpGCOps; /* from rdpdraw.c */

extern int g_con_number; /* in rdpup.c */

/******************************************************************************/
void
rdpPolyPointOrg(DrawablePtr pDrawable, GCPtr pGC, int mode,
                int npt, DDXPointPtr in_pts)
{
  rdpGCPtr priv;
  GCFuncs* oldFuncs;

  GC_OP_PROLOGUE(pGC);
  pGC->ops->PolyPoint(pDrawable, pGC, mode, npt, in_pts);
  GC_OP_EPILOGUE(pGC);
}

/******************************************************************************/
void
rdpPolyPoint(DrawablePtr pDrawable, GCPtr pGC, int mode,
             int npt, DDXPointPtr in_pts)
{
  RegionRec clip_reg;
  RegionRec reg1;
  RegionRec reg2;
  int num_clips;
  int cd;
  int x;
  int y;
  int i;
  int j;
  int got_id;
  int dirty_type;
  int post_process;
  int reset_surface;
  BoxRec box;
  BoxRec total_box;
  DDXPointPtr pts;
  DDXPointRec stack_pts[32];
  struct image_data id;
  WindowPtr pDstWnd;
  PixmapPtr pDstPixmap;
  rdpPixmapRec* pDstPriv;
  rdpPixmapRec* pDirtyPriv;

  LLOGLN(10, ("rdpPolyPoint:"));
  LLOGLN(10, ("rdpPolyPoint:  npt %d", npt));

  if (npt > 32)
  {
    pts = (DDXPointPtr)g_malloc(sizeof(DDXPointRec) * npt, 0);
  }
  else
  {
    pts = stack_pts;
  }
  for (i = 0; i < npt; i++)
  {
    pts[i].x = pDrawable->x + in_pts[i].x;
    pts[i].y = pDrawable->y + in_pts[i].y;
    if (i == 0)
    {
      total_box.x1 = pts[0].x;
      total_box.y1 = pts[0].y;
      total_box.x2 = pts[0].x;
      total_box.y2 = pts[0].y;
    }
    else
    {
      if (pts[i].x < total_box.x1)
      {
        total_box.x1 = pts[i].x;
      }
      if (pts[i].y < total_box.y1)
      {
        total_box.y1 = pts[i].y;
      }
      if (pts[i].x > total_box.x2)
      {
        total_box.x2 = pts[i].x;
      }
      if (pts[i].y > total_box.y2)
      {
        total_box.y2 = pts[i].y;
      }
    }
    /* todo, use this total_box */
  }

  /* do original call */
  rdpPolyPointOrg(pDrawable, pGC, mode, npt, in_pts);

  dirty_type = 0;
  pDirtyPriv = 0;
  post_process = 0;
  reset_surface = 0;
  got_id = 0;
  if (pDrawable->type == DRAWABLE_PIXMAP)
  {
    pDstPixmap = (PixmapPtr)pDrawable;
    pDstPriv = GETPIXPRIV(pDstPixmap);
    if (XRDP_IS_OS(pDstPriv))
    {
      post_process = 1;
      if (g_do_dirty_os)
      {
        LLOGLN(10, ("rdpPolyPoint: gettig dirty"));
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
    if (pDrawable->type == DRAWABLE_WINDOW)
    {
      pDstWnd = (WindowPtr)pDrawable;
      if (pDstWnd->viewable)
      {
        post_process = 1;
        if (g_do_dirty_ons)
        {
          LLOGLN(0, ("rdpPolyPoint: gettig dirty"));
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
    return;
  }

  RegionInit(&clip_reg, NullBox, 0);
  cd = rdp_get_clip(&clip_reg, pDrawable, pGC);
  if (cd == 1)
  {
    if (npt > 0)
    {
      if (dirty_type != 0)
      {
        RegionInit(&reg1, NullBox, 0);
        for (i = 0; i < npt; i++)
        {
          box.x1 = pts[i].x;
          box.y1 = pts[i].y;
          box.x2 = box.x1 + 1;
          box.y2 = box.y1 + 1;
          RegionInit(&reg2, &box, 0);
          RegionUnion(&reg1, &reg1, &reg2);
          RegionUninit(&reg2);
        }
        draw_item_add_fill_region(pDirtyPriv, &reg1, pGC->fgPixel,
                                  pGC->alu);
        RegionUninit(&reg1);
      }
      else if (got_id)
      {
        rdpup_begin_update();
        rdpup_set_fgcolor(pGC->fgPixel);
        for (i = 0; i < npt; i++)
        {
          x = pts[i].x;
          y = pts[i].y;
          rdpup_fill_rect(x, y, 1, 1);
        }
        rdpup_end_update();
      }
    }
  }
  else if (cd == 2)
  {
    num_clips = REGION_NUM_RECTS(&clip_reg);
    if (npt > 0 && num_clips > 0)
    {
      if (dirty_type != 0)
      {
        RegionInit(&reg1, NullBox, 0);
        for (i = 0; i < npt; i++)
        {
          box.x1 = pts[i].x;
          box.y1 = pts[i].y;
          box.x2 = box.x1 + 1;
          box.y2 = box.y1 + 1;
          RegionInit(&reg2, &box, 0);
          RegionUnion(&reg1, &reg1, &reg2);
          RegionUninit(&reg2);
        }
        RegionIntersect(&reg1, &reg1, &clip_reg);
        draw_item_add_fill_region(pDirtyPriv, &reg1, pGC->fgPixel,
                                  pGC->alu);
        RegionUninit(&reg1);
      }
      else if (got_id)
      {
        rdpup_begin_update();
        rdpup_set_fgcolor(pGC->fgPixel);
        for (j = num_clips - 1; j >= 0; j--)
        {
          box = REGION_RECTS(&clip_reg)[j];
          rdpup_set_clip(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
          for (i = 0; i < npt; i++)
          {
            x = pts[i].x;
            y = pts[i].y;
            rdpup_fill_rect(x, y, 1, 1);
          }
        }
        rdpup_reset_clip();
        rdpup_end_update();
      }
    }
  }
  RegionUninit(&clip_reg);
  if (pts != stack_pts)
  {
    g_free(pts);
  }
  if (reset_surface)
  {
    rdpup_switch_os_surface(-1);
  }
}
