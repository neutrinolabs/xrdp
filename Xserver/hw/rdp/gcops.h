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

*/

static void
rdpValidateGC(GCPtr pGC, unsigned long changes, DrawablePtr d);
static void
rdpChangeGC(GCPtr pGC, unsigned long mask);
static void
rdpCopyGC(GCPtr src, unsigned long mask, GCPtr dst);
static void
rdpDestroyGC(GCPtr pGC);
static void
rdpChangeClip(GCPtr pGC, int type, pointer pValue, int nrects);
static void
rdpDestroyClip(GCPtr pGC);
static void
rdpCopyClip(GCPtr dst, GCPtr src);
static void
rdpFillSpans(DrawablePtr pDrawable, GCPtr pGC, int nInit,
             DDXPointPtr pptInit, int * pwidthInit, int fSorted);
static void
rdpSetSpans(DrawablePtr pDrawable, GCPtr pGC, char * psrc,
            DDXPointPtr ppt, int * pwidth, int nspans, int fSorted);
static void
rdpPutImage(DrawablePtr pDrawable, GCPtr pGC, int depth, int x, int y,
            int w, int h, int leftPad, int format, char * pBits);
static RegionPtr
rdpCopyArea(DrawablePtr pSrc, DrawablePtr pDst, GCPtr pGC,
            int srcx, int srcy, int w, int h, int dstx, int dsty);
static RegionPtr
rdpCopyPlane(DrawablePtr pSrcDrawable, DrawablePtr pDstDrawable,
             GCPtr pGC, int srcx, int srcy, int width, int height,
             int dstx, int dsty, unsigned long bitPlane);
static void
rdpPolyPoint(DrawablePtr pDrawable, GCPtr pGC, int mode,
             int npt, DDXPointPtr pptInit);
static void
rdpPolylines(DrawablePtr pDrawable, GCPtr pGC, int mode,
             int npt, DDXPointPtr pptInit);
static void
rdpPolySegment(DrawablePtr pDrawable, GCPtr pGC, int nseg, xSegment * pSegs);
static void
rdpPolyRectangle(DrawablePtr pDrawable, GCPtr pGC, int nrects,
                 xRectangle * pRects);
static void
rdpPolyArc(DrawablePtr pDrawable, GCPtr pGC, int narcs, xArc * parcs);
static void
rdpFillPolygon(DrawablePtr pDrawable, GCPtr pGC,
               int shape, int mode, int count,
               DDXPointPtr pPts);
static void
rdpPolyFillRect(DrawablePtr pDrawable, GCPtr pGC, int nrectFill,
                xRectangle * prectInit);
static void
rdpPolyFillArc(DrawablePtr pDrawable, GCPtr pGC, int narcs, xArc * parcs);
static int
rdpPolyText8(DrawablePtr pDrawable, GCPtr pGC,
             int x, int y, int count, char * chars);
static int
rdpPolyText16(DrawablePtr pDrawable, GCPtr pGC,
              int x, int y, int count, unsigned short * chars);
static void
rdpImageText8(DrawablePtr pDrawable, GCPtr pGC,
              int x, int y, int count, char * chars);
static void
rdpImageText16(DrawablePtr pDrawable, GCPtr pGC,
               int x, int y, int count,
               unsigned short * chars);
static void
rdpImageGlyphBlt(DrawablePtr pDrawable, GCPtr pGC,
                 int x, int y, unsigned int nglyph,
                 CharInfoPtr * ppci, pointer pglyphBase);
static void
rdpPolyGlyphBlt(DrawablePtr pDrawable, GCPtr pGC,
                int x, int y, unsigned int nglyph,
                CharInfoPtr * ppci,
                pointer pglyphBase);
static void
rdpPushPixels(GCPtr pGC, PixmapPtr pBitMap, DrawablePtr pDst,
              int w, int h, int x, int y);
