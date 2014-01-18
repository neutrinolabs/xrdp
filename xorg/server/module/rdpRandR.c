/*
Copyright 2011-2014 Jay Sorg

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

RandR draw calls

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
#include "rdpDraw.h"
#include "rdpReg.h"
#include "rdpMisc.h"

/******************************************************************************/
#define LOG_LEVEL 1
#define LLOGLN(_level, _args) \
    do { if (_level < LOG_LEVEL) { ErrorF _args ; ErrorF("\n"); } } while (0)

/******************************************************************************/
Bool
rdpRRRegisterSize(ScreenPtr pScreen, int width, int height)
{
    int mmwidth;
    int mmheight;
    RRScreenSizePtr pSize;

    LLOGLN(0, ("rdpRRRegisterSize: width %d height %d", width, height));
    mmwidth = PixelToMM(width);
    mmheight = PixelToMM(height);
    pSize = RRRegisterSize(pScreen, width, height, mmwidth, mmheight);
    /* Tell RandR what the current config is */
    RRSetCurrentConfig(pScreen, RR_Rotate_0, 0, pSize);
    return TRUE;
}

/******************************************************************************/
Bool
rdpRRSetConfig(ScreenPtr pScreen, Rotation rotateKind, int rate,
               RRScreenSizePtr pSize)
{
    LLOGLN(0, ("rdpRRSetConfig:"));
    return TRUE;
}

/******************************************************************************/
Bool
rdpRRGetInfo(ScreenPtr pScreen, Rotation *pRotations)
{
    int width;
    int height;
    rdpPtr dev;

    LLOGLN(0, ("rdpRRGetInfo:"));
    dev = rdpGetDevFromScreen(pScreen);
    *pRotations = RR_Rotate_0;
    width = dev->width;
    height = dev->height;
    rdpRRRegisterSize(pScreen, width, height);
    return TRUE;
}

/******************************************************************************/
Bool
rdpRRScreenSetSize(ScreenPtr pScreen, CARD16 width, CARD16 height,
                   CARD32 mmWidth, CARD32 mmHeight)
{
    WindowPtr root;
    PixmapPtr screenPixmap;
    BoxRec box;
    rdpPtr dev;

    LLOGLN(0, ("rdpRRScreenSetSize: width %d height %d mmWidth %d mmHeight %d",
           width, height, (int)mmWidth, (int)mmHeight));
    dev = rdpGetDevFromScreen(pScreen);
    root = rdpGetRootWindowPtr(pScreen);
    if ((width < 1) || (height < 1))
    {
        LLOGLN(10, ("  error width %d height %d", width, height));
        return FALSE;
    }
    dev->width = width;
    dev->height = height;
    dev->paddedWidthInBytes = PixmapBytePad(dev->width, dev->depth);
    dev->sizeInBytes = dev->paddedWidthInBytes * dev->height;
    pScreen->width = width;
    pScreen->height = height;
    pScreen->mmWidth = mmWidth;
    pScreen->mmHeight = mmHeight;
    screenPixmap = pScreen->GetScreenPixmap(pScreen);
    g_free(dev->pfbMemory);
    dev->pfbMemory = (char *) g_malloc(dev->sizeInBytes, 1);
    if (screenPixmap != 0)
    {
        pScreen->ModifyPixmapHeader(screenPixmap, width, height,
                                    -1, -1,
                                    dev->paddedWidthInBytes,
                                    dev->pfbMemory);
    }
    box.x1 = 0;
    box.y1 = 0;
    box.x2 = width;
    box.y2 = height;
    rdpRegionInit(&root->winSize, &box, 1);
    rdpRegionInit(&root->borderSize, &box, 1);
    rdpRegionReset(&root->borderClip, &box);
    rdpRegionBreak(&root->clipList);
    root->drawable.width = width;
    root->drawable.height = height;
    ResizeChildrenWinSize(root, 0, 0, 0, 0);
    RRGetInfo(pScreen, 1);
    LLOGLN(0, ("  screen resized to %dx%d", pScreen->width, pScreen->height));
    RRScreenSizeNotify(pScreen);
    xf86EnableDisableFBAccess(pScreen->myNum, FALSE);
    xf86EnableDisableFBAccess(pScreen->myNum, TRUE);
    return TRUE;
}

