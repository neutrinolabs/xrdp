/*
Copyright 2013-2014 Jay Sorg

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

This is the main driver file

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* this should be before all X11 .h files */
#include <xorg-server.h>
#include <xorgVersion.h>

/* all driver need this */
#include <xf86.h>
#include <xf86_OSproc.h>

#include <mipointer.h>
#include <fb.h>
#include <micmap.h>
#include <mi.h>
#include <randrstr.h>

#include <xf86Modes.h>

#include "rdp.h"
#include "rdpPri.h"
#include "rdpDraw.h"
#include "rdpGC.h"
#include "rdpCursor.h"
#include "rdpRandR.h"
#include "rdpMisc.h"
#include "rdpComposite.h"
#include "rdpTrapezoids.h"
#include "rdpGlyphs.h"
#include "rdpPixmap.h"
#include "rdpClientCon.h"
#include "rdpXv.h"
#include "rdpSimd.h"

#define LLOG_LEVEL 1
#define LLOGLN(_level, _args) \
  do \
  { \
    if (_level < LLOG_LEVEL) \
    { \
      ErrorF _args ; \
      ErrorF("\n"); \
    } \
  } \
  while (0)

int g_bpp = 32;
int g_depth = 24;
int g_rgb_bits = 8;
int g_redOffset = 16;
int g_redBits = 8;
int g_greenOffset = 8;
int g_greenBits = 8;
int g_blueOffset = 0;
int g_blueBits = 8;

static int g_setup_done = 0;
static OsTimerPtr g_timer = 0;

/* Supported "chipsets" */
static SymTabRec g_Chipsets[] =
{
    { 0, XRDP_DRIVER_NAME },
    { -1, 0 }
};

static XF86ModuleVersionInfo g_VersRec =
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

/*****************************************************************************/
static Bool
rdpAllocRec(ScrnInfoPtr pScrn)
{
    LLOGLN(10, ("rdpAllocRec:"));
    if (pScrn->driverPrivate != 0)
    {
        return TRUE;
    }
    /* xnfcalloc exits if alloc failed */
    pScrn->driverPrivate = xnfcalloc(sizeof(rdpRec), 1);
    return TRUE;
}

/*****************************************************************************/
static void
rdpFreeRec(ScrnInfoPtr pScrn)
{
    LLOGLN(10, ("rdpFreeRec:"));
    if (pScrn->driverPrivate == 0)
    {
        return;
    }
    free(pScrn->driverPrivate);
    pScrn->driverPrivate = 0;
}

