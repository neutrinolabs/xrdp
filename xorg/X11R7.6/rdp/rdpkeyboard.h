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

keyboard map for base rules

*/

#ifndef _RDPKEYBOARD_H
#define _RDPKEYBOARD_H

#define MIN_KEY_CODE 8
#define MAX_KEY_CODE 255
#define NO_OF_KEYS ((MAX_KEY_CODE - MIN_KEY_CODE) + 1)
#define GLYPHS_PER_KEY 2

#define RDPSCAN_Tab         15
#define RDPSCAN_Return      28 /* ext is used to know KP or not */
#define RDPSCAN_Control     29 /* ext is used to know L or R */
#define RDPSCAN_Shift_L     42
#define RDPSCAN_Slash       53
#define RDPSCAN_Shift_R     54
#define RDPSCAN_KP_Multiply 55
#define RDPSCAN_Alt         56 /* ext is used to know L or R */
#define RDPSCAN_Caps_Lock   58
#define RDPSCAN_Pause       69
#define RDPSCAN_Scroll_Lock 70
#define RDPSCAN_KP_7        71 /* KP7 or home */
#define RDPSCAN_KP_8        72 /* KP8 or up */
#define RDPSCAN_KP_9        73 /* KP9 or page up */
#define RDPSCAN_KP_4        75 /* KP4 or left */
#define RDPSCAN_KP_6        77 /* KP6 or right */
#define RDPSCAN_KP_1        79 /* KP1 or home */
#define RDPSCAN_KP_2        80 /* KP2 or up */
#define RDPSCAN_KP_3        81 /* KP3 or page down */
#define RDPSCAN_KP_0        82 /* KP0 or insert */
#define RDPSCAN_KP_Decimal  83 /* KP. or delete */
#define RDPSCAN_89          89
#define RDPSCAN_90          90
#define RDPSCAN_LWin        91
#define RDPSCAN_RWin        92
#define RDPSCAN_Menu        93
#define RDPSCAN_115         115
#define RDPSCAN_126         126

void
rdpEnqueueKey(int type, int scancode);
void
check_keysa(void);
void
sendDownUpKeyEvent(int type, int x_scancode);

#endif
