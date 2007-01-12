/*
Copyright 2005-2007 Jay Sorg

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

Xserver drawing ops and funcs

*/

#include "rdp.h"
#include "gcops.h"

#define DEBUG_OUT_FUNCS(arg)
#define DEBUG_OUT_OPS(arg)
/*#define DEBUG_OUT_FUNCS(arg) ErrorF arg*/
/*#define DEBUG_OUT_OPS(arg) ErrorF arg*/

#if 0
int atemp = 0;
char atext[256];

#define DEBUG_OUT_OPS(arg) \
{ \
  if (atemp == 0) \
  { \
    g_file_delete("/home/jj/atemp.log"); \
    atemp = g_file_open("/home/jj/atemp.log"); \
    g_set_file_rights("/home/jj/atemp.log", 1, 1); \
  } \
  g_sprintf(atext, arg ); \
  g_file_write(atemp, atext, g_strlen(atext)); \
}
#endif

#if 0
int atemp = 0;
char atext[256];
#define DEBUG_OUT_OPS1(arg) \
{ \
  if (atemp == 0) \
  { \
    g_file_delete("/home/jj/atemp.log"); \
    atemp = g_file_open("/home/jj/atemp.log"); \
    g_set_file_rights("/home/jj/atemp.log", 1, 1); \
  } \
  g_sprintf arg ; \
  g_file_write(atemp, atext, g_strlen(atext)); \
}
#endif

extern rdpScreenInfo g_rdpScreen; /* from rdpmain.c */
extern int g_rdpGCIndex; /* from rdpmain.c */
extern int g_Bpp; /* from rdpmain.c */
extern ScreenPtr g_pScreen; /* from rdpmain.c */

ColormapPtr g_rdpInstalledColormap;

static GCFuncs g_rdpGCFuncs =
{
  rdpValidateGC, rdpChangeGC, rdpCopyGC, rdpDestroyGC, rdpChangeClip,
  rdpDestroyClip, rdpCopyClip
};

static GCOps g_rdpGCOps =
{
  rdpFillSpans, rdpSetSpans, rdpPutImage, rdpCopyArea, rdpCopyPlane,
  rdpPolyPoint, rdpPolylines, rdpPolySegment, rdpPolyRectangle,
  rdpPolyArc, rdpFillPolygon, rdpPolyFillRect, rdpPolyFillArc,
  rdpPolyText8, rdpPolyText16, rdpImageText8, rdpImageText16,
  rdpImageGlyphBlt, rdpPolyGlyphBlt, rdpPushPixels
};

