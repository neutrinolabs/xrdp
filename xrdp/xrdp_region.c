
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
   Copyright (C) Jay Sorg 2004-2007

   region

*/

#include "xrdp.h"

/*****************************************************************************/
struct xrdp_region* APP_CC
xrdp_region_create(struct xrdp_wm* wm)
{
  struct xrdp_region* self;

  self = (struct xrdp_region*)g_malloc(sizeof(struct xrdp_region), 1);
  self->wm = wm;
  self->rects = list_create();
  self->rects->auto_free = 1;
  return self;
}

/*****************************************************************************/
void APP_CC
xrdp_region_delete(struct xrdp_region* self)
{
  if (self == 0)
  {
    return;
  }
  list_delete(self->rects);
  g_free(self);
}

/*****************************************************************************/
int APP_CC
xrdp_region_add_rect(struct xrdp_region* self, struct xrdp_rect* rect)
{
  struct xrdp_rect* r;

  r = (struct xrdp_rect*)g_malloc(sizeof(struct xrdp_rect), 1);
  *r = *rect;
  list_add_item(self->rects, (long)r);
  return 0;
}

/*****************************************************************************/
int APP_CC
xrdp_region_insert_rect(struct xrdp_region* self, int i, int left,
                        int top, int right, int bottom)
{
  struct xrdp_rect* r;

  r = (struct xrdp_rect*)g_malloc(sizeof(struct xrdp_rect), 1);
  r->left = left;
  r->top = top;
  r->right = right;
  r->bottom = bottom;
  list_insert_item(self->rects, i, (long)r);
  return 0;
}

/*****************************************************************************/
int APP_CC
xrdp_region_subtract_rect(struct xrdp_region* self,
                          struct xrdp_rect* rect)
{
  struct xrdp_rect* r;
  struct xrdp_rect rect1;
  int i;

