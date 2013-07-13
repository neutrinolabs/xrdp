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

to deal with privates changing in xorg versions

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

#include "rdpPri.h"

/*****************************************************************************/
rdpDevPrivateKey
rdpAllocateGCPrivate(ScreenPtr pScreen, int bytes)
{
    return 0;
}

/*****************************************************************************/
rdpDevPrivateKey
rdpAllocatePixmapPrivate(ScreenPtr pScreen, int bytes)
{
    return 0;
}

/*****************************************************************************/
rdpDevPrivateKey
rdpAllocateWindowPrivate(ScreenPtr pScreen, int bytes)
{
    return 0;
}

/*****************************************************************************/
void*
rdpGetGCPrivate(GCPtr pGC, rdpDevPrivateKey key)
{
    return 0;
}

/*****************************************************************************/
void*
rdpGetPixmapPrivate(PixmapPtr pPixmap, rdpDevPrivateKey key)
{
    return 0;
}

/*****************************************************************************/
void*
rdpGetWindowPrivate(WindowPtr pWindow, rdpDevPrivateKey key)
{
    return 0;
}

/*****************************************************************************/
int
rdpPrivateInit(void)
{
    return 0;
}
