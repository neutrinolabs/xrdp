/*
Copyright 2005-2007 Jay Sorg

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

#include "rdp.h"

static DeviceIntPtr kbdDevice;
static int g_old_button_mask = 0;
static int g_caps_lock = 0;
extern int g_Bpp; /* from rdpmain.c */
extern ScreenPtr g_pScreen; /* from rdpmain.c */
extern rdpScreenInfo rdpScreen; /* from rdpmain.c */

#define CONTROL_L_KEY_CODE MIN_KEY_CODE
#define CONTROL_R_KEY_CODE (MIN_KEY_CODE + 1)
#define MIN_KEY_CODE 8
#define MAX_KEY_CODE 255
#define NO_OF_KEYS (MAX_KEY_CODE - MIN_KEY_CODE + 1)
#define GLYPHS_PER_KEY 2
#define SHIFT_L_KEY_CODE (MIN_KEY_CODE + 2)
#define SHIFT_R_KEY_CODE (MIN_KEY_CODE + 3)
#define META_L_KEY_CODE (MIN_KEY_CODE + 4)
#define META_R_KEY_CODE (MIN_KEY_CODE + 5)
#define ALT_L_KEY_CODE (MIN_KEY_CODE + 6)
#define ALT_R_KEY_CODE (MIN_KEY_CODE + 7)
#define N_PREDEFINED_KEYS (sizeof(kbdMap) / (sizeof(KeySym) * GLYPHS_PER_KEY))

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