/*****************************************************************************/
static Bool
rdpPreInit(ScrnInfoPtr pScrn, int flags)
{
    rgb zeros1;
    Gamma zeros2;
    int got_res_match;
    char **modename;
    DisplayModePtr mode;
    rdpPtr dev;

    LLOGLN(0, ("rdpPreInit:"));
    if (flags & PROBE_DETECT)
    {
        return FALSE;
    }
    if (pScrn->numEntities != 1)
    {
        return FALSE;
    }

    rdpAllocRec(pScrn);
    dev = XRDPPTR(pScrn);

    dev->width = 1024;
    dev->height = 768;

    pScrn->monitor = pScrn->confScreen->monitor;
    pScrn->bitsPerPixel = g_bpp;
    pScrn->virtualX = dev->width;
    pScrn->displayWidth = dev->width;
    pScrn->virtualY = dev->height;
    pScrn->progClock = 1;
    pScrn->rgbBits = g_rgb_bits;
    pScrn->depth = g_depth;
    pScrn->chipset = XRDP_DRIVER_NAME;
    pScrn->currentMode = pScrn->modes;
    pScrn->offset.blue = g_blueOffset;
    pScrn->offset.green = g_greenOffset;
    pScrn->offset.red = g_redOffset;
    pScrn->mask.blue = ((1 << g_blueBits) - 1) << pScrn->offset.blue;
    pScrn->mask.green = ((1 << g_greenBits) - 1) << pScrn->offset.green;
    pScrn->mask.red = ((1 << g_redBits) - 1) << pScrn->offset.red;

    if (!xf86SetDepthBpp(pScrn, g_depth, g_bpp, g_bpp,
                         Support24bppFb | Support32bppFb |
                         SupportConvert32to24 | SupportConvert24to32))
    {
        LLOGLN(0, ("rdpPreInit: xf86SetDepthBpp failed"));
        rdpFreeRec(pScrn);
        return FALSE;
    }
    xf86PrintDepthBpp(pScrn);
    g_memset(&zeros1, 0, sizeof(zeros1));
    if (!xf86SetWeight(pScrn, zeros1, zeros1))
    {
        LLOGLN(0, ("rdpPreInit: xf86SetWeight failed"));
        rdpFreeRec(pScrn);
        return FALSE;
    }
    g_memset(&zeros2, 0, sizeof(zeros2));
    if (!xf86SetGamma(pScrn, zeros2))
    {
        LLOGLN(0, ("rdpPreInit: xf86SetGamma failed"));
        rdpFreeRec(pScrn);
        return FALSE;
    }
    if (!xf86SetDefaultVisual(pScrn, -1))
    {
        LLOGLN(0, ("rdpPreInit: xf86SetDefaultVisual failed"));
        rdpFreeRec(pScrn);
        return FALSE;
    }
    xf86SetDpi(pScrn, 0, 0);
    if (0 == pScrn->display->modes)
    {
        LLOGLN(0, ("rdpPreInit: modes error"));
        rdpFreeRec(pScrn);
        return FALSE;
    }

    pScrn->virtualX = pScrn->display->virtualX;
    pScrn->virtualY = pScrn->display->virtualY;

    got_res_match = 0;
    for (modename = pScrn->display->modes; *modename != 0; modename++)
    {
        for (mode = pScrn->monitor->Modes; mode != 0; mode = mode->next)
        {
            LLOGLN(10, ("%s %s", mode->name, *modename));
            if (0 == strcmp(mode->name, *modename))
            {
                break;
            }
        }
        if (0 == mode)
        {
            xf86DrvMsg(pScrn->scrnIndex, X_INFO, "\tmode \"%s\" not found\n",
                       *modename);
            continue;
        }
        xf86DrvMsg(pScrn->scrnIndex, X_INFO, "\tmode \"%s\" ok\n", *modename);
        LLOGLN(10, ("%d %d %d %d", mode->HDisplay, dev->width,
               mode->VDisplay, dev->height));
        if ((mode->HDisplay == dev->width) && (mode->VDisplay == dev->height))
        {
            pScrn->virtualX = mode->HDisplay;
            pScrn->virtualY = mode->VDisplay;
            got_res_match = 1;
        }
        if (got_res_match)
        {
            pScrn->modes = xf86DuplicateMode(mode);
            pScrn->modes->next = pScrn->modes;
            pScrn->modes->prev = pScrn->modes;
            dev->num_modes = 1;
            break;
        }
    }
    pScrn->currentMode = pScrn->modes;
    xf86PrintModes(pScrn);
    LLOGLN(10, ("rdpPreInit: out fPtr->num_modes %d", dev->num_modes));
    if (!got_res_match)
    {
        LLOGLN(0, ("rdpPreInit: could not find screen resolution %dx%d",
               dev->width, dev->height));
        return FALSE;
    }
    return TRUE;
}

/******************************************************************************/
static miPointerSpriteFuncRec g_rdpSpritePointerFuncs =
{
    /* these are in rdpCursor.c */
    rdpSpriteRealizeCursor,
    rdpSpriteUnrealizeCursor,
    rdpSpriteSetCursor,
    rdpSpriteMoveCursor,
    rdpSpriteDeviceCursorInitialize,
    rdpSpriteDeviceCursorCleanup
};

/******************************************************************************/
static Bool
rdpSaveScreen(ScreenPtr pScreen, int on)
{
    LLOGLN(0, ("rdpSaveScreen:"));
    return TRUE;
}

/******************************************************************************/
static Bool
rdpResizeSession(rdpPtr dev, int width, int height)
{
    int mmwidth;
    int mmheight;
    RRScreenSizePtr pSize;
    Bool ok;

    LLOGLN(0, ("rdpResizeSession: width %d height %d", width, height));
    mmwidth = PixelToMM(width);
    mmheight = PixelToMM(height);

    pSize = RRRegisterSize(dev->pScreen, width, height, mmwidth, mmheight);
    RRSetCurrentConfig(dev->pScreen, RR_Rotate_0, 0, pSize);

    ok = TRUE;
    if ((dev->width != width) || (dev->height != height))
    {
        LLOGLN(0, ("  calling RRScreenSizeSet"));
        ok = RRScreenSizeSet(dev->pScreen, width, height, mmwidth, mmheight);
        LLOGLN(0, ("  RRScreenSizeSet ok %d", ok));
    }
    return ok;
}

