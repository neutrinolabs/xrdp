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
rdpFillPolygonOrg(DrawablePtr pDrawable, GCPtr pGC,
                  int shape, int mode, int count,
                  DDXPointPtr pPts)
{
  rdpGCPtr priv;
  GCFuncs* oldFuncs;

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
  RegionRec clip_reg;
  RegionRec box_reg;
  RegionRec reg1;
  int num_clips;
  int cd;
  int maxx;
  int maxy;
  int minx;
  int miny;
  int i;
  int j;
  int got_id;
  int dirty_type;
  int post_process;
  int reset_surface;
  BoxRec box;
  struct image_data id;
  WindowPtr pDstWnd;
  PixmapPtr pDstPixmap;
  rdpPixmapRec* pDstPriv;
  rdpPixmapRec* pDirtyPriv;

  LLOGLN(10, ("rdpFillPolygon:"));

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
    for (i = 1; i < count; i++)
    {
      if (pPts[i].x > maxx)
      {
        maxx = pPts[i].x;
      }
      if (pPts[i].x < minx)
      {
        minx = pPts[i].x;
      }
      if (pPts[i].y > maxy)
      {
        maxy = pPts[i].y;
      }
      if (pPts[i].y < miny)
      {
        miny = pPts[i].y;
      }
    }
    box.x1 = pDrawable->x + minx;
    box.y1 = pDrawable->y + miny;
    box.x2 = pDrawable->x + maxx + 1;
    box.y2 = pDrawable->y + maxy + 1;
  }

  /* do original call */
  rdpFillPolygonOrg(pDrawable, pGC, shape, mode, count, pPts);

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
        LLOGLN(10, ("rdpFillPolygon: gettig dirty"));
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
          LLOGLN(0, ("rdpFillPolygon: gettig dirty"));
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
    if (dirty_type != 0)
    {
      RegionInit(&reg1, &box, 0);
      draw_item_add_img_region(pDirtyPriv, &reg1, GXcopy, dirty_type);
      RegionUninit(&reg1);
    }
    else if (got_id)
    {
      rdpup_begin_update();
      rdpup_send_area(&id, box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
      rdpup_end_update();
    }
  }
  else if (cd == 2)
  {
    RegionInit(&box_reg, &box, 0);
    RegionIntersect(&clip_reg, &clip_reg, &box_reg);
    num_clips = REGION_NUM_RECTS(&clip_reg);
    if (num_clips > 0)
    {
      if (dirty_type != 0)
      {
        draw_item_add_img_region(pDirtyPriv, &clip_reg, GXcopy, dirty_type);
      }
      else if (got_id)
      {
        rdpup_begin_update();
        for (j = num_clips - 1; j >= 0; j--)
        {
          box = REGION_RECTS(&clip_reg)[j];
          rdpup_send_area(&id, box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
        }
        rdpup_end_update();
      }
    }
    RegionUninit(&box_reg);
  }
  RegionUninit(&clip_reg);
  if (reset_surface)
  {
    rdpup_switch_os_surface(-1);
  }
}
