/*
Copyright 2005-2012 Jay Sorg

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

keyboard and mouse stuff

*/

/* control notes */
/* rdesktop sends control before scan code 69 but it doesn't set the
   flags right so control down is used to determine between pause and
   num lock */
/* this should be fixed in rdesktop */
/* g_pause_spe flag for specal control sent by ms client before scan code
   69 is sent to tell that its pause, not num lock.  both pause and num
   lock use scan code 69 */

/* tab notes */
/* mstsc send tab up without a tab down to mark the mstsc has gained focus
   this should have sure control alt and shift are all up
   rdesktop does not do this */
/* this should be fixed in rdesktop */

#include "rdp.h"
#include "cursors.h"

#if 1
#define DEBUG_OUT_INPUT(arg)
#else
#define DEBUG_OUT_INPUT(arg) ErrorF arg
#endif

extern ScreenPtr g_pScreen; /* in rdpmain.c */
extern DeviceIntPtr g_pointer; /* in rdpmain.c */
extern DeviceIntPtr g_keyboard; /* in rdpmain.c */
extern rdpScreenInfoRec g_rdpScreen; /* from rdpmain.c */

static int g_old_button_mask = 0;
static int g_pause_spe = 0;
static int g_ctrl_down = 0;
static int g_alt_down = 0;
static int g_shift_down = 0;
static int g_tab_down = 0;
/* this is toggled every time num lock key is released, not like the
   above *_down vars */
static int g_scroll_lock_down = 0;
static int g_initialized_mouse_pointer_cache = 0;

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

#if 0
/******************************************************************************/
static void
rdpSendBell(void)
{
    DEBUG_OUT_INPUT(("rdpSendBell\n"));
}
#endif

/******************************************************************************/
void
KbdDeviceInit(DeviceIntPtr pDevice, KeySymsPtr pKeySyms, CARD8 *pModMap)
{
    int i;

    DEBUG_OUT_INPUT(("KbdDeviceInit\n"));

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
    pKeySyms->map = (KeySym *)g_malloc(i, 1);

    if (pKeySyms->map == 0)
    {
        rdpLog("KbdDeviceInit g_malloc failed\n");
        exit(1);
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
void
KbdDeviceOn(void)
{
    DEBUG_OUT_INPUT(("KbdDeviceOn\n"));
}

/******************************************************************************/
void
KbdDeviceOff(void)
{
    DEBUG_OUT_INPUT(("KbdDeviceOff\n"));
}

/******************************************************************************/
void
rdpBell(int volume, DeviceIntPtr pDev, pointer ctrl, int cls)
{
    ErrorF("rdpBell:\n");
}

/******************************************************************************/
void
rdpChangeKeyboardControl(DeviceIntPtr pDev, KeybdCtrl *ctrl)
{
    ErrorF("rdpChangeKeyboardControl:\n");
}

/******************************************************************************/
int
rdpKeybdProc(DeviceIntPtr pDevice, int onoff)
{
    KeySymsRec keySyms;
    CARD8 modMap[MAP_LENGTH];
    DevicePtr pDev;
    XkbRMLVOSet set;

    DEBUG_OUT_INPUT(("rdpKeybdProc\n"));
    pDev = (DevicePtr)pDevice;

    switch (onoff)
    {
        case DEVICE_INIT:
            KbdDeviceInit(pDevice, &keySyms, modMap);
            memset(&set, 0, sizeof(set));
            set.rules = "base";
            set.model = "pc104";
            set.layout = "us";
            set.variant = "";
            set.options = "";
            InitKeyboardDeviceStruct(pDevice, &set, rdpBell,
                                     rdpChangeKeyboardControl);
            //XkbDDXChangeControls(pDevice, 0, 0);
            break;
        case DEVICE_ON:
            pDev->on = 1;
            KbdDeviceOn();
            break;
        case DEVICE_OFF:
            pDev->on = 0;
            KbdDeviceOff();
            break;
        case DEVICE_CLOSE:

            if (pDev->on)
            {
                KbdDeviceOff();
            }

            break;
    }

    return Success;
}

/******************************************************************************/
void
PtrDeviceControl(DeviceIntPtr dev, PtrCtrl *ctrl)
{
    DEBUG_OUT_INPUT(("PtrDeviceControl\n"));
}

/******************************************************************************/
void
PtrDeviceInit(void)
{
    DEBUG_OUT_INPUT(("PtrDeviceInit\n"));
}

/******************************************************************************/
void
PtrDeviceOn(DeviceIntPtr pDev)
{
    DEBUG_OUT_INPUT(("PtrDeviceOn\n"));
}

/******************************************************************************/
void
PtrDeviceOff(void)
{
    DEBUG_OUT_INPUT(("PtrDeviceOff\n"));
}

/******************************************************************************/
static void
rdpMouseCtrl(DeviceIntPtr pDevice, PtrCtrl *pCtrl)
{
    ErrorF("rdpMouseCtrl:\n");
}

/******************************************************************************/
int
rdpMouseProc(DeviceIntPtr pDevice, int onoff)
{
    BYTE map[6];
    DevicePtr pDev;
    Atom btn_labels[6];
    Atom axes_labels[2];

    DEBUG_OUT_INPUT(("rdpMouseProc\n"));
    pDev = (DevicePtr)pDevice;

    switch (onoff)
    {
        case DEVICE_INIT:
            PtrDeviceInit();
            map[0] = 0;
            map[1] = 1;
            map[2] = 2;
            map[3] = 3;
            map[4] = 4;
            map[5] = 5;

            btn_labels[0] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_LEFT);
            btn_labels[1] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_MIDDLE);
            btn_labels[2] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_RIGHT);
            btn_labels[3] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_WHEEL_UP);
            btn_labels[4] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_WHEEL_DOWN);

            axes_labels[0] = XIGetKnownProperty(AXIS_LABEL_PROP_REL_X);
            axes_labels[1] = XIGetKnownProperty(AXIS_LABEL_PROP_REL_Y);

            InitPointerDeviceStruct(pDev, map, 5, btn_labels, rdpMouseCtrl,
                                    GetMotionHistorySize(), 2, axes_labels);

            break;
        case DEVICE_ON:
            pDev->on = 1;
            PtrDeviceOn(pDevice);
            break;
        case DEVICE_OFF:
            pDev->on = 0;
            PtrDeviceOff();
            break;
        case DEVICE_CLOSE:

            if (pDev->on)
            {
                PtrDeviceOff();
            }

            break;
    }

    return Success;
}