/******************************************************************************/
/* returns error */
static CARD32
rdpDeferredRandR(OsTimerPtr timer, CARD32 now, pointer arg)
{
    ScreenPtr pScreen;
    rrScrPrivPtr pRRScrPriv;
    rdpPtr dev;
    char *envvar;
    int width;
    int height;

    pScreen = (ScreenPtr) arg;
    dev = rdpGetDevFromScreen(pScreen);
    LLOGLN(0, ("rdpDeferredRandR:"));
    pRRScrPriv = rrGetScrPriv(pScreen);
    if (pRRScrPriv == 0)
    {
        LLOGLN(0, ("rdpDeferredRandR: rrGetScrPriv failed"));
        return 1;
    }

    dev->rrSetConfig          = pRRScrPriv->rrSetConfig;
    dev->rrGetInfo            = pRRScrPriv->rrGetInfo;
    dev->rrScreenSetSize      = pRRScrPriv->rrScreenSetSize;
    dev->rrCrtcSet            = pRRScrPriv->rrCrtcSet;
    dev->rrCrtcSetGamma       = pRRScrPriv->rrCrtcSetGamma;
    dev->rrCrtcGetGamma       = pRRScrPriv->rrCrtcGetGamma;
    dev->rrOutputSetProperty  = pRRScrPriv->rrOutputSetProperty;
    dev->rrOutputValidateMode = pRRScrPriv->rrOutputValidateMode;
    dev->rrModeDestroy        = pRRScrPriv->rrModeDestroy;
    dev->rrOutputGetProperty  = pRRScrPriv->rrOutputGetProperty;
    dev->rrGetPanning         = pRRScrPriv->rrGetPanning;
    dev->rrSetPanning         = pRRScrPriv->rrSetPanning;

    LLOGLN(10, ("  rrSetConfig = %p", dev->rrSetConfig));
    LLOGLN(10, ("  rrGetInfo = %p", dev->rrGetInfo));
    LLOGLN(10, ("  rrScreenSetSize = %p", dev->rrScreenSetSize));
    LLOGLN(10, ("  rrCrtcSet = %p", dev->rrCrtcSet));
    LLOGLN(10, ("  rrCrtcSetGamma = %p", dev->rrCrtcSetGamma));
    LLOGLN(10, ("  rrCrtcGetGamma = %p", dev->rrCrtcGetGamma));
    LLOGLN(10, ("  rrOutputSetProperty = %p", dev->rrOutputSetProperty));
    LLOGLN(10, ("  rrOutputValidateMode = %p", dev->rrOutputValidateMode));
    LLOGLN(10, ("  rrModeDestroy = %p", dev->rrModeDestroy));
    LLOGLN(10, ("  rrOutputGetProperty = %p", dev->rrOutputGetProperty));
    LLOGLN(10, ("  rrGetPanning = %p", dev->rrGetPanning));
    LLOGLN(10, ("  rrSetPanning = %p", dev->rrSetPanning));

    pRRScrPriv->rrSetConfig          = rdpRRSetConfig;
    pRRScrPriv->rrGetInfo            = rdpRRGetInfo;
    pRRScrPriv->rrScreenSetSize      = rdpRRScreenSetSize;
    pRRScrPriv->rrCrtcSet            = rdpRRCrtcSet;
    pRRScrPriv->rrCrtcSetGamma       = rdpRRCrtcSetGamma;
    pRRScrPriv->rrCrtcGetGamma       = rdpRRCrtcGetGamma;
    pRRScrPriv->rrOutputSetProperty  = rdpRROutputSetProperty;
    pRRScrPriv->rrOutputValidateMode = rdpRROutputValidateMode;
    pRRScrPriv->rrModeDestroy        = rdpRRModeDestroy;
    pRRScrPriv->rrOutputGetProperty  = rdpRROutputGetProperty;
    pRRScrPriv->rrGetPanning         = rdpRRGetPanning;
    pRRScrPriv->rrSetPanning         = rdpRRSetPanning;

    rdpResizeSession(dev, dev->width, dev->height);

    envvar = getenv("XRDP_START_WIDTH");
    if (envvar != 0)
    {
        width = atoi(envvar);
        if ((width >= 16) && (width < 8192))
        {
            envvar = getenv("XRDP_START_HEIGHT");
            if (envvar != 0)
            {
                height = atoi(envvar);
                if ((height >= 16) && (height < 8192))
                {
                    rdpResizeSession(dev, width, height);
                }
            }
        }
    }

    return 0;
}

