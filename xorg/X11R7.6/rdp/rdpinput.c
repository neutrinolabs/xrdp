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
   this should make sure control alt and shift are all up
   rdesktop does not do this */
/* this should be fixed in rdesktop */

#include "rdp.h"
#include <sys/types.h>
#include <sys/wait.h>

#include "rdpkeyboard.h"
#include "rdpkeyboardbase.h"
#include "rdpkeyboardevdev.h"

#define LOG_LEVEL 1
#define LLOG(_level, _args) \
    do { if (_level < LOG_LEVEL) { ErrorF _args ; } } while (0)
#define LLOGLN(_level, _args) \
    do { if (_level < LOG_LEVEL) { ErrorF _args ; ErrorF("\n"); } } while (0)

extern ScreenPtr g_pScreen; /* in rdpmain.c */
extern DeviceIntPtr g_pointer; /* in rdpmain.c */
extern DeviceIntPtr g_keyboard; /* in rdpmain.c */
extern rdpScreenInfoRec g_rdpScreen; /* from rdpmain.c */
extern int g_shift_down; /* in rdpmain.c */
extern int g_alt_down; /* in rdpmain.c */
extern int g_ctrl_down; /* in rdpmain.c */

static int g_old_button_mask = 0;
/* this is toggled every time num lock key is released, not like the
   above *_down vars */
static int g_scroll_lock_down = 0;
static OsTimerPtr g_kbtimer = 0;
static OsTimerPtr g_timer = 0;
static int g_x = 0;
static int g_y = 0;
static int g_timer_schedualed = 0;
static int g_delay_motion = 1; /* turn on or off */
static int g_use_evdev = 1;

/* Copied from Xvnc/lib/font/util/utilbitmap.c */
static unsigned char g_reverse_byte[0x100] =
{
    0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
    0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
    0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
    0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
    0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
    0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
    0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
    0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
    0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
    0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
    0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
    0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
    0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
    0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
    0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
    0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
    0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
    0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
    0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
    0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
    0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
    0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
    0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
    0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
    0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
    0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
    0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
    0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
    0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
    0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
    0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
    0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
};

#if 0
/******************************************************************************/
static void
rdpSendBell(void)
{
    LLOGLN(10, ("rdpSendBell:"));
}
#endif

/******************************************************************************/
void
KbdDeviceOn(void)
{
    LLOGLN(10, ("KbdDeviceOn:"));
}

/******************************************************************************/
void
KbdDeviceOff(void)
{
    LLOGLN(10, ("KbdDeviceOff:"));
}

/******************************************************************************/
void
rdpBell(int volume, DeviceIntPtr pDev, pointer ctrl, int cls)
{
    LLOGLN(0, ("rdpBell:"));
}

/******************************************************************************/
static CARD32
rdpInDeferredUpdateCallback(OsTimerPtr timer, CARD32 now, pointer arg)
{
    LLOGLN(10, ("rdpInDeferredUpdateCallback:"));

    /* our keyboard device */
    XkbSetRepeatKeys(g_keyboard, -1, AutoRepeatModeOff);
    /* the main one for the server */
    XkbSetRepeatKeys(inputInfo.keyboard, -1, AutoRepeatModeOff);

    return 0;
}

/******************************************************************************/
void
rdpChangeKeyboardControl(DeviceIntPtr pDev, KeybdCtrl *ctrl)
{
    XkbControlsPtr ctrls;

    LLOGLN(0, ("rdpChangeKeyboardControl:"));
    ctrls = 0;
    if (pDev != 0)
    {
        if (pDev->key != 0)
        {
            if (pDev->key->xkbInfo != 0)
            {
                if (pDev->key->xkbInfo->desc != 0)
                {
                    if (pDev->key->xkbInfo->desc->ctrls != 0)
                    {
                        ctrls = pDev->key->xkbInfo->desc->ctrls;
                    }
                }
            }
        }
    }
    if (ctrls != 0)
    {
        if (ctrls->enabled_ctrls & XkbRepeatKeysMask)
        {
            LLOGLN(10, ("rdpChangeKeyboardControl: autoRepeat on"));
            /* schedual to turn off the autorepeat after 100 ms so any app
             * polling it will be happy it's on */
            g_kbtimer = TimerSet(g_kbtimer, 0, 100,
                                 rdpInDeferredUpdateCallback, 0);
        }
        else
        {
            LLOGLN(10, ("rdpChangeKeyboardControl: autoRepeat off"));
        }
    }
}

