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
rdpPolyRectangleOrg(DrawablePtr pDrawable, GCPtr pGC, int nrects,
                    xRectangle* rects)
{
  rdpGCPtr priv;
  GCFuncs* oldFuncs;

  GC_OP_PROLOGUE(pGC);
  pGC->ops->PolyRectangle(pDrawable, pGC, nrects, rects);
  GC_OP_EPILOGUE(pGC);
}

/******************************************************************************/
/* tested with pGC->lineWidth = 0, 1, 2, 4 and opcodes 3 and 6 */
void
rdpPolyRectangle(DrawablePtr pDrawable, GCPtr pGC, int nrects,
                 xRectangle* rects)
{
  RegionRec clip_reg;
  RegionPtr fill_reg;
  int num_clips;
  int cd;
  int lw;
  int i;
  int j;
  int up;
  int down;
  int got_id;
  int dirty_type;
  int post_process;
  int reset_surface;
  xRectangle* regRects;
  xRectangle* r;
  xRectangle* rect1;
  BoxRec box;
  struct image_data id;

  WindowPtr pDstWnd;
  PixmapPtr pDstPixmap;
  rdpPixmapRec* pDstPriv;
  rdpPixmapRec* pDirtyPriv;

  LLOGLN(10, ("rdpPolyRectangle:"));

  /* make a copy of rects */
  rect1 = (xRectangle*)g_malloc(sizeof(xRectangle) * nrects, 0);
  for (i = 0; i < nrects; i++)
  {
    rect1[i] = rects[i];
  }

  /* do original call */
  rdpPolyRectangleOrg(pDrawable, pGC, nrects, rects);

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
        LLOGLN(10, ("rdpPolyRectangle: gettig dirty"));
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
          LLOGLN(0, ("rdpPolyRectangle: gettig dirty"));
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
    g_free(rect1);
    return;
  }

  RegionInit(&clip_reg, NullBox, 0);
  cd = rdp_get_clip(&clip_reg, pDrawable, pGC);
  regRects = 0;
  if ((cd != 0) && (nrects > 0))
  {
    regRects = (xRectangle*)g_malloc(nrects * 4 * sizeof(xRectangle), 0);
    lw = pGC->lineWidth;
    if (lw < 1)
    {
      lw = 1;
    }
    up = lw / 2;
    down = 1 + (lw - 1) / 2;
    for (i = 0; i < nrects; i++)
    {
      r = regRects + i * 4;
      r->x = (rect1[i].x + pDrawable->x) - up;
      r->y = (rect1[i].y + pDrawable->y) - up;
      r->width = rect1[i].width + up + down;
      r->height = lw;
      r++;
      r->x = (rect1[i].x + pDrawable->x) - up;
      r->y = (rect1[i].y + pDrawable->y) + down;
      r->width = lw;
      r->height = MAX(rect1[i].height - (up + down), 0);
      r++;
      r->x = ((rect1[i].x + rect1[i].width) + pDrawable->x) - up;
      r->y = (rect1[i].y + pDrawable->y) + down;
      r->width = lw;
      r->height = MAX(rect1[i].height - (up + down), 0);
      r++;
      r->x = (rect1[i].x + pDrawable->x) - up;
      r->y = ((rect1[i].y + rect1[i].height) + pDrawable->y) - up;
      r->width = rect1[i].width + up + down;
      r->height = lw;
    }
  }
  if (cd == 1)
  {
    if (regRects != 0)
    {
      if (dirty_type != 0)
      {
        fill_reg = RegionFromRects(nrects * 4, regRects, CT_NONE);
        if (pGC->lineStyle == LineSolid)
        {
          draw_item_add_fill_region(pDirtyPriv, fill_reg, pGC->fgPixel,
                                    pGC->alu);
        }
        else
        {
          draw_item_add_img_region(pDirtyPriv, fill_reg, GXcopy, dirty_type);
        }
        RegionDestroy(fill_reg);
      }
      else if (got_id)
      {
        rdpup_begin_update();
        if (pGC->lineStyle == LineSolid)
        {
          rdpup_set_fgcolor(pGC->fgPixel);
          rdpup_set_opcode(pGC->alu);
          for (i = 0; i < nrects * 4; i++)
          {
            r = regRects + i;
            rdpup_fill_rect(r->x, r->y, r->width, r->height);
          }
          rdpup_set_opcode(GXcopy);
        }
        else
        {
          for (i = 0; i < nrects * 4; i++)
          {
            r = regRects + i;
            rdpup_send_area(&id, r->x, r->y, r->width, r->height);
          }
        }
        rdpup_end_update();
      }
    }
  }
  else if (cd == 2)
  {
    if (regRects != 0)
    {
      fill_reg = RegionFromRects(nrects * 4, regRects, CT_NONE);
      RegionIntersect(&clip_reg, &clip_reg, fill_reg);
      num_clips = REGION_NUM_RECTS(&clip_reg);
      if (num_clips > 0)
      {
        if (dirty_type != 0)
        {
          if (pGC->lineStyle == LineSolid)
          {
            draw_item_add_fill_region(pDirtyPriv, &clip_reg, pGC->fgPixel,
                                      pGC->alu);
          }
          else
          {
            draw_item_add_img_region(pDirtyPriv, &clip_reg, GXcopy, dirty_type);
          }
        }
        else if (got_id)
        {
          rdpup_begin_update();
          if (pGC->lineStyle == LineSolid)
          {
            rdpup_set_fgcolor(pGC->fgPixel);
            rdpup_set_opcode(pGC->alu);
            for (j = num_clips - 1; j >= 0; j--)
            {
              box = REGION_RECTS(&clip_reg)[j];
              rdpup_fill_rect(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
            }
            rdpup_set_opcode(GXcopy);
          }
          else
          {
            for (j = num_clips - 1; j >= 0; j--)
            {
              box = REGION_RECTS(&clip_reg)[j];
              rdpup_send_area(&id, box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
            }
          }
          rdpup_end_update();
        }
      }
      RegionDestroy(fill_reg);
    }
  }
  RegionUninit(&clip_reg);
  g_free(regRects);
  g_free(rect1);
  if (reset_surface)
  {
    rdpup_switch_os_surface(-1);
  }
}