/******************************************************************************/
Bool
rdpCursorOffScreen(ScreenPtr *ppScreen, int *x, int *y)
{
    DEBUG_OUT_INPUT(("rdpCursorOffScreen\n"));
    return 0;
}

/******************************************************************************/
void
rdpCrossScreen(ScreenPtr pScreen, Bool entering)
{
    DEBUG_OUT_INPUT(("rdpCrossScreen\n"));
}

/******************************************************************************/
void
rdpPointerWarpCursor(DeviceIntPtr pDev, ScreenPtr pScr, int x, int y)
{
    ErrorF("rdpPointerWarpCursor:\n");
    miPointerWarpCursor(pDev, pScr, x, y);
}

/******************************************************************************/
void
rdpPointerEnqueueEvent(DeviceIntPtr pDev, InternalEvent *event)
{
    ErrorF("rdpPointerEnqueueEvent:\n");
}

/******************************************************************************/
void
rdpPointerNewEventScreen(DeviceIntPtr pDev, ScreenPtr pScr, Bool fromDIX)
{
    ErrorF("rdpPointerNewEventScreen:\n");
}

/******************************************************************************/
Bool
rdpSpriteRealizeCursor(DeviceIntPtr pDev, ScreenPtr pScr, CursorPtr pCurs)
{
    DEBUG_OUT_INPUT(("rdpSpriteRealizeCursor\n"));
    return 1;
}

/******************************************************************************/
Bool
rdpSpriteUnrealizeCursor(DeviceIntPtr pDev, ScreenPtr pScr, CursorPtr pCurs)
{
    DEBUG_OUT_INPUT(("hi rdpSpriteUnrealizeCursor\n"));
    return 1;
}

