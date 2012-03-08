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

This is the main file called from main.c
Sets up the  functions

*/

#include "rdp.h"

#if 1
#define DEBUG_OUT(arg)
#else
#define DEBUG_OUT(arg) ErrorF arg
#endif

rdpScreenInfoRec g_rdpScreen; /* the one screen */
ScreenPtr g_pScreen = 0;

//int g_rdpGCIndex = -1;
DevPrivateKeyRec g_rdpGCIndex;

/* set all these at once, use function set_bpp */
int g_bpp = 16;
int g_Bpp = 2;
int g_Bpp_mask = 0xffff;
static int g_firstTime = 1;
static int g_redBits = 5;
static int g_greenBits = 6;
static int g_blueBits = 5;
static int g_initOutputCalled = 0;
/* Common pixmap formats */
static PixmapFormatRec g_formats[MAXFORMATS] =
{
  { 1, 1, BITMAP_SCANLINE_PAD },
  { 4, 8, BITMAP_SCANLINE_PAD },
  { 8, 8, BITMAP_SCANLINE_PAD },
  { 15, 16, BITMAP_SCANLINE_PAD },
  { 16, 16, BITMAP_SCANLINE_PAD },
  { 24, 32, BITMAP_SCANLINE_PAD },
  { 32, 32, BITMAP_SCANLINE_PAD },
};
static int g_numFormats = 7;
static miPointerSpriteFuncRec g_rdpSpritePointerFuncs =
{
  /* these are in rdpinput.c */
  rdpSpriteRealizeCursor,
  rdpSpriteUnrealizeCursor,
  rdpSpriteSetCursor,
  rdpSpriteMoveCursor
};
static miPointerScreenFuncRec g_rdpPointerCursorFuncs =
{
  /* these are in rdpinput.c */
  rdpCursorOffScreen,
  rdpCrossScreen,
  miPointerWarpCursor /* don't need to set last 2 funcs
                         EnqueueEvent and NewEventScreen */
};

#define FB_GET_SCREEN_PIXMAP(s)    ((PixmapPtr) ((s)->devPrivate))

#if 0
static OsTimerPtr g_updateTimer = 0;
#endif
static XID g_wid = 0;

static Bool
rdpRandRGetInfo(ScreenPtr pScreen, Rotation* pRotations);
static Bool
rdpRandRSetConfig(ScreenPtr pScreen, Rotation rotateKind, int rate,
                  RRScreenSizePtr pSize);

/******************************************************************************/
/* returns error, zero is good */
static int
set_bpp(int bpp)
{
  int rv;

  rv = 0;
  g_bpp = bpp;
  if (g_bpp == 8)
  {
    g_Bpp = 1;
    g_Bpp_mask = 0xff;
    g_redBits = 3;
    g_greenBits = 3;
    g_blueBits = 2;
  }
  else if (g_bpp == 15)
  {
    g_Bpp = 2;
    g_Bpp_mask = 0x7fff;
    g_redBits = 5;
    g_greenBits = 5;
    g_blueBits = 5;
  }
  else if (g_bpp == 16)
  {
    g_Bpp = 2;
    g_Bpp_mask = 0xffff;
    g_redBits = 5;
    g_greenBits = 6;
    g_blueBits = 5;
  }
  else if (g_bpp == 24)
  {
    g_Bpp = 4;
    g_Bpp_mask = 0xffffff;
    g_redBits = 8;
    g_greenBits = 8;
    g_blueBits = 8;
  }
  else if (g_bpp == 32)
  {
    g_Bpp = 4;
    g_Bpp_mask = 0xffffff;
    g_redBits = 8;
    g_greenBits = 8;
    g_blueBits = 8;
  }
  else
  {
    rv = 1;
  }
  return rv;
}

/******************************************************************************/
static void
rdpWakeupHandler(int i, pointer blockData, unsigned long err,
                 pointer pReadmask)
{
  g_pScreen->WakeupHandler = g_rdpScreen.WakeupHandler;
  g_pScreen->WakeupHandler(i, blockData, err, pReadmask);
  g_pScreen->WakeupHandler = rdpWakeupHandler;
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
  rdpup_check();
}

