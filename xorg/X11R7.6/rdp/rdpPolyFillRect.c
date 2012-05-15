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

extern GCOps g_rdpGCOps; /* from rdpdraw.c */

extern int g_con_number; /* in rdpup.c */

/******************************************************************************/
static void
rdpPolyFillRectOrg(DrawablePtr pDrawable, GCPtr pGC, int nrectFill,
                   xRectangle* prectInit)
{
  rdpGCPtr priv;
  GCFuncs* oldFuncs;

  GC_OP_PROLOGUE(pGC);
  pGC->ops->PolyFillRect(pDrawable, pGC, nrectFill, prectInit);
  GC_OP_EPILOGUE(pGC);
}

/******************************************************************************/
void
rdpPolyFillRect(DrawablePtr pDrawable, GCPtr pGC, int nrectFill,
                xRectangle* prectInit)
{
  int j;
  int cd;
  int num_clips;
  RegionRec clip_reg;
  RegionPtr fill_reg;
  BoxRec box;

  int got_id;
  struct image_data id;
  WindowPtr pDstWnd;
  PixmapPtr pDstPixmap;
  rdpPixmapRec* pDstPriv;

  LLOGLN(10, ("rdpPolyFillRect:"));

  /* make a copy of rects */
  fill_reg = RegionFromRects(nrectFill, prectInit, CT_NONE);

  /* do original call */
  rdpPolyFillRectOrg(pDrawable, pGC, nrectFill, prectInit);

  got_id = 0;
  if (pDrawable->type == DRAWABLE_PIXMAP)
  {
    pDstPixmap = (PixmapPtr)pDrawable;
    pDstPriv = GETPIXPRIV(pDstPixmap);
    if (XRDP_IS_OS(pDstPriv))
    {
      rdpup_switch_os_surface(pDstPriv->rdpid);
      rdpup_get_pixmap_image_rect(pDstPixmap, &id);
      got_id = 1;
    }
  }
  else
  {
    if (pDrawable->type == DRAWABLE_WINDOW)
    {
      pDstWnd = (WindowPtr)pDrawable;
      if (pDstWnd->viewable)
      {
        rdpup_get_screen_image_rect(&id);
        got_id = 1;
      }
    }
  }
  if (!got_id)
  {
    RegionDestroy(fill_reg);
    return;
  }
  RegionTranslate(fill_reg, pDrawable->x, pDrawable->y);
  RegionInit(&clip_reg, NullBox, 0);
  cd = rdp_get_clip(&clip_reg, pDrawable, pGC);
  if (cd == 1) /* no clip */
  {
    rdpup_begin_update();
    if (pGC->fillStyle == 0 && /* solid fill */
        (pGC->alu == GXclear ||
         pGC->alu == GXset ||
         pGC->alu == GXinvert ||
         pGC->alu == GXnoop ||
         pGC->alu == GXand ||
         pGC->alu == GXcopy /*||
         pGC->alu == GXxor*/)) /* todo, why dosen't xor work? */
    {
      rdpup_set_fgcolor(pGC->fgPixel);
      rdpup_set_opcode(pGC->alu);
      for (j = REGION_NUM_RECTS(fill_reg) - 1; j >= 0; j--)
      {
        box = REGION_RECTS(fill_reg)[j];
        rdpup_fill_rect(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
      }
      rdpup_set_opcode(GXcopy);
    }
    else /* non solid fill */
    {
      for (j = REGION_NUM_RECTS(fill_reg) - 1; j >= 0; j--)
      {
        box = REGION_RECTS(fill_reg)[j];
        rdpup_send_area(&id, box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
      }
    }
    rdpup_end_update();
  }
  else if (cd == 2) /* clip */
  {
    RegionIntersect(&clip_reg, &clip_reg, fill_reg);
    num_clips = REGION_NUM_RECTS(&clip_reg);
    if (num_clips > 0)
    {
      rdpup_begin_update();
      if (pGC->fillStyle == 0 && /* solid fill */
          (pGC->alu == GXclear ||
           pGC->alu == GXset ||
           pGC->alu == GXinvert ||
           pGC->alu == GXnoop ||
           pGC->alu == GXand ||
           pGC->alu == GXcopy /*||
           pGC->alu == GXxor*/)) /* todo, why dosen't xor work? */
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
      else /* non solid fill */
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
  RegionUninit(&clip_reg);
  RegionDestroy(fill_reg);
  rdpup_switch_os_surface(-1);
}