/******************************************************************************/
int
rdpGetBit(unsigned char *line, int x)
{
    unsigned char mask;

    if (screenInfo.bitmapBitOrder == LSBFirst)
    {
        mask = (1 << (x & 7));
    }
    else
    {
        mask = (0x80 >> (x & 7));
    }
    line += (x >> 3);
    if (*line & mask)
    {
        return 1;
    }
    return 0;
}

/******************************************************************************/
void
rdpSetBit(unsigned char *line, int x)
{
    unsigned char mask = (0x80 >> (x & 7));
    line += (x >> 3);
    *line |= mask;
}

/******************************************************************************/
void
rdpDrawPredefinedCursor(const char * shape, int x, int y, int w, int h, int fgcolor, int bgcolor,
                        char * cur_data, char * cur_mask)
{
    int color = -1;
    if (x < w && y < h)
    {
        char c = shape[x + y * w];
        if (c == 'F')
        {
            color = fgcolor;
        }
        else if (c == 'B')
        {
            color = bgcolor;
        }
    }

    if (color != -1)
    {
        char *dst = cur_data + (3 * (((31 - y) << 5) + x));
        dst[0] = color >> 16;
        dst[1] = color >> 8;
        dst[2] = color;
    }
    else
    {
        rdpSetBit((unsigned char *) cur_mask, ((31 - y) << 5) + x);
    }
}

/******************************************************************************/
void
rdpInitializeMousePointerCache(const int fgcolor, const int bgcolor)
{
    int x, y;

    for (y = 0; y < 32; y++)
    {
        for (x = 0; x < 32; x++)
        {
            rdpDrawPredefinedCursor(shape_2382, x, y, 24, 24, fgcolor, bgcolor, data_2382, mask_2382);
            rdpDrawPredefinedCursor(shape_1103, x, y, 24, 24, fgcolor, bgcolor, data_1103, mask_1103);
            rdpDrawPredefinedCursor(shape_3307, x, y, 24, 24, fgcolor, bgcolor, data_3307, mask_3307);
            rdpDrawPredefinedCursor(shape_2926, x, y, 24, 24, fgcolor, bgcolor, data_2926, mask_2926);
            rdpDrawPredefinedCursor(shape_3153, x, y, 24, 24, fgcolor, bgcolor, data_3153, mask_3153);
            rdpDrawPredefinedCursor(shape_3373, x, y, 24, 24, fgcolor, bgcolor, data_3373, mask_3373);
            rdpDrawPredefinedCursor(shape_3794, x, y, 24, 24, fgcolor, bgcolor, data_3794, mask_3794);
            rdpDrawPredefinedCursor(shape_2905, x, y, 24, 24, fgcolor, bgcolor, data_2905, mask_2905);
            rdpDrawPredefinedCursor(shape_3063, x, y, 24, 24, fgcolor, bgcolor, data_3063, mask_3063);
            rdpDrawPredefinedCursor(shape_2711, x, y, 24, 24, fgcolor, bgcolor, data_2711, mask_2711);
            rdpDrawPredefinedCursor(shape_4223, x, y, 24, 24, fgcolor, bgcolor, data_4223, mask_4223);
            rdpDrawPredefinedCursor(shape_3128, x, y, 24, 24, fgcolor, bgcolor, data_3128, mask_3128);
            rdpDrawPredefinedCursor(shape_3484, x, y, 24, 24, fgcolor, bgcolor, data_3484, mask_3484);
            rdpDrawPredefinedCursor(shape_1268, x, y, 24, 24, fgcolor, bgcolor, data_1268, mask_1268);
            rdpDrawPredefinedCursor(shape_3728, x, y, 24, 24, fgcolor, bgcolor, data_3728, mask_3728);
            rdpDrawPredefinedCursor(shape_4838, x, y, 24, 24, fgcolor, bgcolor, data_4838, mask_4838);
            rdpDrawPredefinedCursor(shape_5185, x, y, 24, 24, fgcolor, bgcolor, data_5185, mask_5185);
            rdpDrawPredefinedCursor(shape_4976, x, y, 24, 24, fgcolor, bgcolor, data_4976, mask_4976);
            rdpDrawPredefinedCursor(shape_4877, x, y, 24, 24, fgcolor, bgcolor, data_4877, mask_4877);
            rdpDrawPredefinedCursor(shape_5008, x, y, 24, 24, fgcolor, bgcolor, data_5008, mask_5008);
            rdpDrawPredefinedCursor(shape_5180, x, y, 24, 24, fgcolor, bgcolor, data_5180, mask_5180);
            rdpDrawPredefinedCursor(shape_5068, x, y, 24, 24, fgcolor, bgcolor, data_5068, mask_5068);
            rdpDrawPredefinedCursor(shape_4866, x, y, 24, 24, fgcolor, bgcolor, data_4866, mask_4866);
            rdpDrawPredefinedCursor(shape_3501, x, y, 24, 24, fgcolor, bgcolor, data_3501, mask_3501);
            rdpDrawPredefinedCursor(shape_1848, x, y, 24, 24, fgcolor, bgcolor, data_1848, mask_1848);
            rdpDrawPredefinedCursor(shape_2760, x, y, 24, 24, fgcolor, bgcolor, data_2760, mask_2760);
            rdpDrawPredefinedCursor(shape_2949, x, y, 24, 24, fgcolor, bgcolor, data_2949, mask_2949);
            rdpDrawPredefinedCursor(shape_1662, x, y, 24, 24, fgcolor, bgcolor, data_1662, mask_1662);
            rdpDrawPredefinedCursor(shape_1721, x, y, 24, 24, fgcolor, bgcolor, data_1721, mask_1721);
            rdpDrawPredefinedCursor(shape_3007, x, y, 24, 24, fgcolor, bgcolor, data_3007, mask_3007);
            rdpDrawPredefinedCursor(shape_4045, x, y, 24, 24, fgcolor, bgcolor, data_4045, mask_4045);
            rdpDrawPredefinedCursor(shape_4153, x, y, 24, 24, fgcolor, bgcolor, data_4153, mask_4153);
            rdpDrawPredefinedCursor(shape_4483, x, y, 24, 24, fgcolor, bgcolor, data_4483, mask_4483);

            rdpDrawPredefinedCursor(shape_5144, x, y, 25, 25, fgcolor, bgcolor, data_5144, mask_5144);
            rdpDrawPredefinedCursor(shape_5150, x, y, 25, 25, fgcolor, bgcolor, data_5150, mask_5150);

            rdpDrawPredefinedCursor(shape_5583, x, y, 29, 29, fgcolor, bgcolor, data_5583, mask_5583);

            rdpDrawPredefinedCursor(shape_2494, x, y, 32, 32, fgcolor, bgcolor, data_2494, mask_2494);
        }
    }
    ErrorF("Initialized mouse pointer cache\n");
}