/******************************************************************************/
/* returns boolean, true if everything is ok */
static Bool
rdpScreenInit(int index, ScreenPtr pScreen, int argc, char** argv)
{
  int dpix;
  int dpiy;
  int ret;
  Bool vis_found;
  VisualPtr vis;
  PictureScreenPtr ps;
  rrScrPrivPtr pRRScrPriv;

  g_pScreen = pScreen;

  /*dpix = 75;
  dpiy = 75;*/
  dpix = PixelDPI;
  dpiy = PixelDPI;
  if (monitorResolution != 0)
  {
    dpix = monitorResolution;
    dpiy = monitorResolution;
  }
  g_rdpScreen.paddedWidthInBytes = PixmapBytePad(g_rdpScreen.width,
                                                 g_rdpScreen.depth);
  g_rdpScreen.bitsPerPixel = rdpBitsPerPixel(g_rdpScreen.depth);
  ErrorF("\n");
  ErrorF("X11rdp, an X server for xrdp\n");
  ErrorF("Version %s\n", X11RDPVER);
  ErrorF("Copyright (C) 2005-2008 Jay Sorg\n");
  ErrorF("See http://xrdp.sf.net for information on xrdp.\n");
#if defined(XORG_VERSION_CURRENT) && defined (XVENDORNAME)
  ErrorF("Underlying X server release %d, %s\n",
         XORG_VERSION_CURRENT, XVENDORNAME);
#endif
#if defined(XORG_RELEASE)
  ErrorF("Xorg %s\n", XORG_RELEASE);
#endif
  ErrorF("Screen width %d height %d depth %d bpp %d\n", g_rdpScreen.width,
         g_rdpScreen.height, g_rdpScreen.depth, g_rdpScreen.bitsPerPixel);
  ErrorF("dpix %d dpiy %d\n", dpix, dpiy);
  if (g_rdpScreen.pfbMemory == 0)
  {
    g_rdpScreen.sizeInBytes =
           (g_rdpScreen.paddedWidthInBytes * g_rdpScreen.height);
    ErrorF("buffer size %d\n", g_rdpScreen.sizeInBytes);
    g_rdpScreen.pfbMemory = (char*)g_malloc(2048 * 2048 * 4, 1);
  }
  if (g_rdpScreen.pfbMemory == 0)
  {
    rdpLog("rdpScreenInit g_malloc failed\n");
    return 0;
  }
  miClearVisualTypes();
  if (defaultColorVisualClass == -1)
  {
    defaultColorVisualClass = TrueColor;
  }
  if (!miSetVisualTypes(g_rdpScreen.depth,
                         miGetDefaultVisualMask(g_rdpScreen.depth),
                         8, defaultColorVisualClass))
  {
    rdpLog("rdpScreenInit miSetVisualTypes failed\n");
    return 0;
  }
  miSetPixmapDepths();
  switch (g_rdpScreen.bitsPerPixel)
  {
    case 8:
      ret = fbScreenInit(pScreen, g_rdpScreen.pfbMemory,
                          g_rdpScreen.width, g_rdpScreen.height,
                          dpix, dpiy, g_rdpScreen.paddedWidthInBytes, 8);
      break;
    case 16:
      ret = fbScreenInit(pScreen, g_rdpScreen.pfbMemory,
                          g_rdpScreen.width, g_rdpScreen.height,
                          dpix, dpiy, g_rdpScreen.paddedWidthInBytes / 2, 16);
      break;
    case 32:
      ret = fbScreenInit(pScreen, g_rdpScreen.pfbMemory,
                          g_rdpScreen.width, g_rdpScreen.height,
                          dpix, dpiy, g_rdpScreen.paddedWidthInBytes / 4, 32);
      break;
    default:
      return 0;
  }
  if (!ret)
  {
    return 0;
  }

  miInitializeBackingStore(pScreen);

  /* this is for rgb, not bgr, just doing rgb for now */
  vis = g_pScreen->visuals + (g_pScreen->numVisuals - 1);
  while (vis >= pScreen->visuals)
  {
    if ((vis->class | DynamicClass) == DirectColor)
    {
      vis->offsetBlue = 0;
      vis->blueMask = (1 << g_blueBits) - 1;
      vis->offsetGreen = g_blueBits;
      vis->greenMask = ((1 << g_greenBits) - 1) << vis->offsetGreen;
      vis->offsetRed = g_blueBits + g_greenBits;
      vis->redMask = ((1 << g_redBits) - 1) << vis->offsetRed;
    }
    vis--;
  }

  if (g_rdpScreen.bitsPerPixel > 4)
  {
    fbPictureInit(pScreen, 0, 0);
  }

  //if (!miAllocateGCPrivate(pScreen, g_rdpGCIndex, sizeof(rdpGCRec)))
  if (!dixRegisterPrivateKey(&g_rdpGCIndex, PRIVATE_GC, sizeof(rdpGCRec)))
  {
    FatalError("rdpScreenInit: miAllocateGCPrivate failed\n");
  }

  /* Random screen procedures */
  g_rdpScreen.CloseScreen = pScreen->CloseScreen;
  /* GC procedures */
  g_rdpScreen.CreateGC = pScreen->CreateGC;
  /* Pixmap procudures */
  g_rdpScreen.CreatePixmap = pScreen->CreatePixmap;
  g_rdpScreen.DestroyPixmap = pScreen->DestroyPixmap;
  /* Window Procedures */
  //g_rdpScreen.PaintWindowBackground = pScreen->PaintWindowBackground;
  //g_rdpScreen.PaintWindowBorder = pScreen->PaintWindowBorder;
  g_rdpScreen.CopyWindow = pScreen->CopyWindow;
  g_rdpScreen.ClearToBackground = pScreen->ClearToBackground;
  /* Backing store procedures */
  g_rdpScreen.RestoreAreas = pScreen->RestoreAreas;
  g_rdpScreen.WakeupHandler = pScreen->WakeupHandler;
  ps = GetPictureScreenIfSet(pScreen);
  if (ps)
  {
    g_rdpScreen.Composite = ps->Composite;
  }
  pScreen->blackPixel = g_rdpScreen.blackPixel;
  pScreen->whitePixel = g_rdpScreen.whitePixel;
  /* Random screen procedures */
  pScreen->CloseScreen = rdpCloseScreen;
  pScreen->WakeupHandler = rdpWakeupHandler;
  if (ps)
  {
    ps->Composite = rdpComposite;
  }
  pScreen->SaveScreen = rdpSaveScreen;
  /* GC procedures */
  pScreen->CreateGC = rdpCreateGC;
  /* Pixmap procedures */
  /* pScreen->CreatePixmap = rdpCreatePixmap; */
  /* pScreen->DestroyPixmap = rdpDestroyPixmap; */
  /* Window Procedures */
  //pScreen->PaintWindowBackground = rdpPaintWindowBackground;
  //pScreen->PaintWindowBorder = rdpPaintWindowBorder;
  pScreen->CopyWindow = rdpCopyWindow;
  pScreen->ClearToBackground = rdpClearToBackground;
  /* Backing store procedures */
  pScreen->RestoreAreas = rdpRestoreAreas;
  miPointerInitialize(pScreen, &g_rdpSpritePointerFuncs,
                      &g_rdpPointerCursorFuncs, 1);

  vis_found = 0;
  vis = g_pScreen->visuals + (g_pScreen->numVisuals - 1);
  while (vis >= pScreen->visuals)
  {
    if (vis->vid == pScreen->rootVisual)
    {
      vis_found = 1;
    }
    vis--;
  }
  if (!vis_found)
  {
    rdpLog("rdpScreenInit: couldn't find root visual\n");
    exit(1);
  }
  if (g_rdpScreen.bitsPerPixel == 1)
  {
    ret = fbCreateDefColormap(pScreen);
  }
  else
  {
    ret = fbCreateDefColormap(pScreen);
  }
  if (ret)
  {
    ret = rdpup_init();
  }
  if (ret)
  {
    RegisterBlockAndWakeupHandlers(rdpBlockHandler1, rdpWakeupHandler1, NULL);
  }
  if (!RRScreenInit(pScreen))
  {
    ErrorF("rdpmain.c: RRScreenInit: screen init failed\n");
  }
  else
  {
    pRRScrPriv = rrGetScrPriv(pScreen);
    pRRScrPriv->rrGetInfo = rdpRandRGetInfo;
    pRRScrPriv->rrSetConfig = rdpRandRSetConfig;
  }
  return ret;
}