/******************************************************************************/
/* return 0, draw nothing */
/* return 1, draw with no clip */
/* return 2, draw using clip */
static int
rdp_get_clip(RegionPtr pRegion, DrawablePtr pDrawable, GCPtr pGC)
{
  WindowPtr pWindow;
  RegionPtr temp;
  BoxRec box;
  int rv;

  rv = 0;
  if (pDrawable->type == DRAWABLE_WINDOW)
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
      if (miRegionNotEmpty(temp))
      {
        switch (pGC->clientClipType)
        {
          case CT_NONE:
            rv = 2;
            miRegionCopy(pRegion, temp);
            break;
          case CT_REGION:
            rv = 2;
            miRegionCopy(pRegion, pGC->clientClip);
            miTranslateRegion(pRegion,
                              pDrawable->x + pGC->clipOrg.x,
                              pDrawable->y + pGC->clipOrg.y);
            miIntersect(pRegion, pRegion, temp);
            break;
          default:
            ErrorF("unimp clip type %d\n", pGC->clientClipType);
            break;
        }
        if (rv == 2) /* check if the clip is the entire screen */
        {
          box.x1 = 0;
          box.y1 = 0;
          box.x2 = g_rdpScreen.width;
          box.y2 = g_rdpScreen.height;
          if (miRectIn(pRegion, &box) == rgnIN)
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
static void
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
#define GC_FUNC_PROLOGUE(_pGC) \
{ \
  priv = (rdpGCPtr)(_pGC->devPrivates[g_rdpGCIndex].ptr); \
  (_pGC)->funcs = priv->funcs; \
  if (priv->ops != 0) \
  { \
    (_pGC)->ops = priv->ops; \
  } \
}

/******************************************************************************/
#define GC_FUNC_EPILOGUE(_pGC) \
{ \
  priv->funcs = (_pGC)->funcs; \
  (_pGC)->funcs = &g_rdpGCFuncs; \
  if (priv->ops != 0) \
  { \
    priv->ops = (_pGC)->ops; \
    (_pGC)->ops = &g_rdpGCOps; \
  } \
}

/******************************************************************************/
static void
rdpValidateGC(GCPtr pGC, unsigned long changes, DrawablePtr d)
{
  rdpGCRec* priv;
  int viewable;
  RegionPtr pRegion;

  DEBUG_OUT_FUNCS(("in rdpValidateGC\n"));
  GC_FUNC_PROLOGUE(pGC);
  pGC->funcs->ValidateGC(pGC, changes, d);
  viewable = d->type == DRAWABLE_WINDOW && ((WindowPtr)d)->viewable;
  if (viewable)
  {
    if (pGC->subWindowMode == IncludeInferiors)
    {
      pRegion = &(((WindowPtr)d)->borderClip);
    }
    else
    {
      pRegion = &(((WindowPtr)d)->clipList);
    }
    viewable = miRegionNotEmpty(pRegion);
  }
  priv->ops = 0;
  if (viewable)
  {
    priv->ops = pGC->ops;
  }
  GC_FUNC_EPILOGUE(pGC);
}

/******************************************************************************/
static void
rdpChangeGC(GCPtr pGC, unsigned long mask)
{
  rdpGCRec* priv;

  DEBUG_OUT_FUNCS(("in rdpChangeGC\n"));
  GC_FUNC_PROLOGUE(pGC);
  pGC->funcs->ChangeGC(pGC, mask);
  GC_FUNC_EPILOGUE(pGC);
}

/******************************************************************************/
static void
rdpCopyGC(GCPtr src, unsigned long mask, GCPtr dst)
{
  rdpGCRec* priv;

  DEBUG_OUT_FUNCS(("in rdpCopyGC\n"));
  GC_FUNC_PROLOGUE(dst);
  dst->funcs->CopyGC(src, mask, dst);
  GC_FUNC_EPILOGUE(dst);
}

/******************************************************************************/
static void
rdpDestroyGC(GCPtr pGC)
{
  rdpGCRec* priv;

  DEBUG_OUT_FUNCS(("in rdpDestroyGC\n"));
  GC_FUNC_PROLOGUE(pGC);
  pGC->funcs->DestroyGC(pGC);
  GC_FUNC_EPILOGUE(pGC);
}

/******************************************************************************/
static void
rdpChangeClip(GCPtr pGC, int type, pointer pValue, int nrects)
{
  rdpGCRec* priv;

  DEBUG_OUT_FUNCS(("in rdpChangeClip\n"));
  GC_FUNC_PROLOGUE(pGC);
  pGC->funcs->ChangeClip(pGC, type, pValue, nrects);
  GC_FUNC_EPILOGUE(pGC);
}

/******************************************************************************/
static void
rdpDestroyClip(GCPtr pGC)
{
  rdpGCRec* priv;

  DEBUG_OUT_FUNCS(("in rdpDestroyClip\n"));
  GC_FUNC_PROLOGUE(pGC);
  pGC->funcs->DestroyClip(pGC);
  GC_FUNC_EPILOGUE(pGC);
}

/******************************************************************************/
static void
rdpCopyClip(GCPtr dst, GCPtr src)
{
  rdpGCRec* priv;

  DEBUG_OUT_FUNCS(("in rdpCopyClip\n"));
  GC_FUNC_PROLOGUE(dst);
  dst->funcs->CopyClip(dst, src);
  GC_FUNC_EPILOGUE(dst);
}

/******************************************************************************/
#define GC_OP_PROLOGUE(_pGC) \
{ \
  priv = (rdpGCPtr)pGC->devPrivates[g_rdpGCIndex].ptr; \
  oldFuncs = _pGC->funcs; \
  (_pGC)->funcs = priv->funcs; \
  (_pGC)->ops = priv->ops; \
}

/******************************************************************************/
#define GC_OP_EPILOGUE(_pGC) \
{ \
  priv->ops = (_pGC)->ops; \
  (_pGC)->funcs = oldFuncs; \
  (_pGC)->ops = &g_rdpGCOps; \
}

/******************************************************************************/
static void
rdpFillSpans(DrawablePtr pDrawable, GCPtr pGC, int nInit,
             DDXPointPtr pptInit, int* pwidthInit, int fSorted)
{
  rdpGCPtr priv;
  RegionRec clip_reg;
  int cd;
  int j;
  GCFuncs* oldFuncs;
  BoxRec box;

  DEBUG_OUT_OPS(("in rdpFillSpans\n"));
  GC_OP_PROLOGUE(pGC)
  pGC->ops->FillSpans(pDrawable, pGC, nInit, pptInit, pwidthInit, fSorted);
  miRegionInit(&clip_reg, NullBox, 0);
  cd = rdp_get_clip(&clip_reg, pDrawable, pGC);
  if (cd == 1)
  {
    rdpup_begin_update();
    miRegionCopy(&clip_reg, &(((WindowPtr)pDrawable)->borderClip));
    for (j = REGION_NUM_RECTS(&clip_reg) - 1; j >= 0; j--)
    {
      box = REGION_RECTS(&clip_reg)[j];
      rdpup_send_area(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
    }
    rdpup_end_update();
  }
  else if (cd == 2)
  {
    rdpup_begin_update();
    miIntersect(&clip_reg, &clip_reg,
                &(((WindowPtr)pDrawable)->borderClip));
    for (j = REGION_NUM_RECTS(&clip_reg) - 1; j >= 0; j--)
    {
      box = REGION_RECTS(&clip_reg)[j];
      rdpup_send_area(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
    }
    rdpup_end_update();
  }
  miRegionUninit(&clip_reg);
  GC_OP_EPILOGUE(pGC);
}

/******************************************************************************/
static void
rdpSetSpans(DrawablePtr pDrawable, GCPtr pGC, char* psrc,
            DDXPointPtr ppt, int* pwidth, int nspans, int fSorted)
{
  rdpGCPtr priv;
  RegionRec clip_reg;
  int cd;
  int j;
  GCFuncs* oldFuncs;
  BoxRec box;

  DEBUG_OUT_OPS(("in rdpSetSpans\n"));
  GC_OP_PROLOGUE(pGC);
  pGC->ops->SetSpans(pDrawable, pGC, psrc, ppt, pwidth, nspans, fSorted);
  miRegionInit(&clip_reg, NullBox, 0);
  cd = rdp_get_clip(&clip_reg, pDrawable, pGC);
  if (cd == 1)
  {
    rdpup_begin_update();
    miRegionCopy(&clip_reg, &(((WindowPtr)pDrawable)->borderClip));
    for (j = REGION_NUM_RECTS(&clip_reg) - 1; j >= 0; j--)
    {
      box = REGION_RECTS(&clip_reg)[j];
      rdpup_send_area(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
    }
    rdpup_end_update();
  }
  else if (cd == 2)
  {
    rdpup_begin_update();
    miIntersect(&clip_reg, &clip_reg,
                &((WindowPtr)pDrawable)->borderClip);
    for (j = REGION_NUM_RECTS(&clip_reg) - 1; j >= 0; j--)
    {
      box = REGION_RECTS(&clip_reg)[j];
      rdpup_send_area(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
    }
    rdpup_end_update();
  }
  miRegionUninit(&clip_reg);
  GC_OP_EPILOGUE(pGC);
}

/******************************************************************************/
static void
rdpPutImage(DrawablePtr pDrawable, GCPtr pGC, int depth, int x, int y,
            int w, int h, int leftPad, int format, char* pBits)
{
  rdpGCPtr priv;
  RegionRec clip_reg;
  int cd;
  int j;
  GCFuncs* oldFuncs;
  BoxRec box;

  DEBUG_OUT_OPS(("in rdpPutImage\n"));
  GC_OP_PROLOGUE(pGC);
  pGC->ops->PutImage(pDrawable, pGC, depth, x, y, w, h, leftPad,
                     format, pBits);
  miRegionInit(&clip_reg, NullBox, 0);
  cd = rdp_get_clip(&clip_reg, pDrawable, pGC);
  if (cd == 1)
  {
    rdpup_begin_update();
    rdpup_send_area(pDrawable->x + x, pDrawable->y + y, w, h);
    rdpup_end_update();
  }
  else if (cd == 2)
  {
    rdpup_begin_update();
    for (j = REGION_NUM_RECTS(&clip_reg) - 1; j >= 0; j--)
    {
      box = REGION_RECTS(&clip_reg)[j];
      rdpup_set_clip(box.x1, box.y1, (box.x2 - box.x1), (box.y2 - box.y1));
      rdpup_send_area(pDrawable->x + x, pDrawable->y + y, w, h);
    }
    rdpup_reset_clip();
    rdpup_end_update();
  }
  miRegionUninit(&clip_reg);
  GC_OP_EPILOGUE(pGC);
}

/******************************************************************************/
static RegionPtr
rdpCopyArea(DrawablePtr pSrc, DrawablePtr pDst, GCPtr pGC,
            int srcx, int srcy, int w, int h, int dstx, int dsty)
{
  rdpGCPtr priv;
  RegionPtr rv;
  RegionRec clip_reg;
  RegionRec box_reg;
  int num_clips;
  int cd;
  int j;
  int can_do_screen_blt;
  int dx;
  int dy;
  BoxRec box;
  BoxPtr pbox;
  GCFuncs* oldFuncs;

  DEBUG_OUT_OPS(("in rdpCopyArea\n"));
  GC_OP_PROLOGUE(pGC);
  rv = pGC->ops->CopyArea(pSrc, pDst, pGC, srcx, srcy, w, h, dstx, dsty);
  miRegionInit(&clip_reg, NullBox, 0);
  cd = rdp_get_clip(&clip_reg, pDst, pGC);
  can_do_screen_blt = pSrc->type == DRAWABLE_WINDOW &&
                      ((WindowPtr)pSrc)->viewable &&
                      pGC->alu == GXcopy;
  if (cd == 1)
  {
    rdpup_begin_update();
    if (can_do_screen_blt)
    {
      rdpup_screen_blt(pDst->x + dstx, pDst->y + dsty, w, h,
                       pSrc->x + srcx, pSrc->y + srcy);
    }
    else
    {
      rdpup_send_area(pDst->x + dstx, pDst->y + dsty, w, h);
    }
    rdpup_end_update();
  }
  else if (cd == 2)
  {
    num_clips = REGION_NUM_RECTS(&clip_reg);
    if (num_clips > 0)
    {
      rdpup_begin_update();
      if (can_do_screen_blt)
      {
        dx = dstx - srcx;
        dy = dsty - srcy;
        if (dy < 0 || (dy == 0 && dx < 0))
        {
          for (j = 0; j < num_clips; j++)
          {
            box = REGION_RECTS(&clip_reg)[j];
            rdpup_set_clip(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
            rdpup_screen_blt(pDst->x + dstx, pDst->y + dsty, w, h,
                             pSrc->x + srcx, pSrc->y + srcy);
          }
        }
        else
        {
          for (j = num_clips - 1; j >= 0; j--)
          {
            box = REGION_RECTS(&clip_reg)[j];
            rdpup_set_clip(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
            rdpup_screen_blt(pDst->x + dstx, pDst->y + dsty, w, h,
                             pSrc->x + srcx, pSrc->y + srcy);
          }
        }
        rdpup_reset_clip();
      }
      else
      {
        box.x1 = pDst->x + dstx;
        box.y1 = pDst->y + dsty;
        box.x2 = box.x1 + w;
        box.y2 = box.y1 + h;
        miRegionInit(&box_reg, &box, 0);
        miIntersect(&clip_reg, &clip_reg, &box_reg);
        num_clips = REGION_NUM_RECTS(&clip_reg);
        if (num_clips < 10)
        {
          for (j = num_clips - 1; j >= 0; j--)
          {
            box = REGION_RECTS(&clip_reg)[j];
            rdpup_send_area(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
          }
        }
        else
        {
          pbox = miRegionExtents(&clip_reg);
          rdpup_send_area(pbox->x1, pbox->y1, pbox->x2 - pbox->x1,
                          pbox->y2 - pbox->y1);
        }
        miRegionUninit(&box_reg);
      }
      rdpup_end_update();
    }
  }
  miRegionUninit(&clip_reg);
  GC_OP_EPILOGUE(pGC);
  return rv;
}

/******************************************************************************/
static RegionPtr
rdpCopyPlane(DrawablePtr pSrcDrawable, DrawablePtr pDstDrawable,
             GCPtr pGC, int srcx, int srcy, int width, int height,
             int dstx, int dsty, unsigned long bitPlane)
{
  rdpGCPtr priv;
  RegionPtr rv;
  RegionRec reg;
  int cd;
  GCFuncs* oldFuncs;

  DEBUG_OUT_OPS(("in rdpCopyPlane\n"));
  GC_OP_PROLOGUE(pGC);
  rv = pGC->ops->CopyPlane(pSrcDrawable, pDstDrawable, pGC, srcx, srcy,
                           width, height, dstx, dsty, bitPlane);
  miRegionInit(&reg, NullBox, 0);
  cd = rdp_get_clip(&reg, pDstDrawable, pGC);
  if (cd == 1)
  {
  }
  else if (cd == 2)
  {
  }
  miRegionUninit(&reg);
  GC_OP_EPILOGUE(pGC);
  return rv;
}

/******************************************************************************/
static void
rdpPolyPoint(DrawablePtr pDrawable, GCPtr pGC, int mode,
             int npt, DDXPointPtr in_pts)
{
  rdpGCPtr priv;
  RegionRec clip_reg;
  int num_clips;
  int cd;
  int x;
  int y;
  int i;
  int j;
  GCFuncs* oldFuncs;
  BoxRec box;
  BoxRec total_box;
  DDXPointPtr pts;
  DDXPointRec stack_pts[32];

  DEBUG_OUT_OPS(("in rdpPolyPoint\n"));
  GC_OP_PROLOGUE(pGC);
  if (npt > 32)
  {
    pts = (DDXPointPtr)Xalloc(sizeof(DDXPointRec) * npt);
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
  pGC->ops->PolyPoint(pDrawable, pGC, mode, npt, in_pts);
  miRegionInit(&clip_reg, NullBox, 0);
  cd = rdp_get_clip(&clip_reg, pDrawable, pGC);
  if (cd == 1)
  {
    if (npt > 0)
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
  else if (cd == 2)
  {
    num_clips = REGION_NUM_RECTS(&clip_reg);
    if (npt > 0 && num_clips > 0)
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
  miRegionUninit(&clip_reg);
  if (pts != stack_pts)
  {
    Xfree(pts);
  }
  GC_OP_EPILOGUE(pGC);
}

/******************************************************************************/
static void
rdpPolylines(DrawablePtr pDrawable, GCPtr pGC, int mode,
             int npt, DDXPointPtr pptInit)
{
  rdpGCPtr priv;
  RegionRec clip_reg;
  int num_clips;
  int cd;
  int i;
  int j;
  int x1;
  int y1;
  int x2;
  int y2;
  GCFuncs* oldFuncs;
  BoxRec box;
  DDXPointPtr ppts;

  DEBUG_OUT_OPS(("in rdpPolylines\n"));
  GC_OP_PROLOGUE(pGC);
  ppts = 0;
  if (npt > 0)
  {
    ppts = (DDXPointPtr)Xalloc(sizeof(DDXPointRec) * npt);
    for (i = 0; i < npt; i++)
    {
      ppts[i] = pptInit[i];
    }
  }
  pGC->ops->Polylines(pDrawable, pGC, mode, npt, pptInit);
  miRegionInit(&clip_reg, NullBox, 0);
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
  miRegionUninit(&clip_reg);
  Xfree(ppts);
  GC_OP_EPILOGUE(pGC);
}

/******************************************************************************/
static void
rdpPolySegment(DrawablePtr pDrawable, GCPtr pGC, int nseg, xSegment* pSegs)
{
  rdpGCPtr priv;
  RegionRec clip_reg;
  int cd;
  int i;
  int j;
  GCFuncs* oldFuncs;
  xSegment* segs;
  BoxRec box;

  DEBUG_OUT_OPS(("in rdpPolySegment\n"));
  GC_OP_PROLOGUE(pGC);
  segs = 0;
  if (nseg) /* get the rects */
  {
    segs = (xSegment*)Xalloc(nseg * sizeof(xSegment));
    for (i = 0; i < nseg; i++)
    {
      segs[i].x1 = pSegs[i].x1 + pDrawable->x;
      segs[i].y1 = pSegs[i].y1 + pDrawable->y;
      segs[i].x2 = pSegs[i].x2 + pDrawable->x;
      segs[i].y2 = pSegs[i].y2 + pDrawable->y;
    }
  }
  pGC->ops->PolySegment(pDrawable, pGC, nseg, pSegs);
  miRegionInit(&clip_reg, NullBox, 0);
  cd = rdp_get_clip(&clip_reg, pDrawable, pGC);
  if (cd == 1) /* no clip */
  {
    if (segs != 0)
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
  else if (cd == 2) /* clip */
  {
    if (segs != 0)
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
        }
      }
      rdpup_reset_clip();
      rdpup_set_opcode(GXcopy);
      rdpup_end_update();
    }
  }
  Xfree(segs);
  miRegionUninit(&clip_reg);
  GC_OP_EPILOGUE(pGC);
}

/******************************************************************************/
/* tested with pGC->lineWidth = 0, 1, 2, 4 and opcodes 3 and 6 */
static void
rdpPolyRectangle(DrawablePtr pDrawable, GCPtr pGC, int nrects,
                 xRectangle* rects)
{
  rdpGCPtr priv;
  RegionRec clip_reg;
  RegionPtr fill_reg;
  int num_clips;
  int cd;
  int lw;
  int i;
  int j;
  int up;
  int down;
  GCFuncs* oldFuncs;
  xRectangle* regRects;
  xRectangle* r;
  xRectangle* rect1;
  BoxRec box;

  DEBUG_OUT_OPS(("in rdpPolyRectangle\n"));
  GC_OP_PROLOGUE(pGC);
  /* make a copy of rects */
  rect1 = (xRectangle*)Xalloc(sizeof(xRectangle) * nrects);
  for (i = 0; i < nrects; i++)
  {
    rect1[i] = rects[i];
  }
  pGC->ops->PolyRectangle(pDrawable, pGC, nrects, rects);
  miRegionInit(&clip_reg, NullBox, 0);
  cd = rdp_get_clip(&clip_reg, pDrawable, pGC);
  regRects = 0;
  if (cd != 0 && nrects > 0)
  {
    regRects = (xRectangle*)Xalloc(nrects * 4 * sizeof(xRectangle));
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
          rdpup_send_area(r->x, r->y, r->width, r->height);
        }
      }
      rdpup_end_update();
    }
  }
  else if (cd == 2)
  {
    if (regRects != 0)
    {
      fill_reg = miRectsToRegion(nrects * 4, regRects, CT_NONE);
      miIntersect(&clip_reg, &clip_reg, fill_reg);
      num_clips = REGION_NUM_RECTS(&clip_reg);
      if (num_clips > 0)
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
            rdpup_send_area(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
          }
        }
        rdpup_end_update();
      }
      miRegionDestroy(fill_reg);
    }
  }
  miRegionUninit(&clip_reg);
  Xfree(regRects);
  Xfree(rect1);
  GC_OP_EPILOGUE(pGC);
}

/******************************************************************************/
static void
rdpPolyArc(DrawablePtr pDrawable, GCPtr pGC, int narcs, xArc* parcs)
{
  rdpGCPtr priv;
  RegionRec clip_reg;
  RegionPtr tmpRegion;
  int cd;
  int lw;
  int extra;
  int i;
  int num_clips;
  GCFuncs* oldFuncs;
  xRectangle* rects;
  BoxRec box;

  DEBUG_OUT_OPS(("in rdpPolyArc\n"));
  GC_OP_PROLOGUE(pGC);
  rects = 0;
  if (narcs > 0)
  {
    rects = (xRectangle*)Xalloc(narcs * sizeof(xRectangle));
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
  pGC->ops->PolyArc(pDrawable, pGC, narcs, parcs);
  miRegionInit(&clip_reg, NullBox, 0);
  cd = rdp_get_clip(&clip_reg, pDrawable, pGC);
  if (cd == 1)
  {
    if (rects != 0)
    {
      tmpRegion = miRectsToRegion(narcs, rects, CT_NONE);
      num_clips = REGION_NUM_RECTS(tmpRegion);
      if (num_clips > 0)
      {
        rdpup_begin_update();
        for (i = num_clips - 1; i >= 0; i--)
        {
          box = REGION_RECTS(tmpRegion)[i];
          rdpup_send_area(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
        }
        rdpup_end_update();
      }
      miRegionDestroy(tmpRegion);
    }
  }
  else if (cd == 2)
  {
    if (rects != 0)
    {
      tmpRegion = miRectsToRegion(narcs, rects, CT_NONE);
      miIntersect(tmpRegion, tmpRegion, &clip_reg);
      num_clips = REGION_NUM_RECTS(tmpRegion);
      if (num_clips > 0)
      {
        rdpup_begin_update();
        for (i = num_clips - 1; i >= 0; i--)
        {
          box = REGION_RECTS(tmpRegion)[i];
          rdpup_send_area(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
        }
        rdpup_end_update();
      }
      miRegionDestroy(tmpRegion);
    }
  }
  miRegionUninit(&clip_reg);
  Xfree(rects);
  GC_OP_EPILOGUE(pGC);
}

/******************************************************************************/
static void
rdpFillPolygon(DrawablePtr pDrawable, GCPtr pGC,
               int shape, int mode, int count,
               DDXPointPtr pPts)
{
  rdpGCPtr priv;
  RegionRec clip_reg;
  RegionRec box_reg;
  int num_clips;
  int cd;
  int maxx;
  int maxy;
  int minx;
  int miny;
  int i;
  int j;
  GCFuncs* oldFuncs;
  BoxRec box;

  DEBUG_OUT_OPS(("in rdpFillPolygon\n"));
  GC_OP_PROLOGUE(pGC);
  pGC->ops->FillPolygon(pDrawable, pGC, shape, mode, count, pPts);
  miRegionInit(&clip_reg, NullBox, 0);
  cd = rdp_get_clip(&clip_reg, pDrawable, pGC);
  if (cd != 0)
  {
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
        box.x1 = pDrawable->x + minx;
        box.y1 = pDrawable->y + miny;
        box.x2 = pDrawable->x + maxx + 1;
        box.y2 = pDrawable->y + maxy + 1;
      }
    }
  }
  if (cd == 1)
  {
    rdpup_begin_update();
    rdpup_send_area(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
    rdpup_end_update();
  }
  else if (cd == 2)
  {
    miRegionInit(&box_reg, &box, 0);
    miIntersect(&clip_reg, &clip_reg, &box_reg);
    num_clips = REGION_NUM_RECTS(&clip_reg);
    if (num_clips > 0)
    {
      rdpup_begin_update();
      for (j = num_clips - 1; j >= 0; j--)
      {
        box = REGION_RECTS(&clip_reg)[j];
        rdpup_send_area(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
      }
      rdpup_end_update();
    }
    miRegionUninit(&box_reg);
  }
  miRegionUninit(&clip_reg);
  GC_OP_EPILOGUE(pGC);
}

/******************************************************************************/
static void
rdpPolyFillRect(DrawablePtr pDrawable, GCPtr pGC, int nrectFill,
                xRectangle* prectInit)
{
  int i;
  int j;
  int cd;
  int num_clips;
  xRectangle* copy_of_rects;
  rdpGCPtr priv;
  RegionRec clip_reg;
  RegionPtr fill_reg;
  BoxRec box;
  GCFuncs* oldFuncs;

  DEBUG_OUT_OPS(("in rdpPolyFillRect\n"));
  GC_OP_PROLOGUE(pGC);
  /* make a copy of rects */
  copy_of_rects = (xRectangle*)Xalloc(sizeof(xRectangle) * nrectFill);
  for (i = 0; i < nrectFill; i++)
  {
    copy_of_rects[i] = prectInit[i];
  }
  fill_reg = miRectsToRegion(nrectFill, copy_of_rects, CT_NONE);
  Xfree(copy_of_rects);
  miTranslateRegion(fill_reg, pDrawable->x, pDrawable->y);
  pGC->ops->PolyFillRect(pDrawable, pGC, nrectFill, prectInit);
  miRegionInit(&clip_reg, NullBox, 0);
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
        rdpup_send_area(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
      }
    }
    rdpup_end_update();
  }
  else if (cd == 2) /* clip */
  {
    miIntersect(&clip_reg, &clip_reg, fill_reg);
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
          rdpup_send_area(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
        }
      }
      rdpup_end_update();
    }
  }
  miRegionUninit(&clip_reg);
  miRegionDestroy(fill_reg);
  GC_OP_EPILOGUE(pGC);
}

/******************************************************************************/
static void
rdpPolyFillArc(DrawablePtr pDrawable, GCPtr pGC, int narcs, xArc* parcs)
{
  rdpGCPtr priv;
  RegionRec reg;
  int cd;
  GCFuncs* oldFuncs;

  DEBUG_OUT_OPS(("in rdpPolyFillArc\n"));
  GC_OP_PROLOGUE(pGC);
  pGC->ops->PolyFillArc(pDrawable, pGC, narcs, parcs);
  miRegionInit(&reg, NullBox, 0);
  cd = rdp_get_clip(&reg, pDrawable, pGC);
  if (cd == 1)
  {
  }
  else if (cd == 2)
  {
  }
  miRegionUninit(&reg);
  GC_OP_EPILOGUE(pGC);
}

/******************************************************************************/
static int
rdpPolyText8(DrawablePtr pDrawable, GCPtr pGC,
             int x, int y, int count, char* chars)
{
  rdpGCPtr priv;
  RegionRec reg;
  RegionRec reg1;
  int num_clips;
  int cd;
  int j;
  int rv;
  GCFuncs* oldFuncs;
  BoxRec box;

  DEBUG_OUT_OPS(("in rdpPolyText8\n"));
  GC_OP_PROLOGUE(pGC);
  if (count != 0)
  {
    GetTextBoundingBox(pDrawable, pGC->font, x, y, count, &box);
  }
  rv = pGC->ops->PolyText8(pDrawable, pGC, x, y, count, chars);
  miRegionInit(&reg, NullBox, 0);
  if (count == 0)
  {
    cd = 0;
  }
  else
  {
    cd = rdp_get_clip(&reg, pDrawable, pGC);
  }
  if (cd == 1)
  {
    rdpup_begin_update();
    rdpup_send_area(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
    rdpup_end_update();
  }
  else if (cd == 2)
  {
    miRegionInit(&reg1, &box, 0);
    miIntersect(&reg, &reg, &reg1);
    num_clips = REGION_NUM_RECTS(&reg);
    if (num_clips > 0)
    {
      rdpup_begin_update();
      for (j = num_clips - 1; j >= 0; j--)
      {
        box = REGION_RECTS(&reg)[j];
        rdpup_send_area(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
      }
      rdpup_end_update();
    }
    miRegionUninit(&reg1);
  }
  miRegionUninit(&reg);
  GC_OP_EPILOGUE(pGC);
  return rv;
}

/******************************************************************************/
static int
rdpPolyText16(DrawablePtr pDrawable, GCPtr pGC,
              int x, int y, int count, unsigned short* chars)
{
  rdpGCPtr priv;
  RegionRec reg;
  RegionRec reg1;
  int num_clips;
  int cd;
  int j;
  int rv;
  GCFuncs* oldFuncs;
  BoxRec box;

  DEBUG_OUT_OPS(("in rdpPolyText16\n"));
  GC_OP_PROLOGUE(pGC);
  if (count != 0)
  {
    GetTextBoundingBox(pDrawable, pGC->font, x, y, count, &box);
  }
  rv = pGC->ops->PolyText16(pDrawable, pGC, x, y, count, chars);
  miRegionInit(&reg, NullBox, 0);
  if (count == 0)
  {
    cd = 0;
  }
  else
  {
    cd = rdp_get_clip(&reg, pDrawable, pGC);
  }
  if (cd == 1)
  {
    rdpup_begin_update();
    rdpup_send_area(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
    rdpup_end_update();
  }
  else if (cd == 2)
  {
    miRegionInit(&reg1, &box, 0);
    miIntersect(&reg, &reg, &reg1);
    num_clips = REGION_NUM_RECTS(&reg);
    if (num_clips > 0)
    {
      rdpup_begin_update();
      for (j = num_clips - 1; j >= 0; j--)
      {
        box = REGION_RECTS(&reg)[j];
        rdpup_send_area(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
      }
      rdpup_end_update();
    }
    miRegionUninit(&reg1);
  }
  miRegionUninit(&reg);
  GC_OP_EPILOGUE(pGC);
  return rv;
}

/******************************************************************************/
static void
rdpImageText8(DrawablePtr pDrawable, GCPtr pGC,
              int x, int y, int count, char* chars)
{
  rdpGCPtr priv;
  RegionRec clip_reg;
  RegionRec box_reg;
  int num_clips;
  int cd;
  int j;
  GCFuncs* oldFuncs;
  BoxRec box;

  DEBUG_OUT_OPS(("in rdpImageText8\n"));
  GC_OP_PROLOGUE(pGC);
  if (count != 0)
  {
    GetTextBoundingBox(pDrawable, pGC->font, x, y, count, &box);
  }
  pGC->ops->ImageText8(pDrawable, pGC, x, y, count, chars);
  miRegionInit(&clip_reg, NullBox, 0);
  if (count == 0)
  {
    cd = 0;
  }
  else
  {
    cd = rdp_get_clip(&clip_reg, pDrawable, pGC);
  }
  if (cd == 1)
  {
    rdpup_begin_update();
    rdpup_send_area(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
    rdpup_end_update();
  }
  else if (cd == 2)
  {
    miRegionInit(&box_reg, &box, 0);
    miIntersect(&clip_reg, &clip_reg, &box_reg);
    num_clips = REGION_NUM_RECTS(&clip_reg);
    if (num_clips > 0)
    {
      rdpup_begin_update();
      for (j = num_clips - 1; j >= 0; j--)
      {
        box = REGION_RECTS(&clip_reg)[j];
        rdpup_send_area(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
      }
      rdpup_end_update();
    }
    miRegionUninit(&box_reg);
  }
  miRegionUninit(&clip_reg);
  GC_OP_EPILOGUE(pGC);
}

/******************************************************************************/
static void
rdpImageText16(DrawablePtr pDrawable, GCPtr pGC,
               int x, int y, int count,
               unsigned short* chars)
{
  rdpGCPtr priv;
  RegionRec clip_reg;
  RegionRec box_reg;
  int num_clips;
  int cd;
  int j;
  GCFuncs* oldFuncs;
  BoxRec box;

  DEBUG_OUT_OPS(("in rdpImageText16\n"));
  GC_OP_PROLOGUE(pGC);
  if (count != 0)
  {
    GetTextBoundingBox(pDrawable, pGC->font, x, y, count, &box);
  }
  pGC->ops->ImageText16(pDrawable, pGC, x, y, count, chars);
  miRegionInit(&clip_reg, NullBox, 0);
  if (count == 0)
  {
    cd = 0;
  }
  else
  {
    cd = rdp_get_clip(&clip_reg, pDrawable, pGC);
  }
  if (cd == 1)
  {
    rdpup_begin_update();
    rdpup_send_area(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
    rdpup_end_update();
  }
  else if (cd == 2)
  {
    miRegionInit(&box_reg, &box, 0);
    miIntersect(&clip_reg, &clip_reg, &box_reg);
    num_clips = REGION_NUM_RECTS(&clip_reg);
    if (num_clips > 0)
    {
      rdpup_begin_update();
      for (j = num_clips - 1; j >= 0; j--)
      {
        box = REGION_RECTS(&clip_reg)[j];
        rdpup_send_area(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
      }
      rdpup_end_update();
    }
    miRegionUninit(&box_reg);
  }
  miRegionUninit(&clip_reg);
  GC_OP_EPILOGUE(pGC);
}

/******************************************************************************/
static void
rdpImageGlyphBlt(DrawablePtr pDrawable, GCPtr pGC,
                 int x, int y, unsigned int nglyph,
                 CharInfoPtr* ppci, pointer pglyphBase)
{
  rdpGCPtr priv;
  RegionRec reg;
  RegionRec box_reg;
  int num_clips;
  int cd;
  int j;
  GCFuncs* oldFuncs;
  BoxRec box;

  DEBUG_OUT_OPS(("in rdpImageGlyphBlt\n"));
  GC_OP_PROLOGUE(pGC);
  if (nglyph != 0)
  {
    GetTextBoundingBox(pDrawable, pGC->font, x, y, nglyph, &box);
  }
  pGC->ops->ImageGlyphBlt(pDrawable, pGC, x, y, nglyph, ppci, pglyphBase);
  miRegionInit(&reg, NullBox, 0);
  if (nglyph == 0)
  {
    cd = 0;
  }
  else
  {
    cd = rdp_get_clip(&reg, pDrawable, pGC);
  }
  if (cd == 1)
  {
    rdpup_begin_update();
    rdpup_send_area(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
    rdpup_end_update();
  }
  else if (cd == 2)
  {
    miRegionInit(&box_reg, &box, 0);
    miIntersect(&reg, &reg, &box_reg);
    num_clips = REGION_NUM_RECTS(&reg);
    if (num_clips > 0)
    {
      rdpup_begin_update();
      for (j = num_clips - 1; j >= 0; j--)
      {
        box = REGION_RECTS(&reg)[j];
        rdpup_send_area(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
      }
      rdpup_end_update();
    }
    miRegionUninit(&box_reg);
  }
  miRegionUninit(&reg);
  GC_OP_EPILOGUE(pGC);
}

/******************************************************************************/
static void
rdpPolyGlyphBlt(DrawablePtr pDrawable, GCPtr pGC,
                int x, int y, unsigned int nglyph,
                CharInfoPtr* ppci,
                pointer pglyphBase)
{
  rdpGCPtr priv;
  RegionRec reg;
  RegionRec box_reg;
  int num_clips;
  int cd;
  int j;
  GCFuncs* oldFuncs;
  BoxRec box;

  DEBUG_OUT_OPS(("in rdpPolyGlyphBlt\n"));
  GC_OP_PROLOGUE(pGC);
  if (nglyph != 0)
  {
    GetTextBoundingBox(pDrawable, pGC->font, x, y, nglyph, &box);
  }
  pGC->ops->PolyGlyphBlt(pDrawable, pGC, x, y, nglyph, ppci, pglyphBase);
  miRegionInit(&reg, NullBox, 0);
  if (nglyph == 0)
  {
    cd = 0;
  }
  else
  {
    cd = rdp_get_clip(&reg, pDrawable, pGC);
  }
  if (cd == 1)
  {
    rdpup_begin_update();
    rdpup_send_area(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
    rdpup_end_update();
  }
  else if (cd == 2)
  {
    miRegionInit(&box_reg, &box, 0);
    miIntersect(&reg, &reg, &box_reg);
    num_clips = REGION_NUM_RECTS(&reg);
    if (num_clips > 0)
    {
      rdpup_begin_update();
      for (j = num_clips - 1; j >= 0; j--)
      {
        box = REGION_RECTS(&reg)[j];
        rdpup_send_area(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
      }
      rdpup_end_update();
    }
    miRegionUninit(&box_reg);
  }
  miRegionUninit(&reg);
  GC_OP_EPILOGUE(pGC);
}

/******************************************************************************/
static void
rdpPushPixels(GCPtr pGC, PixmapPtr pBitMap, DrawablePtr pDst,
              int w, int h, int x, int y)
{
  rdpGCPtr priv;
  RegionRec clip_reg;
  RegionRec box_reg;
  int num_clips;
  int cd;
  int j;
  GCFuncs* oldFuncs;
  BoxRec box;

  DEBUG_OUT_OPS(("in rdpPushPixels\n"));
  GC_OP_PROLOGUE(pGC);
  pGC->ops->PushPixels(pGC, pBitMap, pDst, w, h, x, y);
  miRegionInit(&clip_reg, NullBox, 0);
  cd = rdp_get_clip(&clip_reg, pDst, pGC);
  if (cd == 1)
  {
    rdpup_begin_update();
    rdpup_send_area(x, y, w, h);
    rdpup_end_update();
  }
  else if (cd == 2)
  {
    miRegionInit(&box_reg, &box, 0);
    miIntersect(&clip_reg, &clip_reg, &box_reg);
    num_clips = REGION_NUM_RECTS(&clip_reg);
    if (num_clips > 0)
    {
      rdpup_begin_update();
      for (j = num_clips - 1; j >= 0; j--)
      {
        box = REGION_RECTS(&clip_reg)[j];
        rdpup_send_area(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
      }
      rdpup_end_update();
    }
    miRegionUninit(&box_reg);
  }
  miRegionUninit(&clip_reg);
  GC_OP_EPILOGUE(pGC);
}

/******************************************************************************/
Bool
rdpCloseScreen(int i, ScreenPtr pScreen)
{
  DEBUG_OUT_OPS(("in rdpCloseScreen\n"));
  pScreen->CloseScreen = g_rdpScreen.CloseScreen;
  pScreen->CreateGC = g_rdpScreen.CreateGC;
  pScreen->PaintWindowBackground = g_rdpScreen.PaintWindowBackground;
  pScreen->PaintWindowBorder = g_rdpScreen.PaintWindowBorder;
  pScreen->CopyWindow = g_rdpScreen.CopyWindow;
  pScreen->ClearToBackground = g_rdpScreen.ClearToBackground;
  pScreen->RestoreAreas = g_rdpScreen.RestoreAreas;
  return 1;
}

/******************************************************************************/
Bool
rdpCreateGC(GCPtr pGC)
{
  rdpGCRec* priv;
  Bool rv;

  DEBUG_OUT_OPS(("in rdpCreateGC\n"));
  rv = 0;
  if (g_rdpGCIndex != -1)
  {
    priv = (rdpGCPtr)pGC->devPrivates[g_rdpGCIndex].ptr;
    g_pScreen->CreateGC = g_rdpScreen.CreateGC;
    rv = g_pScreen->CreateGC(pGC);
    if (rv)
    {
      priv->funcs = pGC->funcs;
      priv->ops = 0;
      pGC->funcs = &g_rdpGCFuncs;
    }
    else
    {
      ErrorF("error in rdpCreateGC\n");
    }
    g_pScreen->CreateGC = rdpCreateGC;
  }
  else
  {
    ErrorF("error in rdpCreateGC\n");
  }
  return rv;
}

/******************************************************************************/
void
rdpPaintWindowBackground(WindowPtr pWin, RegionPtr pRegion, int what)
{
  int j;
  RegionRec reg;
  BoxRec box;

  DEBUG_OUT_OPS(("in rdpPaintWindowBackground\n"));
  miRegionInit(&reg, NullBox, 0);
  miRegionCopy(&reg, pRegion);
  g_pScreen->PaintWindowBackground = g_rdpScreen.PaintWindowBackground;
  g_pScreen->PaintWindowBackground(pWin, pRegion, what);
  rdpup_begin_update();
  if (what == PW_BACKGROUND && pWin->backgroundState == BackgroundPixel)
  {
    rdpup_set_fgcolor(pWin->background.pixel);
    for (j = REGION_NUM_RECTS(&reg) - 1; j >= 0; j--)
    {
      box = REGION_RECTS(&reg)[j];
      rdpup_fill_rect(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
    }
  }
  else if (what == PW_BORDER && pWin->borderIsPixel)
  {
    rdpup_set_fgcolor(pWin->border.pixel);
    for (j = REGION_NUM_RECTS(&reg) - 1; j >= 0; j--)
    {
      box = REGION_RECTS(&reg)[j];
      rdpup_fill_rect(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
    }
  }
  else
  {
    for (j = REGION_NUM_RECTS(&reg) - 1; j >= 0; j--)
    {
      box = REGION_RECTS(&reg)[j];
      rdpup_send_area(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
    }
  }
  rdpup_end_update();
  miRegionUninit(&reg);
  g_pScreen->PaintWindowBackground = rdpPaintWindowBackground;
}

/******************************************************************************/
void
rdpPaintWindowBorder(WindowPtr pWin, RegionPtr pRegion, int what)
{
  int j;
  RegionRec reg;
  BoxRec box;

  DEBUG_OUT_OPS(("in rdpPaintWindowBorder\n"));
  miRegionInit(&reg, NullBox, 0);
  miRegionCopy(&reg, pRegion);
  g_pScreen->PaintWindowBackground = g_rdpScreen.PaintWindowBackground;
  g_pScreen->PaintWindowBackground(pWin, pRegion, what);
  rdpup_begin_update();
  if (what == PW_BACKGROUND && pWin->backgroundState == BackgroundPixel)
  {
    rdpup_set_fgcolor(pWin->background.pixel);
    for (j = REGION_NUM_RECTS(&reg) - 1; j >= 0; j--)
    {
      box = REGION_RECTS(&reg)[j];
      rdpup_fill_rect(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
    }
  }
  else if (what == PW_BORDER && pWin->borderIsPixel)
  {
    rdpup_set_fgcolor(pWin->border.pixel);
    for (j = REGION_NUM_RECTS(&reg) - 1; j >= 0; j--)
    {
      box = REGION_RECTS(&reg)[j];
      rdpup_fill_rect(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
    }
  }
  else
  {
    for (j = REGION_NUM_RECTS(&reg) - 1; j >= 0; j--)
    {
      box = REGION_RECTS(&reg)[j];
      rdpup_send_area(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
    }
  }
  rdpup_end_update();
  miRegionUninit(&reg);
  g_pScreen->PaintWindowBackground = rdpPaintWindowBackground;
}

/******************************************************************************/
void
rdpCopyWindow(WindowPtr pWin, DDXPointRec ptOldOrg, RegionPtr pOldRegion)
{
  RegionRec reg;
  RegionRec clip;
  int dx;
  int dy;
  int i;
  int j;
  int num_clip_rects;
  int num_reg_rects;
  BoxRec box1;
  BoxRec box2;

  DEBUG_OUT_OPS(("in rdpCopyWindow\n"));
  miRegionInit(&reg, NullBox, 0);
  miRegionCopy(&reg, pOldRegion);
  g_pScreen->CopyWindow = g_rdpScreen.CopyWindow;
  g_pScreen->CopyWindow(pWin, ptOldOrg, pOldRegion);
  miRegionInit(&clip, NullBox, 0);
  miRegionCopy(&clip, &pWin->borderClip);
  dx = pWin->drawable.x - ptOldOrg.x;
  dy = pWin->drawable.y - ptOldOrg.y;
  rdpup_begin_update();
  num_clip_rects = REGION_NUM_RECTS(&clip);
  num_reg_rects = REGION_NUM_RECTS(&reg);
  /* should maybe sort the rects instead of checking dy < 0 */
  /* If we can depend on the rects going from top to bottom, left
     to right we are ok */
  if (dy < 0 || (dy == 0 && dx < 0))
  {
    for (j = 0; j < num_clip_rects; j++)
    {
      box1 = REGION_RECTS(&clip)[j];
      rdpup_set_clip(box1.x1, box1.y1, box1.x2 - box1.x1, box1.y2 - box1.y1);
      for (i = 0; i < num_reg_rects; i++)
      {
        box2 = REGION_RECTS(&reg)[i];
        rdpup_screen_blt(box2.x1 + dx, box2.y1 + dy, box2.x2 - box2.x1,
                         box2.y2 - box2.y1, box2.x1, box2.y1);
      }
    }
  }
  else
  {
    for (j = num_clip_rects - 1; j >= 0; j--)
    {
      box1 = REGION_RECTS(&clip)[j];
      rdpup_set_clip(box1.x1, box1.y1, box1.x2 - box1.x1, box1.y2 - box1.y1);
      for (i = num_reg_rects - 1; i >= 0; i--)
      {
        box2 = REGION_RECTS(&reg)[i];
        rdpup_screen_blt(box2.x1 + dx, box2.y1 + dy, box2.x2 - box2.x1,
                         box2.y2 - box2.y1, box2.x1, box2.y1);
      }
    }
  }
  rdpup_reset_clip();
  rdpup_end_update();
  miRegionUninit(&reg);
  miRegionUninit(&clip);
  g_pScreen->CopyWindow = rdpCopyWindow;
}

/******************************************************************************/
void
rdpClearToBackground(WindowPtr pWin, int x, int y, int w, int h,
                     Bool generateExposures)
{
  int j;
  BoxRec box;
  RegionRec reg;

  DEBUG_OUT_OPS(("in rdpClearToBackground\n"));
  g_pScreen->ClearToBackground = g_rdpScreen.ClearToBackground;
  g_pScreen->ClearToBackground(pWin, x, y, w, h, generateExposures);
  if (!generateExposures)
  {
    if (w > 0 && h > 0)
    {
      box.x1 = x;
      box.y1 = y;
      box.x2 = box.x1 + w;
      box.y2 = box.y1 + h;
    }
    else
    {
      box.x1 = pWin->drawable.x;
      box.y1 = pWin->drawable.y;
      box.x2 = box.x1 + pWin->drawable.width;
      box.y2 = box.y1 + pWin->drawable.height;
    }
    miRegionInit(&reg, &box, 0);
    miIntersect(&reg, &reg, &pWin->clipList);
    rdpup_begin_update();
    for (j = REGION_NUM_RECTS(&reg) - 1; j >= 0; j--)
    {
      box = REGION_RECTS(&reg)[j];
      rdpup_send_area(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
    }
    rdpup_end_update();
    miRegionUninit(&reg);
  }
  g_pScreen->ClearToBackground = rdpClearToBackground;
}

/******************************************************************************/
RegionPtr
rdpRestoreAreas(WindowPtr pWin, RegionPtr prgnExposed)
{
  RegionRec reg;
  RegionPtr rv;
  int j;
  BoxRec box;

  DEBUG_OUT_OPS(("in rdpRestoreAreas\n"));
  miRegionInit(&reg, NullBox, 0);
  miRegionCopy(&reg, prgnExposed);
  g_pScreen->RestoreAreas = g_rdpScreen.RestoreAreas;
  rv = g_pScreen->RestoreAreas(pWin, prgnExposed);
  rdpup_begin_update();
  for (j = REGION_NUM_RECTS(&reg) - 1; j >= 0; j--)
  {
    box = REGION_RECTS(&reg)[j];
    rdpup_send_area(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
  }
  rdpup_end_update();
  miRegionUninit(&reg);
  g_pScreen->RestoreAreas = rdpRestoreAreas;
  return rv;
}

/******************************************************************************/
void
rdpInstallColormap(ColormapPtr pmap)
{
  ColormapPtr oldpmap;

  oldpmap = g_rdpInstalledColormap;
  if (pmap != oldpmap)
  {
    if (oldpmap != (ColormapPtr)None)
    {
      WalkTree(pmap->pScreen, TellLostMap, (char*)&oldpmap->mid);
    }
    /* Install pmap */
    g_rdpInstalledColormap = pmap;
    WalkTree(pmap->pScreen, TellGainedMap, (char*)&pmap->mid);
    /*rfbSetClientColourMaps(0, 0);*/
  }
  /*g_rdpScreen.InstallColormap(pmap);*/
}

/******************************************************************************/
void
rdpUninstallColormap(ColormapPtr pmap)
{
  ColormapPtr curpmap;

  curpmap = g_rdpInstalledColormap;
  if (pmap == curpmap)
  {
    if (pmap->mid != pmap->pScreen->defColormap)
    {
      curpmap = (ColormapPtr)LookupIDByType(pmap->pScreen->defColormap,
                                            RT_COLORMAP);
      pmap->pScreen->InstallColormap(curpmap);
    }
  }
}

/******************************************************************************/
int
rdpListInstalledColormaps(ScreenPtr pScreen, Colormap* pmaps)
{
  *pmaps = g_rdpInstalledColormap->mid;
  return 1;
}

/******************************************************************************/
void
rdpStoreColors(ColormapPtr pmap, int ndef, xColorItem* pdefs)
{
}

/******************************************************************************/
Bool
rdpSaveScreen(ScreenPtr pScreen, int on)
{
  return 1;
}

/******************************************************************************/
/* it looks like all the antialias draws go through here */
void
rdpComposite(CARD8 op, PicturePtr pSrc, PicturePtr pMask, PicturePtr pDst,
             INT16 xSrc, INT16 ySrc, INT16 xMask, INT16 yMask, INT16 xDst,
             INT16 yDst, CARD16 width, CARD16 height)
{
  BoxRec box;
  PictureScreenPtr ps;
  RegionRec reg1;
  RegionRec reg2;
  DrawablePtr p;
  int j;
  int num_clips;

  DEBUG_OUT_OPS(("in rdpComposite\n"));
  ps = GetPictureScreen(g_pScreen);
  ps->Composite = g_rdpScreen.Composite;
  ps->Composite(op, pSrc, pMask, pDst, xSrc, ySrc,
                xMask, yMask, xDst, yDst, width, height);
  p = pDst->pDrawable;
  if (p->type == DRAWABLE_WINDOW)
  {
    if (pDst->clientClipType == CT_REGION)
    {
      box.x1 = p->x + xDst;
      box.y1 = p->y + yDst;
      box.x2 = box.x1 + width;
      box.y2 = box.y1 + height;
      miRegionInit(&reg1, &box, 0);
      miRegionInit(&reg2, NullBox, 0);
      miRegionCopy(&reg2, pDst->clientClip);
      miTranslateRegion(&reg2, p->x + pDst->clipOrigin.x,
                        p->y + pDst->clipOrigin.y);
      miIntersect(&reg1, &reg1, &reg2);
      num_clips = REGION_NUM_RECTS(&reg1);
      if (num_clips > 0)
      {
        rdpup_begin_update();
        for (j = num_clips - 1; j >= 0; j--)
        {
          box = REGION_RECTS(&reg1)[j];
          rdpup_send_area(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
        }
        rdpup_end_update();
      }
      miRegionUninit(&reg1);
      miRegionUninit(&reg2);
    }
    else
    {
      box.x1 = p->x + xDst;
      box.y1 = p->y + yDst;
      box.x2 = box.x1 + width;
      box.y2 = box.y1 + height;
      rdpup_begin_update();
      rdpup_send_area(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
      rdpup_end_update();
    }
  }
  ps->Composite = rdpComposite;
}
