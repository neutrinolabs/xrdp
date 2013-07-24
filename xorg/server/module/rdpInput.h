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

input

*/

#ifndef _RDPINPUT_H
#define _RDPINPUT_H

typedef int (*rdpInputEventProcPtr)(int msg, long param1, long param2,
                                    long param3, long param4);

int
rdpRegisterInputCallback(int type, rdpInputEventProcPtr proc);
int
rdpUnregisterInputCallback(rdpInputEventProcPtr proc);
int
rdpInputKeyboardEvent(int msg, long param1, long param2,
                      long param3, long param4);
int
rdpInputMouseEvent(int msg, long param1, long param2,
                   long param3, long param4);

#endif