static KeySym kbdMap[] =
{
  /* non shift          shift                  index */
  /* key map */
  XK_Control_L,         NoSymbol,             /* 8 */
  XK_Control_R,         NoSymbol,             /* 9 */
  XK_Shift_L,           NoSymbol,             /* 10 */
  XK_Shift_R,           NoSymbol,
  XK_Meta_L,            NoSymbol,
  XK_Meta_R,            NoSymbol,
  XK_Alt_L,             NoSymbol,
  XK_Alt_R,             NoSymbol,
  /* Standard US keyboard */
  XK_space,             NoSymbol,
  XK_0,                 XK_parenright,
  XK_1,                 XK_exclam,
  XK_2,                 XK_at,
  XK_3,                 XK_numbersign,        /* 20 */
  XK_4,                 XK_dollar,
  XK_5,                 XK_percent,
  XK_6,                 XK_asciicircum,
  XK_7,                 XK_ampersand,
  XK_8,                 XK_asterisk,
  XK_9,                 XK_parenleft,
  XK_minus,             XK_underscore,
  XK_equal,             XK_plus,
  XK_bracketleft,       XK_braceleft,
  XK_bracketright,      XK_braceright,        /* 30 */
  XK_semicolon,         XK_colon,
  XK_apostrophe,        XK_quotedbl,
  XK_grave,             XK_asciitilde,
  XK_comma,             XK_less,
  XK_period,            XK_greater,
  XK_slash,             XK_question,
  XK_backslash,         XK_bar,
  XK_a,                 XK_A,
  XK_b,                 XK_B,
  XK_c,                 XK_C,                 /* 40 */
  XK_d,                 XK_D,
  XK_e,                 XK_E,
  XK_f,                 XK_F,
  XK_g,                 XK_G,
  XK_h,                 XK_H,
  XK_i,                 XK_I,
  XK_j,                 XK_J,
  XK_k,                 XK_K,
  XK_l,                 XK_L,
  XK_m,                 XK_M,                 /* 50 */
  XK_n,                 XK_N,
  XK_o,                 XK_O,
  XK_p,                 XK_P,
  XK_q,                 XK_Q,
  XK_r,                 XK_R,
  XK_s,                 XK_S,
  XK_t,                 XK_T,
  XK_u,                 XK_U,
  XK_v,                 XK_V,
  XK_w,                 XK_W,                 /* 60 */
  XK_x,                 XK_X,
  XK_y,                 XK_Y,
  XK_z,                 XK_Z,
  /* Other useful keys */
  XK_BackSpace,         NoSymbol,
  XK_Return,            NoSymbol,
  XK_Tab,               NoSymbol,
  XK_Escape,            NoSymbol,
  XK_Delete,            NoSymbol,
  XK_Home,              NoSymbol,
  XK_End,               NoSymbol,             /* 70 */
  XK_Page_Up,           NoSymbol,
  XK_Page_Down,         NoSymbol,
  XK_Up,                NoSymbol,
  XK_Down,              NoSymbol,
  XK_Left,              NoSymbol,
  XK_Right,             NoSymbol,
  XK_F1,                NoSymbol,
  XK_F2,                NoSymbol,
  XK_F3,                NoSymbol,
  XK_F4,                NoSymbol,             /* 80 */
  XK_F5,                NoSymbol,
  XK_F6,                NoSymbol,
  XK_F7,                NoSymbol,
  XK_F8,                NoSymbol,
  XK_F9,                NoSymbol,
  XK_F10,               NoSymbol,
  XK_F11,               NoSymbol,
  XK_F12,               NoSymbol,

  XK_KP_Multiply,       NoSymbol,             /* 89 */
  XK_Print,             NoSymbol,             /* 90 */
  XK_Caps_Lock,         NoSymbol,             /* 91 */
  XK_Num_Lock,          NoSymbol,             /* 92 */
  XK_Scroll_Lock,       NoSymbol,             /* 93 */
  XK_KP_Home,           NoSymbol,             /* 94 */
  XK_KP_7,              NoSymbol,             /* 95 */
  XK_KP_Up,             NoSymbol,             /* 96 */
  XK_KP_8,              NoSymbol,             /* 97 */
  XK_KP_Page_Up,        NoSymbol,             /* 98 */
  XK_KP_9,              NoSymbol,             /* 99 */
  XK_KP_Subtract,       NoSymbol,             /* 100 */
  XK_KP_Left,           NoSymbol,             /* 101 */
  XK_KP_4,              NoSymbol,             /* 102 */
  XK_KP_5,              NoSymbol,             /* 103 */
  XK_KP_Right,          NoSymbol,             /* 104 */
  XK_KP_6,              NoSymbol,             /* 105 */
  XK_KP_Add,            NoSymbol,             /* 106 */
  XK_KP_End,            NoSymbol,             /* 107 */
  XK_KP_1,              NoSymbol,             /* 108 */
  XK_KP_Down,           NoSymbol,             /* 109 */
  XK_KP_2,              NoSymbol,             /* 110 */
  XK_KP_Page_Down,      NoSymbol,             /* 111 */
  XK_KP_3,              NoSymbol,             /* 112 */
  XK_KP_Insert,         NoSymbol,             /* 113 */
  XK_KP_0,              NoSymbol,             /* 114 */
  XK_Insert,            NoSymbol,             /* 115 */
  XK_KP_Delete,         NoSymbol,             /* 116 */
  XK_KP_Decimal,        NoSymbol,             /* 117 */
  XK_A,                 XK_a,                 /* 118 */
  XK_B,                 XK_b,                 /* 119 */
  XK_C,                 XK_c,                 /* 120 */
  XK_D,                 XK_d,                 /* 121 */
  XK_E,                 XK_e,                 /* 122 */
  XK_F,                 XK_f,                 /* 123 */
  XK_G,                 XK_g,                 /* 124 */
  XK_H,                 XK_h,                 /* 125 */
  XK_I,                 XK_i,                 /* 126 */
  XK_J,                 XK_j,                 /* 127 */
  XK_K,                 XK_k,                 /* 128 */
  XK_L,                 XK_l,                 /* 129 */
  XK_M,                 XK_m,                 /* 130 */
  XK_N,                 XK_n,                 /* 131 */
  XK_O,                 XK_o,                 /* 132 */
  XK_P,                 XK_p,                 /* 133 */
  XK_Q,                 XK_q,                 /* 134 */
  XK_R,                 XK_r,                 /* 135 */
  XK_S,                 XK_s,                 /* 136 */
  XK_T,                 XK_t,                 /* 137 */
  XK_U,                 XK_u,                 /* 138 */
  XK_V,                 XK_v,                 /* 139 */
  XK_W,                 XK_w,                 /* 140 */
  XK_X,                 XK_x,                 /* 141 */
  XK_Y,                 XK_y,                 /* 142 */
  XK_Z,                 XK_z                  /* 143 */

};