/******************************************************************************/
void
getCachedMousePointerSprite(const int id, char **data, char **mask)
{
    switch (id)
    {
        case 2382: CACHED_MP_DATA(data_2382, mask_2382);
        case 1103: CACHED_MP_DATA(data_1103, mask_1103);
        case 3307: CACHED_MP_DATA(data_3307, mask_3307);
        case 2926: CACHED_MP_DATA(data_2926, mask_2926);
        case 3153: CACHED_MP_DATA(data_3153, mask_3153);
        case 3373: CACHED_MP_DATA(data_3373, mask_3373);
        case 3794: CACHED_MP_DATA(data_3794, mask_3794);
        case 2905: CACHED_MP_DATA(data_2905, mask_2905);
        case 3063: CACHED_MP_DATA(data_3063, mask_3063);
        case 2711: CACHED_MP_DATA(data_2711, mask_2711);
        case 4223: CACHED_MP_DATA(data_4223, mask_4223);
        case 3128: CACHED_MP_DATA(data_3128, mask_3128);
        case 3484: CACHED_MP_DATA(data_3484, mask_3484);
        case 1268: CACHED_MP_DATA(data_1268, mask_1268);
        case 2494: CACHED_MP_DATA(data_2494, mask_2494);
        case 3728: CACHED_MP_DATA(data_3728, mask_3728);
        case 4838: CACHED_MP_DATA(data_4838, mask_4838);
        case 5185: CACHED_MP_DATA(data_5185, mask_5185);
        case 4976: CACHED_MP_DATA(data_4976, mask_4976);
        case 4877: CACHED_MP_DATA(data_4877, mask_4877);
        case 5008: CACHED_MP_DATA(data_5008, mask_5008);
        case 5180: CACHED_MP_DATA(data_5180, mask_5180);
        case 5068: CACHED_MP_DATA(data_5068, mask_5068);
        case 4866: CACHED_MP_DATA(data_4866, mask_4866);
        case 3501: CACHED_MP_DATA(data_3501, mask_3501);
        case 1848: CACHED_MP_DATA(data_1848, mask_1848);
        case 2760: CACHED_MP_DATA(data_2760, mask_2760);
        case 2949: CACHED_MP_DATA(data_2949, mask_2949);
        case 1662: CACHED_MP_DATA(data_1662, mask_1662);
        case 1721: CACHED_MP_DATA(data_1721, mask_1721);
        case 3007: CACHED_MP_DATA(data_3007, mask_3007);
        case 4045: CACHED_MP_DATA(data_4045, mask_4045);
        case 4153: CACHED_MP_DATA(data_4153, mask_4153);
        case 4483: CACHED_MP_DATA(data_4483, mask_4483);
        case 5144: CACHED_MP_DATA(data_5144, mask_5144);
        case 5150: CACHED_MP_DATA(data_5150, mask_5150);
        case 5583: CACHED_MP_DATA(data_5583, mask_5583);
        default:
            *data = NULL;
            *mask = NULL;
    }
}

