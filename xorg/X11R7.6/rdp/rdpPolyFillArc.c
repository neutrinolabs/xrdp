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
rdpPolyFillArcOrg(DrawablePtr pDrawable, GCPtr pGC, int narcs, xArc* parcs)
{
  rdpGCPtr priv;
  GCFuncs* oldFuncs;

  GC_OP_PROLOGUE(pGC);
  pGC->ops->PolyFillArc(pDrawable, pGC, narcs, parcs);
  GC_OP_EPILOGUE(pGC);
}

/******************************************************************************/
void
rdpPolyFillArc(DrawablePtr pDrawable, GCPtr pGC, int narcs, xArc* parcs)
{
  RegionRec clip_reg;
  RegionPtr tmpRegion;
  int cd;
  int lw;
  int extra;
  int i;
  int num_clips;
  int got_id;
  int dirty_type;
  int post_process;
  int reset_surface;
  xRectangle* rects;
  BoxRec box;
  struct image_data id;
  WindowPtr pDstWnd;
  PixmapPtr pDstPixmap;
  rdpPixmapRec* pDstPriv;
  rdpPixmapRec* pDirtyPriv;

  LLOGLN(10, ("rdpPolyFillArc:"));

  rects = 0;
  if (narcs > 0)
  {
    rects = (xRectangle*)g_malloc(narcs * sizeof(xRectangle), 0);
    lw = pGC->lineWidth;
    if (lw == 0)
    {
      lw = 1;
    }
    extra = lw / 2;
    for (i = 0; i < narcs; i++)
    {
      rects[i].x = (parcs[i].x - extra) + pDrawable->x;
      rects[i].y = (parcs[i].y - extra) + pDrawable->y;
      rects[i].width = parcs[i].width + lw;
      rects[i].height = parcs[i].height + lw;
    }
  }

  /* do original call */
  rdpPolyFillArcOrg(pDrawable, pGC, narcs, parcs);

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
        LLOGLN(10, ("rdpPolyFillArc: gettig dirty"));
        pDstPriv->is_dirty = 1;
        pDirtyPriv = pDstPriv;
        dirty_type = RDI_IMGLY;
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
          LLOGLN(0, ("rdpPolyFillArc: gettig dirty"));
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
    g_free(rects);
    return;
  }

  RegionInit(&clip_reg, NullBox, 0);
  cd = rdp_get_clip(&clip_reg, pDrawable, pGC);
  if (cd == 1)
  {
    if (rects != 0)
    {
      tmpRegion = RegionFromRects(narcs, rects, CT_NONE);
      num_clips = REGION_NUM_RECTS(tmpRegion);
      if (num_clips > 0)
      {
        if (dirty_type != 0)
        {
          draw_item_add_img_region(pDirtyPriv, tmpRegion, GXcopy, dirty_type);
        }
        else if (got_id)
        {
          rdpup_begin_update();
          for (i = num_clips - 1; i >= 0; i--)
          {
            box = REGION_RECTS(tmpRegion)[i];
            rdpup_send_area(&id, box.x1, box.y1, box.x2 - box.x1,
                            box.y2 - box.y1);
          }
          rdpup_end_update();
        }
      }
      RegionDestroy(tmpRegion);
    }
  }
  else if (cd == 2)
  {
    if (rects != 0)
    {
      tmpRegion = RegionFromRects(narcs, rects, CT_NONE);
      RegionIntersect(tmpRegion, tmpRegion, &clip_reg);
      num_clips = REGION_NUM_RECTS(tmpRegion);
      if (num_clips > 0)
      {
        if (dirty_type != 0)
        {
          draw_item_add_img_region(pDirtyPriv, tmpRegion, GXcopy, dirty_type);
        }
        else if (got_id)
        {
          rdpup_begin_update();
          for (i = num_clips - 1; i >= 0; i--)
          {
            box = REGION_RECTS(tmpRegion)[i];
            rdpup_send_area(&id, box.x1, box.y1, box.x2 - box.x1,
                            box.y2 - box.y1);
          }
          rdpup_end_update();
        }
      }
      RegionDestroy(tmpRegion);
    }
  }
  RegionUninit(&clip_reg);
  g_free(rects);
  if (reset_surface)
  {
    rdpup_switch_os_surface(-1);
  }
}
