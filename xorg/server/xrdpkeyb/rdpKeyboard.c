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

xrdp keyboard module

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* this should be before all X11 .h files */
#include <xorg-server.h>

/* all driver need this */
#include <xf86.h>
#include <xf86_OSproc.h>

#include "xf86Xinput.h"

#include <mipointer.h>
#include <fb.h>
#include <micmap.h>
#include <mi.h>

#include <xkbsrv.h>

#include "X11/keysym.h"

#include "rdp.h"
#include "rdpInput.h"
#include "rdpDraw.h"

/******************************************************************************/
#define LOG_LEVEL 1
#define LLOGLN(_level, _args) \
    do { if (_level < LOG_LEVEL) { ErrorF _args ; ErrorF("\n"); } } while (0)

#define XRDP_DRIVER_NAME "XRDPKEYB"
#define XRDP_NAME "XRDPKEYB"
#define XRDP_VERSION 1000

#define PACKAGE_VERSION_MAJOR 1
#define PACKAGE_VERSION_MINOR 0
#define PACKAGE_VERSION_PATCHLEVEL 0

#define MIN_KEY_CODE 8
#define MAX_KEY_CODE 255
#define NO_OF_KEYS ((MAX_KEY_CODE - MIN_KEY_CODE) + 1)
#define GLYPHS_PER_KEY 2
/* control */
#define CONTROL_L_KEY_CODE 37
#define CONTROL_R_KEY_CODE 109
/* shift */
#define SHIFT_L_KEY_CODE 50
#define SHIFT_R_KEY_CODE 62
/* win keys */
#define SUPER_L_KEY_CODE 115
#define SUPER_R_KEY_CODE 116
/* alt */
#define ALT_L_KEY_CODE 64
#define ALT_R_KEY_CODE 113
/* caps lock */
#define CAPS_LOCK_KEY_CODE 66
/* num lock */
#define NUM_LOCK_KEY_CODE 77

#define N_PREDEFINED_KEYS \
    (sizeof(g_kbdMap) / (sizeof(KeySym) * GLYPHS_PER_KEY))

