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

keyboard map for evdev rules

*/

#include "rdp.h"
#include "rdpkeyboard.h"
#include "rdpkeyboardevdev.h"

extern DeviceIntPtr g_keyboard; /* in rdpmain.c */
extern int g_shift_down; /* in rdpmain.c */
extern int g_alt_down; /* in rdpmain.c */
extern int g_ctrl_down; /* in rdpmain.c */
extern int g_pause_spe; /* in rdpmain.c */
extern int g_tab_down; /* in rdpmain.c */

#define LOG_LEVEL 1
#define LLOGLN(_level, _args) \
    do { if (_level < LOG_LEVEL) { ErrorF _args ; ErrorF("\n"); } } while (0)

#define XSCAN_Tab         23
#define XSCAN_Return      36 /* above right shift */
#define XSCAN_Control_L   37
#define XSCAN_Shift_L     50
#define XSCAN_slash       61
#define XSCAN_Shift_R     62
#define XSCAN_KP_Multiply 63
#define XSCAN_Alt_L       64
#define XSCAN_Caps_Lock   66 /* caps lock */
#define XSCAN_Num_Lock    77 /* num lock */
#define XSCAN_KP_7        79
#define XSCAN_KP_8        80
#define XSCAN_KP_9        81
#define XSCAN_KP_4        83
#define XSCAN_KP_6        85
#define XSCAN_KP_1        87
#define XSCAN_KP_2        88
#define XSCAN_KP_3        89
#define XSCAN_KP_0        90
#define XSCAN_KP_Decimal  91
/* "/ ?" on br keybaord */
#define XSCAN_97          97
#define XSCAN_Enter       104 /* on keypad */
#define XSCAN_Control_R   105
#define XSCAN_KP_Divide   106
#define XSCAN_Print       107
#define XSCAN_Alt_R       108
#define XSCAN_Home        110
#define XSCAN_Up          111
#define XSCAN_Prior       112
#define XSCAN_Left        113
#define XSCAN_Right       114
#define XSCAN_End         115
#define XSCAN_Down        116
#define XSCAN_Next        117
#define XSCAN_Insert      118
#define XSCAN_Delete      119
#define XSCAN_Pause       127
/* . on br keypad */
#define XSCAN_129         129
#define XSCAN_LWin        133
#define XSCAN_RWin        134
#define XSCAN_Menu        135
#define XSCAN_LMeta       156
#define XSCAN_RMeta       156