/******************************************************************************/
static void
rdpBlockHandler1(pointer blockData, OSTimePtr pTimeout, pointer pReadmask)
{
}

/******************************************************************************/
static void
rdpWakeupHandler1(pointer blockData, int result, pointer pReadmask)
{
    rdpClientConCheck((ScreenPtr)blockData);
}

/*****************************************************************************/
static Bool
#if XORG_VERSION_CURRENT < XORG_VERSION_NUMERIC(1, 13, 0, 0, 0)
rdpScreenInit(int scrnIndex, ScreenPtr pScreen, int argc, char **argv)
#else
rdpScreenInit(ScreenPtr pScreen, int argc, char **argv)
#endif
{
    ScrnInfoPtr pScrn;
    rdpPtr dev;
    VisualPtr vis;
    Bool vis_found;
    PictureScreenPtr ps;

    pScrn = xf86Screens[pScreen->myNum];
    dev = XRDPPTR(pScrn);

    dev->pScreen = pScreen;

    miClearVisualTypes();
    miSetVisualTypes(pScrn->depth, miGetDefaultVisualMask(pScrn->depth),
                     pScrn->rgbBits, TrueColor);
    miSetPixmapDepths();
    LLOGLN(0, ("rdpScreenInit: virtualX %d virtualY %d rgbBits %d depth %d",
           pScrn->virtualX, pScrn->virtualY, pScrn->rgbBits, pScrn->depth));

    dev->depth = pScrn->depth;
    dev->paddedWidthInBytes = PixmapBytePad(dev->width, dev->depth);
    dev->bitsPerPixel = rdpBitsPerPixel(dev->depth);
    dev->sizeInBytes = dev->paddedWidthInBytes * dev->height;
    LLOGLN(0, ("rdpScreenInit: pfbMemory bytes %d", dev->sizeInBytes));
    dev->pfbMemory_alloc = (char *) g_malloc(dev->sizeInBytes + 16, 1);
    dev->pfbMemory = (char*) RDPALIGN(dev->pfbMemory_alloc, 16);
    LLOGLN(0, ("rdpScreenInit: pfbMemory %p", dev->pfbMemory));
    if (!fbScreenInit(pScreen, dev->pfbMemory,
                      pScrn->virtualX, pScrn->virtualY,
                      pScrn->xDpi, pScrn->yDpi, pScrn->displayWidth,
                      pScrn->bitsPerPixel))
    {
        LLOGLN(0, ("rdpScreenInit: fbScreenInit failed"));
        return FALSE;
    }

#if XORG_VERSION_CURRENT < XORG_VERSION_NUMERIC(1, 14, 0, 0, 0)
    /* 1.13 has this function, 1.14 and up does not */
    miInitializeBackingStore(pScreen);
#endif

    /* try in init simd functions */
    rdpSimdInit(pScreen, pScrn);

#if defined(XvExtension) && XvExtension
    /* XVideo */
    if (!rdpXvInit(pScreen, pScrn))
    {
        LLOGLN(0, ("rdpScreenInit: rdpXvInit failed"));
    }
#endif

    vis = pScreen->visuals + (pScreen->numVisuals - 1);
    while (vis >= pScreen->visuals)
    {
        if ((vis->class | DynamicClass) == DirectColor)
        {
            vis->offsetBlue = pScrn->offset.blue;
            vis->blueMask = pScrn->mask.blue;
            vis->offsetGreen = pScrn->offset.green;
            vis->greenMask = pScrn->mask.green;
            vis->offsetRed = pScrn->offset.red;
            vis->redMask = pScrn->mask.red;
        }
        vis--;
    }
    fbPictureInit(pScreen, 0, 0);
    xf86SetBlackWhitePixels(pScreen);
    xf86SetBackingStore(pScreen);

#if 1
    /* hardware cursor */
    dev->pCursorFuncs = xf86GetPointerScreenFuncs();
    miPointerInitialize(pScreen, &g_rdpSpritePointerFuncs,
                        dev->pCursorFuncs, 0);
#else
    /* software cursor */
    dev->pCursorFuncs = xf86GetPointerScreenFuncs();
    miDCInitialize(pScreen, dev->pCursorFuncs);
#endif

    fbCreateDefColormap(pScreen);

    /* must assign this one */
    pScreen->SaveScreen = rdpSaveScreen;

    vis_found = FALSE;
    vis = pScreen->visuals + (pScreen->numVisuals - 1);
    while (vis >= pScreen->visuals)
    {
        if (vis->vid == pScreen->rootVisual)
        {
            vis_found = TRUE;
        }
        vis--;
    }
    if (!vis_found)
    {
        LLOGLN(0, ("rdpScreenInit: no root visual"));
        return FALSE;
    }

    dev->privateKeyRecGC = rdpAllocateGCPrivate(pScreen, sizeof(rdpGCRec));
    dev->privateKeyRecPixmap = rdpAllocatePixmapPrivate(pScreen, sizeof(rdpPixmapRec));

    dev->CopyWindow = pScreen->CopyWindow;
    pScreen->CopyWindow = rdpCopyWindow;

    dev->CreateGC = pScreen->CreateGC;
    pScreen->CreateGC = rdpCreateGC;

    dev->CreatePixmap = pScreen->CreatePixmap;
    pScreen->CreatePixmap = rdpCreatePixmap;

    dev->DestroyPixmap = pScreen->DestroyPixmap;
    pScreen->DestroyPixmap = rdpDestroyPixmap;

    dev->ModifyPixmapHeader = pScreen->ModifyPixmapHeader;
    pScreen->ModifyPixmapHeader = rdpModifyPixmapHeader;

    ps = GetPictureScreenIfSet(pScreen);
    if (ps != 0)
    {
        /* composite */
        dev->Composite = ps->Composite;
        ps->Composite = rdpComposite;
        /* glyphs */
        dev->Glyphs = ps->Glyphs;
        ps->Glyphs = rdpGlyphs;
        /* trapezoids */
        dev->Trapezoids = ps->Trapezoids;
        ps->Trapezoids = rdpTrapezoids;
    }

    RegisterBlockAndWakeupHandlers(rdpBlockHandler1, rdpWakeupHandler1, pScreen);

    g_timer = TimerSet(g_timer, 0, 10, rdpDeferredRandR, pScreen);

    if (rdpClientConInit(dev) != 0)
    {
        LLOGLN(0, ("rdpScreenInit: rdpClientConInit failed"));
    }

    dev->Bpp_mask = 0x00FFFFFF;
    dev->Bpp = 4;
    dev->bitsPerPixel = 32;

    LLOGLN(0, ("rdpScreenInit: out"));
    return TRUE;
}