static KeySym g_kbdMap[] =
{
    NoSymbol,        NoSymbol,        /* 8 */
    XK_Escape,       NoSymbol,        /* 9 */
    XK_1,            XK_exclam,       /* 10 */
    XK_2,            XK_at,
    XK_3,            XK_numbersign,
    XK_4,            XK_dollar,
    XK_5,            XK_percent,
    XK_6,            XK_asciicircum,
    XK_7,            XK_ampersand,
    XK_8,            XK_asterisk,
    XK_9,            XK_parenleft,
    XK_0,            XK_parenright,
    XK_minus,        XK_underscore,   /* 20 */
    XK_equal,        XK_plus,
    XK_BackSpace,    NoSymbol,
    XK_Tab,          XK_ISO_Left_Tab,
    XK_Q,            NoSymbol,
    XK_W,            NoSymbol,
    XK_E,            NoSymbol,
    XK_R,            NoSymbol,
    XK_T,            NoSymbol,
    XK_Y,            NoSymbol,
    XK_U,            NoSymbol,        /* 30 */
    XK_I,            NoSymbol,
    XK_O,            NoSymbol,
    XK_P,            NoSymbol,
    XK_bracketleft,  XK_braceleft,
    XK_bracketright, XK_braceright,
    XK_Return,       NoSymbol,
    XK_Control_L,    NoSymbol,
    XK_A,            NoSymbol,
    XK_S,            NoSymbol,
    XK_D,            NoSymbol,        /* 40 */
    XK_F,            NoSymbol,
    XK_G,            NoSymbol,
    XK_H,            NoSymbol,
    XK_J,            NoSymbol,
    XK_K,            NoSymbol,
    XK_L,            NoSymbol,
    XK_semicolon,    XK_colon,
    XK_apostrophe,   XK_quotedbl,
    XK_grave,        XK_asciitilde,
    XK_Shift_L,      NoSymbol,        /* 50 */
    XK_backslash,    XK_bar,
    XK_Z,            NoSymbol,
    XK_X,            NoSymbol,
    XK_C,            NoSymbol,
    XK_V,            NoSymbol,
    XK_B,            NoSymbol,
    XK_N,            NoSymbol,
    XK_M,            NoSymbol,
    XK_comma,        XK_less,
    XK_period,       XK_greater,      /* 60 */
    XK_slash,        XK_question,
    XK_Shift_R,      NoSymbol,
    XK_KP_Multiply,  NoSymbol,
    XK_Alt_L,        NoSymbol,
    XK_space,        NoSymbol,
    XK_Caps_Lock,    NoSymbol,
    XK_F1,           NoSymbol,
    XK_F2,           NoSymbol,
    XK_F3,           NoSymbol,
    XK_F4,           NoSymbol,        /* 70 */
    XK_F5,           NoSymbol,
    XK_F6,           NoSymbol,
    XK_F7,           NoSymbol,
    XK_F8,           NoSymbol,
    XK_F9,           NoSymbol,
    XK_F10,          NoSymbol,
    XK_Num_Lock,     NoSymbol,
    XK_Scroll_Lock,  NoSymbol,
    XK_KP_Home,      XK_KP_7,
    XK_KP_Up,        XK_KP_8,         /* 80 */
    XK_KP_Prior,     XK_KP_9,
    XK_KP_Subtract,  NoSymbol,
    XK_KP_Left,      XK_KP_4,
    XK_KP_Begin,     XK_KP_5,
    XK_KP_Right,     XK_KP_6,
    XK_KP_Add,       NoSymbol,
    XK_KP_End,       XK_KP_1,
    XK_KP_Down,      XK_KP_2,
    XK_KP_Next,      XK_KP_3,
    XK_KP_Insert,    XK_KP_0,         /* 90 */
    XK_KP_Delete,    XK_KP_Decimal,
    NoSymbol,        NoSymbol,
    NoSymbol,        NoSymbol,
    NoSymbol,        NoSymbol,
    XK_F11,          NoSymbol,
    XK_F12,          NoSymbol,
    XK_Home,         NoSymbol,
    XK_Up,           NoSymbol,
    XK_Prior,        NoSymbol,
    XK_Left,         NoSymbol,        /* 100 */
    XK_Print,        NoSymbol,
    XK_Right,        NoSymbol,
    XK_End,          NoSymbol,
    XK_Down,         NoSymbol,
    XK_Next,         NoSymbol,
    XK_Insert,       NoSymbol,
    XK_Delete,       NoSymbol,
    XK_KP_Enter,     NoSymbol,
    XK_Control_R,    NoSymbol,
    XK_Pause,        NoSymbol,        /* 110 */
    XK_Print,        NoSymbol,
    XK_KP_Divide,    NoSymbol,
    XK_Alt_R,        NoSymbol,
    NoSymbol,        NoSymbol,
    XK_Super_L,      NoSymbol,
    XK_Super_R,      NoSymbol,
    XK_Menu,         NoSymbol,
    NoSymbol,        NoSymbol,
    NoSymbol,        NoSymbol,
    NoSymbol,        NoSymbol,        /* 120 */
    NoSymbol,        NoSymbol
};

/******************************************************************************/
static void
rdpEnqueueKey(DeviceIntPtr device, int type, int scancode)
{
    if (type == KeyPress)
    {
        xf86PostKeyboardEvent(device, scancode, TRUE);
    }
    else
    {
        xf86PostKeyboardEvent(device, scancode, FALSE);
    }
}

/******************************************************************************/
static void
sendDownUpKeyEvent(DeviceIntPtr device, int type, int x_scancode)
{
    /* need this cause rdp and X11 repeats are different */
    /* if type is keydown, send keyup + keydown */
    if (type == KeyPress)
    {
        rdpEnqueueKey(device, KeyRelease, x_scancode);
        rdpEnqueueKey(device, KeyPress, x_scancode);
    }
    else
    {
        rdpEnqueueKey(device, KeyRelease, x_scancode);
    }
}

