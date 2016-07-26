/*
Copyright 2011-2013 Jay Sorg

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

RandR extension implementation

*/

#include "rdp.h"
#include "rdprandr.h"

#if 1
#define DEBUG_OUT(arg)
#else
#define DEBUG_OUT(arg) ErrorF arg
#endif

extern rdpScreenInfoRec g_rdpScreen; /* from rdpmain.c */
extern DeviceIntPtr g_pointer; /* in rdpmain.c */
extern DeviceIntPtr g_keyboard; /* in rdpmain.c */
extern ScreenPtr g_pScreen; /* in rdpmain.c */
extern WindowPtr g_invalidate_window; /* in rdpmain.c */

static XID g_wid = 0;

static int g_panning = 0;

#define LOG_LEVEL 1
#define LLOG(_level, _args) \
    do { if (_level < LOG_LEVEL) { ErrorF _args ; } } while (0)
#define LLOGLN(_level, _args) \
    do { if (_level < LOG_LEVEL) { ErrorF _args ; ErrorF("\n"); } } while (0)

/******************************************************************************/
Bool
rdpRRRegisterSize(ScreenPtr pScreen, int width, int height)
{
    int mmwidth;
    int mmheight;
    RRScreenSizePtr pSize;

    ErrorF("rdpRRRegisterSize: width %d height %d\n", width, height);
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
    ErrorF("rdpRRSetConfig:\n");
    return TRUE;
}

/******************************************************************************/
Bool
rdpRRGetInfo(ScreenPtr pScreen, Rotation *pRotations)
{
    ErrorF("rdpRRGetInfo:\n");
    *pRotations = RR_Rotate_0;
    return TRUE;
}

/******************************************************************************/
/* for lack of a better way, a window is created that covers the area and
   when its deleted, it's invalidated */
static int
rdpInvalidateArea(ScreenPtr pScreen, int x, int y, int cx, int cy)
{
    WindowPtr pWin;
    int result;
    int attri;
    XID attributes[4];
    Mask mask;

    DEBUG_OUT(("rdpInvalidateArea:\n"));
    mask = 0;
    attri = 0;
    attributes[attri++] = pScreen->blackPixel;
    mask |= CWBackPixel;
    attributes[attri++] = xTrue;
    mask |= CWOverrideRedirect;

    if (g_wid == 0)
    {
        g_wid = FakeClientID(0);
    }

    pWin = CreateWindow(g_wid, pScreen->root,
                        x, y, cx, cy, 0, InputOutput, mask,
                        attributes, 0, serverClient,
                        wVisual(pScreen->root), &result);

    if (result == 0)
    {
        g_invalidate_window = pWin;
        MapWindow(pWin, serverClient);
        DeleteWindow(pWin, None);
        g_invalidate_window = pWin;
    }

    return 0;
}

/******************************************************************************/
Bool
rdpRRScreenSetSize(ScreenPtr pScreen, CARD16 width, CARD16 height,
                   CARD32 mmWidth, CARD32 mmHeight)
{
    PixmapPtr screenPixmap;
    BoxRec box;

    ErrorF("rdpRRScreenSetSize: width %d height %d mmWidth %d mmHeight %d\n",
           width, height, (int)mmWidth, (int)mmHeight);

    if ((width < 1) || (height < 1))
    {
        ErrorF("  error width %d height %d\n", width, height);
        return FALSE;
    }

    g_rdpScreen.width = width;
    g_rdpScreen.height = height;
    g_rdpScreen.paddedWidthInBytes =
        PixmapBytePad(g_rdpScreen.width, g_rdpScreen.depth);
    g_rdpScreen.sizeInBytes =
        g_rdpScreen.paddedWidthInBytes * g_rdpScreen.height;
    pScreen->width = width;
    pScreen->height = height;
    pScreen->mmWidth = mmWidth;
    pScreen->mmHeight = mmHeight;

    screenPixmap = pScreen->GetScreenPixmap(pScreen);

    if (screenPixmap != 0)
    {
        ErrorF("  resizing screenPixmap [%p] to %dx%d, currently at %dx%d\n",
               (void *)screenPixmap, width, height,
               screenPixmap->drawable.width, screenPixmap->drawable.height);
        if (g_rdpScreen.sizeInBytes > g_rdpScreen.sizeInBytesAlloc)
        {
            g_free(g_rdpScreen.pfbMemory);
            g_rdpScreen.pfbMemory = (char*)g_malloc(g_rdpScreen.sizeInBytes, 1);
            g_rdpScreen.sizeInBytesAlloc = g_rdpScreen.sizeInBytes;
            ErrorF("new buffer size %d\n", g_rdpScreen.sizeInBytes);
        }
        pScreen->ModifyPixmapHeader(screenPixmap, width, height,
                                    g_rdpScreen.depth, g_rdpScreen.bitsPerPixel,
                                    g_rdpScreen.paddedWidthInBytes,
                                    g_rdpScreen.pfbMemory);
        ErrorF("  pixmap resized to %dx%d\n",
               screenPixmap->drawable.width, screenPixmap->drawable.height);
    }

    DEBUG_OUT(("  root window %p\n", (void *)pScreen->root));
    box.x1 = 0;
    box.y1 = 0;
    box.x2 = width;
    box.y2 = height;
    RegionInit(&pScreen->root->winSize, &box, 1);
    RegionInit(&pScreen->root->borderSize, &box, 1);
    RegionReset(&pScreen->root->borderClip, &box);
    RegionBreak(&pScreen->root->clipList);
    pScreen->root->drawable.width = width;
    pScreen->root->drawable.height = height;
    ResizeChildrenWinSize(pScreen->root, 0, 0, 0, 0);
    RRGetInfo(pScreen, 1);
    RRScreenSizeNotify(pScreen);
    rdpInvalidateArea(g_pScreen, 0, 0, g_rdpScreen.width, g_rdpScreen.height);
    ErrorF("  screen resized to %dx%d\n",
           pScreen->width, pScreen->height);
    return TRUE;
}