/******************************************************************************/
/*
0x00000401  Arabic (101)
0x00000402  Bulgarian
0x00000404  Chinese (Traditional) - US Keyboard
0x00000405  Czech
0x00000406  Danish
0x00000407  German
0x00000408  Greek
0x00000409  US
0x0000040A  Spanish
0x0000040B  Finnish
0x0000040C  French
0x0000040D  Hebrew
0x0000040E  Hungarian
0x0000040F  Icelandic
0x00000410  Italian
0x00000411  Japanese
0x00000412  Korean
0x00000413  Dutch
0x00000414  Norwegian
0x00000415  Polish (Programmers)
0x00000416  Portuguese (Brazilian ABNT)
0x00000418  Romanian
0x00000419  Russian
0x0000041A  Croatian
0x0000041B  Slovak
0x0000041C  Albanian
0x0000041D  Swedish
0x0000041E  Thai Kedmanee
0x0000041F  Turkish Q
0x00000420  Urdu
0x00000422  Ukrainian
0x00000423  Belarusian
0x00000424  Slovenian
0x00000425  Estonian
0x00000426  Latvian
0x00000427  Lithuanian IBM
0x00000429  Farsi
0x0000042A  Vietnamese
0x0000042B  Armenian Eastern
0x0000042C  Azeri Latin
0x0000042F  FYRO Macedonian
0x00000437  Georgian
0x00000438  Faeroese
0x00000439  Devanagari - INSCRIPT
0x0000043A  Maltese 47-key
0x0000043B  Norwegian with Sami
0x0000043F  Kazakh
0x00000440  Kyrgyz Cyrillic
0x00000444  Tatar
0x00000445  Bengali
0x00000446  Punjabi
0x00000447  Gujarati
0x00000449  Tamil
0x0000044A  Telugu
0x0000044B  Kannada
0x0000044C  Malayalam
0x0000044E  Marathi
0x00000450  Mongolian Cyrillic
0x00000452  United Kingdom Extended
0x0000045A  Syriac
0x00000461  Nepali
0x00000463  Pashto
0x00000465  Divehi Phonetic
0x0000046E  Luxembourgish
0x00000481  Maori
0x00000804  Chinese (Simplified) - US Keyboard
0x00000807  Swiss German
0x00000809  United Kingdom
0x0000080A  Latin American
0x0000080C  Belgian French
0x00000813  Belgian (Period)
0x00000816  Portuguese
0x0000081A  Serbian (Latin)
0x0000082C  Azeri Cyrillic
0x0000083B  Swedish with Sami
0x00000843  Uzbek Cyrillic
0x0000085D  Inuktitut Latin
0x00000C0C  Canadian French (legacy)
0x00000C1A  Serbian (Cyrillic)
0x00001009  Canadian French
0x0000100C  Swiss French
0x0000141A  Bosnian
0x00001809  Irish
0x0000201A  Bosnian Cyrillic
*/

