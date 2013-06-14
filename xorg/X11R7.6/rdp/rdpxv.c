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

XVideo extension

*/

#include "rdp.h"

#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvproto.h>
#include "xvdix.h"

static DevPrivateKey g_XvScreenKey;

#define GET_XV_SCREEN(pScreen) \
    ((XvScreenPtr)dixLookupPrivate(&(pScreen)->devPrivates, g_XvScreenKey))

#define LOG_LEVEL 1
#define LLOG(_level, _args) \
    do { if (_level < LOG_LEVEL) { ErrorF _args ; } } while (0)
#define LLOGLN(_level, _args) \
    do { if (_level < LOG_LEVEL) { ErrorF _args ; ErrorF("\n"); } } while (0)

/*****************************************************************************/
static int
rdpXvAllocatePort(unsigned long port, XvPortPtr pPort, XvPortPtr* ppPort)
{
    LLOGLN(0, ("rdpXvAllocatePort:"));
    *ppPort = pPort;
    return Success;
}

/*****************************************************************************/
static int
rdpXvFreePort(XvPortPtr pPort)
{
    LLOGLN(0, ("rdpXvFreePort:"));
    return Success;
}

/*****************************************************************************/
static int
rdpXvPutVideo(ClientPtr client, DrawablePtr pDraw, XvPortPtr pPort, GCPtr pGC,
              INT16 vid_x, INT16 vid_y, CARD16 vid_w, CARD16 vid_h,
              INT16 drw_x, INT16 drw_y, CARD16 drw_w, CARD16 drw_h)
{
    LLOGLN(0, ("rdpXvPutVideo:"));
    return Success;
}

/*****************************************************************************/
static int
rdpXvPutStill(ClientPtr client, DrawablePtr pDraw, XvPortPtr pPort, GCPtr pGC,
              INT16 vid_x, INT16 vid_y, CARD16 vid_w, CARD16 vid_h,
              INT16 drw_x, INT16 drw_y, CARD16 drw_w, CARD16 drw_h)
{
    LLOGLN(0, ("rdpXvPutStill:"));
    return Success;
}

/*****************************************************************************/
static int
rdpXvGetVideo(ClientPtr client, DrawablePtr pDraw, XvPortPtr pPort, GCPtr pGC,
              INT16 vid_x, INT16 vid_y, CARD16 vid_w, CARD16 vid_h,
              INT16 drw_x, INT16 drw_y, CARD16 drw_w, CARD16 drw_h)
{
    LLOGLN(0, ("rdpXvGetVideo:"));
    return Success;
}

/*****************************************************************************/
static int
rdpXvGetStill(ClientPtr client, DrawablePtr pDraw, XvPortPtr pPort, GCPtr pGC,
              INT16 vid_x, INT16 vid_y, CARD16 vid_w, CARD16 vid_h,
              INT16 drw_x, INT16 drw_y, CARD16 drw_w, CARD16 drw_h)
{
    LLOGLN(0, ("rdpXvGetStill:"));
    return Success;
}

/*****************************************************************************/
static int
rdpXvStopVideo(ClientPtr client, XvPortPtr pPort, DrawablePtr pDraw)
{
    LLOGLN(0, ("rdpXvStopVideo:"));
    return Success;
}

/*****************************************************************************/
static int
rdpXvSetPortAttribute(ClientPtr client, XvPortPtr pPort, Atom attribute,
                       INT32 value)
{
    LLOGLN(0, ("rdpXvxSetPortAttribute:"));
    return Success;
}

/*****************************************************************************/
static int
rdpXvGetPortAttribute(ClientPtr client, XvPortPtr pPort, Atom attribute,
                      INT32* p_value)
{
    LLOGLN(0, ("rdpXvGetPortAttribute:"));
    return Success;
}

