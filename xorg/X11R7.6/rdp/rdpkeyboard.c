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

keyboard

*/

#include "rdp.h"
#include "rdpkeyboard.h"
#include "rdpkeyboardevdev.h"

#define LOG_LEVEL 1
#define LLOGLN(_level, _args) \
    do { if (_level < LOG_LEVEL) { ErrorF _args ; ErrorF("\n"); } } while (0)

extern DeviceIntPtr g_keyboard; /* in rdpmain.c */
extern int g_shift_down; /* in rdpmain.c */
extern int g_alt_down; /* in rdpmain.c */
extern int g_ctrl_down; /* in rdpmain.c */
extern int g_pause_spe; /* in rdpmain.c */
extern int g_tab_down; /* in rdpmain.c */

/******************************************************************************/
void
rdpEnqueueKey(int type, int scancode)
{
    int i;
    int n;
    EventListPtr rdp_events;
    xEvent *pev;

    i = GetEventList(&rdp_events);
    n = GetKeyboardEvents(rdp_events, g_keyboard, type, scancode);

    for (i = 0; i < n; i++)
    {
        pev = (rdp_events + i)->event;
        mieqEnqueue(g_keyboard, (InternalEvent *)pev);
    }
}

/******************************************************************************/
void
check_keysa(void)
{
    if (g_ctrl_down != 0)
    {
        rdpEnqueueKey(KeyRelease, g_ctrl_down);
        g_ctrl_down = 0;
    }

    if (g_alt_down != 0)
    {
        rdpEnqueueKey(KeyRelease, g_alt_down);
        g_alt_down = 0;
    }

    if (g_shift_down != 0)
    {
        rdpEnqueueKey(KeyRelease, g_shift_down);
        g_shift_down = 0;
    }
}

/******************************************************************************/
void
sendDownUpKeyEvent(int type, int x_scancode)
{
    /* if type is keydown, send keyup + keydown */
    if (type == KeyPress)
    {
        rdpEnqueueKey(KeyRelease, x_scancode);
        rdpEnqueueKey(KeyPress, x_scancode);
    }
    else
    {
        rdpEnqueueKey(KeyRelease, x_scancode);
    }
}
