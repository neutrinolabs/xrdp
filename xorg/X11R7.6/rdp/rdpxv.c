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

#include <fourcc.h>

extern rdpScreenInfoRec g_rdpScreen; /* from rdpmain.c */

static DevPrivateKey g_XvScreenKey;
static char g_xv_adaptor_name[] = "xrdp XVideo adaptor";
static char g_xv_encoding_name[] = "XV_IMAGE";

#define GET_XV_SCREEN(pScreen) \
    ((XvScreenPtr)dixLookupPrivate(&(pScreen)->devPrivates, g_XvScreenKey))

#define LOG_LEVEL 1
#define LLOG(_level, _args) \
    do { if (_level < LOG_LEVEL) { ErrorF _args ; } } while (0)
#define LLOGLN(_level, _args) \
    do { if (_level < LOG_LEVEL) { ErrorF _args ; ErrorF("\n"); } } while (0)

#define T_NUM_ENCODINGS 1
static XvEncodingRec g_encodings[T_NUM_ENCODINGS];

#define T_NUM_FORMATS 1
static XvFormatRec g_formats[T_NUM_FORMATS];

#define T_NUM_PORTS 1
static XvPortRec g_ports[T_NUM_PORTS];

#define FOURCC_RV15 0x35315652
#define FOURCC_RV16 0x36315652
#define FOURCC_RV24 0x34325652
#define FOURCC_RV32 0x32335652