/******************************************************************************/
int
rdpLoadLayout(struct xrdp_client_info *client_info)
{
    XkbRMLVOSet set;
    XkbSrvInfoPtr xkbi;
    XkbDescPtr xkb;
    KeySymsPtr keySyms;
    DeviceIntPtr pDev;
    KeyCode first_key;
    CARD8 num_keys;

    int keylayout = client_info->keylayout;

    LLOGLN(0, ("rdpLoadLayout: keylayout 0x%8.8x variant %s display %s",
               keylayout, client_info->variant, display));
    memset(&set, 0, sizeof(set));
    if (g_use_evdev)
    {
        set.rules = "evdev";
    }
    else
    {
        set.rules = "base";
    }

    set.model = "pc105";
    set.layout = "us";
    set.variant = "";
    set.options = "terminate:ctrl_alt_bksp,grp:shift_caps_toggle";

    if (strlen(client_info->model) > 0)
    {
        set.model = client_info->model;
    }
    if (strlen(client_info->variant) > 0)
    {
        set.variant = client_info->variant;
    }
    if (strlen(client_info->layout) > 0)
    {
        set.layout = client_info->layout;
    }

 retry:
    /* free some stuff so we can call InitKeyboardDeviceStruct again */
    xkbi = g_keyboard->key->xkbInfo;
    xkb = xkbi->desc;
    XkbFreeKeyboard(xkb, 0, TRUE);
    free(xkbi);
    g_keyboard->key->xkbInfo = NULL;
    free(g_keyboard->kbdfeed);
    g_keyboard->kbdfeed = NULL;
    free(g_keyboard->key);
    g_keyboard->key = NULL;

    /* init keyboard and reload the map */
    if (!InitKeyboardDeviceStruct(g_keyboard, &set, rdpBell,
                                  rdpChangeKeyboardControl))
    {
        LLOGLN(0, ("rdpLoadLayout: InitKeyboardDeviceStruct failed"));
        return 1;
    }

    /* notify the X11 clients eg. X_ChangeKeyboardMapping */
    keySyms = XkbGetCoreMap(g_keyboard);
    if (keySyms)
    {
        first_key = keySyms->minKeyCode;
        num_keys = (keySyms->maxKeyCode - keySyms->minKeyCode) + 1;
        XkbApplyMappingChange(g_keyboard, keySyms, first_key, num_keys,
                              NULL, serverClient);
        for (pDev = inputInfo.devices; pDev; pDev = pDev->next)
        {
            if ((pDev->coreEvents || pDev == inputInfo.keyboard) && pDev->key)
            {
                XkbApplyMappingChange(pDev, keySyms, first_key, num_keys,
                                      NULL, serverClient);
            }
        }
    }
    else
    {
        /* sometimes, variant doesn't support all layouts */
        set.variant = "";
        goto retry;
    }

    return 0;
}

/******************************************************************************/
int
rdpKeybdProc(DeviceIntPtr pDevice, int onoff)
{
    DevicePtr pDev;
    XkbRMLVOSet set;
    int ok;

    LLOGLN(10, ("rdpKeybdProc:"));
    pDev = (DevicePtr)pDevice;

    switch (onoff)
    {
        case DEVICE_INIT:
            LLOGLN(10, ("rdpKeybdProc: DEVICE_INIT"));
            memset(&set, 0, sizeof(set));
            if (g_use_evdev)
            {
                set.rules = "evdev";
            }
            else
            {
                set.rules = "base";
            }
            set.model = "pc105";
            set.layout = "us";
            set.variant = "";
            set.options = "terminate:ctrl_alt_bksp,grp:shift_caps_toggle";
            ok = InitKeyboardDeviceStruct(pDevice, &set, rdpBell,
                                          rdpChangeKeyboardControl);
            LLOGLN(10, ("rdpKeybdProc: InitKeyboardDeviceStruct %d", ok));
            break;
        case DEVICE_ON:
            LLOGLN(10, ("rdpKeybdProc: DEVICE_ON"));
            pDev->on = 1;
            KbdDeviceOn();
            break;
        case DEVICE_OFF:
            LLOGLN(10, ("rdpKeybdProc: DEVICE_OFF"));
            pDev->on = 0;
            KbdDeviceOff();
            break;
        case DEVICE_CLOSE:
            LLOGLN(10, ("rdpKeybdProc: DEVICE_CLOSE"));
            if (pDev->on)
            {
                pDev->on = 0;
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
    LLOGLN(10, ("PtrDeviceControl:"));
}

/******************************************************************************/
void
PtrDeviceInit(void)
{
    LLOGLN(10, ("PtrDeviceInit:"));
}

/******************************************************************************/
void
PtrDeviceOn(DeviceIntPtr pDev)
{
    LLOGLN(10, ("PtrDeviceOn:"));
}

/******************************************************************************/
void
PtrDeviceOff(void)
{
    LLOGLN(10, ("PtrDeviceOff:"));
}

/******************************************************************************/
static void
rdpMouseCtrl(DeviceIntPtr pDevice, PtrCtrl *pCtrl)
{
    LLOGLN(0, ("rdpMouseCtrl:"));
}

/******************************************************************************/
int
rdpMouseProc(DeviceIntPtr pDevice, int onoff)
{
    BYTE map[8];
    DevicePtr pDev;
    Atom btn_labels[8];
    Atom axes_labels[2];

    LLOGLN(10, ("rdpMouseProc:"));
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
            map[6] = 6;
            map[7] = 7;

            btn_labels[0] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_LEFT);
            btn_labels[1] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_MIDDLE);
            btn_labels[2] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_RIGHT);
            btn_labels[3] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_WHEEL_UP);
            btn_labels[4] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_WHEEL_DOWN);
            btn_labels[5] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_HWHEEL_LEFT);
            btn_labels[6] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_HWHEEL_RIGHT);

            axes_labels[0] = XIGetKnownProperty(AXIS_LABEL_PROP_REL_X);
            axes_labels[1] = XIGetKnownProperty(AXIS_LABEL_PROP_REL_Y);

            InitPointerDeviceStruct(pDev, map, 7, btn_labels, rdpMouseCtrl,
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
    LLOGLN(10, ("rdpCursorOffScreen:"));
    return 0;
}

