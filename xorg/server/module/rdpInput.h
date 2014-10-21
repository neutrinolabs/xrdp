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

input

*/

#ifndef _RDPINPUT_H
#define _RDPINPUT_H

#include <xorg-server.h>
#include <xorgVersion.h>
#include <xf86.h>

typedef int (*rdpInputEventProcPtr)(rdpPtr dev, int msg,
                                    long param1, long param2,
                                    long param3, long param4);

extern _X_EXPORT int
rdpRegisterInputCallback(int type, rdpInputEventProcPtr proc);
extern _X_EXPORT int
rdpUnregisterInputCallback(rdpInputEventProcPtr proc);
extern _X_EXPORT int
rdpInputKeyboardEvent(rdpPtr dev, int msg,
                      long param1, long param2,
                      long param3, long param4);
extern _X_EXPORT int
rdpInputMouseEvent(rdpPtr dev, int msg,
                   long param1, long param2,
                   long param3, long param4);
extern _X_EXPORT int
rdpInputInit(void);

#endif