/******************************************************************************/
/* this is the first function called, it can be called many times
   returns the number or parameters processed
   if it dosen't apply to the rdp part, return 0 */
int
ddxProcessArgument(int argc, char** argv, int i)
{
  if (g_firstTime)
  {
    memset(&g_rdpScreen, 0, sizeof(g_rdpScreen));
    g_rdpScreen.width  = 800;
    g_rdpScreen.height = 600;
    g_rdpScreen.depth = 24;
    set_bpp(24);
    g_rdpScreen.blackPixel = 1;
    g_firstTime = 0;
    RRExtensionInit();
  }
  if (strcmp(argv[i], "-geometry") == 0)
  {
    if (i + 1 >= argc)
    {
      UseMsg();
    }
    if (sscanf(argv[i + 1], "%dx%d", &g_rdpScreen.width,
               &g_rdpScreen.height) != 2)
    {
      ErrorF("Invalid geometry %s\n", argv[i + 1]);
      UseMsg();
    }
    return 2;
  }
  if (strcmp (argv[i], "-depth") == 0)
  {
    if (i + 1 >= argc)
    {
      UseMsg();
    }
    g_rdpScreen.depth = atoi(argv[i + 1]);
    if (set_bpp(g_rdpScreen.depth) != 0)
    {
      UseMsg();
    }
    return 2;
  }
  return 0;
}

