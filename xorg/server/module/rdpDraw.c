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

misc draw calls

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

#include "rdp.h"

#define XRDP_DRIVER_NAME "XRDPDEV"
#define XRDP_NAME "XRDPDEV"
#define XRDP_VERSION 1000

#define PACKAGE_VERSION_MAJOR 1
#define PACKAGE_VERSION_MINOR 0
#define PACKAGE_VERSION_PATCHLEVEL 0

/******************************************************************************/
#define LOG_LEVEL 1
#define LLOGLN(_level, _args) \
    do { if (_level < LOG_LEVEL) { ErrorF _args ; ErrorF("\n"); } } while (0)

/*****************************************************************************/
PixmapPtr
rdpCreatePixmap(ScreenPtr pScreen, int width, int height, int depth,
                unsigned usage_hint)
{
    ScrnInfoPtr pScrn;
    rdpPtr dev;
    PixmapPtr rv;

    pScrn = xf86Screens[pScreen->myNum];
    dev = XRDPPTR(pScrn);
    pScreen->CreatePixmap = dev->CreatePixmap;
    rv = pScreen->CreatePixmap(pScreen, 0, 0, 0, 0);
    pScreen->CreatePixmap = rdpCreatePixmap;
    return rv;
}

/******************************************************************************/
Bool
rdpDestroyPixmap(PixmapPtr pPixmap)
{
  Bool rv;
  ScreenPtr pScreen;
  rdpPtr dev;
  ScrnInfoPtr pScrn;

  LLOGLN(10, ("rdpDestroyPixmap: refcnt %d", pPixmap->refcnt));
  pScreen = pPixmap->drawable.pScreen;
  pScrn = xf86Screens[pScreen->myNum];
  dev = XRDPPTR(pScrn);
  pScreen->DestroyPixmap = dev->DestroyPixmap;
  rv = pScreen->DestroyPixmap(pPixmap);
  pScreen->DestroyPixmap = rdpDestroyPixmap;
  return rv;
}

/******************************************************************************/
Bool
rdpModifyPixmapHeader(PixmapPtr pPixmap, int width, int height, int depth,
                      int bitsPerPixel, int devKind, pointer pPixData)
{
  Bool rv;
  ScreenPtr pScreen;
  rdpPtr dev;
  ScrnInfoPtr pScrn;

  pScreen = pPixmap->drawable.pScreen;
  pScrn = xf86Screens[pScreen->myNum];
  dev = XRDPPTR(pScrn);
  pScreen->ModifyPixmapHeader = dev->ModifyPixmapHeader;
  rv = pScreen->ModifyPixmapHeader(pPixmap, width, height, depth, bitsPerPixel,
                                   devKind, pPixData);
  pScreen->ModifyPixmapHeader = rdpModifyPixmapHeader;
  return rv;
}

/*****************************************************************************/
void
rdpCopyWindow(WindowPtr pWin, DDXPointRec ptOldOrg, RegionPtr pOldRegion)
{
    ScrnInfoPtr pScrn;
    ScreenPtr pScreen;
    rdpPtr dev;

    pScreen = pWin->drawable.pScreen;
    pScrn = xf86Screens[pScreen->myNum];
    dev = XRDPPTR(pScrn);
    dev->pScreen->CopyWindow = dev->CopyWindow;
    dev->pScreen->CopyWindow(pWin, ptOldOrg, pOldRegion);
    dev->pScreen->CopyWindow = rdpCopyWindow;
}

/*****************************************************************************/
static pointer
RDPSetup(pointer Module, pointer Options, int *ErrorMajor, int *ErrorMinor)
{
    static int initialised = 0;

    LLOGLN(0, ("RDPSetup:"));
    if (!initialised)
    {
        initialised = 1;
        //xf86AddModuleInfo(&THINC, Module);
        //LoaderRefSymLists(cursorSymbols, NULL);
    }
    return (pointer) 1;
}

static MODULESETUPPROTO(RDPSetup);
static XF86ModuleVersionInfo RDPVersRec =
{
    XRDP_DRIVER_NAME,
    MODULEVENDORSTRING,
    MODINFOSTRING1,
    MODINFOSTRING2,
    XORG_VERSION_CURRENT,
    PACKAGE_VERSION_MAJOR,
    PACKAGE_VERSION_MINOR,
    PACKAGE_VERSION_PATCHLEVEL,
    ABI_CLASS_VIDEODRV,
    ABI_VIDEODRV_VERSION,
    0,
    { 0, 0, 0, 0 }
};

XF86ModuleData xorgxrdpModuleData = { &RDPVersRec, RDPSetup, NULL };
