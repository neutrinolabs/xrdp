
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
   Copyright (C) Jay Sorg 2004

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
int color16(int r, int g, int b)
{
  r = r >> 3;
  g = g >> 2;
  b = b >> 3;
  return (r << 11) | (g << 5) | b;
}

/*****************************************************************************/
int color24(int r, int g, int b)
{
  return r | (g << 8) | (b << 16);
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
