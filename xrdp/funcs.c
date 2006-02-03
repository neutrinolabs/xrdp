/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   xrdp: A Remote Desktop Protocol server.
   Copyright (C) Jay Sorg 2004-2006

   simple functions

*/

#include "xrdp.h"

/*****************************************************************************/
/* returns boolean */
int APP_CC
rect_contains_pt(struct xrdp_rect* in, int x, int y)
{
  if (x < in->left)
  {
    return 0;
  }
  if (y < in->top)
  {
    return 0;
  }
  if (x >= in->right)
  {
    return 0;
  }
  if (y >= in->bottom)
  {
    return 0;
  }
  return 1;
}

/*****************************************************************************/
int APP_CC
rect_intersect(struct xrdp_rect* in1, struct xrdp_rect* in2,
               struct xrdp_rect* out)
{
  int rv;
  struct xrdp_rect dumby;

  if (out == 0)
  {
    out = &dumby;
  }
  *out = *in1;
  if (in2->left > in1->left)
  {
    out->left = in2->left;
  }
  if (in2->top > in1->top)
  {
    out->top = in2->top;
  }
  if (in2->right < in1->right)
  {
    out->right = in2->right;
  }
  if (in2->bottom < in1->bottom)
  {
    out->bottom = in2->bottom;
  }
  rv = !ISRECTEMPTY(*out);
  if (!rv)
  {
    g_memset(out, 0, sizeof(struct xrdp_rect));
  }
  return rv;
}

/*****************************************************************************/
/* returns boolean */
int APP_CC
rect_contained_by(struct xrdp_rect* in1, int left, int top,
                  int right, int bottom)
{
  if (left < in1->left || top < in1->top ||
      right > in1->right || bottom > in1->bottom)
  {
    return 0;
  }
  else
  {
    return 1;
  }
}

/*****************************************************************************/
/* adjust the bounds to fit in the bitmap */
/* return false if there is nothing to draw else return true */
int APP_CC
check_bounds(struct xrdp_bitmap* b, int* x, int* y, int* cx, int* cy)
{
  if (*x >= b->width)
  {
    return 0;
  }
  if (*y >= b->height)
  {
    return 0;
  }
  if (*x < 0)
  {
    *cx += *x;
    *x = 0;
  }
  if (*y < 0)
  {
    *cy += *y;
    *y = 0;
  }
  if (*cx <= 0)
  {
    return 0;
  }
  if (*cy <= 0)
  {
    return 0;
  }
  if (*x + *cx > b->width)
  {
    *cx = b->width - *x;
  }
  if (*y + *cy > b->height)
  {
    *cy = b->height - *y;
  }
  return 1;
}

/*****************************************************************************/
/*  scan codes
    1 0x01  esc
    2 0x02  1 or !
    3 0x03  2 or @
    4 0x04  3 or #
    5 0x05  4 or $
    6 0x06  5 or %
    7 0x07  6 or ^
    8 0x08  7 or &
    9 0x09  8 or *
    10 0x0a 9 or (
    11 0x0b 0 or )
    12 0x0c - or _
    13 0x0d = or +
    14 0x0e backspace
    15 0x0f tab
    16 0x10 q or Q
    17 0x11 w or W
    18 0x12 e or E
    19 0x13 r or R
    20 0x14 t or T
    21 0x15 y or Y
    22 0x16 u or U
    23 0x17 i or I
    24 0x18 o or O
    25 0x19 p or P
    26 0x1a [ or {
    27 0x1b ] or }
    28 0x1c enter, keypad is ext
    29 0x1d left or right ctrl, ext flag is right
    30 0x1e a or A
    31 0x1f s or S
    32 0x20 d or D
    33 0x21 f or F
    34 0x22 g or G
    35 0x23 h or H
    36 0x24 j or J
    37 0x25 k or K
    38 0x26 l or L
    39 0x27 ; or :
    40 0x28 ' or "
    41 0x29  ` or ~
    42 0x2a left shift
    43 0x2b \
    44 0x2c z or Z
    45 0x2d x or X
    46 0x2e c or C
    47 0x2f v or V
    48 0x30 b or B
    49 0x31 n or N
    50 0x32 m or M
    51 0x33 , or <
    52 0x34 . or >
    53 0x35 / can be / on keypad, ext flag is keypad
    54 0x36 right shift
    55 0x37 * on keypad or print screen if ext
    56 0x38 left or right alt, ext flag is right
    57 0x39 space
    58 0x3a caps lock
    59 0x3b F1
    60 0x3c F2
    61 0x3d F3
    62 0x3e F4
    63 0x3f F5
    64 0x40 F6
    65 0x41 F7
    66 0x42 F8
    67 0x43 F9
    68 0x44 F10
    69 0x45 num lock
    70 0x46 scroll lock
    71 0x47 7 or home on keypad, ext flag is not keypad
    72 0x48 8 or arrow up on keypad, ext flag is not keypad
    73 0x49 9 or page up
    74 0x4a -(minus) on keypad
    75 0x4b 4 or arrow left on keypad, ext flag is not keypad
    76 0x4c middle(5 key) of keypad
    77 0x4d 6 or arrow right, can be on keypad, ext flag in not keypad
    78 0x4e + on keypad
    79 0x4f 1 or end
    80 0x50 2 or arrow down, can be on keypad, ext flag in not keypad
    81 0x51 3 or page down
    82 0x52 0 or insert on keypad, ext flag is not keypad
    83 0x53 . or delete on keypad, ext flag is not keypad
    84 0x54
    85 0x55
    86 0x56
    87 0x57 F11
    88 0x58 F12
    89 0x59
    90 0x5a
    91 0x5b left win key ext always on
    92 0x5c right win key ext always on
    93 0x5d menu key ext always on
*/