/*****************************************************************************/
static Bool
#if XORG_VERSION_CURRENT < XORG_VERSION_NUMERIC(1, 13, 0, 0, 0)
rdpSwitchMode(int a, DisplayModePtr b, int c)
#else
rdpSwitchMode(ScrnInfoPtr a, DisplayModePtr b)
#endif
{
    LLOGLN(0, ("rdpSwitchMode:"));
    return TRUE;
}

/*****************************************************************************/
static void
#if XORG_VERSION_CURRENT < XORG_VERSION_NUMERIC(1, 13, 0, 0, 0)
rdpAdjustFrame(int a, int b, int c, int d)
#else
rdpAdjustFrame(ScrnInfoPtr a, int b, int c)
#endif
{
    LLOGLN(10, ("rdpAdjustFrame:"));
}

/*****************************************************************************/
static Bool
#if XORG_VERSION_CURRENT < XORG_VERSION_NUMERIC(1, 13, 0, 0, 0)
rdpEnterVT(int a, int b)
#else
rdpEnterVT(ScrnInfoPtr a)
#endif
{
    LLOGLN(0, ("rdpEnterVT:"));
    return TRUE;
}

/*****************************************************************************/
static void
#if XORG_VERSION_CURRENT < XORG_VERSION_NUMERIC(1, 13, 0, 0, 0)
rdpLeaveVT(int a, int b)
#else
rdpLeaveVT(ScrnInfoPtr a)
#endif
{
    LLOGLN(0, ("rdpLeaveVT:"));
}

/*****************************************************************************/
static ModeStatus
#if XORG_VERSION_CURRENT < XORG_VERSION_NUMERIC(1, 13, 0, 0, 0)
rdpValidMode(int a, DisplayModePtr b, Bool c, int d)
#else
rdpValidMode(ScrnInfoPtr a, DisplayModePtr b, Bool c, int d)
#endif
{
    LLOGLN(0, ("rdpValidMode:"));
    return 0;
}

/*****************************************************************************/
static void
#if XORG_VERSION_CURRENT < XORG_VERSION_NUMERIC(1, 13, 0, 0, 0)
rdpFreeScreen(int a, int b)
#else
rdpFreeScreen(ScrnInfoPtr a)
#endif
{
    LLOGLN(0, ("rdpFreeScreen:"));
}