/******************************************************************************/
void
rdpSpriteSetCursor(DeviceIntPtr pDev, ScreenPtr pScr, CursorPtr pCurs,
                   int x, int y)
{
    char cur_data[32 * (32 * 3)];
    char cur_mask[32 * (32 / 8)];
    char *pmask = NULL;
    char *pdata = NULL;
    char *mask;
    char *data;
    int i;
    int j;
    int w;
    int h;
    int xhot;
    int yhot;
    int paddedRowBytes;
    int fgcolor;
    int bgcolor;

    if (pCurs == 0 || pCurs->bits == 0)
    {
        return;
    }

    w = pCurs->bits->width;
    h = pCurs->bits->height;
    xhot = pCurs->bits->xhot;
    yhot = pCurs->bits->yhot;
    paddedRowBytes = PixmapBytePad(w, 1);
    data = (char *) pCurs->bits->source;
    mask = (char *) pCurs->bits->mask;

    fgcolor = ((pCurs->foreRed & 0xff00) << 8) |
              ((pCurs->foreGreen & 0xff00)) |
              ((pCurs->foreBlue >> 8) & 0xff);
    bgcolor = ((pCurs->backRed & 0xff00) << 8) |
              ((pCurs->backGreen & 0xff00)) |
              ((pCurs->backBlue >> 8) & 0xff);

    unsigned int id = 0;
    for (i = 0; i < w * paddedRowBytes; i++)
    {
        id += (unsigned char) data[i];
    }

    if (g_rdpScreen.client_info.fix_mouse_pointer_sprites)
    {
        if (!g_initialized_mouse_pointer_cache)
        {
            rdpInitializeMousePointerCache(fgcolor, bgcolor);
            g_initialized_mouse_pointer_cache = 1;
        }
        getCachedMousePointerSprite(id, &pdata, &pmask);
    }

    if (pdata == NULL || pmask == NULL)
    {
        pdata = cur_data;
        pmask = cur_mask;

        memset(cur_data, 0, sizeof(cur_data));
        memset(cur_mask, 0, sizeof(cur_mask));

        for (j = 0; j < 32; j++)
        {
            for (i = 0; i < 32; i++)
            {
                if (i < w && j < h && rdpGetBit((unsigned char *) mask, i))
                {
                    int bitSet = rdpGetBit((unsigned char *) data, i);
                    int color = bitSet ? fgcolor : bgcolor;

                    char *dst = cur_data + (3 * (((31 - j) << 5) + i));
                    dst[0] = color >> 16;
                    dst[1] = color >> 8;
                    dst[2] = color;
                }
                else
                {
                    rdpSetBit((unsigned char *) cur_mask, ((31 - j) << 5) + i);
                }
            }
            mask += paddedRowBytes;
            data += paddedRowBytes;
        }
    }

    rdpup_begin_update();
    rdpup_set_cursor(xhot, yhot, pdata, pmask);
    rdpup_end_update();
}