/******************************************************************************/
static void
check_keysa(rdpKeyboard *keyboard)
{
    if (keyboard->ctrl_down != 0)
    {
        rdpEnqueueKey(keyboard->device, KeyRelease, keyboard->ctrl_down);
        keyboard->ctrl_down = 0;
    }

    if (keyboard->alt_down != 0)
    {
        rdpEnqueueKey(keyboard->device, KeyRelease, keyboard->alt_down);
        keyboard->alt_down = 0;
    }

    if (keyboard->shift_down != 0)
    {
        rdpEnqueueKey(keyboard->device, KeyRelease, keyboard->shift_down);
        keyboard->shift_down = 0;
    }
}

/**
 * @param down   - true for KeyDown events, false otherwise
 * @param param1 - ASCII code of pressed key
 * @param param2 -
 * @param param3 - scancode of pressed key
 * @param param4 -
 ******************************************************************************/
static void
KbdAddEvent(rdpKeyboard *keyboard, int down, int param1, int param2,
            int param3, int param4)
{
    int rdp_scancode;
    int x_scancode;
    int is_ext;
    int is_spe;
    int type;

    type = down ? KeyPress : KeyRelease;
    rdp_scancode = param3;
    is_ext = param4 & 256; /* 0x100 */
    is_spe = param4 & 512; /* 0x200 */
    x_scancode = 0;

    switch (rdp_scancode)
    {
        case 58: /* caps lock             */
        case 42: /* left shift            */
        case 54: /* right shift           */
        case 70: /* scroll lock           */
            x_scancode = rdp_scancode + MIN_KEY_CODE;

            if (x_scancode > 0)
            {
                rdpEnqueueKey(keyboard->device, type, x_scancode);
            }

            break;

        case 56: /* left - right alt button */

            if (is_ext)
            {
                x_scancode = 113; /* right alt button */
            }
            else
            {
                x_scancode = 64;  /* left alt button   */
            }

            rdpEnqueueKey(keyboard->device, type, x_scancode);
            break;

        case 15: /* tab */

            if (!down && !keyboard->tab_down)
            {
                /* leave x_scancode 0 here, we don't want the tab key up */
                check_keysa(keyboard);
            }
            else
            {
                sendDownUpKeyEvent(keyboard->device, type, 23);
            }

            keyboard->tab_down = down;
            break;

        case 29: /* left or right ctrl */

            /* this is to handle special case with pause key sending control first */
            if (is_spe)
            {
                if (down)
                {
                    keyboard->pause_spe = 1;
                    /* leave x_scancode 0 here, we don't want the control key down */
                }
            }
            else
            {
                x_scancode = is_ext ? 109 : 37;
                keyboard->ctrl_down = down ? x_scancode : 0;
                rdpEnqueueKey(keyboard->device, type, x_scancode);
            }

            break;

        case 69: /* Pause or Num Lock */

            if (keyboard->pause_spe)
            {
                x_scancode = 110;

                if (!down)
                {
                    keyboard->pause_spe = 0;
                }
            }
            else
            {
                x_scancode = keyboard->ctrl_down ? 110 : 77;
            }

            sendDownUpKeyEvent(keyboard->device, type, x_scancode);
            break;

        case 28: /* Enter or Return */
            x_scancode = is_ext ? 108 : 36;
            sendDownUpKeyEvent(keyboard->device, type, x_scancode);
            break;

        case 53: /* / */
            x_scancode = is_ext ? 112 : 61;
            sendDownUpKeyEvent(keyboard->device, type, x_scancode);
            break;

        case 55: /* * on KP or Print Screen */
            x_scancode = is_ext ? 111 : 63;
            sendDownUpKeyEvent(keyboard->device, type, x_scancode);
            break;

        case 71: /* 7 or Home */
            x_scancode = is_ext ? 97 : 79;
            sendDownUpKeyEvent(keyboard->device, type, x_scancode);
            break;

        case 72: /* 8 or Up */
            x_scancode = is_ext ? 98 : 80;
            sendDownUpKeyEvent(keyboard->device, type, x_scancode);
            break;

        case 73: /* 9 or PgUp */
            x_scancode = is_ext ? 99 : 81;
            sendDownUpKeyEvent(keyboard->device, type, x_scancode);
            break;

        case 75: /* 4 or Left */
            x_scancode = is_ext ? 100 : 83;
            sendDownUpKeyEvent(keyboard->device, type, x_scancode);
            break;

        case 77: /* 6 or Right */
            x_scancode = is_ext ? 102 : 85;
            sendDownUpKeyEvent(keyboard->device, type, x_scancode);
            break;

        case 79: /* 1 or End */
            x_scancode = is_ext ? 103 : 87;
            sendDownUpKeyEvent(keyboard->device, type, x_scancode);
            break;

        case 80: /* 2 or Down */
            x_scancode = is_ext ? 104 : 88;
            sendDownUpKeyEvent(keyboard->device, type, x_scancode);
            break;

        case 81: /* 3 or PgDn */
            x_scancode = is_ext ? 105 : 89;
            sendDownUpKeyEvent(keyboard->device, type, x_scancode);
            break;

        case 82: /* 0 or Insert */
            x_scancode = is_ext ? 106 : 90;
            sendDownUpKeyEvent(keyboard->device, type, x_scancode);
            break;

        case 83: /* . or Delete */
            x_scancode = is_ext ? 107 : 91;
            sendDownUpKeyEvent(keyboard->device, type, x_scancode);
            break;

        case 91: /* left win key */
            rdpEnqueueKey(keyboard->device, type, 115);
            break;

        case 92: /* right win key */
            rdpEnqueueKey(keyboard->device, type, 116);
            break;

        case 93: /* menu key */
            rdpEnqueueKey(keyboard->device, type, 117);
            break;

        default:
            x_scancode = rdp_scancode + MIN_KEY_CODE;

            if (x_scancode > 0)
            {
                sendDownUpKeyEvent(keyboard->device, type, x_scancode);
            }

            break;
    }
}

