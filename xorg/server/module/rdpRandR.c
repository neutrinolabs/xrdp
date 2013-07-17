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

/******************************************************************************/
#define LOG_LEVEL 1
#define LLOGLN(_level, _args) \
    do { if (_level < LOG_LEVEL) { ErrorF _args ; ErrorF("\n"); } } while (0)

/******************************************************************************/
Bool
rdpRRRegisterSize(ScreenPtr pScreen, int width, int height)
{
    LLOGLN(0, ("rdpRRRegisterSize:"));
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
    LLOGLN(0, ("rdpRRGetInfo:"));
    return TRUE;
}

/******************************************************************************/
Bool
rdpRRScreenSetSize(ScreenPtr pScreen, CARD16 width, CARD16 height,
                   CARD32 mmWidth, CARD32 mmHeight)
{
    LLOGLN(0, ("rdpRRScreenSetSize:"));
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
    LLOGLN(0, ("rdpRRCrtcGetGamma:"));
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
rdpRRGetPanning(ScreenPtr pScrn, RRCrtcPtr crtc, BoxPtr totalArea,
                BoxPtr trackingArea, INT16 *border)
{
    LLOGLN(0, ("rdpRRGetPanning:"));
    return TRUE;
}

/******************************************************************************/
Bool
rdpRRSetPanning(ScreenPtr pScrn, RRCrtcPtr crtc, BoxPtr totalArea,
                BoxPtr trackingArea, INT16 *border)
{
    LLOGLN(0, ("rdpRRSetPanning:"));
    return TRUE;
}