  for (i = self->rects->count - 1; i >= 0; i--)
  {
    r = (struct xrdp_rect*)list_get_item(self->rects, i);
    rect1 = *r;
    r = &rect1;
    if (rect->left <= r->left &&
        rect->top <= r->top &&
        rect->right >= r->right &&
        rect->bottom >= r->bottom)
    { /* rect is not visible */
      list_remove_item(self->rects, i);
    }
    else if (rect->right < r->left ||
             rect->bottom < r->top ||
             rect->top > r->bottom ||
             rect->left > r->right)
    { /* rect are not related */
    }
    else if (rect->left <= r->left &&
             rect->right >= r->right &&
             rect->bottom < r->bottom &&
             rect->top <= r->top)
    { /* partially covered(whole top) */
      list_remove_item(self->rects, i);
      xrdp_region_insert_rect(self, i, r->left, rect->bottom,
                              r->right, r->bottom);
    }
    else if (rect->top <= r->top &&
             rect->bottom >= r->bottom &&
             rect->right < r->right &&
             rect->left <= r->left)
    { /* partially covered(left) */
      list_remove_item(self->rects, i);
      xrdp_region_insert_rect(self, i, rect->right, r->top,
                              r->right, r->bottom);
    }
    else if (rect->left <= r->left &&
             rect->right >= r->right &&
             rect->top > r->top &&
             rect->bottom >= r->bottom)
    { /* partially covered(bottom) */
      list_remove_item(self->rects, i);
      xrdp_region_insert_rect(self, i, r->left, r->top,
                              r->right, rect->top);
    }
    else if (rect->top <= r->top &&
             rect->bottom >= r->bottom &&
             rect->left > r->left &&
             rect->right >= r->right)
    { /* partially covered(right) */
      list_remove_item(self->rects, i);
      xrdp_region_insert_rect(self, i, r->left, r->top,
                              rect->left, r->bottom);
    }
    else if (rect->left <= r->left &&
             rect->top <= r->top &&
             rect->right < r->right &&
             rect->bottom < r->bottom)
    { /* partially covered(top left) */
      list_remove_item(self->rects, i);
      xrdp_region_insert_rect(self, i, rect->right, r->top,
                              r->right, rect->bottom);
      xrdp_region_insert_rect(self, i, r->left, rect->bottom,
                              r->right, r->bottom);
    }
    else if (rect->left <= r->left &&
             rect->bottom >= r->bottom &&
             rect->right < r->right &&
             rect->top > r->top)
    { /* partially covered(bottom left) */
      list_remove_item(self->rects, i);
      xrdp_region_insert_rect(self, i, r->left, r->top,
                              r->right, rect->top);
      xrdp_region_insert_rect(self, i, rect->right, rect->top,
                              r->right, r->bottom);
    }
    else if (rect->left > r->left &&
             rect->right >= r->right &&
             rect->top <= r->top &&
             rect->bottom < r->bottom)
    { /* partially covered(top right) */
      list_remove_item(self->rects, i);
      xrdp_region_insert_rect(self, i, r->left, r->top,
                              rect->left, r->bottom);
      xrdp_region_insert_rect(self, i, rect->left, rect->bottom,
                              r->right, r->bottom);
    }
    else if (rect->left > r->left &&
             rect->right >= r->right &&
             rect->top > r->top &&
             rect->bottom >= r->bottom)
    { /* partially covered(bottom right) */
      list_remove_item(self->rects, i);
      xrdp_region_insert_rect(self, i, r->left, r->top,
                              r->right, rect->top);
      xrdp_region_insert_rect(self, i, r->left, rect->top,
                              rect->left, r->bottom);
    }
    else if (rect->left > r->left &&
             rect->top <= r->top &&
             rect->right < r->right &&
             rect->bottom >= r->bottom)
    { /* 2 rects, one on each end */
      list_remove_item(self->rects, i);
      xrdp_region_insert_rect(self, i, r->left, r->top,
                              rect->left, r->bottom);
      xrdp_region_insert_rect(self, i, rect->right, r->top,
                              r->right, r->bottom);
    }
    else if (rect->left <= r->left &&
             rect->top > r->top &&
             rect->right >= r->right &&
             rect->bottom < r->bottom)
    { /* 2 rects, one on each end */
      list_remove_item(self->rects, i);
      xrdp_region_insert_rect(self, i, r->left, r->top,
                              r->right, rect->top);
      xrdp_region_insert_rect(self, i, r->left, rect->bottom,
                              r->right, r->bottom);
    }
    else if (rect->left > r->left &&
             rect->right < r->right &&
             rect->top <= r->top &&
             rect->bottom < r->bottom)
    { /* partially covered(top) */
      list_remove_item(self->rects, i);
      xrdp_region_insert_rect(self, i, r->left, r->top,
                              rect->left, r->bottom);
      xrdp_region_insert_rect(self, i, rect->left, rect->bottom,
                              rect->right, r->bottom);
      xrdp_region_insert_rect(self, i, rect->right, r->top,
                              r->right, r->bottom);
    }
    else if (rect->top > r->top &&
             rect->bottom < r->bottom &&
             rect->left <= r->left &&
             rect->right < r->right)
    { /* partially covered(left) */
      list_remove_item(self->rects, i);
      xrdp_region_insert_rect(self, i, r->left, r->top,
                              r->right, rect->top);
      xrdp_region_insert_rect(self, i, rect->right, rect->top,
                              r->right, rect->bottom);
      xrdp_region_insert_rect(self, i, r->left, rect->bottom,
                              r->right, r->bottom);
    }
    else if (rect->left > r->left &&
             rect->right < r->right &&
             rect->bottom >= r->bottom &&
             rect->top > r->top)
    { /* partially covered(bottom) */
      list_remove_item(self->rects, i);
      xrdp_region_insert_rect(self, i, r->left, r->top,
                              rect->left, r->bottom);
      xrdp_region_insert_rect(self, i, rect->left, r->top,
                              rect->right, rect->top);
      xrdp_region_insert_rect(self, i, rect->right, r->top,
                              r->right, r->bottom);
    }
    else if (rect->top > r->top &&
             rect->bottom < r->bottom &&
             rect->right >= r->right &&
             rect->left > r->left)
    { /* partially covered(right) */
      list_remove_item(self->rects, i);
      xrdp_region_insert_rect(self, i, r->left, r->top,
                              r->right, rect->top);
      xrdp_region_insert_rect(self, i, r->left, rect->top,
                              rect->left, rect->bottom);
      xrdp_region_insert_rect(self, i, r->left, rect->bottom,
                              r->right, r->bottom);
    }
    else if (rect->left > r->left &&
             rect->top > r->top &&
             rect->right < r->right &&
             rect->bottom < r->bottom)
    { /* totally contained, 4 rects */
      list_remove_item(self->rects, i);
      xrdp_region_insert_rect(self, i, r->left, r->top,
                              r->right, rect->top);
      xrdp_region_insert_rect(self, i, r->left, rect->top,
                              rect->left, rect->bottom);
      xrdp_region_insert_rect(self, i, r->left, rect->bottom,
                              r->right, r->bottom);
      xrdp_region_insert_rect(self, i, rect->right, rect->top,
                              r->right, rect->bottom);
    }
    else
    {
      g_writeln("error in xrdp_region_subtract_rect");
    }
  }
  return 0;
}

/*****************************************************************************/
int APP_CC
xrdp_region_get_rect(struct xrdp_region* self, int index,
                     struct xrdp_rect* rect)
{
  struct xrdp_rect* r;

  r = (struct xrdp_rect*)list_get_item(self->rects, index);
  if (r == 0)
  {
    return 1;
  }
  *rect = *r;
  return 0;
}
