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

#ifndef __RDPREG_H
#define __RDPREG_H

Bool
rdpRegionCopy(RegionPtr dst, RegionPtr src);
void
rdpRegionTranslate(RegionPtr pReg, int x, int y);
Bool
rdpRegionNotEmpty(RegionPtr pReg);
Bool
rdpRegionIntersect(RegionPtr newReg, RegionPtr reg1, RegionPtr reg2);
int
rdpRegionContainsRect(RegionPtr region, BoxPtr prect);
void
rdpRegionInit(RegionPtr pReg, BoxPtr rect, int size);
void
rdpRegionUninit(RegionPtr pReg);
RegionPtr
rdpRegionFromRects(int nrects, xRectanglePtr prect, int ctype);
void
rdpRegionDestroy(RegionPtr pReg);
RegionPtr
rdpRegionCreate(BoxPtr rect, int size);
Bool
rdpRegionUnion(RegionPtr newReg, RegionPtr reg1, RegionPtr reg2);
Bool
rdpRegionSubtract(RegionPtr newReg, RegionPtr reg1, RegionPtr reg2);
Bool
rdpRegionInverse(RegionPtr newReg, RegionPtr reg1, BoxPtr invRect);
BoxPtr
rdpRegionExtents(RegionPtr pReg);
void
rdpRegionReset(RegionPtr pReg, BoxPtr pBox);
Bool
rdpRegionBreak(RegionPtr pReg);

#endif