/******************************************************************************/
/* notes -
     scroll lock doesn't seem to be a modifier in X
*/
static void
KbdSync(rdpKeyboard *keyboard, int param1)
{
    int xkb_state;

    xkb_state = XkbStateFieldFromRec(&(keyboard->device->key->xkbInfo->state));

    if ((!(xkb_state & 0x02)) != (!(param1 & 4))) /* caps lock */
    {
        LLOGLN(0, ("KbdSync: toggling caps lock"));
        KbdAddEvent(keyboard, 1, 58, 0, 58, 0);
        KbdAddEvent(keyboard, 0, 58, 49152, 58, 49152);
    }

    if ((!(xkb_state & 0x10)) != (!(param1 & 2))) /* num lock */
    {
        LLOGLN(0, ("KbdSync: toggling num lock"));
        KbdAddEvent(keyboard, 1, 69, 0, 69, 0);
        KbdAddEvent(keyboard, 0, 69, 49152, 69, 49152);
    }

    if ((!(keyboard->scroll_lock_down)) != (!(param1 & 1))) /* scroll lock */
    {
        LLOGLN(0, ("KbdSync: toggling scroll lock"));
        KbdAddEvent(keyboard, 1, 70, 0, 70, 0);
        KbdAddEvent(keyboard, 0, 70, 49152, 70, 49152);
    }
}

/******************************************************************************/
static int
rdpInputKeyboard(rdpPtr dev, int msg, long param1, long param2,
                 long param3, long param4)
{
    rdpKeyboard *keyboard;

    keyboard = &(dev->keyboard);
    LLOGLN(10, ("rdpInputKeyboard:"));
    switch (msg)
    {
        case 15: /* key down */
        case 16: /* key up */
            KbdAddEvent(keyboard, msg == 15, param1, param2, param3, param4);
            break;
        case 17: /* from RDP_INPUT_SYNCHRONIZE */
            KbdSync(keyboard, param1);
            break;
    }
    return 0;
}

