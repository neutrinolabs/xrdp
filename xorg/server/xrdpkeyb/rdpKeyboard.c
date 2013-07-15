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

#include "X11/keysym.h"

#include "rdp.h"

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

static DeviceIntPtr g_keyboard = 0;
static OsTimerPtr g_timer = 0;

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
    XK_grave,    XK_asciitilde,
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
rdpEnqueueKey(int type, int scancode)
{
    int i;
    int n;
    EventListPtr rdp_events;
    xEvent *pev;

    i = GetEventList(&rdp_events);
    LLOGLN(0, ("rdpEnqueueKey: i %d g_keyboard %p %p", i, g_keyboard, rdp_events));
    n = GetKeyboardEvents(rdp_events, g_keyboard, type, scancode);
    LLOGLN(0, ("rdpEnqueueKey: n %d", n));
    for (i = 0; i < n; i++)
    {
        pev = (rdp_events + i)->event;
        mieqEnqueue(g_keyboard, (InternalEvent *)pev);
    }
}

#if 0
/******************************************************************************/
static CARD32
rdpDeferredUpdateCallback(OsTimerPtr timer, CARD32 now, pointer arg)
{
    LLOGLN(0, ("rdpDeferredUpdateCallback:"));

    rdpEnqueueKey(KeyPress, 115);
    rdpEnqueueKey(KeyRelease, 115);

    //xf86PostKeyboardEvent(g_keyboard, 9, 1);
    //xf86PostKeyboardEvent(g_keyboard, 9, 0);

    g_timer = TimerSet(g_timer, 0, 1000, rdpDeferredUpdateCallback, 0);
    return 0;
}
#endif

/******************************************************************************/
void
rdpKeybDeviceInit(DeviceIntPtr pDevice, KeySymsPtr pKeySyms, CARD8 *pModMap)
{
    int i;

    LLOGLN(0, ("rdpKeybDeviceInit:"));
    LLOGLN(10, ("  MAP_LENGTH %d GLYPHS_PER_KEY %d N_PREDEFINED_KEYS %d",
           MAP_LENGTH, GLYPHS_PER_KEY, N_PREDEFINED_KEYS));

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
        LLOGLN(0, ("rdpKeybDeviceInit: malloc failed"));
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
rdpKeybDeviceOn(void)
{
    LLOGLN(0, ("rdpKeybDeviceOn:"));
}

/******************************************************************************/
static void
rdpKeybDeviceOff(void)
{
    LLOGLN(0, ("rdpKeybDeviceOff:"));
}

/******************************************************************************/
static void
rdpKeybBell(int volume, DeviceIntPtr pDev, pointer ctrl, int cls)
{
    LLOGLN(0, ("rdpKeybBell:"));
}

/******************************************************************************/
static void
rdpKeybChangeKeyboardControl(DeviceIntPtr pDev, KeybdCtrl *ctrl)
{
    LLOGLN(0, ("rdpKeybChangeKeyboardControl:"));
}

/******************************************************************************/
static int
rdpKeybControl(DeviceIntPtr device, int what)
{
    KeySymsRec keySyms;
    CARD8 modMap[MAP_LENGTH];
    DevicePtr pDev;
    XkbRMLVOSet set;

    LLOGLN(0, ("rdpKeybControl: what %d", what));
    pDev = (DevicePtr)device;

    switch (what)
    {
        case DEVICE_INIT:
            rdpKeybDeviceInit(device, &keySyms, modMap);
            memset(&set, 0, sizeof(set));
            set.rules = "base";
            set.model = "pc104";
            set.layout = "us";
            set.variant = "";
            set.options = "";
            InitKeyboardDeviceStruct(device, &set, rdpKeybBell,
                                     rdpKeybChangeKeyboardControl);
            g_keyboard = device;
            //g_timer = TimerSet(g_timer, 0, 1000, rdpDeferredUpdateCallback, 0);
            break;
        case DEVICE_ON:
            pDev->on = 1;
            rdpKeybDeviceOn();
            break;
        case DEVICE_OFF:
            pDev->on = 0;
            rdpKeybDeviceOff();
            break;
        case DEVICE_CLOSE:
            if (pDev->on)
            {
                rdpKeybDeviceOff();
            }
            break;
    }
    return Success;
}

/******************************************************************************/
static int
rdpkeybPreInit(InputDriverPtr drv, InputInfoPtr info, int flags)
{
    LLOGLN(0, ("rdpkeybPreInit: drv %p info %p, flags 0x%x",
           drv, info, flags));
    info->device_control = rdpKeybControl;
    info->type_name = "Keyboard";

    return 0;
}

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
static void
rdpkeybUnplug(pointer p)
{
    LLOGLN(0, ("rdpkeybUnplug:"));
}

/******************************************************************************/
static pointer
rdpkeybPlug(pointer module, pointer options, int *errmaj, int *errmin)
{
    LLOGLN(0, ("rdpkeybPlug:"));
    xf86AddInputDriver(&rdpkeyb, module, 0);
    return module;
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
XF86ModuleData xrdpkeybModuleData =
{
    &rdpkeybVersionRec,
    rdpkeybPlug,
    rdpkeybUnplug
};