/*****************************************************************************/
static Bool
rdpProbe(DriverPtr drv, int flags)
{
    int num_dev_sections;
    int i;
    int entity;
    GDevPtr *dev_sections;
    Bool found_screen;
    ScrnInfoPtr pscrn;

    LLOGLN(0, ("rdpProbe:"));
    if (flags & PROBE_DETECT)
    {
        return FALSE;
    }
    /* fbScreenInit, fbPictureInit, ... */
    if (!xf86LoadDrvSubModule(drv, "fb"))
    {
        LLOGLN(0, ("rdpProbe: xf86LoadDrvSubModule for fb failed"));
        return FALSE;
    }

    num_dev_sections = xf86MatchDevice(XRDP_DRIVER_NAME, &dev_sections);
    if (num_dev_sections <= 0)
    {
        LLOGLN(0, ("rdpProbe: xf86MatchDevice failed"));
        return FALSE;
    }

    pscrn = 0;
    found_screen = FALSE;
    for (i = 0; i < num_dev_sections; i++)
    {
        entity = xf86ClaimFbSlot(drv, 0, dev_sections[i], 1);
        pscrn = xf86ConfigFbEntity(pscrn, 0, entity, 0, 0, 0, 0);
        if (pscrn)
        {
            LLOGLN(10, ("rdpProbe: found screen"));
            found_screen = 1;
            pscrn->driverVersion = XRDP_VERSION;
            pscrn->driverName    = XRDP_DRIVER_NAME;
            pscrn->name          = XRDP_DRIVER_NAME;
            pscrn->Probe         = rdpProbe;
            pscrn->PreInit       = rdpPreInit;
            pscrn->ScreenInit    = rdpScreenInit;
            pscrn->SwitchMode    = rdpSwitchMode;
            pscrn->AdjustFrame   = rdpAdjustFrame;
            pscrn->EnterVT       = rdpEnterVT;
            pscrn->LeaveVT       = rdpLeaveVT;
            pscrn->ValidMode     = rdpValidMode;
            pscrn->FreeScreen    = rdpFreeScreen;
            xf86DrvMsg(pscrn->scrnIndex, X_INFO, "%s", "using default device\n");
        }
    }
    free(dev_sections);
    return found_screen;
}

/*****************************************************************************/
static const OptionInfoRec *
rdpAvailableOptions(int chipid, int busid)
{
    LLOGLN(0, ("rdpAvailableOptions:"));
    return 0;
}

#ifndef HW_SKIP_CONSOLE
#define HW_SKIP_CONSOLE 4
#endif

/*****************************************************************************/
static Bool
rdpDriverFunc(ScrnInfoPtr pScrn, xorgDriverFuncOp op, pointer ptr)
{
    xorgHWFlags *flags;
    int rv;

    rv = FALSE;
    LLOGLN(0, ("rdpDriverFunc: op %d", (int)op));
    if (op == GET_REQUIRED_HW_INTERFACES)
    {
        flags = (xorgHWFlags *) ptr;
        *flags = HW_SKIP_CONSOLE;
        rv = TRUE;
    }
    return rv;
}

/*****************************************************************************/
static void
rdpIdentify(int flags)
{
    LLOGLN(0, ("rdpIdentify:"));
    xf86PrintChipsets(XRDP_DRIVER_NAME, "driver for xrdp", g_Chipsets);
}

/*****************************************************************************/
_X_EXPORT DriverRec g_DriverRec =
{
    XRDP_VERSION,
    XRDP_DRIVER_NAME,
    rdpIdentify,
    rdpProbe,
    rdpAvailableOptions,
    0,
    0,
    rdpDriverFunc
};

/*****************************************************************************/
static pointer
xrdpdevSetup(pointer module, pointer opts, int *errmaj, int *errmin)
{
    LLOGLN(0, ("xrdpdevSetup:"));
    if (!g_setup_done)
    {
        g_setup_done = 1;
        xf86AddDriver(&g_DriverRec, module, HaveDriverFuncs);
        return (pointer)1;
    }
    else
    {
        if (errmaj != 0)
        {
            *errmaj = LDR_ONCEONLY;
        }
        return 0;
    }
}

/*****************************************************************************/
static void
xrdpdevTearDown(pointer Module)
{
    LLOGLN(0, ("xrdpdevTearDown:"));
}

/* <drivername>ModuleData */
_X_EXPORT XF86ModuleData xrdpdevModuleData =
{
    &g_VersRec,
    xrdpdevSetup,
    xrdpdevTearDown
};
