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
   Copyright (C) Jay Sorg 2004-2005

   simple functions

*/

#include "xrdp.h"

/*****************************************************************************/
int rect_contains_pt(struct xrdp_rect* in, int x, int y)
{
  if (x < in->left)
    return 0;
  if (y < in->top)
    return 0;
  if (x >= in->right)
    return 0;
  if (y >= in->bottom)
    return 0;
  return 1;
}

/*****************************************************************************/
int rect_intersect(struct xrdp_rect* in1, struct xrdp_rect* in2,
                   struct xrdp_rect* out)
{
  int rv;
  struct xrdp_rect dumby;

  if (out == 0)
    out = &dumby;
  *out = *in1;
  if (in2->left > in1->left)
    out->left = in2->left;
  if (in2->top > in1->top)
    out->top = in2->top;
  if (in2->right < in1->right)
    out->right = in2->right;
  if (in2->bottom < in1->bottom)
    out->bottom = in2->bottom;
  rv = !ISRECTEMPTY(*out);
  if (!rv)
    g_memset(out, 0, sizeof(struct xrdp_rect));
  return rv;
}

/*****************************************************************************/
/* adjust the bounds to fit in the bitmap */
/* return false if there is nothing to draw else return true */
int check_bounds(struct xrdp_bitmap* b, int* x, int* y, int* cx, int* cy)
{
  if (*x >= b->width)
    return 0;
  if (*y >= b->height)
    return 0;
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
    return 0;
  if (*cy <= 0)
    return 0;
  if (*x + *cx > b->width)
    *cx = b->width - *x;
  if (*y + *cy > b->height)
    *cy = b->height - *y;
  return 1;
}

/*  scan codes
    1   esc
    2   1 or ?
    3   2 or @
    4   3 or #
    5   4 or $
    6   5 or %
    7   6 or ^
    8   7 or &
    9   8 or *
    10  9 or (
    11  10 or )
    12  11 or _
    13  12 or +
    14  backspace
    15  tab
    16  q or Q
    17  w or W
    18  e or E
    19  r or R
    20  t or T
    21  y or Y
    22  u or U
    23  i or I
    24  o or O
    25  p or P
    26  [ or {
    27  ] or }
    28  enter, keypad if ext
    29  left or right ctrl, ext flag is right
    30  a or A
    31  s or S
    32  d or D
    33  f or F
    34  g or G
    35  h or H
    36  j or J
    37  k or K
    38  l or L
    39  ; or :
    40  ' or "
    41  ~
    42  left shift
    43  \
    44  z or Z
    45  x or X
    46  c or C
    47  v or V
    48  b or B
    49  n or N
    50  m or M
    51  , or <
    52  . or >
    53  / can be / on keypad, ext flag is keypad
    54  right shift
    55  * on keypad or print screen if ext
    56  left or right alt, ext flag is right
    57  space
    58  caps lock
    59  F1
    60  F2
    61  F3
    62  F4
    63  F5
    64  F6
    65  F7
    66  F8
    67  F9
    68  F10
    69  num lock
    70  scroll lock
    71  7 or home on keypad, ext flag is not keypad
    72  8 or arrow up on keypad, ext flag is not keypad
    73  9 or page up
    74  -(minus) on keypad
    75  4 or arrow left on keypad, ext flag is not keypad
    76  middle(5 key) of keypad
    77  6 or arrow right, can be on keypad, ext flag in not keypad
    78  + on keypad
    79  1 or end
    80  2 or arrow down, can be on keypad, ext flag in not keypad
    81  3 or page down
    82  0 or insert on keypad, ext flag is not keypad
    83  . or delete on keypad, ext flag is not keypad
    87  F11
    88  F12
    91  left win key ext always on
    92  right win key ext always on
    93  menu key ext always on
*/

/* non shift chars */
char chars1[] = {'\0', '\0', '1',  '2',  '3',  '4',  '5',  '6',
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
char chars2[] = {'\0', '\0', '!',  '@',  '#',  '$',  '%',  '^',
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
char get_char_from_scan_code(int device_flags, int scan_code, int* keys,
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
      rv = '/';
  }
  else
  {
    if (shift)
      rv = chars2[scan_code];
    else
      rv = chars1[scan_code];
    if (rv >= 'a' && rv <= 'z' && caps_lock)
      rv = rv - ('a' - 'A');
    else if (rv >= 'A' && rv <= 'Z' && caps_lock)
      rv = rv + ('a' - 'A');
  }
  return rv;
}

/*****************************************************************************/
/* add a ch at index position in text, index starts at 0 */
/* if index = -1 add it to the end */
int add_char_at(char* text, char ch, int index)
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
int remove_char_at(char* text, int index)
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
int set_string(char** in_str, char* in)
{
  if (in_str == 0)
  {
    return 0;
  }
  g_free(*in_str);
  *in_str = g_strdup(in);
  return 0;
}