/******************************************************************************/
void
rdpkeybDeviceInit(DeviceIntPtr pDevice, KeySymsPtr pKeySyms, CARD8 *pModMap)
{
    int i;

    LLOGLN(0, ("rdpkeybDeviceInit:"));
    LLOGLN(10, ("  MAP_LENGTH %d GLYPHS_PER_KEY %d N_PREDEFINED_KEYS %d",
           MAP_LENGTH, GLYPHS_PER_KEY, (int) N_PREDEFINED_KEYS));

    for (i = 0; i < MAP_LENGTH; i++)
    {
        pModMap[i] = NoSymbol;
    }

    pModMap[SHIFT_L_KEY_CODE] = ShiftMask;
    pModMap[SHIFT_R_KEY_CODE] = ShiftMask;
    pModMap[CAPS_LOCK_KEY_CODE] = LockMask;
    pModMap[CONTROL_L_KEY_CODE] = ControlMask;
    pModMap[CONTROL_R_KEY_CODE] = ControlMask;
    pModMap[ALT_L_KEY_CODE] = Mod1Mask;
    pModMap[ALT_R_KEY_CODE] = Mod1Mask;
    pModMap[NUM_LOCK_KEY_CODE] = Mod2Mask;
    pModMap[SUPER_L_KEY_CODE] = Mod4Mask;
    pModMap[SUPER_R_KEY_CODE] = Mod4Mask;
    pKeySyms->minKeyCode = MIN_KEY_CODE;
    pKeySyms->maxKeyCode = MAX_KEY_CODE;
    pKeySyms->mapWidth = GLYPHS_PER_KEY;
    i = sizeof(KeySym) * MAP_LENGTH * GLYPHS_PER_KEY;
    pKeySyms->map = (KeySym *)malloc(i);
    if (pKeySyms->map == 0)
    {
        LLOGLN(0, ("rdpkeybDeviceInit: malloc failed"));
        exit(1);
    }
    else
    {
        memset(pKeySyms->map, 0, i);
    }

    for (i = 0; i < MAP_LENGTH * GLYPHS_PER_KEY; i++)
    {
        pKeySyms->map[i] = NoSymbol;
    }

    for (i = 0; i < N_PREDEFINED_KEYS * GLYPHS_PER_KEY; i++)
    {
        pKeySyms->map[i] = g_kbdMap[i];
    }
}

/******************************************************************************/
static void
rdpkeybDeviceOn(void)
{
    LLOGLN(0, ("rdpkeybDeviceOn:"));
}

/******************************************************************************/
static void
rdpkeybDeviceOff(void)
{
    LLOGLN(0, ("rdpkeybDeviceOff:"));
}

/******************************************************************************/
static void
rdpkeybBell(int volume, DeviceIntPtr pDev, pointer ctrl, int cls)
{
    LLOGLN(0, ("rdpkeybBell:"));
}

/******************************************************************************/
static void
rdpkeybChangeKeyboardControl(DeviceIntPtr pDev, KeybdCtrl *ctrl)
{
    LLOGLN(0, ("rdpkeybChangeKeyboardControl:"));
}