/******************************************************************************/
void
rdpCrossScreen(ScreenPtr pScreen, Bool entering)
{
    LLOGLN(10, ("rdpCrossScreen:"));
}

/******************************************************************************/
void
rdpPointerWarpCursor(DeviceIntPtr pDev, ScreenPtr pScr, int x, int y)
{
    LLOGLN(0, ("rdpPointerWarpCursor:"));
    miPointerWarpCursor(pDev, pScr, x, y);
}

/******************************************************************************/
void
rdpPointerEnqueueEvent(DeviceIntPtr pDev, InternalEvent *event)
{
    LLOGLN(0, ("rdpPointerEnqueueEvent:"));
}

/******************************************************************************/
void
rdpPointerNewEventScreen(DeviceIntPtr pDev, ScreenPtr pScr, Bool fromDIX)
{
    LLOGLN(0, ("rdpPointerNewEventScreen:"));
}

/******************************************************************************/
Bool
rdpSpriteRealizeCursor(DeviceIntPtr pDev, ScreenPtr pScr, CursorPtr pCurs)
{
    LLOGLN(10, ("rdpSpriteRealizeCursor:"));
    return 1;
}

/******************************************************************************/
Bool
rdpSpriteUnrealizeCursor(DeviceIntPtr pDev, ScreenPtr pScr, CursorPtr pCurs)
{
    LLOGLN(10, ("rdpSpriteUnrealizeCursor:"));
    return 1;
}

/******************************************************************************/
int
get_pixel_safe(char *data, int x, int y, int width, int height, int bpp)
{
    int start;
    int shift;
    int c;
    unsigned int *src32;

    if (x < 0)
    {
        return 0;
    }

    if (y < 0)
    {
        return 0;
    }

    if (x >= width)
    {
        return 0;
    }

    if (y >= height)
    {
        return 0;
    }

    if (bpp == 1)
    {
        width = (width + 7) / 8;
        start = (y * width) + x / 8;
        shift = x % 8;
        c = (unsigned char)(data[start]);
#if (X_BYTE_ORDER == X_LITTLE_ENDIAN)
        return (g_reverse_byte[c] & (0x80 >> shift)) != 0;
#else
        return (c & (0x80 >> shift)) != 0;
#endif
    }
    else if (bpp == 32)
    {
        src32 = (unsigned int*)data;
        return src32[y * width + x];
    }

    return 0;
}

