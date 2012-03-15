/*
Copyright 2011-2012 Jay Sorg

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

Bool
rdpRRRegisterSize(ScreenPtr pScreen, int width, int height);
Bool
rdpRRGetInfo(ScreenPtr pScreen, Rotation* pRotations);
Bool
rdpRRSetConfig(ScreenPtr pScreen, Rotation rotateKind, int rate,
               RRScreenSizePtr pSize);
Bool
rdpRRScreenSetSize(ScreenPtr pScreen, CARD16 width, CARD16 height,
                   CARD32 mmWidth, CARD32 mmHeight);
Bool
rdpRRCrtcSet(ScreenPtr pScreen, RRCrtcPtr crtc, RRModePtr mode,
             int x, int y, Rotation rotation, int numOutputs,
             RROutputPtr* outputs);
Bool
rdpRRCrtcSetGamma(ScreenPtr pScreen, RRCrtcPtr crtc);
Bool
rdpRRCrtcGetGamma(ScreenPtr pScreen, RRCrtcPtr crtc);
Bool
rdpRROutputSetProperty(ScreenPtr pScreen, RROutputPtr output, Atom property,
                       RRPropertyValuePtr value);
Bool
rdpRROutputValidateMode(ScreenPtr pScreen, RROutputPtr output,
                        RRModePtr mode);
void
rdpRRModeDestroy(ScreenPtr pScreen, RRModePtr mode);
Bool
rdpRROutputGetProperty(ScreenPtr   pScreen, RROutputPtr output, Atom property);
Bool
rdpRRGetPanning(ScreenPtr pScrn, RRCrtcPtr crtc, BoxPtr totalArea,
                BoxPtr trackingArea, INT16* border);
Bool
rdpRRSetPanning(ScreenPtr pScrn, RRCrtcPtr crtc, BoxPtr totalArea,
                BoxPtr trackingArea, INT16* border);

#endif