/******************************************************************************/
Bool
rdpRRCrtcSet(ScreenPtr pScreen, RRCrtcPtr crtc, RRModePtr mode,
             int x, int y, Rotation rotation, int numOutputs,
             RROutputPtr *outputs)
{
    LLOGLN(0, ("rdpRRCrtcSet:"));
    return TRUE;
}

/******************************************************************************/
Bool
rdpRRCrtcSetGamma(ScreenPtr pScreen, RRCrtcPtr crtc)
{
    LLOGLN(0, ("rdpRRCrtcSetGamma:"));
    return TRUE;
}

/******************************************************************************/
Bool
rdpRRCrtcGetGamma(ScreenPtr pScreen, RRCrtcPtr crtc)
{
    LLOGLN(0, ("rdpRRCrtcGetGamma: %p %p %p %p", crtc, crtc->gammaRed,
           crtc->gammaBlue, crtc->gammaGreen));
    crtc->gammaSize = 1;
    if (crtc->gammaRed == NULL)
    {
        crtc->gammaRed = g_malloc(32, 1);
    }
    if (crtc->gammaBlue == NULL)
    {
        crtc->gammaBlue = g_malloc(32, 1);
    }
    if (crtc->gammaGreen == NULL)
    {
        crtc->gammaGreen = g_malloc(32, 1);
    }
    return TRUE;
}

/******************************************************************************/
Bool
rdpRROutputSetProperty(ScreenPtr pScreen, RROutputPtr output, Atom property,
                       RRPropertyValuePtr value)
{
    LLOGLN(0, ("rdpRROutputSetProperty:"));
    return TRUE;
}

/******************************************************************************/
Bool
rdpRROutputValidateMode(ScreenPtr pScreen, RROutputPtr output,
                        RRModePtr mode)
{
    LLOGLN(0, ("rdpRROutputValidateMode:"));
    return TRUE;
}

/******************************************************************************/
void
rdpRRModeDestroy(ScreenPtr pScreen, RRModePtr mode)
{
    LLOGLN(0, ("rdpRRModeDestroy:"));
}

/******************************************************************************/
Bool
rdpRROutputGetProperty(ScreenPtr pScreen, RROutputPtr output, Atom property)
{
    LLOGLN(0, ("rdpRROutputGetProperty:"));
    return TRUE;
}

/******************************************************************************/
Bool
rdpRRGetPanning(ScreenPtr pScreen, RRCrtcPtr crtc, BoxPtr totalArea,
                BoxPtr trackingArea, INT16 *border)
{
    rdpPtr dev;

    LLOGLN(0, ("rdpRRGetPanning: %p", crtc));
    dev = rdpGetDevFromScreen(pScreen);

    if (totalArea != 0)
    {
        totalArea->x1 = 0;
        totalArea->y1 = 0;
        totalArea->x2 = dev->width;
        totalArea->y2 = dev->height;
    }

    if (trackingArea != 0)
    {
        trackingArea->x1 = 0;
        trackingArea->y1 = 0;
        trackingArea->x2 = dev->width;
        trackingArea->y2 = dev->height;
    }

    if (border != 0)
    {
        border[0] = 0;
        border[1] = 0;
        border[2] = 0;
        border[3] = 0;
    }
    return TRUE;
}

/******************************************************************************/
Bool
rdpRRSetPanning(ScreenPtr pScreen, RRCrtcPtr crtc, BoxPtr totalArea,
                BoxPtr trackingArea, INT16 *border)
{
    LLOGLN(0, ("rdpRRSetPanning:"));
    return TRUE;
}
