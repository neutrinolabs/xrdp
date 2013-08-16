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

GC related calls

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
#include "rdpFillSpans.h"
#include "rdpSetSpans.h"
#include "rdpPutImage.h"
#include "rdpCopyArea.h"
#include "rdpCopyPlane.h"
#include "rdpPolyPoint.h"
#include "rdpPolylines.h"
#include "rdpPolySegment.h"
#include "rdpPolyRectangle.h"
#include "rdpPolyArc.h"
#include "rdpFillPolygon.h"
#include "rdpPolyFillRect.h"
#include "rdpPolyFillArc.h"
#include "rdpPolyText8.h"
#include "rdpPolyText16.h"
#include "rdpImageText8.h"
#include "rdpImageText16.h"
#include "rdpImageGlyphBlt.h"
#include "rdpPolyGlyphBlt.h"
#include "rdpPushPixels.h"
#include "rdpDraw.h"

/******************************************************************************/
#define LOG_LEVEL 1
#define LLOGLN(_level, _args) \
    do { if (_level < LOG_LEVEL) { ErrorF _args ; ErrorF("\n"); } } while (0)

/******************************************************************************/
#define GC_FUNC_VARS rdpPtr dev; rdpGCPtr priv;

/******************************************************************************/
#define GC_FUNC_PROLOGUE(_pGC) \
    do { \
        dev = rdpGetDevFromScreen((_pGC)->pScreen); \
        priv = (rdpGCPtr)rdpGetGCPrivate(_pGC, dev->privateKeyRecGC); \
        (_pGC)->funcs = priv->funcs; \
        if (priv->ops != 0) \
        { \
            (_pGC)->ops = priv->ops; \
        } \
    } while (0)

/******************************************************************************/
#define GC_FUNC_EPILOGUE(_pGC) \
    do { \
        priv->funcs = (_pGC)->funcs; \
        (_pGC)->funcs = &g_rdpGCFuncs; \
        if (priv->ops != 0) \
        { \
            priv->ops = (_pGC)->ops; \
            (_pGC)->ops = &g_rdpGCOps; \
        } \
    } while (0)

static void
rdpValidateGC(GCPtr pGC, unsigned long changes, DrawablePtr d);
static void
rdpChangeGC(GCPtr pGC, unsigned long mask);
static void
rdpCopyGC(GCPtr src, unsigned long mask, GCPtr dst);
static void
rdpDestroyGC(GCPtr pGC);
static void
rdpChangeClip(GCPtr pGC, int type, pointer pValue, int nrects);
static void
rdpDestroyClip(GCPtr pGC);
static void
rdpCopyClip(GCPtr dst, GCPtr src);

GCFuncs g_rdpGCFuncs =
{
    rdpValidateGC, rdpChangeGC, rdpCopyGC, rdpDestroyGC, rdpChangeClip,
    rdpDestroyClip, rdpCopyClip
};

GCOps g_rdpGCOps =
{
    rdpFillSpans, rdpSetSpans, rdpPutImage, rdpCopyArea, rdpCopyPlane,
    rdpPolyPoint, rdpPolylines, rdpPolySegment, rdpPolyRectangle,
    rdpPolyArc, rdpFillPolygon, rdpPolyFillRect, rdpPolyFillArc,
    rdpPolyText8, rdpPolyText16, rdpImageText8, rdpImageText16,
    rdpImageGlyphBlt, rdpPolyGlyphBlt, rdpPushPixels
};

/******************************************************************************/
static void
rdpValidateGC(GCPtr pGC, unsigned long changes, DrawablePtr d)
{
    GC_FUNC_VARS;

    LLOGLN(10, ("rdpValidateGC:"));
    GC_FUNC_PROLOGUE(pGC);
    pGC->funcs->ValidateGC(pGC, changes, d);
    priv->ops = pGC->ops;
    GC_FUNC_EPILOGUE(pGC);
}

/******************************************************************************/
static void
rdpChangeGC(GCPtr pGC, unsigned long mask)
{
    GC_FUNC_VARS;

    LLOGLN(10, ("rdpChangeGC:"));
    GC_FUNC_PROLOGUE(pGC);
    pGC->funcs->ChangeGC(pGC, mask);
    GC_FUNC_EPILOGUE(pGC);
}

/******************************************************************************/
static void
rdpCopyGC(GCPtr src, unsigned long mask, GCPtr dst)
{
    GC_FUNC_VARS;

    LLOGLN(10, ("rdpCopyGC:"));
    GC_FUNC_PROLOGUE(dst);
    dst->funcs->CopyGC(src, mask, dst);
    GC_FUNC_EPILOGUE(dst);
}

/******************************************************************************/
static void
rdpDestroyGC(GCPtr pGC)
{
    GC_FUNC_VARS;

    LLOGLN(10, ("rdpDestroyGC:"));
    GC_FUNC_PROLOGUE(pGC);
    pGC->funcs->DestroyGC(pGC);
    GC_FUNC_EPILOGUE(pGC);
}

/******************************************************************************/
static void
rdpChangeClip(GCPtr pGC, int type, pointer pValue, int nrects)
{
    GC_FUNC_VARS;

    LLOGLN(10, ("rdpChangeClip:"));
    GC_FUNC_PROLOGUE(pGC);
    pGC->funcs->ChangeClip(pGC, type, pValue, nrects);
    GC_FUNC_EPILOGUE(pGC);
}

/******************************************************************************/
static void
rdpDestroyClip(GCPtr pGC)
{
    GC_FUNC_VARS;

    LLOGLN(10, ("rdpDestroyClip:"));
    GC_FUNC_PROLOGUE(pGC);
    pGC->funcs->DestroyClip(pGC);
    GC_FUNC_EPILOGUE(pGC);
}

/******************************************************************************/
static void
rdpCopyClip(GCPtr dst, GCPtr src)
{
    GC_FUNC_VARS;

    LLOGLN(10, ("rdpCopyClip:"));
    GC_FUNC_PROLOGUE(dst);
    dst->funcs->CopyClip(dst, src);
    GC_FUNC_EPILOGUE(dst);
}

/*****************************************************************************/
Bool
rdpCreateGC(GCPtr pGC)
{
    Bool rv;
    rdpPtr dev;
    ScreenPtr pScreen;
    rdpGCPtr priv;

    LLOGLN(10, ("rdpCreateGC:"));
    pScreen = pGC->pScreen;
    dev = rdpGetDevFromScreen(pScreen);
    priv = (rdpGCPtr)rdpGetGCPrivate(pGC, dev->privateKeyRecGC);
    pScreen->CreateGC = dev->CreateGC;
    rv = pScreen->CreateGC(pGC);
    if (rv)
    {
        priv->funcs = pGC->funcs;
        priv->ops = 0;
        pGC->funcs = &g_rdpGCFuncs;
    }
    pScreen->CreateGC = rdpCreateGC;
    return rv;
}