/******************************************************************************/
void
rdpSpriteMoveCursor(DeviceIntPtr pDev, ScreenPtr pScr, int x, int y)
{
    DEBUG_OUT_INPUT(("hi rdpSpriteMoveCursor\n"));
}

/******************************************************************************/
Bool
rdpSpriteDeviceCursorInitialize(DeviceIntPtr pDev, ScreenPtr pScr)
{
    ErrorF("rdpSpriteDeviceCursorInitialize:\n");
    return 1;
}

/******************************************************************************/
void
rdpSpriteDeviceCursorCleanup(DeviceIntPtr pDev, ScreenPtr pScr)
{
    ErrorF("rdpSpriteDeviceCursorCleanup:\n");
}

/******************************************************************************/
static void
rdpEnqueueMotion(int x, int y)
{
    int i;
    int n;
    int valuators[2];
    EventListPtr rdp_events;
    xEvent *pev;

# if 0

    if (x < 128)
    {
        rdpup_begin_update();
        rdpup_send_area(0, 0, 1024, 768);
        rdpup_end_update();
    }

#endif
    miPointerSetPosition(g_pointer, &x, &y);
    valuators[0] = x;
    valuators[1] = y;

    GetEventList(&rdp_events);
    n = GetPointerEvents(rdp_events, g_pointer, MotionNotify, 0,
                         POINTER_ABSOLUTE | POINTER_SCREEN,
                         0, 2, valuators);

    for (i = 0; i < n; i++)
    {
        pev = (rdp_events + i)->event;
        mieqEnqueue(g_pointer, (InternalEvent *)pev);
    }
}

/******************************************************************************/
static void
rdpEnqueueButton(int type, int buttons)
{
    int i;
    int n;
    EventListPtr rdp_events;
    xEvent *pev;

    i = GetEventList(&rdp_events);
    n = GetPointerEvents(rdp_events, g_pointer, type, buttons, 0, 0, 0, 0);

    for (i = 0; i < n; i++)
    {
        pev = (rdp_events + i)->event;
        mieqEnqueue(g_pointer, (InternalEvent *)pev);
    }
}

