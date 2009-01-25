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
   Copyright (C) Jay Sorg 2004-2009

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
/* add a ch at index position in text, index starts at 0 */
/* if index = -1 add it to the end */
int APP_CC
add_char_at(char* text, int text_size, twchar ch, int index)
{
  int len;
  int i;
  twchar* wstr;

  len = g_mbstowcs(0, text, 0);
  wstr = (twchar*)g_malloc((len + 16) * sizeof(twchar), 0);
  g_mbstowcs(wstr, text, len + 1);
  if ((index >= len) || (index < 0))
  {
    wstr[len] = ch;
    wstr[len + 1] = 0;
    g_wcstombs(text, wstr, text_size);
    g_free(wstr);
    return 0;
  }
  for (i = (len - 1); i >= index; i--)
  {
    wstr[i + 1] = wstr[i];
  }
  wstr[i + 1] = ch;
  wstr[len + 1] = 0;
  g_wcstombs(text, wstr, text_size);
  g_free(wstr);
  return 0;
}

/*****************************************************************************/
/* remove a ch at index position in text, index starts at 0 */
/* if index = -1 remove it from the end */
int APP_CC
remove_char_at(char* text, int text_size, int index)
{
  int len;
  int i;
  twchar* wstr;

  len = g_mbstowcs(0, text, 0);
  if (len <= 0)
  {
    return 0;
  }
  wstr = (twchar*)g_malloc((len + 16) * sizeof(twchar), 0);
  g_mbstowcs(wstr, text, len + 1);
  if ((index >= (len - 1)) || (index < 0))
  {
    wstr[len - 1] = 0;
    g_wcstombs(text, wstr, text_size);
    g_free(wstr);
    return 0;
  }
  for (i = index; i < (len - 1); i++)
  {
    wstr[i] = wstr[i + 1];
  }
  wstr[len - 1] = 0;
  g_wcstombs(text, wstr, text_size);
  g_free(wstr);
  return 0;
}

/*****************************************************************************/
int APP_CC
set_string(char** in_str, const char* in)
{
  if (in_str == 0)
  {
    return 0;
  }
  g_free(*in_str);
  *in_str = g_strdup(in);
  return 0;
}

/*****************************************************************************/
int APP_CC
wchar_repeat(twchar* dest, int dest_size_in_wchars, twchar ch, int repeat)
{
  int index;

  for (index = 0; index < repeat; index++)
  {
    if (index >= dest_size_in_wchars)
    {
      break;
    }
    dest[index] = ch;
  }
  return 0;
}
