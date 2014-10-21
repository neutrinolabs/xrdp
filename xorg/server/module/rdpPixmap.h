/*
Copyright 2005-2014 Jay Sorg

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

pixmap calls

*/

#ifndef __RDPPIXMAP_H
#define __RDPPIXAMP_H

#include <xorg-server.h>
#include <xorgVersion.h>
#include <xf86.h>

#if XORG_VERSION_CURRENT < XORG_VERSION_NUMERIC(1, 5, 0, 0, 0)
/* 1.1, 1.2, 1.3, 1.4 */
#define XRDP_PIX 1
#else
/* 1.5, 1.6, 1.7, 1.8, 1.9, 1.10, 1.11, 1.12 */
#define XRDP_PIX 2
#endif

#if XRDP_PIX == 2
extern _X_EXPORT PixmapPtr
rdpCreatePixmap(ScreenPtr pScreen, int width, int height, int depth,
                unsigned usage_hint);
#else
extern _X_EXPORT PixmapPtr
rdpCreatePixmap(ScreenPtr pScreen, int width, int height, int depth);
#endif
extern _X_EXPORT Bool
rdpDestroyPixmap(PixmapPtr pPixmap);
extern _X_EXPORT Bool
rdpModifyPixmapHeader(PixmapPtr pPixmap, int width, int height, int depth,
                      int bitsPerPixel, int devKind, pointer pPixData);

#endif