/*****************************************************************************/
static int
rdpXvQueryBestSize(ClientPtr client, XvPortPtr pPort, CARD8 motion,
                   CARD16 vid_w, CARD16 vid_h, CARD16 drw_w, CARD16 drw_h,
                   unsigned int *p_w, unsigned int *p_h)
{
    LLOGLN(0, ("rdpXvQueryBestSize:"));
    return Success;
}

/*****************************************************************************/
static int
rdpXvPutImage(ClientPtr client, DrawablePtr pDraw, XvPortPtr pPort, GCPtr pGC,
              INT16 src_x, INT16 src_y, CARD16 src_w, CARD16 src_h,
              INT16 drw_x, INT16 drw_y, CARD16 drw_w, CARD16 drw_h,
              XvImagePtr format,  unsigned char* data, Bool sync,
              CARD16 width, CARD16 height)
{
    LLOGLN(0, ("rdpXvPutImage:"));
    return Success;
}

/*****************************************************************************/
static int
rdpXvQueryImageAttributes(ClientPtr client, XvPortPtr pPort, XvImagePtr format,
                          CARD16* width, CARD16* height, int* pitches,
                          int* offsets)
{
    LLOGLN(0, ("rdpXvQueryImageAttributes:"));
    return Success;
}

/*****************************************************************************/
static Bool
rdpXvCloseScreen(int i, ScreenPtr pScreen)
{
    XvScreenPtr pxvs = GET_XV_SCREEN(pScreen);

    LLOGLN(0, ("rdpXvCloseScreen:"));
    free(pxvs->pAdaptors);
    return 0;
}

/*****************************************************************************/
static int
rdpXvQueryAdaptors(ScreenPtr pScreen, XvAdaptorPtr* p_pAdaptors,
                   int* p_nAdaptors)
{
    XvScreenPtr pxvs = GET_XV_SCREEN(pScreen);

    LLOGLN(0, ("rdpXvQueryAdaptors:"));
    *p_nAdaptors = pxvs->nAdaptors;
    *p_pAdaptors = pxvs->pAdaptors;
    return Success;
}

/*****************************************************************************/
static int
rdpXvInitAdaptors(ScreenPtr pScreen)
{
    XvScreenPtr pxvs = GET_XV_SCREEN(pScreen);
    XvAdaptorPtr pAdaptor;

    pxvs->nAdaptors = 0;
    pxvs->pAdaptors = NULL;

    pAdaptor = malloc(sizeof(XvAdaptorRec));
    memset(pAdaptor, 0, sizeof(XvAdaptorRec));
    pAdaptor->type = 0;
    pAdaptor->pScreen = pScreen;

    pAdaptor->ddAllocatePort = rdpXvAllocatePort;
    pAdaptor->ddFreePort = rdpXvFreePort;
    pAdaptor->ddPutStill = rdpXvPutStill;
    pAdaptor->ddGetVideo = rdpXvGetVideo;
    pAdaptor->ddGetStill = rdpXvGetStill;
    pAdaptor->ddStopVideo = rdpXvStopVideo;
    pAdaptor->ddSetPortAttribute = rdpXvSetPortAttribute;
    pAdaptor->ddGetPortAttribute = rdpXvGetPortAttribute;
    pAdaptor->ddQueryBestSize = rdpXvQueryBestSize;
    pAdaptor->ddPutImage = rdpXvPutImage;
    pAdaptor->ddQueryImageAttributes = rdpXvQueryImageAttributes;

    pxvs->pAdaptors = pAdaptor;

    return 0;
}

/*****************************************************************************/
/* returns error */
int
rdpXvInit(ScreenPtr pScreen)
{
    XvScreenPtr pxvs;

    LLOGLN(0, ("rdpXvInit:"));
    XvScreenInit(pScreen);
    g_XvScreenKey = XvGetScreenKey();
    pxvs = GET_XV_SCREEN(pScreen);
    pxvs->nAdaptors = 0;
    pxvs->ddCloseScreen = rdpXvCloseScreen;
    pxvs->ddQueryAdaptors = rdpXvQueryAdaptors;
    rdpXvInitAdaptors(pScreen);
    return 0;
}
