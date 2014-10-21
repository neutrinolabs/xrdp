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

*/

#ifndef _RDPRANDR_H
#define _RDPRANDR_H

#include <xorg-server.h>
#include <xorgVersion.h>
#include <xf86.h>

extern _X_EXPORT Bool
rdpRRRegisterSize(ScreenPtr pScreen, int width, int height);
extern _X_EXPORT Bool
rdpRRGetInfo(ScreenPtr pScreen, Rotation* pRotations);
extern _X_EXPORT Bool
rdpRRSetConfig(ScreenPtr pScreen, Rotation rotateKind, int rate,
               RRScreenSizePtr pSize);
extern _X_EXPORT Bool
rdpRRScreenSetSize(ScreenPtr pScreen, CARD16 width, CARD16 height,
                   CARD32 mmWidth, CARD32 mmHeight);
extern _X_EXPORT Bool
rdpRRCrtcSet(ScreenPtr pScreen, RRCrtcPtr crtc, RRModePtr mode,
             int x, int y, Rotation rotation, int numOutputs,
             RROutputPtr* outputs);
extern _X_EXPORT Bool
rdpRRCrtcSetGamma(ScreenPtr pScreen, RRCrtcPtr crtc);
extern _X_EXPORT Bool
rdpRRCrtcGetGamma(ScreenPtr pScreen, RRCrtcPtr crtc);
extern _X_EXPORT Bool
rdpRROutputSetProperty(ScreenPtr pScreen, RROutputPtr output, Atom property,
                       RRPropertyValuePtr value);
extern _X_EXPORT Bool
rdpRROutputValidateMode(ScreenPtr pScreen, RROutputPtr output,
                        RRModePtr mode);
extern _X_EXPORT void
rdpRRModeDestroy(ScreenPtr pScreen, RRModePtr mode);
extern _X_EXPORT Bool
rdpRROutputGetProperty(ScreenPtr   pScreen, RROutputPtr output, Atom property);
extern _X_EXPORT Bool
rdpRRGetPanning(ScreenPtr pScrn, RRCrtcPtr crtc, BoxPtr totalArea,
                BoxPtr trackingArea, INT16* border);
extern _X_EXPORT Bool
rdpRRSetPanning(ScreenPtr pScrn, RRCrtcPtr crtc, BoxPtr totalArea,
                BoxPtr trackingArea, INT16* border);

#endif