#define T_NUM_IMAGES 8
static XvImageRec g_images[T_NUM_IMAGES] =
{
    {
        FOURCC_RV15,XvRGB,LSBFirst,
        {'R','V','1','5',0,0,0,0,0,0,0,0,0,0,0,0},
        16, XvPacked, 1, 15, 0x001f, 0x03e0, 0x7c00, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        {'R','V','B',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        XvTopToBottom
    },
    {
        FOURCC_RV16,XvRGB,LSBFirst,
        {'R','V','1','6',0,0,0,0,0,0,0,0,0,0,0,0},
        16, XvPacked, 1, 16, 0x001f, 0x07e0, 0xf800, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        {'R','V','B',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        XvTopToBottom
    },
    {
        FOURCC_RV24,XvRGB,LSBFirst,
        {'R','V','2','4',0,0,0,0,0,0,0,0,0,0,0,0},
        32, XvPacked, 1, 24, 0x000000ff, 0x0000ff00, 0x00ff0000, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        {'R','V','B',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        XvTopToBottom
    },
    {
        FOURCC_RV32, XvRGB, LSBFirst,
        {'R','V','3','2',0,0,0,0,0,0,0,0,0,0,0,0},
        32, XvPacked, 1, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        {'R','V','B',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        XvTopToBottom
    },
    XVIMAGE_YV12,
    XVIMAGE_YUY2,
    XVIMAGE_UYVY,
    XVIMAGE_I420
};

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
    LLOGLN(0, ("rdpXvSetPortAttribute:"));
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
rdpXvPrintFormat(int id)
{
    switch (id)
    {
        case FOURCC_YV12:
            /* big buck bunny 480p h264 */
            /* vob files too */
            LLOGLN(0, ("FOURCC_YV12"));
            break;
        case FOURCC_I420:
            LLOGLN(0, ("FOURCC_I420"));
            break;
        case FOURCC_RV15:
            LLOGLN(0, ("FOURCC_RV15"));
            break;
        case FOURCC_RV16:
            LLOGLN(0, ("FOURCC_RV16"));
            break;
        case FOURCC_YUY2:
            LLOGLN(0, ("FOURCC_YUY2"));
            break;
        case FOURCC_UYVY:
            LLOGLN(0, ("FOURCC_UYVY"));
            break;
        case FOURCC_RV24:
            LLOGLN(0, ("FOURCC_RV24"));
            break;
        default:
            LLOGLN(0, ("other"));
            break;
    }
    return 0;
}

/*****************************************************************************/
static int
rdpXvQueryImageAttributes(ClientPtr client, XvPortPtr pPort, XvImagePtr format,
                          CARD16* width, CARD16* height, int* pitches,
                          int* offsets)
{
    int size;
    int tmp;

    LLOGLN(0, ("rdpXvQueryImageAttributes:"));


    size = 0;
    /* this is same code as all drivers currently have */
    if (*width > 2046)
    {
      *width = 2046;
    }
    if (*height > 2046)
    {
      *height = 2046;
    }
    /* make w multiple of 4 so that resizing works properly */
    *width = (*width + 3) & ~3;
    if (offsets)
    {
        offsets[0] = 0;
    }
    LLOGLN(0, ("format %x", format->id));
    rdpXvPrintFormat(format->id);
    switch (format->id)
    {
        case FOURCC_YV12:
        case FOURCC_I420:
            /* make h be even */
            *height = (*height + 1) & ~1;
            /* make w be multiple of 4 (ie. pad it) */
            size = (*width + 3) & ~3;
            /* width of a Y row => width of image */
            if (pitches != 0)
            {
                pitches[0] = size;
            }
            /* offset of U plane => w*h */
            size *= *height;
            if (offsets != 0)
            {
                offsets[1] = size;
            }
            /* width of U, V row => width/2 */
            tmp = ((*width >> 1) +3) & ~3;
            if (pitches != 0)
            {
                pitches[1] = pitches[2] = tmp;
            }
            /* offset of V => Y plane + U plane (w*h + w/2*h/2) */
            tmp *= (*height >> 1);
            size += tmp;
            size += tmp;
            if (offsets != 0)
            {
                offsets[2] = size;
            }
            size += tmp;
            break;
        case FOURCC_RV15:
        case FOURCC_RV16:
        case FOURCC_YUY2:
        case FOURCC_UYVY:
            size = (*width) * 2;
            if (pitches)
            {
                pitches[0] = size;
            }
            size *= *height;
            break;
        case FOURCC_RV24:
            size = (*width) * 3;
            if (pitches)
            {
                pitches[0] = size;
            }
            size *= *height;
            break;
        default:
            LLOGLN(0, ("rdpXvQueryImageAttributes: error"));
            break;
    }
    return size;
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

#ifdef MITSHM
#endif

/*****************************************************************************/
static int
rdpXvInitAdaptors(ScreenPtr pScreen)
{
    XvScreenPtr pxvs = GET_XV_SCREEN(pScreen);
    XvAdaptorPtr pAdaptor;

    pAdaptor = malloc(sizeof(XvAdaptorRec));
    memset(pAdaptor, 0, sizeof(XvAdaptorRec));
    pAdaptor->type = XvInputMask | XvOutputMask | XvImageMask |
                     XvVideoMask | XvStillMask;
    pAdaptor->pScreen = pScreen;

    pAdaptor->name = g_xv_adaptor_name;

    pAdaptor->nEncodings = T_NUM_ENCODINGS;
    pAdaptor->pEncodings = g_encodings;

    pAdaptor->nFormats = T_NUM_FORMATS;
    pAdaptor->pFormats = g_formats;

    pAdaptor->nImages = T_NUM_IMAGES;
    pAdaptor->pImages = g_images;

    pAdaptor->nPorts = T_NUM_PORTS;
    pAdaptor->pPorts = g_ports;

    pAdaptor->ddAllocatePort = rdpXvAllocatePort;
    pAdaptor->ddFreePort = rdpXvFreePort;
    pAdaptor->ddPutVideo = rdpXvPutVideo;
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

    memset(g_encodings, 0, sizeof(g_encodings));
    g_encodings[0].id = FakeClientID(0);
    g_encodings[0].pScreen = pScreen;
    g_encodings[0].name = g_xv_encoding_name;
    g_encodings[0].width = 2046;
    g_encodings[0].height = 2046;
    g_encodings[0].rate.numerator = 1;
    g_encodings[0].rate.denominator = 1;

    memset(g_formats, 0, sizeof(g_formats));
    g_formats[0].depth = g_rdpScreen.depth;
    g_formats[0].visual = pScreen->rootVisual;

    memset(g_ports, 0, sizeof(g_ports));
    g_ports[0].id = FakeClientID(0);
    g_ports[0].pAdaptor = pAdaptor;
    g_ports[0].pNotify = 0;
    g_ports[0].pDraw = 0;
    g_ports[0].grab.id = 0;
    g_ports[0].grab.client = 0;
    g_ports[0].time = currentTime;
    g_ports[0].devPriv.ptr = 0;

    pAdaptor->base_id = g_ports[0].id;

    AddResource(g_ports[0].id, XvRTPort, g_ports);

    pxvs->nAdaptors = 1;
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