/******************************************************************************/
void
set_pixel_safe(char *data, int x, int y, int width, int height, int bpp,
               int pixel)
{
    int start;
    int shift;
    unsigned int *dst32;

    if (x < 0)
    {
        return;
    }

    if (y < 0)
    {
        return;
    }

    if (x >= width)
    {
        return;
    }

    if (y >= height)
    {
        return;
    }

    if (bpp == 1)
    {
        width = (width + 7) / 8;
        start = (y * width) + x / 8;
        shift = x % 8;

        if (pixel & 1)
        {
            data[start] = data[start] | (0x80 >> shift);
        }
        else
        {
            data[start] = data[start] & ~(0x80 >> shift);
        }
    }
    else if (bpp == 24)
    {
        *(data + (3 * (y * width + x)) + 0) = pixel >> 0;
        *(data + (3 * (y * width + x)) + 1) = pixel >> 8;
        *(data + (3 * (y * width + x)) + 2) = pixel >> 16;
    }
    else if (bpp == 32)
    {
        dst32 = (unsigned int*)data;
        dst32[y * width + x] = pixel;
    }
}

/******************************************************************************/
void
rdpSpriteSetCursor(DeviceIntPtr pDev, ScreenPtr pScr, CursorPtr pCurs,
                   int x, int y)
{
    char cur_data[32 * (32 * 4)];
    char cur_mask[32 * (32 / 8)];
    char *mask;
    char *data;
    int i;
    int j;
    int w;
    int h;
    int p;
    int xhot;
    int yhot;
    int paddedRowBytes;
    int fgcolor;
    int bgcolor;
    int bpp;

    if (pCurs == 0)
    {
        return;
    }

    if (pCurs->bits == 0)
    {
        return;
    }

    w = pCurs->bits->width;
    h = pCurs->bits->height;
    if ((pCurs->bits->argb != 0) &&
        (g_rdpScreen.client_info.pointer_flags & 1))
    {
        bpp = 32;
        paddedRowBytes = PixmapBytePad(w, 32);
        xhot = pCurs->bits->xhot;
        yhot = pCurs->bits->yhot;
        data = (char *)(pCurs->bits->argb);
        memset(cur_data, 0, sizeof(cur_data));
        memset(cur_mask, 0, sizeof(cur_mask));

        for (j = 0; j < 32; j++)
        {
            for (i = 0; i < 32; i++)
            {
                p = get_pixel_safe(data, i, j, paddedRowBytes / 4, h, 32);
                set_pixel_safe(cur_data, i, 31 - j, 32, 32, 32, p);
            }
        }
    }
    else
    {
        bpp = 0;
        paddedRowBytes = PixmapBytePad(w, 1);
        xhot = pCurs->bits->xhot;
        yhot = pCurs->bits->yhot;
        data = (char *)(pCurs->bits->source);
        mask = (char *)(pCurs->bits->mask);
        fgcolor = (((pCurs->foreRed >> 8) & 0xff) << 16) |
                  (((pCurs->foreGreen >> 8) & 0xff) << 8) |
                  ((pCurs->foreBlue >> 8) & 0xff);
        bgcolor = (((pCurs->backRed >> 8) & 0xff) << 16) |
                  (((pCurs->backGreen >> 8) & 0xff) << 8) |
                  ((pCurs->backBlue >> 8) & 0xff);
        memset(cur_data, 0, sizeof(cur_data));
        memset(cur_mask, 0, sizeof(cur_mask));

        for (j = 0; j < 32; j++)
        {
            for (i = 0; i < 32; i++)
            {
                p = get_pixel_safe(mask, i, j, paddedRowBytes * 8, h, 1);
                set_pixel_safe(cur_mask, i, 31 - j, 32, 32, 1, !p);

                if (p != 0)
                {
                    p = get_pixel_safe(data, i, j, paddedRowBytes * 8, h, 1);
                    p = p ? fgcolor : bgcolor;
                    set_pixel_safe(cur_data, i, 31 - j, 32, 32, 24, p);
                }
            }
        }
    }

    rdpup_begin_update();
    rdpup_set_cursor_ex(xhot, yhot, cur_data, cur_mask, bpp);
    rdpup_end_update();
}