/******************************************************************************/
void
OsVendorInit(void)
{
}

/******************************************************************************/
/* ddxInitGlobals - called by |InitGlobals| from os/util.c */
void
ddxInitGlobals(void)
{
}

/******************************************************************************/
int
XkbDDXSwitchScreen(DeviceIntPtr dev, KeyCode key, XkbAction* act)
{
  return 1;
}

/******************************************************************************/
int
XkbDDXPrivate(DeviceIntPtr dev, KeyCode key, XkbAction* act)
{
  return 0;
}

/******************************************************************************/
int
XkbDDXTerminateServer(DeviceIntPtr dev, KeyCode key, XkbAction* act)
{
  GiveUp(1);
  return 0;
}

/******************************************************************************/
/* InitOutput is called every time the server resets.  It should call
   AddScreen for each screen (but we only ever have one), and in turn this
   will call rdpScreenInit. */
void
InitOutput(ScreenInfo* screenInfo, int argc, char** argv)
{
  int i;

  g_initOutputCalled = 1;
  /* initialize pixmap formats */
  screenInfo->imageByteOrder = IMAGE_BYTE_ORDER;
  screenInfo->bitmapScanlineUnit = BITMAP_SCANLINE_UNIT;
  screenInfo->bitmapScanlinePad = BITMAP_SCANLINE_PAD;
  screenInfo->bitmapBitOrder = BITMAP_BIT_ORDER;
  screenInfo->numPixmapFormats = g_numFormats;
  for (i = 0; i < g_numFormats; i++)
  {
    screenInfo->formats[i] = g_formats[i];
  }
  //g_rdpGCIndex = miAllocateGCPrivateIndex();
  //if (g_rdpGCIndex < 0)
  //{
  //  FatalError("InitOutput: miAllocateGCPrivateIndex failed\n");
  //}
  if (!AddCallback(&ClientStateCallback, rdpClientStateChange, NULL))
  {
    rdpLog("InitOutput: AddCallback failed\n");
    return;
  }
  /* initialize screen */
  if (AddScreen(rdpScreenInit, argc, argv) == -1)
  {
    FatalError("Couldn't add screen\n");
  }
}

/******************************************************************************/
void
InitInput(int argc, char** argv)
{
  DeviceIntPtr p;
  DeviceIntPtr k;

  k = AddInputDevice(serverClient, rdpKeybdProc, 1);
  p = AddInputDevice(serverClient, rdpMouseProc, 1);
  RegisterKeyboardDevice(k);
  RegisterPointerDevice(p);

// TODO
#if 0
  /* screenInfo must be globally defined */
  miRegisterPointerDevice(screenInfo.screens[0], p);
  mieqInit(k, p);
#endif

}

/******************************************************************************/
void
ddxGiveUp(void)
{
  char unixSocketName[64];

  g_free(g_rdpScreen.pfbMemory);
  if (g_initOutputCalled)
  {
    sprintf(unixSocketName, "/tmp/.X11-unix/X%s", display);
    unlink(unixSocketName);
    sprintf(unixSocketName, "/tmp/.xrdp/xrdp_disconnect_display_%s", display);
    unlink(unixSocketName);
  }
}

/******************************************************************************/
Bool
LegalModifier(unsigned int key, DeviceIntPtr pDev)
{
  return 1; /* true */
}

/******************************************************************************/
void
ProcessInputEvents(void)
{
  mieqProcessInputEvents();
  //miPointerUpdate();
}