/******************************************************************************/
Bool
rdpRRCrtcSet(ScreenPtr pScreen, RRCrtcPtr crtc, RRModePtr mode,
             int x, int y, Rotation rotation, int numOutputs,
             RROutputPtr *outputs)
{
    ErrorF("rdpRRCrtcSet:\n");
    return TRUE;
}

/******************************************************************************/
Bool
rdpRRCrtcSetGamma(ScreenPtr pScreen, RRCrtcPtr crtc)
{
    ErrorF("rdpRRCrtcSetGamma:\n");
    return TRUE;
}

/******************************************************************************/
Bool
rdpRRCrtcGetGamma(ScreenPtr pScreen, RRCrtcPtr crtc)
{
    ErrorF("rdpRRCrtcGetGamma:\n");
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
    ErrorF("rdpRROutputSetProperty:\n");
    return TRUE;
}

/******************************************************************************/
Bool
rdpRROutputValidateMode(ScreenPtr pScreen, RROutputPtr output,
                        RRModePtr mode)
{
    ErrorF("rdpRROutputValidateMode:\n");
    return TRUE;
}

/******************************************************************************/
void
rdpRRModeDestroy(ScreenPtr pScreen, RRModePtr mode)
{
    ErrorF("rdpRRModeDestroy:\n");
}

/******************************************************************************/
Bool
rdpRROutputGetProperty(ScreenPtr pScreen, RROutputPtr output, Atom property)
{
    ErrorF("rdpRROutputGetProperty:\n");
    return TRUE;
}

