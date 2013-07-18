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

to deal with privates changing in xorg versions

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* this should be before all X11 .h files */
#include <xorg-server.h>

/* all driver need this */
#include <xf86.h>
#include <xf86_OSproc.h>

#include <mipointer.h>
#include <fb.h>
#include <micmap.h>
#include <mi.h>

#include "rdpPri.h"

#if XORG_VERSION_CURRENT < (((1) * 10000000) + ((5) * 100000) + ((0) * 1000) + 0)
/* 1.1, 1.2, 1.3, 1.4 */
#define XRDP_PRI 1
#elif XORG_VERSION_CURRENT < (((1) * 10000000) + ((9) * 100000) + ((0) * 1000) + 0)
/* 1.5, 1.6, 1.7, 1.8 */
#define XRDP_PRI 2
#else
/* 1.9, 1.10, 1.11, 1.12 */
#define XRDP_PRI 3
#endif

#define PTR2INT(_ptr) ((int)    ((long) ((void*) (_ptr))))
#define INT2PTR(_int) ((void *) ((long) ((int)   (_int))))

#if XRDP_PRI == 3
static DevPrivateKeyRec g_privateKeyRecGC;
static DevPrivateKeyRec g_privateKeyRecPixmap;
static DevPrivateKeyRec g_privateKeyRecWindow;
#elif XRDP_PRI == 2
static int g_privateKeyRecGC = 0;
static int g_privateKeyRecPixmap = 0;
static int g_privateKeyRecWindow = 0;
#endif

/*****************************************************************************/
rdpDevPrivateKey
rdpAllocateGCPrivate(ScreenPtr pScreen, int bytes)
{
    rdpDevPrivateKey rv;

#if XRDP_PRI == 1
    rv = INT2PTR(AllocateGCPrivateIndex());
    AllocateGCPrivate(pScreen, PTR2INT(rv), bytes);
#elif XRDP_PRI == 2
    dixRequestPrivate(&g_privateKeyRecGC, bytes);
    rv = &g_privateKeyRecGC;
#else
    dixRegisterPrivateKey(&g_privateKeyRecGC, PRIVATE_GC, bytes);
    rv = &g_privateKeyRecGC;
#endif
    return rv;
}

/*****************************************************************************/
rdpDevPrivateKey
rdpAllocatePixmapPrivate(ScreenPtr pScreen, int bytes)
{
    rdpDevPrivateKey rv;

#if XRDP_PRI == 1
    rv = INT2PTR(AllocatePixmapPrivateIndex());
    AllocatePixmapPrivate(pScreen, PTR2INT(rv), bytes);
#elif XRDP_PRI == 2
    dixRequestPrivate(&g_privateKeyRecPixmap, bytes);
    rv = &g_privateKeyRecPixmap;
#else
    dixRegisterPrivateKey(&g_privateKeyRecPixmap, PRIVATE_PIXMAP, bytes);
    rv = &g_privateKeyRecPixmap;
#endif
    return rv;
}

/*****************************************************************************/
rdpDevPrivateKey
rdpAllocateWindowPrivate(ScreenPtr pScreen, int bytes)
{
    rdpDevPrivateKey rv;

#if XRDP_PRI == 1
    rv = INT2PTR(AllocateWindowPrivateIndex());
    AllocateWindowPrivate(pScreen, PTR2INT(rv), bytes);
#elif XRDP_PRI == 2
    dixRequestPrivate(&g_privateKeyRecWindow, bytes);
    rv = &g_privateKeyRecWindow;
#else
    dixRegisterPrivateKey(&g_privateKeyRecWindow, PRIVATE_WINDOW, bytes);
    rv = &g_privateKeyRecWindow;
#endif
    return rv;
}

/*****************************************************************************/
void *
rdpGetGCPrivate(GCPtr pGC, rdpDevPrivateKey key)
{
    void *rv;

#if XRDP_PRI == 1
    rv = pGC->devPrivates[PTR2INT(key)].ptr;
#else
    rv = dixLookupPrivate(&(pGC->devPrivates), key);
#endif
    return rv;
}

/*****************************************************************************/
void *
rdpGetPixmapPrivate(PixmapPtr pPixmap, rdpDevPrivateKey key)
{
    void *rv;

#if XRDP_PRI == 1
    rv = pPixmap->devPrivates[PTR2INT(key)].ptr;
#else
    rv = dixLookupPrivate(&(pPixmap->devPrivates), key);
#endif
    return rv;
}

/*****************************************************************************/
void *
rdpGetWindowPrivate(WindowPtr pWindow, rdpDevPrivateKey key)
{
    void *rv;

#if XRDP_PRI == 1
    rv = pWindow->devPrivates[PTR2INT(key)].ptr;
#else
    rv = dixLookupPrivate(&(pWindow->devPrivates), key);
#endif
    return rv;
}

/*****************************************************************************/
int
rdpPrivateInit(void)
{
#if XRDP_PRI == 3
    memset(&g_privateKeyRecGC, 0, sizeof(g_privateKeyRecGC));
    memset(&g_privateKeyRecWindow, 0, sizeof(g_privateKeyRecWindow));
    memset(&g_privateKeyRecPixmap, 0, sizeof(g_privateKeyRecPixmap));
#endif
    return 0;
}
