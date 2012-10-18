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
static void
rdpPolylinesOrg(DrawablePtr pDrawable, GCPtr pGC, int mode,
                int npt, DDXPointPtr pptInit)
{
  rdpGCPtr priv;
  GCFuncs* oldFuncs;

  GC_OP_PROLOGUE(pGC);
  pGC->ops->Polylines(pDrawable, pGC, mode, npt, pptInit);
  GC_OP_EPILOGUE(pGC);
}

/******************************************************************************/
void
rdpPolylines(DrawablePtr pDrawable, GCPtr pGC, int mode,
             int npt, DDXPointPtr pptInit)
{
  RegionRec clip_reg;
  int num_clips;
  int cd;
  int i;
  int j;
  int got_id;
  int dirty_type;
  int post_process;
  int reset_surface;
  BoxRec box;
  xSegment* segs;
  int nseg;
  struct image_data id;
  WindowPtr pDstWnd;
  PixmapPtr pDstPixmap;
  rdpPixmapRec* pDstPriv;
  rdpPixmapRec* pDirtyPriv;

  LLOGLN(10, ("rdpPolylines:"));
  LLOGLN(10, ("  npt %d mode %d x %d y %d", npt, mode,
         pDrawable->x, pDrawable->y));
#if 0
  LLOGLN(0, ("  points"));
  for (i = 0; i < npt; i++)
  {
    LLOGLN(0, ("    %d %d", pptInit[i].x, pptInit[i].y));
  }
#endif
  /* convert lines to line segments */
  nseg = npt - 1;
  segs = 0;
  if (npt > 1)
  {
    segs = (xSegment*)g_malloc(sizeof(xSegment) * nseg, 0);
    segs[0].x1 = pptInit[0].x + pDrawable->x;
    segs[0].y1 = pptInit[0].y + pDrawable->y;
    if (mode == CoordModeOrigin)
    {
      segs[0].x2 = pptInit[1].x + pDrawable->x;
      segs[0].y2 = pptInit[1].y + pDrawable->y;
      for (i = 2; i < npt; i++)
      {
        segs[i - 1].x1 = segs[i - 2].x2;
        segs[i - 1].y1 = segs[i - 2].y2;
        segs[i - 1].x2 = pptInit[i].x + pDrawable->x;
        segs[i - 1].y2 = pptInit[i].y + pDrawable->y;
      }
    }
    else
    {
      segs[0].x2 = segs[0].x1 + pptInit[1].x;
      segs[0].y2 = segs[0].y1 + pptInit[1].y;
      for (i = 2; i < npt; i++)
      {
        segs[i - 1].x1 = segs[i - 2].x2;
        segs[i - 1].y1 = segs[i - 2].y2;
        segs[i - 1].x2 = segs[i - 1].x1 + pptInit[i].x;
        segs[i - 1].y2 = segs[i - 1].y1 + pptInit[i].y;
      }
    }
  }
  else
  {
    LLOGLN(0, ("rdpPolylines: weird npt [%d]", npt));
  }
#if 0
  LLOGLN(0, ("  segments"));
  for (i = 0; i < nseg; i++)
  {
    LLOGLN(0, ("    %d %d %d %d", segs[i].x1, segs[i].y1,
           segs[i].x2, segs[i].y2));
  }
#endif

  /* do original call */
  rdpPolylinesOrg(pDrawable, pGC, mode, npt, pptInit);

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
        LLOGLN(10, ("rdpPolylines: gettig dirty"));
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
          LLOGLN(0, ("rdpPolylines: gettig dirty"));
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
    g_free(segs);
    return;
  }

  RegionInit(&clip_reg, NullBox, 0);
  cd = rdp_get_clip(&clip_reg, pDrawable, pGC);
  if (cd == 1)
  {
    if (segs != 0)
    {
      if (dirty_type != 0)
      {
        RegionUninit(&clip_reg);
        RegionInit(&clip_reg, NullBox, 0);
        RegionAroundSegs(&clip_reg, segs, nseg);
        draw_item_add_line_region(pDirtyPriv, &clip_reg, pGC->fgPixel,
                                  pGC->alu, pGC->lineWidth, segs, nseg, 0);
      }
      else if (got_id)
      {
        rdpup_begin_update();
        rdpup_set_fgcolor(pGC->fgPixel);
        rdpup_set_opcode(pGC->alu);
        rdpup_set_pen(0, pGC->lineWidth);
        for (i = 0; i < nseg; i++)
        {
          rdpup_draw_line(segs[i].x1, segs[i].y1, segs[i].x2, segs[i].y2);
        }
        rdpup_set_opcode(GXcopy);
        rdpup_end_update();
      }
    }
  }
  else if (cd == 2)
  {
    num_clips = REGION_NUM_RECTS(&clip_reg);
    if (nseg != 0 && num_clips > 0)
    {
      if (dirty_type != 0)
      {
        draw_item_add_line_region(pDirtyPriv, &clip_reg, pGC->fgPixel,
                                  pGC->alu, pGC->lineWidth, segs, nseg, 0);
      }
      else if (got_id)
      {
        rdpup_begin_update();
        rdpup_set_fgcolor(pGC->fgPixel);
        rdpup_set_opcode(pGC->alu);
        rdpup_set_pen(0, pGC->lineWidth);
        for (j = num_clips - 1; j >= 0; j--)
        {
          box = REGION_RECTS(&clip_reg)[j];
          rdpup_set_clip(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
          for (i = 0; i < nseg; i++)
          {
            rdpup_draw_line(segs[i].x1, segs[i].y1, segs[i].x2, segs[i].y2);
          }
        }
        rdpup_reset_clip();
        rdpup_set_opcode(GXcopy);
        rdpup_end_update();
      }
    }
  }
  g_free(segs);
  RegionUninit(&clip_reg);
  if (reset_surface)
  {
    rdpup_switch_os_surface(-1);
  }
}
