/*
Copyright 2005-2014 Jay Sorg

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* this should be before all X11 .h files */
#include <xorg-server.h>

/* all driver need this */
#include <xf86.h>
#include <xf86_OSproc.h>

#include "rdp.h"
#include "rdpDraw.h"

#define LOG_LEVEL 1
#define LLOGLN(_level, _args) \
    do { if (_level < LOG_LEVEL) { ErrorF _args ; ErrorF("\n"); } } while (0)

/******************************************************************************/
int
rdpPolyText8Org(DrawablePtr pDrawable, GCPtr pGC,
                int x, int y, int count, char *chars)
{
    GC_OP_VARS;
    int rv;

    GC_OP_PROLOGUE(pGC);
    rv = pGC->ops->PolyText8(pDrawable, pGC, x, y, count, chars);
    GC_OP_EPILOGUE(pGC);
    return rv;
}

/******************************************************************************/
int
rdpPolyText8(DrawablePtr pDrawable, GCPtr pGC,
             int x, int y, int count, char *chars)
{
    int rv;

    LLOGLN(0, ("rdpPolyText8:"));
    /* do original call */
    rv = rdpPolyText8Org(pDrawable, pGC, x, y, count, chars);
    return rv;
}