/******************************************************************************/
static void
rdpSendBell()
{
  ErrorF("hi rdpSendBell\n");
}

/******************************************************************************/
void
KbdDeviceInit(DeviceIntPtr pDevice, KeySymsPtr pKeySyms, CARD8* pModMap)
{
  int i;

  ErrorF("hi KbdDeviceInit\n");
  kbdDevice = pDevice;
  for (i = 0; i < MAP_LENGTH; i++)
  {
    pModMap[i] = NoSymbol;
  }
  pModMap[CONTROL_L_KEY_CODE] = ControlMask;
  pModMap[CONTROL_R_KEY_CODE] = ControlMask;
  pModMap[SHIFT_L_KEY_CODE] = ShiftMask;
  pModMap[SHIFT_R_KEY_CODE] = ShiftMask;
  pModMap[META_L_KEY_CODE] = Mod4Mask;
  pModMap[META_R_KEY_CODE] = Mod4Mask;
  pModMap[ALT_L_KEY_CODE] = Mod1Mask;
  pModMap[ALT_R_KEY_CODE] = Mod1Mask;
  pKeySyms->minKeyCode = MIN_KEY_CODE;
  pKeySyms->maxKeyCode = MAX_KEY_CODE;
  pKeySyms->mapWidth = GLYPHS_PER_KEY;
  pKeySyms->map = (KeySym *)Xalloc(sizeof(KeySym) * MAP_LENGTH *
                                                    GLYPHS_PER_KEY);
  if (pKeySyms->map == 0)
  {
    rdpLog("Xalloc failed\n");
    exit(1);
  }
  for (i = 0; i < MAP_LENGTH * GLYPHS_PER_KEY  ; i++)
  {
    pKeySyms->map[i] = NoSymbol;
  }
  for (i = 0; i < N_PREDEFINED_KEYS * GLYPHS_PER_KEY; i++)
  {
    pKeySyms->map[i] = kbdMap[i];
  }
}

/******************************************************************************/
void
KbdDeviceOn(void)
{
  ErrorF("hi KbdDeviceOn\n");
  /*usleep(1000000);*/
  /*ErrorF("bye KbdDeviceOn\n");*/
}

/******************************************************************************/
void
KbdDeviceOff(void)
{
  ErrorF("hi KbdDeviceOff\n");
}

