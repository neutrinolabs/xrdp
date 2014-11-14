/*
Copyright 2013-2014 Jay Sorg

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

#ifndef __RDPREG_H
#define __RDPREG_H

#include <xorg-server.h>
#include <xorgVersion.h>
#include <xf86.h>

extern _X_EXPORT Bool
rdpRegionCopy(RegionPtr dst, RegionPtr src);
extern _X_EXPORT void
rdpRegionTranslate(RegionPtr pReg, int x, int y);
extern _X_EXPORT Bool
rdpRegionNotEmpty(RegionPtr pReg);
extern _X_EXPORT Bool
rdpRegionIntersect(RegionPtr newReg, RegionPtr reg1, RegionPtr reg2);
extern _X_EXPORT int
rdpRegionContainsRect(RegionPtr region, BoxPtr prect);
extern _X_EXPORT void
rdpRegionInit(RegionPtr pReg, BoxPtr rect, int size);
extern _X_EXPORT void
rdpRegionUninit(RegionPtr pReg);
extern _X_EXPORT RegionPtr
rdpRegionFromRects(int nrects, xRectanglePtr prect, int ctype);
extern _X_EXPORT void
rdpRegionDestroy(RegionPtr pReg);
extern _X_EXPORT RegionPtr
rdpRegionCreate(BoxPtr rect, int size);
extern _X_EXPORT Bool
rdpRegionUnion(RegionPtr newReg, RegionPtr reg1, RegionPtr reg2);
extern _X_EXPORT Bool
rdpRegionSubtract(RegionPtr newReg, RegionPtr reg1, RegionPtr reg2);
extern _X_EXPORT Bool
rdpRegionInverse(RegionPtr newReg, RegionPtr reg1, BoxPtr invRect);
extern _X_EXPORT BoxPtr
rdpRegionExtents(RegionPtr pReg);
extern _X_EXPORT void
rdpRegionReset(RegionPtr pReg, BoxPtr pBox);
extern _X_EXPORT Bool
rdpRegionBreak(RegionPtr pReg);
extern _X_EXPORT void
rdpRegionUnionRect(RegionPtr pReg, BoxPtr prect);
extern _X_EXPORT int
rdpRegionPixelCount(RegionPtr pReg);

#endif
