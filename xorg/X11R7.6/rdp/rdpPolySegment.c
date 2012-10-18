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
rdpPolySegmentOrg(DrawablePtr pDrawable, GCPtr pGC, int nseg, xSegment* pSegs)
{
  rdpGCPtr priv;
  GCFuncs* oldFuncs;

  GC_OP_PROLOGUE(pGC);
  pGC->ops->PolySegment(pDrawable, pGC, nseg, pSegs);
  GC_OP_EPILOGUE(pGC);
}

/******************************************************************************/
void
rdpPolySegment(DrawablePtr pDrawable, GCPtr pGC, int nseg, xSegment* pSegs)
{
  RegionRec clip_reg;
  int cd;
  int i;
  int j;
  int got_id;
  int dirty_type;
  int post_process;
  int reset_surface;
  xSegment* segs;
  BoxRec box;
  struct image_data id;
  WindowPtr pDstWnd;
  PixmapPtr pDstPixmap;
  rdpPixmapRec* pDstPriv;
  rdpPixmapRec* pDirtyPriv;

  LLOGLN(10, ("rdpPolySegment:"));
  LLOGLN(10, ("  nseg %d", nseg));

  segs = 0;
  if (nseg) /* get the rects */
  {
    segs = (xSegment*)g_malloc(nseg * sizeof(xSegment), 0);
    for (i = 0; i < nseg; i++)
    {
      segs[i].x1 = pSegs[i].x1 + pDrawable->x;
      segs[i].y1 = pSegs[i].y1 + pDrawable->y;
      segs[i].x2 = pSegs[i].x2 + pDrawable->x;
      segs[i].y2 = pSegs[i].y2 + pDrawable->y;
    }
  }

  /* do original call */
  rdpPolySegmentOrg(pDrawable, pGC, nseg, pSegs);

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
        LLOGLN(10, ("rdpPolySegment: gettig dirty"));
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
          LLOGLN(0, ("rdpPolySegment: gettig dirty"));
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
  LLOGLN(10, ("rdpPolySegment: cd %d", cd));
  if (cd == 1) /* no clip */
  {
    if (segs != 0)
    {
      if (dirty_type != 0)
      {
        RegionUninit(&clip_reg);
        RegionInit(&clip_reg, NullBox, 0);
        RegionAroundSegs(&clip_reg, segs, nseg);
        draw_item_add_line_region(pDirtyPriv, &clip_reg, pGC->fgPixel,
                                  pGC->alu, pGC->lineWidth, segs, nseg, 1);
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
  else if (cd == 2) /* clip */
  {
    if (segs != 0)
    {
      if (dirty_type != 0)
      {
        draw_item_add_line_region(pDirtyPriv, &clip_reg, pGC->fgPixel,
                                  pGC->alu, pGC->lineWidth, segs, nseg, 1);
      }
      else if (got_id)
      {
        rdpup_begin_update();
        rdpup_set_fgcolor(pGC->fgPixel);
        rdpup_set_opcode(pGC->alu);
        rdpup_set_pen(0, pGC->lineWidth);
        for (j = REGION_NUM_RECTS(&clip_reg) - 1; j >= 0; j--)
        {
          box = REGION_RECTS(&clip_reg)[j];
          rdpup_set_clip(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
          for (i = 0; i < nseg; i++)
          {
            rdpup_draw_line(segs[i].x1, segs[i].y1, segs[i].x2, segs[i].y2);
            LLOGLN(10, ("  %d %d %d %d", segs[i].x1, segs[i].y1,
                   segs[i].x2, segs[i].y2));
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
