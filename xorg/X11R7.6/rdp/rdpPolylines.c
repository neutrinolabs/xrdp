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
  int x1;
  int y1;
  int x2;
  int y2;
  int got_id;
  BoxRec box;
  DDXPointPtr ppts;
  struct image_data id;
  WindowPtr pDstWnd;
  PixmapPtr pDstPixmap;
  rdpPixmapRec* pDstPriv;

  LLOGLN(10, ("rdpPolylines:"));

  ppts = 0;
  if (npt > 0)
  {
    ppts = (DDXPointPtr)g_malloc(sizeof(DDXPointRec) * npt, 0);
    for (i = 0; i < npt; i++)
    {
      ppts[i] = pptInit[i];
    }
  }

  /* do original call */
  rdpPolylinesOrg(pDrawable, pGC, mode, npt, pptInit);

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
    g_free(ppts);
    return;
  }

  RegionInit(&clip_reg, NullBox, 0);
  cd = rdp_get_clip(&clip_reg, pDrawable, pGC);
  if (cd == 1)
  {
    if (ppts != 0)
    {
      rdpup_begin_update();
      rdpup_set_fgcolor(pGC->fgPixel);
      rdpup_set_opcode(pGC->alu);
      rdpup_set_pen(0, pGC->lineWidth);
      x1 = ppts[0].x + pDrawable->x;
      y1 = ppts[0].y + pDrawable->y;
      for (i = 1; i < npt; i++)
      {
        if (mode == CoordModeOrigin)
        {
          x2 = pDrawable->x + ppts[i].x;
          y2 = pDrawable->y + ppts[i].y;
        }
        else
        {
          x2 = x1 + ppts[i].x;
          y2 = y1 + ppts[i].y;
        }
        rdpup_draw_line(x1, y1, x2, y2);
        x1 = x2;
        y1 = y2;
      }
      rdpup_set_opcode(GXcopy);
      rdpup_end_update();
    }
  }
  else if (cd == 2)
  {
    num_clips = REGION_NUM_RECTS(&clip_reg);
    if (ppts != 0 && num_clips > 0)
    {
      rdpup_begin_update();
      rdpup_set_fgcolor(pGC->fgPixel);
      rdpup_set_opcode(pGC->alu);
      rdpup_set_pen(0, pGC->lineWidth);
      for (j = num_clips - 1; j >= 0; j--)
      {
        box = REGION_RECTS(&clip_reg)[j];
        rdpup_set_clip(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
        x1 = ppts[0].x + pDrawable->x;
        y1 = ppts[0].y + pDrawable->y;
        for (i = 1; i < npt; i++)
        {
          if (mode == CoordModeOrigin)
          {
            x2 = pDrawable->x + ppts[i].x;
            y2 = pDrawable->y + ppts[i].y;
          }
          else
          {
            x2 = x1 + ppts[i].x;
            y2 = y1 + ppts[i].y;
          }
          rdpup_draw_line(x1, y1, x2, y2);
          x1 = x2;
          y1 = y2;
        }
      }
      rdpup_reset_clip();
      rdpup_set_opcode(GXcopy);
      rdpup_end_update();
    }
  }
  RegionUninit(&clip_reg);
  g_free(ppts);
  rdpup_switch_os_surface(-1);
}
