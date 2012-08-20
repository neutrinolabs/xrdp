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

#if 1
#define DEBUG_OUT_INPUT(arg)
#else
#define DEBUG_OUT_INPUT(arg) ErrorF arg
#endif

extern ScreenPtr g_pScreen; /* in rdpmain.c */
extern DeviceIntPtr g_pointer; /* in rdpmain.c */
extern DeviceIntPtr g_keyboard; /* in rdpmain.c */

static int g_old_button_mask = 0;
static int g_pause_spe = 0;
static int g_ctrl_down = 0;
static int g_alt_down = 0;
static int g_shift_down = 0;
static int g_tab_down = 0;
/* this is toggled every time num lock key is released, not like the
   above *_down vars */
static int g_scroll_lock_down = 0;

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
  XK_grave,	   XK_asciitilde,
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
KbdDeviceInit(DeviceIntPtr pDevice, KeySymsPtr pKeySyms, CARD8* pModMap)
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
  pKeySyms->map = (KeySym*)g_malloc(i, 1);
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
rdpChangeKeyboardControl(DeviceIntPtr pDev, KeybdCtrl* ctrl)
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
PtrDeviceControl(DeviceIntPtr dev, PtrCtrl* ctrl)
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
rdpMouseCtrl(DeviceIntPtr pDevice, PtrCtrl* pCtrl)
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
rdpCursorOffScreen(ScreenPtr* ppScreen, int* x, int* y)
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
rdpPointerEnqueueEvent(DeviceIntPtr pDev, InternalEvent* event)
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
get_pixel_safe(char* data, int x, int y, int width, int height, int bpp)
{
  int start;
  int shift;
  int c;

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
  return 0;
}

/******************************************************************************/
void
set_pixel_safe(char* data, int x, int y, int width, int height, int bpp,
               int pixel)
{
  int start;
  int shift;

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
}

/******************************************************************************/
void
rdpSpriteSetCursor(DeviceIntPtr pDev, ScreenPtr pScr, CursorPtr pCurs,
                   int x, int y)
{
  char cur_data[32 * (32 * 3)];
  char cur_mask[32 * (32 / 8)];
  char* mask;
  char* data;
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
  paddedRowBytes = PixmapBytePad(w, 1);
  xhot = pCurs->bits->xhot;
  yhot = pCurs->bits->yhot;
  /* ErrorF("xhot %d yhot %d\n", xhot, yhot); */
  data = (char*)(pCurs->bits->source);
  mask = (char*)(pCurs->bits->mask);
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
  rdpup_begin_update();
  rdpup_set_cursor(xhot, yhot, cur_data, cur_mask);
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
  xEvent* pev;

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
    mieqEnqueue(g_pointer, (InternalEvent*)pev);
  }
}

/******************************************************************************/
static void
rdpEnqueueButton(int type, int buttons)
{
  int i;
  int n;
  EventListPtr rdp_events;
  xEvent* pev;

  i = GetEventList(&rdp_events);
  n = GetPointerEvents(rdp_events, g_pointer, type, buttons, 0, 0, 0, 0);
  for (i = 0; i < n; i++)
  {
    pev = (rdp_events + i)->event;
    mieqEnqueue(g_pointer, (InternalEvent*)pev);
  }
}

/******************************************************************************/
static void
rdpEnqueueKey(int type, int scancode)
{
  int i;
  int n;
  EventListPtr rdp_events;
  xEvent* pev;

  i = GetEventList(&rdp_events);
  n = GetKeyboardEvents(rdp_events, g_keyboard, type, scancode);
  for (i = 0; i < n; i++)
  {
    pev = (rdp_events + i)->event;
    mieqEnqueue(g_keyboard, (InternalEvent*)pev);
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
  /* if type is keydown, send keydown + keyup */
  /* this allows us to ignore keyup events    */
  if (type == KeyPress)
  {
    rdpEnqueueKey(KeyPress, x_scancode);
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
