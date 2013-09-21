/*
Copyright 2013 Jay Sorg

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

to deal with regions changing in xorg versions

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* this should be before all X11 .h files */
#include <xorg-server.h>

/* all driver need this */
#include <xf86.h>
#include <xf86_OSproc.h>

/*
miRegionCopy      ->      RegionCopy
miTranslateRegion ->      RegionTranslate
miRegionNotEmpty  ->      RegionNotEmpty
miIntersect       ->      RegionIntersect
miRectIn          ->      RegionContainsRect
miRegionInit      ->      RegionInit
miRegionUninit    ->      RegionUninit
miRectsToRegion   ->      RegionFromRects
miRegionDestroy   ->      RegionDestroy
miRegionCreate    ->      RegionCreate
miUnion           ->      RegionUnion
miRegionExtents   ->      RegionExtents
miRegionReset     ->      RegionReset
miRegionBreak     ->      RegionBreak
*/

#if XORG_VERSION_CURRENT < (((1) * 10000000) + ((9) * 100000) + ((0) * 1000) + 0)
/* 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8 */
#define XRDP_REG 1
#else
/* 1.9, 1.10, 1.11, 1.12 */
#define XRDP_REG 2
#endif

/*****************************************************************************/
Bool
rdpRegionCopy(RegionPtr dst, RegionPtr src)
{
#if XRDP_REG == 1
  return miRegionCopy(dst, src);
#else
  return RegionCopy(dst, src);
#endif
}

/*****************************************************************************/
void
rdpRegionTranslate(RegionPtr pReg, int x, int y)
{
#if XRDP_REG == 1
  miTranslateRegion(pReg, x, y);
#else
  RegionTranslate(pReg, x, y);
#endif
}

/*****************************************************************************/
Bool
rdpRegionNotEmpty(RegionPtr pReg)
{
#if XRDP_REG == 1
  return miRegionNotEmpty(pReg);
#else
  return RegionNotEmpty(pReg);
#endif
}

/*****************************************************************************/
Bool
rdpRegionIntersect(RegionPtr newReg, RegionPtr reg1, RegionPtr reg2)
{
#if XRDP_REG == 1
  return miIntersect(newReg, reg1, reg2);
#else
  return RegionIntersect(newReg, reg1, reg2);
#endif
}

/*****************************************************************************/
int
rdpRegionContainsRect(RegionPtr region, BoxPtr prect)
{
#if XRDP_REG == 1
  return miRectIn(region, prect);
#else
  return RegionContainsRect(region, prect);
#endif
}

/*****************************************************************************/
void
rdpRegionInit(RegionPtr pReg, BoxPtr rect, int size)
{
#if XRDP_REG == 1
  miRegionInit(pReg, rect, size);
#else
  RegionInit(pReg, rect, size);
#endif
}

/*****************************************************************************/
void
rdpRegionUninit(RegionPtr pReg)
{
#if XRDP_REG == 1
  miRegionUninit(pReg);
#else
  RegionUninit(pReg);
#endif
}

/*****************************************************************************/
RegionPtr
rdpRegionFromRects(int nrects, xRectanglePtr prect, int ctype)
{
#if XRDP_REG == 1
  return miRectsToRegion(nrects, prect, ctype);
#else
  return RegionFromRects(nrects, prect, ctype);
#endif
}

/*****************************************************************************/
void
rdpRegionDestroy(RegionPtr pReg)
{
#if XRDP_REG == 1
  miRegionDestroy(pReg);
#else
  RegionDestroy(pReg);
#endif
}

/*****************************************************************************/
RegionPtr
rdpRegionCreate(BoxPtr rect, int size)
{
#if XRDP_REG == 1
  return miRegionCreate(rect, size);
#else
  return RegionCreate(rect, size);
#endif
}

/*****************************************************************************/
Bool
rdpRegionUnion(RegionPtr newReg, RegionPtr reg1, RegionPtr reg2)
{
#if XRDP_REG == 1
  return miUnion(newReg, reg1, reg2);
#else
  return RegionUnion(newReg, reg1, reg2);
#endif
}

/*****************************************************************************/
Bool
rdpRegionSubtract(RegionPtr newReg, RegionPtr reg1, RegionPtr reg2)
{
#if XRDP_REG == 1
  return miSubtract(newReg, reg1, reg2);
#else
  return RegionSubtract(newReg, reg1, reg2);
#endif
}

/*****************************************************************************/
Bool
rdpRegionInverse(RegionPtr newReg, RegionPtr reg1, BoxPtr invRect)
{
#if XRDP_REG == 1
  return miInverse(newReg, reg1, invRect);
#else
  return RegionInverse(newReg, reg1, invRect);
#endif
}

/*****************************************************************************/
BoxPtr
rdpRegionExtents(RegionPtr pReg)
{
#if XRDP_REG == 1
    return miRegionExtents(pReg);
#else
    return RegionExtents(pReg);
#endif
}

/*****************************************************************************/
void
rdpRegionReset(RegionPtr pReg, BoxPtr pBox)
{
#if XRDP_REG == 1
    miRegionReset(pReg, pBox);
#else
    RegionReset(pReg, pBox);
#endif
}

/*****************************************************************************/
Bool
rdpRegionBreak(RegionPtr pReg)
{
#if XRDP_REG == 1
    return miRegionBreak(pReg);
#else
    return RegionBreak(pReg);
#endif
}