/* non shift chars */
static char chars1[] =
{'\0', '\0', '1',  '2',  '3',  '4',  '5',  '6',
  '7',  '8',  '9',  '0',  '-',  '=',  '\0', '\0',
  'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',
  'o',  'p',  '[',  ']',  '\0', '\0', 'a',  's',
  'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',
  '\'', '`',  '\0', '\\', 'z',  'x',  'c',  'v',
  'b',  'n',  'm',  ',',  '.',  '/',  '\0', '*',
  '\0', ' ',  '\0', '\0', '\0', '\0', '\0', '\0',
  '\0', '\0', '\0', '\0', '\0', '\0', '\0', '7',
  '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',
  '2',  '3',  '0',  '.',  '\0', '\0', '\0', '\0',
  '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
  '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
  '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
  '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
  '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0'};
/* shift chars */
static char chars2[] =
{'\0', '\0', '!',  '@',  '#',  '$',  '%',  '^',
  '&',  '*',  '(',  ')',  '_',  '+',  '\0', '\0',
  'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',
  'O',  'P',  '{',  '}',  '\0', '\0', 'A',  'S',
  'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',
  '"',  '~', '\0', '|',  'Z',  'X',  'C',  'V',
  'B',  'N',  'M',  '<',  '>',  '?',  '\0', '*',
 '\0', ' ',  '\0', '\0', '\0', '\0', '\0', '\0',
 '\0', '\0', '\0', '\0', '\0', '\0', '\0', '7',
  '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',
  '2',  '3',  '0',  '.',  '\0', '\0', '\0', '\0',
 '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
 '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
 '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
 '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
 '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0'};

/*****************************************************************************/
char APP_CC
get_char_from_scan_code(int device_flags, int scan_code, int* keys,
                        int caps_lock, int num_lock, int scroll_lock)
{
  char rv;
  int shift;
  int ext;

  shift = keys[42] || keys[54];
  ext = device_flags & 0x0100;
  rv = 0;
  if (scan_code >= 128)
  {
    scan_code = scan_code % 128;
    num_lock = 0;
  }
  if (!num_lock)
  {
    switch (scan_code)
    {
      case 71: /* 7 */
      case 72: /* 8 */
      case 73: /* 9 */
      case 75: /* 4 */
      case 76: /* 5 */
      case 77: /* 6 */
      case 79: /* 1 */
      case 80: /* 2 */
      case 81: /* 3 */
      case 82: /* 0 */
      case 83: /* . */
        return rv;
    }
  }
  if (ext)
  {
    if (scan_code == 53)
    {
      rv = '/';
    }
  }
  else
  {
    if (shift)
    {
      rv = chars2[scan_code];
    }
    else
    {
      rv = chars1[scan_code];
    }
    if (rv >= 'a' && rv <= 'z' && caps_lock)
    {
      rv = rv - ('a' - 'A');
    }
    else if (rv >= 'A' && rv <= 'Z' && caps_lock)
    {
      rv = rv + ('a' - 'A');
    }
  }
  return rv;
}

/*****************************************************************************/
/* add a ch at index position in text, index starts at 0 */
/* if index = -1 add it to the end */
int APP_CC
add_char_at(char* text, char ch, int index)
{
  int len;
  int i;

  len = g_strlen(text);
  if (index >= len || index < 0)
  {
    text[len] = ch;
    text[len + 1] = 0;
    return 0;
  }
  for (i = len - 1; i >= index; i--)
  {
    text[i + 1] = text[i];
  }
  text[i + 1] = ch;
  text[len + 1] = 0;
  return 0;
}

/*****************************************************************************/
/* remove a ch at index position in text, index starts at 0 */
/* if index = -1 remove it from the end */
int APP_CC
remove_char_at(char* text, int index)
{
  int len;
  int i;

  len = g_strlen(text);
  if (len <= 0)
  {
    return 0;
  }
  if (index >= len - 1 || index < 0)
  {
    text[len - 1] = 0;
    return 0;
  }
  for (i = index; i < len - 1; i++)
  {
    text[i] = text[i + 1];
  }
  text[len - 1] = 0;
  return 0;
}

/*****************************************************************************/
int APP_CC
set_string(char** in_str, char* in)
{
  if (in_str == 0)
  {
    return 0;
  }
  g_free(*in_str);
  *in_str = g_strdup(in);
  return 0;
}