/******************************************************************************/
void
KbdAddEvent_evdev(int down, int param1, int param2, int param3, int param4)
{
    int rdp_scancode;
    int x_scancode;
    int is_ext;
    int is_spe;
    int type;

    LLOGLN(10, ("KbdAddEvent_evdev: down=0x%x param1=0x%x param2=0x%x "
           "param3=0x%x param4=0x%x", down, param1, param2, param3, param4));
    if (g_keyboard == 0)
    {
        return;
    }
    type = down ? KeyPress : KeyRelease;
    rdp_scancode = param3;
    is_ext = param4 & 256; /* 0x100 */
    is_spe = param4 & 512; /* 0x200 */
    x_scancode = 0;

    switch (rdp_scancode)
    {
        case RDPSCAN_Caps_Lock:   /* caps lock             */
        case RDPSCAN_Shift_L:     /* left shift            */
        case RDPSCAN_Shift_R:     /* right shift           */
        case RDPSCAN_Scroll_Lock: /* scroll lock           */
            x_scancode = rdp_scancode + MIN_KEY_CODE;

            if (x_scancode > 0)
            {
                /* left or right shift */
                if ((rdp_scancode == RDPSCAN_Shift_L) ||
                    (rdp_scancode == RDPSCAN_Shift_R))
                {
                    g_shift_down = down ? x_scancode : 0;
                }
                rdpEnqueueKey(type, x_scancode);
            }
            break;

        case RDPSCAN_Alt: /* left - right alt button */

            if (is_ext)
            {
                x_scancode = XSCAN_Alt_R; /* right alt button */
            }
            else
            {
                x_scancode = XSCAN_Alt_L;  /* left alt button   */
            }

            g_alt_down = down ? x_scancode : 0;
            rdpEnqueueKey(type, x_scancode);
            break;

        case RDPSCAN_Tab: /* tab */

            if (!down && !g_tab_down)
            {
                check_keysa(); /* leave x_scancode 0 here, we don't want the tab key up */
            }
            else
            {
                sendDownUpKeyEvent(type, XSCAN_Tab);
            }

            g_tab_down = down;
            break;

        case RDPSCAN_Control: /* left or right ctrl */

            /* this is to handle special case with pause key sending control first */
            if (is_spe)
            {
                if (down)
                {
                    g_pause_spe = 1;
                    /* leave x_scancode 0 here, we don't want the control key down */
                }
            }
            else
            {
                x_scancode = is_ext ? XSCAN_Control_R : XSCAN_Control_L;
                g_ctrl_down = down ? x_scancode : 0;
                rdpEnqueueKey(type, x_scancode);
            }

            break;

        case RDPSCAN_Pause: /* Pause or Num Lock */

            if (g_pause_spe)
            {
                x_scancode = XSCAN_Pause;

                if (!down)
                {
                    g_pause_spe = 0;
                }
            }
            else
            {
                x_scancode = g_ctrl_down ? XSCAN_Pause : XSCAN_Num_Lock;
            }

            rdpEnqueueKey(type, x_scancode);
            break;

        case RDPSCAN_Return: /* Enter or Return */
            x_scancode = is_ext ? XSCAN_Enter : XSCAN_Return;
            sendDownUpKeyEvent(type, x_scancode);
            break;

        case RDPSCAN_Slash: /* / */
            x_scancode = is_ext ? XSCAN_KP_Divide : XSCAN_slash;
            sendDownUpKeyEvent(type, x_scancode);
            break;

        case RDPSCAN_KP_Multiply: /* * on KP or Print Screen */
            x_scancode = is_ext ? XSCAN_Print : XSCAN_KP_Multiply;
            sendDownUpKeyEvent(type, x_scancode);
            break;

        case RDPSCAN_KP_7: /* 7 or Home */
            x_scancode = is_ext ? XSCAN_Home : XSCAN_KP_7;
            sendDownUpKeyEvent(type, x_scancode);
            break;

        case RDPSCAN_KP_8: /* 8 or Up */
            x_scancode = is_ext ? XSCAN_Up : XSCAN_KP_8;
            sendDownUpKeyEvent(type, x_scancode);
            break;

        case RDPSCAN_KP_9: /* 9 or PgUp */
            x_scancode = is_ext ? XSCAN_Prior : XSCAN_KP_9;
            sendDownUpKeyEvent(type, x_scancode);
            break;

        case RDPSCAN_KP_4: /* 4 or Left */
            x_scancode = is_ext ? XSCAN_Left : XSCAN_KP_4;
            sendDownUpKeyEvent(type, x_scancode);
            break;

        case RDPSCAN_KP_6: /* 6 or Right */
            x_scancode = is_ext ? XSCAN_Right : XSCAN_KP_6;
            sendDownUpKeyEvent(type, x_scancode);
            break;

        case RDPSCAN_KP_1: /* 1 or End */
            x_scancode = is_ext ? XSCAN_End : XSCAN_KP_1;
            sendDownUpKeyEvent(type, x_scancode);
            break;

        case RDPSCAN_KP_2: /* 2 or Down */
            x_scancode = is_ext ? XSCAN_Down : XSCAN_KP_2;
            sendDownUpKeyEvent(type, x_scancode);
            break;

        case RDPSCAN_KP_3: /* 3 or PgDn */
            x_scancode = is_ext ? XSCAN_Next : XSCAN_KP_3;
            sendDownUpKeyEvent(type, x_scancode);
            break;

        case RDPSCAN_KP_0: /* 0 or Insert */
            x_scancode = is_ext ? XSCAN_Insert : XSCAN_KP_0;
            sendDownUpKeyEvent(type, x_scancode);
            break;

        case RDPSCAN_KP_Decimal: /* . or Delete */
            x_scancode = is_ext ? XSCAN_Delete : XSCAN_KP_Decimal;
            sendDownUpKeyEvent(type, x_scancode);
            break;

        case RDPSCAN_LWin: /* left win key */
            rdpEnqueueKey(type, XSCAN_LWin);
            break;

        case RDPSCAN_RWin: /* right win key */
            rdpEnqueueKey(type, XSCAN_RWin);
            break;

        case RDPSCAN_Menu: /* menu key */
            rdpEnqueueKey(type, XSCAN_Menu);
            break;

        case RDPSCAN_89: /* left meta */
            rdpEnqueueKey(type, XSCAN_LMeta);
            break;

        case RDPSCAN_90: /* right meta */
            rdpEnqueueKey(type, XSCAN_RMeta);
            break;

        case RDPSCAN_115:
            rdpEnqueueKey(type, XSCAN_97); /* "/ ?" on br keybaord */
            break;

        case RDPSCAN_126:
            rdpEnqueueKey(type, XSCAN_129); /* . on br keypad */
            break;

        default:
            x_scancode = rdp_scancode + MIN_KEY_CODE;

            if (x_scancode > 0)
            {
                LLOGLN(10, ("KbdAddEvent_evdev: rdp_scancode %d x_scancode %d",
                       rdp_scancode, x_scancode));
                sendDownUpKeyEvent(type, x_scancode);
            }

            break;
    }
}