/******************************************************************************/
/* needed for some reason? todo
   needs to be rfb */
void
rfbRootPropertyChange(PropertyPtr pProp)
{
}

/******************************************************************************/
void
AbortDDX(void)
{
  ddxGiveUp();
}

/******************************************************************************/
void
OsVendorFatalError(void)
{
}

/******************************************************************************/
/* print the command list parameters and exit the program */
void
ddxUseMsg(void)
{
  ErrorF("\n");
  ErrorF("X11rdp specific options\n");
  ErrorF("-geometry WxH          set framebuffer width & height\n");
  ErrorF("-depth D               set framebuffer depth\n");
  ErrorF("\n");
  exit(1);
}

/******************************************************************************/
void
OsVendorPreInit(void)
{
}

/******************************************************************************/
/*
 * Answer queries about the RandR features supported.
   1280x1024+0+0 359mm x 287mm
 */
static Bool
rdpRandRGetInfo(ScreenPtr pScreen, Rotation* pRotations)
{
  int n;
  int width;
  int height;
  int mmwidth;
  int mmheight;
  Rotation rotateKind;
  RRScreenSizePtr pSize;
  rrScrPrivPtr pRRScrPriv;

  ErrorF("rdpRandRGetInfo:\n");

  pRRScrPriv = rrGetScrPriv(pScreen);

  DEBUG_OUT(("rdpRandRGetInfo: nSizes %d\n", pRRScrPriv->nSizes));
  for (n = 0; n < pRRScrPriv->nSizes; n++)
  {
    DEBUG_OUT(("rdpRandRGetInfo: width %d height %d\n",
               pRRScrPriv->pSizes[n].width,
               pRRScrPriv->pSizes[n].height));
  }

  /* Don't support rotations, yet */
  *pRotations = RR_Rotate_0;

  /* Bail if no depth has a visual associated with it */
  for (n = 0; n < pScreen->numDepths; n++)
  {
    if (pScreen->allowedDepths[n].numVids)
    {
      break;
    }
  }
  if (n == pScreen->numDepths)
  {
    return FALSE;
  }

  /* Only one allowed rotation for now */
  rotateKind = RR_Rotate_0;

  for (n = 0; n < pRRScrPriv->nSizes; n++)
  {
    RRRegisterSize(pScreen, pRRScrPriv->pSizes[n].width,
                   pRRScrPriv->pSizes[n].height,
                   pRRScrPriv->pSizes[n].mmWidth,
                   pRRScrPriv->pSizes[n].mmHeight);
  }
  /*
   * Register supported sizes.  This can be called many times, but
   * we only support one size for now.
   */

#if 0
  width = 800;
  height = 600;
  mmwidth = PixelToMM(width);
  mmheight = PixelToMM(height);
  RRRegisterSize(pScreen, width, height, mmwidth, mmheight);

  width = 1024;
  height = 768;
  mmwidth = PixelToMM(width);
  mmheight = PixelToMM(height);
  RRRegisterSize(pScreen, width, height, mmwidth, mmheight);

  width = 1280;
  height = 1024;
  mmwidth = PixelToMM(width);
  mmheight = PixelToMM(height);
  RRRegisterSize(pScreen, width, height, mmwidth, mmheight);
#endif

  width = g_rdpScreen.rdp_width;
  height = g_rdpScreen.rdp_height;
  mmwidth = PixelToMM(width);
  mmheight = PixelToMM(height);
  pSize = RRRegisterSize(pScreen, width, height, mmwidth, mmheight);

  /* Tell RandR what the current config is */
  RRSetCurrentConfig(pScreen, rotateKind, 0, pSize);

  return TRUE;
}

#if 0
/******************************************************************************/
static CARD32
rdpDeferredDrawCallback(OsTimerPtr timer, CARD32 now, pointer arg)
{
  WindowPtr pWin;

  DEBUG_OUT(("rdpDeferredDrawCallback:\n"));
  pWin = (WindowPtr)arg;
  DeleteWindow(pWin, None);
  /*
  FreeResource(g_wid, RT_NONE);
  g_wid = 0;
  */
return 0;
}
#endif

/******************************************************************************/
/* for lack of a better way, a window is created that covers a the area and
   when its deleted, it's invalidated */
