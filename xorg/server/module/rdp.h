/*
Copyright 2005-2013 Jay Sorg

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

#ifndef _RDP_H
#define _RDP_H

#include <xorg-server.h>
#include <scrnintstr.h>
#include <gcstruct.h>
#include <mipointer.h>

#include "rdpPri.h"

/* move this to common header */
struct _rdpRec
{
    char data[1024];
    int width;
    int height;
    int num_modes;
    char *ptr;
    ScreenPtr pScreen;
    rdpDevPrivateKey privateKeyRecGC;
    rdpDevPrivateKey privateKeyRecPixmap;

    CopyWindowProcPtr CopyWindow;
    CreateGCProcPtr CreateGC;
    CreatePixmapProcPtr CreatePixmap;
    DestroyPixmapProcPtr DestroyPixmap;
    ModifyPixmapHeaderProcPtr ModifyPixmapHeader;

    miPointerScreenFuncPtr pCursorFuncs;

};
typedef struct _rdpRec rdpRec;
typedef struct _rdpRec * rdpPtr;
#define XRDPPTR(_p) ((rdpPtr)((_p)->driverPrivate))

struct _rdpGCRec
{
    GCFuncs *funcs;
    GCOps *ops;
};
typedef struct _rdpGCRec rdpGCRec;
typedef struct _rdpGCRec * rdpGCPtr;

struct _rdpPixmapRec
{
    int i1;
};
typedef struct _rdpPixmapRec rdpPixmapRec;
typedef struct _rdpPixmapRec * rdpPixmapPtr;

#endif