/******************************************************************************/
static int
rdpkeybControl(DeviceIntPtr device, int what)
{
    KeySymsRec keySyms;
    CARD8 modMap[MAP_LENGTH];
    DevicePtr pDev;
    XkbRMLVOSet set;
    rdpPtr dev;

    LLOGLN(0, ("rdpkeybControl: what %d", what));
    pDev = (DevicePtr)device;

    switch (what)
    {
        case DEVICE_INIT:
            rdpkeybDeviceInit(device, &keySyms, modMap);
            memset(&set, 0, sizeof(set));
            set.rules = "base";
            set.model = "pc104";
            set.layout = "us";
            set.variant = "";
            set.options = "";
            InitKeyboardDeviceStruct(device, &set, rdpkeybBell,
                                     rdpkeybChangeKeyboardControl);
            dev = rdpGetDevFromScreen(NULL);
            dev->keyboard.device = device;
            rdpRegisterInputCallback(0, rdpInputKeyboard);
            break;
        case DEVICE_ON:
            pDev->on = 1;
            rdpkeybDeviceOn();
            break;
        case DEVICE_OFF:
            pDev->on = 0;
            rdpkeybDeviceOff();
            break;
        case DEVICE_CLOSE:
            if (pDev->on)
            {
                rdpkeybDeviceOff();
            }
            break;
    }
    return Success;
}

#if XORG_VERSION_CURRENT < (((1) * 10000000) + ((9) * 100000) + ((0) * 1000) + 1)

/* debian 6
   ubuntu 10.04 */

/******************************************************************************/
static InputInfoPtr
rdpkeybPreInit(InputDriverPtr drv, IDevPtr dev, int flags)
{
    InputInfoPtr info;

    LLOGLN(0, ("rdpkeybPreInit: drv %p dev %p, flags 0x%x",
           drv, dev, flags));
    info = xf86AllocateInput(drv, 0);
    info->name = dev->identifier;
    info->device_control = rdpkeybControl;
    info->flags = XI86_CONFIGURED | XI86_ALWAYS_CORE | XI86_SEND_DRAG_EVENTS |
                  XI86_CORE_KEYBOARD | XI86_KEYBOARD_CAPABLE;
    info->type_name = "Keyboard";
    info->fd = -1;
    info->conf_idev = dev;

    return info;
}

#else

/* debian 7
   ubuntu 12.04 */

/******************************************************************************/
static int
rdpkeybPreInit(InputDriverPtr drv, InputInfoPtr info, int flags)
{
    LLOGLN(0, ("rdpkeybPreInit: drv %p info %p, flags 0x%x",
           drv, info, flags));
    info->device_control = rdpkeybControl;
    info->type_name = "Keyboard";

    return 0;
}

#endif

/******************************************************************************/
static void
rdpkeybUnInit(InputDriverPtr drv, InputInfoPtr info, int flags)
{
    LLOGLN(0, ("rdpkeybUnInit: drv %p info %p, flags 0x%x",
           drv, info, flags));
}

/******************************************************************************/
static InputDriverRec rdpkeyb =
{
    PACKAGE_VERSION_MAJOR,  /* version   */
    XRDP_NAME,              /* name      */
    NULL,                   /* identify  */
    rdpkeybPreInit,         /* preinit   */
    rdpkeybUnInit,          /* uninit    */
    NULL,                   /* module    */
    0                       /* ref count */
};

/******************************************************************************/
static pointer
rdpkeybPlug(pointer module, pointer options, int *errmaj, int *errmin)
{
    LLOGLN(0, ("rdpkeybPlug:"));
    xf86AddInputDriver(&rdpkeyb, module, 0);
    return module;
}

/******************************************************************************/
static void
rdpkeybUnplug(pointer p)
{
    LLOGLN(0, ("rdpkeybUnplug:"));
}

/******************************************************************************/
static XF86ModuleVersionInfo rdpkeybVersionRec =
{
    XRDP_DRIVER_NAME,
    MODULEVENDORSTRING,
    MODINFOSTRING1,
    MODINFOSTRING2,
    XORG_VERSION_CURRENT,
    PACKAGE_VERSION_MAJOR,
    PACKAGE_VERSION_MINOR,
    PACKAGE_VERSION_PATCHLEVEL,
    ABI_CLASS_XINPUT,
    ABI_XINPUT_VERSION,
    MOD_CLASS_XINPUT,
    { 0, 0, 0, 0 }
};

/******************************************************************************/
_X_EXPORT XF86ModuleData xrdpkeybModuleData =
{
    &rdpkeybVersionRec,
    rdpkeybPlug,
    rdpkeybUnplug
};