static int
rdpInvalidateArea(ScreenPtr pScreen, int x, int y, int cx, int cy)
{
  WindowPtr rootWindow;
  WindowPtr pWin;
  int result;
  int attri;
  XID attributes[4];
  Mask mask;

  DEBUG_OUT(("rdpInvalidateArea:\n"));
  rootWindow = 0; // GetCurrentRootWindow();
  if (rootWindow != 0)
  {
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
    pWin = CreateWindow(g_wid, rootWindow,
                        x, y, cx, cy, 0, InputOutput, mask,
                        attributes, 0, serverClient,
                        wVisual(rootWindow), &result);
    if (result == 0)
    {
      MapWindow(pWin, serverClient);
      DeleteWindow(pWin, None);
#if 0
      g_updateTimer = TimerSet(g_updateTimer, 0, 50,
                               rdpDeferredDrawCallback, pWin);
#endif
    }
  }
  return 0;
}

/******************************************************************************/
/*
 * Respond to resize/rotate request from either X Server or X client app
 */
static Bool
rdpRandRSetConfig(ScreenPtr pScreen, Rotation rotateKind, int rate,
                  RRScreenSizePtr pSize)
{
  PixmapPtr screenPixmap;
  WindowPtr rootWindow;
  BoxRec box;

  if ((pSize->width < 1) || (pSize->height < 1))
  {
    ErrorF("rdpRandRSetConfig: error width %d height %d\n",
           pSize->width, pSize->height);
    return FALSE;
  }
  ErrorF("rdpRandRSetConfig: width %d height %d\n",
         pSize->width, pSize->height);
  g_rdpScreen.width = pSize->width;
  g_rdpScreen.height = pSize->height;
  g_rdpScreen.paddedWidthInBytes =
    PixmapBytePad(g_rdpScreen.width, g_rdpScreen.depth);
  g_rdpScreen.sizeInBytes =
    g_rdpScreen.paddedWidthInBytes * g_rdpScreen.height;
  pScreen->width = pSize->width;
  pScreen->height = pSize->height;
  DEBUG_OUT(("rdpRandRSetConfig: pScreen %dx%d pSize %dx%d\n",
             pScreen->mmWidth, pScreen->mmHeight,
             pSize->mmWidth, pSize->mmHeight));
  pScreen->mmWidth = pSize->mmWidth;
  pScreen->mmHeight = pSize->mmHeight;
#if 0
  g_free(g_rdpScreen.pfbMemory);
  g_rdpScreen.pfbMemory = (char*)g_malloc(g_rdpScreen.sizeInBytes, 1);
#endif
  screenPixmap = FB_GET_SCREEN_PIXMAP(pScreen);
  if (screenPixmap != 0)
  {
    DEBUG_OUT(("rdpRandRSetConfig: resizing screenPixmap [%p] to %dx%d, "
               "currently at %dx%d\n",
               (void*)screenPixmap, pSize->width, pSize->height,
               screenPixmap->drawable.width, screenPixmap->drawable.height));
    pScreen->ModifyPixmapHeader(screenPixmap,
                                pSize->width, pSize->height,
                                g_rdpScreen.depth, g_rdpScreen.bitsPerPixel,
                                g_rdpScreen.paddedWidthInBytes,
                                g_rdpScreen.pfbMemory);
    DEBUG_OUT(("rdpRandRSetConfig: resized to %dx%d\n",
               screenPixmap->drawable.width, screenPixmap->drawable.height));
    /* memset(g_rdpScreen.pfbMemory, 0xff, 2048 * 2048 * 4); */
  }
  rootWindow = 0; // GetCurrentRootWindow();
  if (rootWindow != 0)
  {
    DEBUG_OUT(("rdpRandRSetConfig: rootWindow %p\n", (void*)rootWindow));
    box.x1 = 0;
    box.y1 = 0;
    box.x2 = pSize->width;
    box.y2 = pSize->height;
    RegionInit(&rootWindow->winSize, &box, 1);
    RegionInit(&rootWindow->borderSize, &box, 1);
    RegionReset(&rootWindow->borderClip, &box);
    RegionBreak(&rootWindow->clipList);
    rootWindow->drawable.width = pSize->width;
    rootWindow->drawable.height = pSize->height;
    ResizeChildrenWinSize(rootWindow, 0, 0, 0, 0);
  }
  rdpInvalidateArea(g_pScreen, 0, 0, g_rdpScreen.width, g_rdpScreen.height);
  return TRUE;
}