/******************************************************************************/
Bool
rdpRRGetPanning(ScreenPtr pScrn, RRCrtcPtr crtc, BoxPtr totalArea,
                BoxPtr trackingArea, INT16 *border)
{
    ErrorF("rdpRRGetPanning:\n");

    if (!g_panning)
    {
        return FALSE;
    }

    if (totalArea != 0)
    {
        totalArea->x1 = 0;
        totalArea->y1 = 0;
        totalArea->x2 = g_rdpScreen.width;
        totalArea->y2 = g_rdpScreen.height;
    }

    if (trackingArea != 0)
    {
        trackingArea->x1 = 0;
        trackingArea->y1 = 0;
        trackingArea->x2 = g_rdpScreen.width;
        trackingArea->y2 = g_rdpScreen.height;
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
rdpRRSetPanning(ScreenPtr pScrn, RRCrtcPtr crtc, BoxPtr totalArea,
                BoxPtr trackingArea, INT16 *border)
{
    ErrorF("rdpRRSetPanning:\n");
    return TRUE;
}

/******************************************************************************/
static RROutputPtr
rdpRRAddOutput(const char *aname, int x, int y, int width, int height)
{
    RRModePtr mode;
    RRCrtcPtr crtc;
    RROutputPtr output;
    xRRModeInfo modeInfo;
    char name[64];

    sprintf (name, "%dx%d", width, height);
    memset (&modeInfo, 0, sizeof(modeInfo));
    modeInfo.width = width;
    modeInfo.height = height;
    modeInfo.nameLength = strlen(name);
    mode = RRModeGet(&modeInfo, name);
    if (mode == 0)
    {
        LLOGLN(0, ("rdpRRAddOutput: RRModeGet failed"));
        return 0;
    }

    crtc = RRCrtcCreate(g_pScreen, NULL);
    if (crtc == 0)
    {
        LLOGLN(0, ("rdpRRAddOutput: RRCrtcCreate failed"));
        RRModeDestroy(mode);
        return 0;
    }
    output = RROutputCreate(g_pScreen, aname, strlen(aname), NULL);
    if (output == 0)
    {
        LLOGLN(0, ("rdpRRAddOutput: RROutputCreate failed"));
        RRCrtcDestroy(crtc);
        RRModeDestroy(mode);
        return 0;
    }
    if (!RROutputSetClones(output, NULL, 0))
    {
        LLOGLN(0, ("rdpRRAddOutput: RROutputSetClones failed"));
    }
    if (!RROutputSetModes(output, &mode, 1, 0))
    {
        LLOGLN(0, ("rdpRRAddOutput: RROutputSetModes failed"));
    }
    if (!RROutputSetCrtcs(output, &crtc, 1))
    {
        LLOGLN(0, ("rdpRRAddOutput: RROutputSetCrtcs failed"));
    }
    if (!RROutputSetConnection(output, RR_Connected))
    {
        LLOGLN(0, ("rdpRRAddOutput: RROutputSetConnection failed"));
    }
    RRCrtcNotify(crtc, mode, x, y, RR_Rotate_0, NULL, 1, &output);

    return output;
}

/******************************************************************************/
static void
RRSetPrimaryOutput(rrScrPrivPtr pScrPriv, RROutputPtr output)
{
    if (pScrPriv->primaryOutput == output)
    {
        return;
    }
    /* clear the old primary */
    if (pScrPriv->primaryOutput)
    {
        RROutputChanged(pScrPriv->primaryOutput, 0);
        pScrPriv->primaryOutput = NULL;
    }
    /* set the new primary */
    if (output)
    {
        pScrPriv->primaryOutput = output;
        RROutputChanged(output, 0);
    }
    pScrPriv->layoutChanged = TRUE;
}

/******************************************************************************/
int
rdpRRSetRdpOutputs(void)
{
    rrScrPrivPtr pRRScrPriv;
    int index;
    int width;
    int height;
    char text[256];
    RROutputPtr output;

    pRRScrPriv = rrGetScrPriv(g_pScreen);

    LLOGLN(0, ("rdpRRSetRdpOutputs: numCrtcs %d", pRRScrPriv->numCrtcs));
    while (pRRScrPriv->numCrtcs > 0)
    {
        RRCrtcDestroy(pRRScrPriv->crtcs[0]);
    }
    LLOGLN(0, ("rdpRRSetRdpOutputs: numOutputs %d", pRRScrPriv->numOutputs));
    while (pRRScrPriv->numOutputs > 0)
    {
        RROutputDestroy(pRRScrPriv->outputs[0]);
    }

    if (g_rdpScreen.client_info.monitorCount == 0)
    {
        rdpRRAddOutput("rdp0", 0, 0, g_rdpScreen.width, g_rdpScreen.height);
    }
    else
    {
        for (index = 0; index < g_rdpScreen.client_info.monitorCount; index++)
        {
            snprintf(text, 255, "rdp%d", index);
            width = g_rdpScreen.client_info.minfo[index].right - g_rdpScreen.client_info.minfo[index].left + 1;
            height = g_rdpScreen.client_info.minfo[index].bottom - g_rdpScreen.client_info.minfo[index].top + 1;
            output = rdpRRAddOutput(text,
                                    g_rdpScreen.client_info.minfo[index].left,
                                    g_rdpScreen.client_info.minfo[index].top,
                                    width, height);
            if ((output != 0) && (g_rdpScreen.client_info.minfo[index].is_primary))
            {
                RRSetPrimaryOutput(pRRScrPriv, output);
            }
        }
    }

#if 0
    for (index = 0; index < pRRScrPriv->numOutputs; index++)
    {
        RROutputSetCrtcs(pRRScrPriv->outputs[index], pRRScrPriv->crtcs,
                         pRRScrPriv->numCrtcs);
    }
#endif

    return 0;
}

