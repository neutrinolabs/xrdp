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

#ifndef __RDPDRAW_H
#define __RDPDRAW_H

/******************************************************************************/
#define GC_OP_PROLOGUE(_pGC) \
{ \
  priv = (rdpGCPtr)dixGetPrivateAddr(&(pGC->devPrivates), &g_rdpGCIndex); \
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

int
rdp_get_clip(RegionPtr pRegion, DrawablePtr pDrawable, GCPtr pGC);
void
GetTextBoundingBox(DrawablePtr pDrawable, FontPtr font, int x, int y,
                   int n, BoxPtr pbox);

#endif
