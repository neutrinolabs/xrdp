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
Bool
rdpCloseScreen(int index, ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn;
    rdpPtr dev;
    Bool rv;

    LLOGLN(0, ("rdpCloseScreen:"));
    pScrn = xf86Screens[pScreen->myNum];
    dev = XRDPPTR(pScrn);
    dev->pScreen->CloseScreen = dev->CloseScreen;
    rv = dev->pScreen->CloseScreen(index, pScreen);
    dev->pScreen->CloseScreen = rdpCloseScreen;
    return rv;
}

/******************************************************************************/
WindowPtr
rdpGetRootWindowPtr(ScreenPtr pScreen)
{
#if XORG_VERSION_CURRENT < (((1) * 10000000) + ((9) * 100000) + ((0) * 1000) + 0)
    return WindowTable[pScreen->myNum]; /* in globals.c */
#else
    return pScreen->root;
#endif
}