/******************************************************************************/
static void
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
PtrAddEvent(int buttonMask, int x, int y)
{
    int i;
    int type;
    int buttons;

    rdpEnqueueMotion(x, y);

    for (i = 0; i < 5; i++)
    {
        if ((buttonMask ^ g_old_button_mask) & (1 << i))
        {
            if (buttonMask & (1 << i))
            {
                type = ButtonPress;
                buttons = i + 1;
                rdpEnqueueButton(type, buttons);
            }
            else
            {
                type = ButtonRelease;
                buttons = i + 1;
                rdpEnqueueButton(type, buttons);
            }
        }
    }

    g_old_button_mask = buttonMask;
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

/**
 * @param down   - true for KeyDown events, false otherwise
 * @param param1 - ASCII code of pressed key
 * @param param2 -
 * @param param3 - scancode of pressed key
 * @param param4 -
 ******************************************************************************/
void
KbdAddEvent(int down, int param1, int param2, int param3, int param4)
{
    int rdp_scancode;
    int x_scancode;
    int is_ext;
    int is_spe;
    int type;

#if 0
    fprintf(stderr, "down=0x%x param1=0x%x param2=0x%x param3=0x%x "
            "param4=0x%x\n", down, param1, param2, param3, param4);
#endif

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
                rdpEnqueueKey(type, x_scancode);
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

            rdpEnqueueKey(type, x_scancode);
            break;

        case 15: /* tab */

            if (!down && !g_tab_down)
            {
                check_keysa(); /* leave x_scancode 0 here, we don't want the tab key up */
            }
            else
            {
                sendDownUpKeyEvent(type, 23);
            }

            g_tab_down = down;
            break;

        case 29: /* left or right ctrl */

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
                x_scancode = is_ext ? 109 : 37;
                g_ctrl_down = down ? x_scancode : 0;
                rdpEnqueueKey(type, x_scancode);
            }

            break;

        case 69: /* Pause or Num Lock */

            if (g_pause_spe)
            {
                x_scancode = 110;

                if (!down)
                {
                    g_pause_spe = 0;
                }
            }
            else
            {
                x_scancode = g_ctrl_down ? 110 : 77;
            }

            sendDownUpKeyEvent(type, x_scancode);
            break;

        case 28: /* Enter or Return */
            x_scancode = is_ext ? 108 : 36;
            sendDownUpKeyEvent(type, x_scancode);
            break;

        case 53: /* / */
            x_scancode = is_ext ? 112 : 61;
            sendDownUpKeyEvent(type, x_scancode);
            break;

        case 55: /* * on KP or Print Screen */
            x_scancode = is_ext ? 111 : 63;
            sendDownUpKeyEvent(type, x_scancode);
            break;

        case 71: /* 7 or Home */
            x_scancode = is_ext ? 97 : 79;
            sendDownUpKeyEvent(type, x_scancode);
            break;

        case 72: /* 8 or Up */
            x_scancode = is_ext ? 98 : 80;
            sendDownUpKeyEvent(type, x_scancode);
            break;

        case 73: /* 9 or PgUp */
            x_scancode = is_ext ? 99 : 81;
            sendDownUpKeyEvent(type, x_scancode);
            break;

        case 75: /* 4 or Left */
            x_scancode = is_ext ? 100 : 83;
            sendDownUpKeyEvent(type, x_scancode);
            break;

        case 77: /* 6 or Right */
            x_scancode = is_ext ? 102 : 85;
            sendDownUpKeyEvent(type, x_scancode);
            break;

        case 79: /* 1 or End */
            x_scancode = is_ext ? 103 : 87;
            sendDownUpKeyEvent(type, x_scancode);
            break;

        case 80: /* 2 or Down */
            x_scancode = is_ext ? 104 : 88;
            sendDownUpKeyEvent(type, x_scancode);
            break;

        case 81: /* 3 or PgDn */
            x_scancode = is_ext ? 105 : 89;
            sendDownUpKeyEvent(type, x_scancode);
            break;

        case 82: /* 0 or Insert */
            x_scancode = is_ext ? 106 : 90;
            sendDownUpKeyEvent(type, x_scancode);
            break;

        case 83: /* . or Delete */
            x_scancode = is_ext ? 107 : 91;
            sendDownUpKeyEvent(type, x_scancode);
            break;

        case 91: /* left win key */
            rdpEnqueueKey(type, 115);
            break;

        case 92: /* right win key */
            rdpEnqueueKey(type, 116);
            break;

        case 93: /* menu key */
            rdpEnqueueKey(type, 117);
            break;

        default:
            x_scancode = rdp_scancode + MIN_KEY_CODE;

            if (x_scancode > 0)
            {
                sendDownUpKeyEvent(type, x_scancode);
            }

            break;
    }
}

/******************************************************************************/
/* notes -
     scroll lock doesn't seem to be a modifier in X
*/
void
KbdSync(int param1)
{
    int xkb_state;

    xkb_state = XkbStateFieldFromRec(&(g_keyboard->key->xkbInfo->state));

    if ((!(xkb_state & 0x02)) != (!(param1 & 4))) /* caps lock */
    {
        ErrorF("KbdSync: toggling caps lock\n");
        KbdAddEvent(1, 58, 0, 58, 0);
        KbdAddEvent(0, 58, 49152, 58, 49152);
    }

    if ((!(xkb_state & 0x10)) != (!(param1 & 2))) /* num lock */
    {
        ErrorF("KbdSync: toggling num lock\n");
        KbdAddEvent(1, 69, 0, 69, 0);
        KbdAddEvent(0, 69, 49152, 69, 49152);
    }

    if ((!(g_scroll_lock_down)) != (!(param1 & 1))) /* scroll lock */
    {
        ErrorF("KbdSync: toggling scroll lock\n");
        KbdAddEvent(1, 70, 0, 70, 0);
        KbdAddEvent(0, 70, 49152, 70, 49152);
    }
}