/******************************************************************************/
int
rdpKeybdProc(DeviceIntPtr pDevice, int onoff)
{
  KeySymsRec keySyms;
  CARD8 modMap[MAP_LENGTH];
  DevicePtr pDev;

  ErrorF("hi rdpKeybdProc\n");
  pDev = (DevicePtr)pDevice;
  switch (onoff)
  {
    case DEVICE_INIT:
      KbdDeviceInit(pDevice, &keySyms, modMap);
      InitKeyboardDeviceStruct(pDev, &keySyms, modMap,
                               (BellProcPtr)rdpSendBell,
                               (KbdCtrlProcPtr)NoopDDA);
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
PtrDeviceControl(DevicePtr dev, PtrCtrl* ctrl)
{
  ErrorF("hi PtrDeviceControl\n");
}

/******************************************************************************/
void
PtrDeviceInit(void)
{
  ErrorF("hi PtrDeviceInit\n");
}

/******************************************************************************/
void
PtrDeviceOn(DeviceIntPtr pDev)
{
  ErrorF("hi PtrDeviceOn\n");
}

/******************************************************************************/
void
PtrDeviceOff(void)
{
  ErrorF("hi PtrDeviceOff\n");
}

/******************************************************************************/
int
rdpMouseProc(DeviceIntPtr pDevice, int onoff)
{
  BYTE map[6];
  DevicePtr pDev;

  ErrorF("hi rdpMouseProc\n");
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
      InitPointerDeviceStruct(pDev, map, 5, miPointerGetMotionEvents,
                              PtrDeviceControl,
                              miPointerGetMotionBufferSize());
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
  /*ErrorF("hi rdpCursorOffScreen\n");*/
  return 0;
}

/******************************************************************************/
void
rdpCrossScreen(ScreenPtr pScreen, Bool entering)
{
  /*ErrorF("hi rdpCrossScreen\n");*/
}

/******************************************************************************/
Bool
rdpSpriteRealizeCursor(ScreenPtr pScreen, CursorPtr pCursor)
{
  /*ErrorF("hi rdpSpriteRealizeCursor\n");*/
  return 1;
}

/******************************************************************************/
Bool
rdpSpriteUnrealizeCursor(ScreenPtr pScreen, CursorPtr pCursor)
{
  /*ErrorF("hi rdpSpriteUnrealizeCursor\n");*/
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
    /* todo, for now checking processor but is there a better way?
       maybe LSBFirst */
#if defined(__sparc__) || defined(__PPC__)
    return (c & (0x80 >> shift)) != 0;
#else
    return (g_reverse_byte[c] & (0x80 >> shift)) != 0;
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
rdpSpriteSetCursor(ScreenPtr pScreen, CursorPtr pCursor, int x, int y)
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

  if (pCursor == 0)
  {
    return;
  }
  if (pCursor->bits == 0)
  {
    return;
  }
  w = pCursor->bits->width;
  h = pCursor->bits->height;
  paddedRowBytes = PixmapBytePad(w, 1);
  xhot = pCursor->bits->xhot;
  yhot = pCursor->bits->yhot;
  data = (char*)(pCursor->bits->source);
  mask = (char*)(pCursor->bits->mask);
  fgcolor = (((pCursor->foreRed >> 8) & 0xff) << 16) |
            (((pCursor->foreGreen >> 8) & 0xff) << 8) |
            ((pCursor->foreBlue >> 8) & 0xff);
  bgcolor = (((pCursor->backRed >> 8) & 0xff) << 16) |
            (((pCursor->backGreen >> 8) & 0xff) << 8) |
            ((pCursor->backBlue >> 8) & 0xff);
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
rdpSpriteMoveCursor(ScreenPtr pScreen, int x, int y)
{
  /*ErrorF("hi rdpSpriteMoveCursor\n");*/
}

/******************************************************************************/
void
PtrAddEvent(int buttonMask, int x, int y)
{
  xEvent ev;
  int i;
  unsigned long time;

  time = GetTimeInMillis();
  miPointerAbsoluteCursor(x, y, time);
  for (i = 0; i < 5; i++)
  {
    if ((buttonMask ^ g_old_button_mask) & (1 << i))
    {
      if (buttonMask & (1 << i))
      {
        ev.u.u.type = ButtonPress;
        ev.u.u.detail = i + 1;
        ev.u.keyButtonPointer.time = time;
        mieqEnqueue(&ev);
      }
      else
      {
        ev.u.u.type = ButtonRelease;
        ev.u.u.detail = i + 1;
        ev.u.keyButtonPointer.time = time;
        mieqEnqueue(&ev);
      }
    }
  }
  g_old_button_mask = buttonMask;
}

/******************************************************************************/
void
KbdAddEvent(int down, int param1, int param2, int param3, int param4)
{
  xEvent ev;
  unsigned long time;
  int ch;

  time = GetTimeInMillis();
  if (down)
  {
    ev.u.u.type = KeyPress;
  }
  else
  {
    ev.u.u.type = KeyRelease;
  }
  ev.u.keyButtonPointer.time = time;
  ch = 0;
  switch (param3)
  {
    case 1: /* esc */
      ch = 67;
      break;
    case 2: /* 1 or ! */
      ch = 18;
      break;
    case 3: /* 2 or @ */
      ch = 19;
      break;
    case 4: /* 3 or # */
      ch = 20;
      break;
    case 5: /* 4 or $ */
      ch = 21;
      break;
    case 6: /* 5 or % */
      ch = 22;
      break;
    case 7: /* 6 or ^ */
      ch = 23;
      break;
    case 8: /* 7 or & */
      ch = 24;
      break;
    case 9: /* 8 or * */
      ch = 25;
      break;
    case 10: /* 9 or ( */
      ch = 26;
      break;
    case 11: /* 0 or ) */
      ch = 17;
      break;
    case 12: /* - or _ */
      ch = 27;
      break;
    case 13: /* = or + */
      ch = 28;
      break;
    case 14: /* backspace */
      ch = 64;
      break;
    case 15: /* tab */
      ch = 66;
      break;
    case 16: /* q */
      ch = 54;
      break;
    case 17: /* w */
      ch = 60;
      break;
    case 18: /* e */
      ch = 42;
      break;
    case 19: /* r */
      ch = 55;
      break;
    case 20: /* t */
      ch = 57;
      break;
    case 21: /* y */
      ch = 62;
      break;
    case 22: /* u */
      ch = 58;
      break;
    case 23: /* i */
      ch = 46;
      break;
    case 24: /* o */
      ch = 52;
      break;
    case 25: /* p */
      ch = 53;
      break;
    case 26: /* [ or { */
      ch = 29;
      break;
    case 27: /* ] or } */
      ch = 30;
      break;
    case 28: /* enter */
      ch = 65;
      break;
    case 29: /* left and right control */
      if (param4 & 0x100) /* rdp ext flag */
      {
        ch = 9;
      }
      else
      {
        ch = 8;
      }
      break;
    case 30: /* a */
      /*ch = g_caps_lock ? 118 : 38;*/
      /*ch = 118;*/
      ch = 38;
      break;
    case 31: /* s */
      ch = 56;
      break;
    case 32: /* d */
      ch = 41;
      break;
    case 33: /* f */
      ch = 43;
      break;
    case 34: /* g */
      ch = 44;
      break;
    case 35: /* h */
      ch = 45;
      break;
    case 36: /* j */
      ch = 47;
      break;
    case 37: /* k */
      ch = 48;
      break;
    case 38: /* l */
      ch = 49;
      break;
    case 39: /* : or ; */
      ch = 31;
      break;
    case 40: /* ' or " */
      ch = 32;
      break;
    case 41: /* ` or ~ */
      ch = 33;
      break;
    case 42: /* left shift */
      ch = 10;
      break;
    case 43: /* / */
      ch = 36;
      break;
    case 44: /* z */
      ch = 63;
      break;
    case 45: /* x */
      ch = 61;
      break;
    case 46: /* c */
      ch = 40;
      break;
    case 47: /* v */
      ch = 59;
      break;
    case 48: /* b */
      ch = 39;
      break;
    case 49: /* n */
      ch = 51;
      break;
    case 50: /* m */
      ch = 50;
      break;
    case 51: /* , or < */
      ch = 34;
      break;
    case 52: /* . or > */
      ch = 35;
      break;
    case 53: /* / or ? */
      ch = 36;
      break;
    case 54: /* right shift */
      ch = 11;
      break;
    case 55: /* * on keypad or print screen if ext */
      if (param4 & 0x100) /* rdp ext flag */
      {
        ch = 90;
      }
      else
      {
        ch = 89;
      }
      break;
    case 56: /* left and right alt */
      if (param4 & 0x100) /* rdp ext flag */
      {
        ch = 15; /* right alt */
      }
      else
      {
        ch = 14; /* left alt */
      }
      break;
    case 57: /* space */
      ch = 16;
      break;
    case 58: /* caps lock */
      ch = 91;
      g_caps_lock = down;
      break;
    case 59: /* F1 */
      ch = 77;
      break;
    case 60: /* F2 */
      ch = 78;
      break;
    case 61: /* F3 */
      ch = 79;
      break;
    case 62: /* F4 */
      ch = 80;
      break;
    case 63: /* F5 */
      ch = 81;
      break;
    case 64: /* F6 */
      ch = 82;
      break;
    case 65: /* F7 */
      ch = 83;
      break;
    case 66: /* F8 */
      ch = 84;
      break;
    case 67: /* F9 */
      ch = 85;
      break;
    case 68: /* F10 */
      ch = 86;
      break;
    case 69: /* Num lock */
      ch = 92;
      break;
    case 70: /* Scroll lock */
      ch = 93;
      break;
    case 71: /* 7 or home */
      if (param2 == 0xffff) /* ascii 7 */
      {
        ch = 95; /* keypad 7 */
      }
      else if (param4 & 0x100)
      {
        ch = 69; /* non keypad home */
      }
      else
      {
        ch = 94; /* keypad home */
      }
      break;
    case 72: /* 8 or up arrow */
      if (param2 == 0xffff) /* ascii 8 */
      {
        ch = 97; /* keypad 8 */
      }
      else if (param4 & 0x100)
      {
        ch = 73; /* non keypad up */
      }
      else
      {
        ch = 96; /* keypad up */
      }
      break;
    case 73: /* 9 or page up */
      if (param2 == 0xffff) /* ascii 9 */
      {
        ch = 99; /* keypad 9 */
      }
      else if (param4 & 0x100)
      {
        ch = 71; /* non keypad page up */
      }
      else
      {
        ch = 98; /* keypad page up */
      }
      break;
    case 74: /* minus on keypad */
      ch = 100;
      break;
    case 75: /* 4 or left arrow */
      if (param2 == 0xffff) /* ascii 4 */
      {
        ch = 102; /* keypad 4 */
      }
      else if (param4 & 0x100)
      {
        ch = 75; /* non keypad left */
      }
      else
      {
        ch = 101; /* keypad left */
      }
      break;
    case 76: /* 5 on keypad */
      ch = 103;
      break;
    case 77: /* 6 or right arrow */
      if (param2 == 0xffff) /* ascii 6 */
      {
        ch = 105; /* keypad 6 */
      }
      else if (param4 & 0x100)
      {
        ch = 76; /* non keypad right */
      }
      else
      {
        ch = 104; /* keypad right */
      }
      break;
    case 78: /* plus on keypad */
      ch = 106;
      break;
    case 79: /* 1 or end */
      if (param2 == 0xffff) /* ascii 1 */
      {
        ch = 108; /* keypad 1 */
      }
      else if (param4 & 0x100)
      {
        ch = 70; /* non keypad end */
      }
      else
      {
        ch = 107; /* keypad end */
      }
      break;
    case 80: /* 2 or down arrow */
      if (param2 == 0xffff) /* ascii 2 */
      {
        ch = 110; /* keypad 2 */
      }
      else if (param4 & 0x100)
      {
        ch = 74; /* non keypad down arrow */
      }
      else
      {
        ch = 109; /* keypad down arrow */
      }
      break;
    case 81: /* 3 or page down */
      if (param2 == 0xffff) /* ascii 3 */
      {
        ch = 112; /* keypad 3 */
      }
      else if (param4 & 0x100)
      {
        ch = 72; /* non keypad page down */
      }
      else
      {
        ch = 111; /* keypad page down */
      }
      break;
    case 82: /* 0 or insert */
      if (param2 == 0xffff) /* ascii 0 */
      {
        ch = 114; /* keypad 0 */
      }
      else if (param4 & 0x100)
      {
        ch = 115; /* non keypad insert */
      }
      else
      {
        ch = 113; /* keypad insert */
      }
      break;
    case 83: /* . or delete */
      if (param2 == 0xffff) /* ascii . */
      {
        ch = 117; /* keypad . */
      }
      else if (param4 & 0x100)
      {
        ch = 68; /* non keypad delete */
      }
      else
      {
        ch = 116; /* keypad delete */
      }
      break;
    case 87: /* F11 */
      ch = 87;
      break;
    case 88: /* F12 */
      ch = 88;
      break;
  }
  if (ch > 0)
  {
    ev.u.u.detail = ch;
    mieqEnqueue(&ev);
  }
}
