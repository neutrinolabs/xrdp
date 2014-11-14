/*
Copyright 2014 Jay Sorg

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

XVideo

*/

#ifndef __RDPXV_H
#define __RDPXV_H

#include <xorg-server.h>
#include <xorgVersion.h>
#include <xf86.h>

extern _X_EXPORT Bool
rdpXvInit(ScreenPtr pScreen, ScrnInfoPtr pScrn);
extern _X_EXPORT int
YV12_to_RGB32(unsigned char *yuvs, int width, int height, int *rgbs);
extern _X_EXPORT int
I420_to_RGB32(unsigned char *yuvs, int width, int height, int *rgbs);
extern _X_EXPORT int
YUY2_to_RGB32(unsigned char *yuvs, int width, int height, int *rgbs);
extern _X_EXPORT int
UYVY_to_RGB32(unsigned char *yuvs, int width, int height, int *rgbs);

#endif