/******************************************************************************/
void
rdpSpriteMoveCursor(DeviceIntPtr pDev, ScreenPtr pScr, int x, int y)
{
    LLOGLN(10, ("rdpSpriteMoveCursor:"));
}

/******************************************************************************/
Bool
rdpSpriteDeviceCursorInitialize(DeviceIntPtr pDev, ScreenPtr pScr)
{
    LLOGLN(0, ("rdpSpriteDeviceCursorInitialize:"));
    return 1;
}

/******************************************************************************/
void
rdpSpriteDeviceCursorCleanup(DeviceIntPtr pDev, ScreenPtr pScr)
{
    LLOGLN(0, ("rdpSpriteDeviceCursorCleanup:"));
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

    LLOGLN(10, ("rdpEnqueueMotion: x %d y %d", x, y));
# if 0

    if (x < 128)
    {
        rdpup_begin_update();
        rdpup_send_area(0, 0, 0, 1024, 768);
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

    LLOGLN(10, ("rdpEnqueueButton:"));
    i = GetEventList(&rdp_events);
    n = GetPointerEvents(rdp_events, g_pointer, type, buttons, 0, 0, 0, 0);

    for (i = 0; i < n; i++)
    {
        pev = (rdp_events + i)->event;
        mieqEnqueue(g_pointer, (InternalEvent *)pev);
    }
}

/******************************************************************************/
static CARD32
rdpDeferredInputCallback(OsTimerPtr timer, CARD32 now, pointer arg)
{
    LLOGLN(10, ("rdpDeferredInputCallback:"));
    g_timer_schedualed = 0;
    rdpEnqueueMotion(g_x, g_y);
    return 0;
}

/******************************************************************************/
void
PtrAddEvent(int buttonMask, int x, int y)
{
    int i;
    int type;
    int buttons;
    int send_now;

    LLOGLN(10, ("PtrAddEvent: x %d y %d", x, y));
    if (g_pointer == 0)
    {
        return;
    }
    send_now = (buttonMask ^ g_old_button_mask) || (g_delay_motion == 0);
    LLOGLN(10, ("PtrAddEvent: send_now %d g_timer_schedualed %d",
           send_now, g_timer_schedualed));
    if (send_now)
    {
        if (g_timer_schedualed)
        {
            g_timer_schedualed = 0;
            TimerCancel(g_timer);
        }
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
    else
    {
        g_x = x;
        g_y = y;
        if (!g_timer_schedualed)
        {
            g_timer_schedualed = 1;
            g_timer = TimerSet(g_timer, 0, 60, rdpDeferredInputCallback, 0);
        }
    }
}

/******************************************************************************/
void
KbdAddEvent(int down, int param1, int param2, int param3, int param4)
{
    if (g_use_evdev)
    {
        KbdAddEvent_evdev(down, param1, param2, param3, param4);
    }
    else
    {
        KbdAddEvent_base(down, param1, param2, param3, param4);
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

    if (g_keyboard == 0)
    {
        return;
    }
    xkb_state = XkbStateFieldFromRec(&(g_keyboard->key->xkbInfo->state));

    if ((!(xkb_state & 0x02)) != (!(param1 & 4))) /* caps lock */
    {
        LLOGLN(0, ("KbdSync: toggling caps lock"));
        KbdAddEvent(1, 58, 0, 58, 0);
        KbdAddEvent(0, 58, 49152, 58, 49152);
    }

    if ((!(xkb_state & 0x10)) != (!(param1 & 2))) /* num lock */
    {
        LLOGLN(0, ("KbdSync: toggling num lock"));
        KbdAddEvent(1, 69, 0, 69, 0);
        KbdAddEvent(0, 69, 49152, 69, 49152);
    }

    if ((!(g_scroll_lock_down)) != (!(param1 & 1))) /* scroll lock */
    {
        LLOGLN(0, ("KbdSync: toggling scroll lock"));
        KbdAddEvent(1, 70, 0, 70, 0);
        KbdAddEvent(0, 70, 49152, 70, 49152);
    }
}
