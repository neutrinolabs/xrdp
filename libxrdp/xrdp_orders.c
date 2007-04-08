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

   orders

*/

#include "libxrdp.h"

/*****************************************************************************/
struct xrdp_orders* APP_CC
xrdp_orders_create(struct xrdp_session* session, struct xrdp_rdp* rdp_layer)
{
  struct xrdp_orders* self;

  self = (struct xrdp_orders*)g_malloc(sizeof(struct xrdp_orders), 1);
  self->session = session;
  self->rdp_layer = rdp_layer;
  make_stream(self->out_s);
  init_stream(self->out_s, 16384);
  self->orders_state.clip_right = 1; /* silly rdp right clip */
  self->orders_state.clip_bottom = 1; /* silly rdp bottom clip */
  return self;
}

/*****************************************************************************/
void APP_CC
xrdp_orders_delete(struct xrdp_orders* self)
{
  if (self == 0)
  {
    return;
  }
  free_stream(self->out_s);
  g_free(self->orders_state.text_data);
  g_free(self);
}

/*****************************************************************************/
/* set all values to zero */
/* returns error */
int APP_CC
xrdp_orders_reset(struct xrdp_orders* self)
{
  if (xrdp_orders_force_send(self) != 0)
  {
    return 1;
  }
  g_free(self->orders_state.text_data);
  g_memset(&(self->orders_state), 0, sizeof(self->orders_state));
  self->order_count_ptr = 0;
  self->order_count = 0;
  self->order_level = 0;
  self->orders_state.clip_right = 1; /* silly rdp right clip */
  self->orders_state.clip_bottom = 1; /* silly rdp bottom clip */
  return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
xrdp_orders_init(struct xrdp_orders* self)
{
  self->order_level++;
  if (self->order_level == 1)
  {
    self->order_count = 0;
    /* is this big enough */
    if (xrdp_rdp_init_data(self->rdp_layer, self->out_s) != 0)
    {
      return 1;
    }
    out_uint16_le(self->out_s, RDP_UPDATE_ORDERS);
    out_uint8s(self->out_s, 2); /* pad */
    self->order_count_ptr = self->out_s->p;
    out_uint8s(self->out_s, 2); /* number of orders, set later */
    out_uint8s(self->out_s, 2); /* pad */
  }
  return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
xrdp_orders_send(struct xrdp_orders* self)
{
  int rv;

  rv = 0;
  if (self->order_level > 0)
  {
    self->order_level--;
    if ((self->order_level == 0) && (self->order_count > 0))
    {
      s_mark_end(self->out_s);
      DEBUG(("xrdp_orders_send sending %d orders", self->order_count));
      self->order_count_ptr[0] = self->order_count;
      self->order_count_ptr[1] = self->order_count >> 8;
      self->order_count = 0;
      if (xrdp_rdp_send_data(self->rdp_layer, self->out_s,
                             RDP_DATA_PDU_UPDATE) != 0)
      {
        rv = 1;
      }
    }
  }
  return rv;
}

/*****************************************************************************/
/* returns error */
int APP_CC
xrdp_orders_force_send(struct xrdp_orders* self)
{
  if ((self->order_level > 0) && (self->order_count > 0))
  {
    s_mark_end(self->out_s);
    DEBUG(("xrdp_orders_force_send sending %d orders", self->order_count));
    self->order_count_ptr[0] = self->order_count;
    self->order_count_ptr[1] = self->order_count >> 8;
    if (xrdp_rdp_send_data(self->rdp_layer, self->out_s,
                           RDP_DATA_PDU_UPDATE) != 0)
    {
      return 1;
    }
  }
  self->order_count = 0;
  self->order_level = 0;
  return 0;
}

/*****************************************************************************/
/* check if the current order will fit in packet size of 16384, if not */
/* send what we got and init a new one */
/* returns error */
static int APP_CC
xrdp_orders_check(struct xrdp_orders* self, int max_size)
{
  int size;
  int max_packet_size;

  if (self->rdp_layer->client_info.bpp == 8)
  {
    max_packet_size = 8000;
  }
  else
  {
    max_packet_size = 16000;
  }
  if (self->order_level < 1)
  {
    if (max_size > max_packet_size)
    {
      return 1;
    }
    else
    {
      return 0;
    }
  }
  size = self->out_s->p - self->order_count_ptr;
  if ((size < 0) || (size > max_packet_size))
  {
    return 1;
  }
  if ((size + max_size + 100) > max_packet_size)
  {
    xrdp_orders_force_send(self);
    xrdp_orders_init(self);
  }
  return 0;
}

/*****************************************************************************/
/* check if rect is the same as the last one sent */
/* returns boolean */
static int APP_CC
xrdp_orders_last_bounds(struct xrdp_orders* self, struct xrdp_rect* rect)
{
  if (rect == 0)
  {
    return 0;
  }
  if (rect->left == self->orders_state.clip_left &&
      rect->top == self->orders_state.clip_top &&
      rect->right == self->orders_state.clip_right &&
      rect->bottom == self->orders_state.clip_bottom)
  {
    return 1;
  }
  return 0;
}

/*****************************************************************************/
/* check if all coords are withing 256 bytes */
/* returns boolean */
static int APP_CC
xrdp_orders_send_delta(struct xrdp_orders* self, int* vals, int count)
{
  int i;

  for (i = 0; i < count; i += 2)
  {
    if (g_abs(vals[i] - vals[i + 1]) >= 128)
    {
      return 0;
    }
  }
  return 1;
}

/*****************************************************************************/
/* returns error */
static int APP_CC
xrdp_orders_out_bounds(struct xrdp_orders* self, struct xrdp_rect* rect)
{
  char* bounds_flags_ptr;
  int bounds_flags;

  bounds_flags = 0;
  bounds_flags_ptr = self->out_s->p;
  out_uint8s(self->out_s, 1);
  /* left */
  if (rect->left == self->orders_state.clip_left)
  {
  }
  else if (g_abs(rect->left - self->orders_state.clip_left) < 128)
  {
    bounds_flags |= 0x10;
  }
  else
  {
    bounds_flags |= 0x01;
  }
  /* top */
  if (rect->top == self->orders_state.clip_top)
  {
  }
  else if (g_abs(rect->top - self->orders_state.clip_top) < 128)
  {
    bounds_flags |= 0x20;
  }
  else
  {
    bounds_flags |= 0x02;
  }
  /* right */
  if (rect->right == self->orders_state.clip_right)
  {
  }
  else if (g_abs(rect->right - self->orders_state.clip_right) < 128)
  {
    bounds_flags |= 0x40;
  }
  else
  {
    bounds_flags |= 0x04;
  }
  /* bottom */
  if (rect->bottom == self->orders_state.clip_bottom)
  {
  }
  else if (g_abs(rect->bottom - self->orders_state.clip_bottom) < 128)
  {
    bounds_flags |= 0x80;
  }
  else
  {
    bounds_flags |= 0x08;
  }
  /* left */
  if (bounds_flags & 0x01)
  {
    out_uint16_le(self->out_s, rect->left);
  }
  else if (bounds_flags & 0x10)
  {
    out_uint8(self->out_s, rect->left - self->orders_state.clip_left);
  }
  self->orders_state.clip_left = rect->left;
  /* top */
  if (bounds_flags & 0x02)
  {
    out_uint16_le(self->out_s, rect->top);
  }
  else if (bounds_flags & 0x20)
  {
    out_uint8(self->out_s, rect->top - self->orders_state.clip_top);
  }
  self->orders_state.clip_top = rect->top;
  /* right */
  if (bounds_flags & 0x04)
  {
    out_uint16_le(self->out_s, rect->right - 1); /* silly rdp right clip */
  }
  else if (bounds_flags & 0x40)
  {
    out_uint8(self->out_s, rect->right - self->orders_state.clip_right);
  }
  self->orders_state.clip_right = rect->right;
  /* bottom */
  if (bounds_flags & 0x08)
  {
    out_uint16_le(self->out_s, rect->bottom - 1); /* silly rdp bottom clip */
  }
  else if (bounds_flags & 0x80)
  {
    out_uint8(self->out_s, rect->bottom - self->orders_state.clip_bottom);
  }
  self->orders_state.clip_bottom = rect->bottom;
  /* set flags */
  *bounds_flags_ptr = bounds_flags;
  return 0;
}

/*****************************************************************************/
/* returns error */
/* send a solid rect to client */
/* max size 23 */
int APP_CC
xrdp_orders_rect(struct xrdp_orders* self, int x, int y, int cx, int cy,
                 int color, struct xrdp_rect* rect)
{
  int order_flags;
  int vals[8];
  int present;
  char* present_ptr;

  xrdp_orders_check(self, 23);
  self->order_count++;
  order_flags = RDP_ORDER_STANDARD;
  if (self->orders_state.last_order != RDP_ORDER_RECT)
  {
    order_flags |= RDP_ORDER_CHANGE;
  }
  self->orders_state.last_order = RDP_ORDER_RECT;
  if (rect != 0)
  {
    /* if clip is present, still check if its needed */
    if (x < rect->left || y < rect->top ||
        x + cx > rect->right || y + cy > rect->bottom)
    {
      order_flags |= RDP_ORDER_BOUNDS;
      if (xrdp_orders_last_bounds(self, rect))
      {
        order_flags |= RDP_ORDER_LASTBOUNDS;
      }
    }
  }
  vals[0] = x;
  vals[1] = self->orders_state.rect_x;
  vals[2] = y;
  vals[3] = self->orders_state.rect_y;
  vals[4] = cx;
  vals[5] = self->orders_state.rect_cx;
  vals[6] = cy;
  vals[7] = self->orders_state.rect_cy;
  if (xrdp_orders_send_delta(self, vals, 8))
  {
    order_flags |= RDP_ORDER_DELTA;
  }
  out_uint8(self->out_s, order_flags)
  if (order_flags & RDP_ORDER_CHANGE)
  {
    out_uint8(self->out_s, self->orders_state.last_order);
  }
  present = 0;
  present_ptr = self->out_s->p; /* hold 1 byte present pointer */
  out_uint8s(self->out_s, 1)
  if ((order_flags & RDP_ORDER_BOUNDS) &&
      !(order_flags & RDP_ORDER_LASTBOUNDS))
  {
    xrdp_orders_out_bounds(self, rect);
  }
  if (x != self->orders_state.rect_x)
  {
    present |= 0x01;
    if (order_flags & RDP_ORDER_DELTA)
    {
      out_uint8(self->out_s, x - self->orders_state.rect_x);
    }
    else
    {
      out_uint16_le(self->out_s, x);
    }
    self->orders_state.rect_x = x;
  }
  if (y != self->orders_state.rect_y)
  {
    present |= 0x02;
    if (order_flags & RDP_ORDER_DELTA)
    {
      out_uint8(self->out_s, y - self->orders_state.rect_y);
    }
    else
    {
      out_uint16_le(self->out_s, y);
    }
    self->orders_state.rect_y = y;
  }
  if (cx != self->orders_state.rect_cx)
  {
    present |= 0x04;
    if (order_flags & RDP_ORDER_DELTA)
    {
      out_uint8(self->out_s, cx - self->orders_state.rect_cx);
    }
    else
    {
      out_uint16_le(self->out_s, cx);
    }
    self->orders_state.rect_cx = cx;
  }
  if (cy != self->orders_state.rect_cy)
  {
    present |= 0x08;
    if (order_flags & RDP_ORDER_DELTA)
    {
      out_uint8(self->out_s, cy - self->orders_state.rect_cy);
    }
    else
    {
      out_uint16_le(self->out_s, cy);
    }
    self->orders_state.rect_cy = cy;
  }
  if ((color & 0xff) != (self->orders_state.rect_color & 0xff))
  {
    present |= 0x10;
    self->orders_state.rect_color =
                 (self->orders_state.rect_color & 0xffff00) | (color & 0xff);
    out_uint8(self->out_s, color);
  }
  if ((color & 0xff00) != (self->orders_state.rect_color & 0xff00))
  {
    present |= 0x20;
    self->orders_state.rect_color =
                 (self->orders_state.rect_color & 0xff00ff) | (color & 0xff00);
    out_uint8(self->out_s, color >> 8);
  }
  if ((color & 0xff0000) != (self->orders_state.rect_color & 0xff0000))
  {
    present |= 0x40;
    self->orders_state.rect_color =
                 (self->orders_state.rect_color & 0x00ffff) | (color & 0xff0000);
    out_uint8(self->out_s, color >> 16);
  }
  present_ptr[0] = present;
  return 0;
}

/*****************************************************************************/
/* returns error */
/* send a screen blt order */
/* max size 25 */
int APP_CC
xrdp_orders_screen_blt(struct xrdp_orders* self, int x, int y,
                       int cx, int cy, int srcx, int srcy,
                       int rop, struct xrdp_rect* rect)
{
  int order_flags;
  int vals[12];
  int present;
  char* present_ptr;

  xrdp_orders_check(self, 25);
  self->order_count++;
  order_flags = RDP_ORDER_STANDARD;
  if (self->orders_state.last_order != RDP_ORDER_SCREENBLT)
  {
    order_flags |= RDP_ORDER_CHANGE;
  }
  self->orders_state.last_order = RDP_ORDER_SCREENBLT;
  if (rect != 0)
  {
    /* if clip is present, still check if its needed */
    if (x < rect->left || y < rect->top ||
        x + cx > rect->right || y + cy > rect->bottom)
    {
      order_flags |= RDP_ORDER_BOUNDS;
      if (xrdp_orders_last_bounds(self, rect))
      {
        order_flags |= RDP_ORDER_LASTBOUNDS;
      }
    }
  }
  vals[0] = x;
  vals[1] = self->orders_state.scr_blt_x;
  vals[2] = y;
  vals[3] = self->orders_state.scr_blt_y;
  vals[4] = cx;
  vals[5] = self->orders_state.scr_blt_cx;
  vals[6] = cy;
  vals[7] = self->orders_state.scr_blt_cy;
  vals[8] = srcx;
  vals[9] = self->orders_state.scr_blt_srcx;
  vals[10] = srcy;
  vals[11] = self->orders_state.scr_blt_srcy;
  if (xrdp_orders_send_delta(self, vals, 12))
  {
    order_flags |= RDP_ORDER_DELTA;
  }
  out_uint8(self->out_s, order_flags);
  if (order_flags & RDP_ORDER_CHANGE)
  {
    out_uint8(self->out_s, self->orders_state.last_order);
  }
  present = 0;
  present_ptr = self->out_s->p; /* hold 1 byte present pointer */
  out_uint8s(self->out_s, 1)
  if ((order_flags & RDP_ORDER_BOUNDS) &&
      !(order_flags & RDP_ORDER_LASTBOUNDS))
  {
    xrdp_orders_out_bounds(self, rect);
  }
  if (x != self->orders_state.scr_blt_x)
  {
    present |= 0x01;
    if (order_flags & RDP_ORDER_DELTA)
    {
      out_uint8(self->out_s, x - self->orders_state.scr_blt_x);
    }
    else
    {
      out_uint16_le(self->out_s, x);
    }
    self->orders_state.scr_blt_x = x;
  }
  if (y != self->orders_state.scr_blt_y)
  {
    present |= 0x02;
    if (order_flags & RDP_ORDER_DELTA)
    {
      out_uint8(self->out_s, y - self->orders_state.scr_blt_y);
    }
    else
    {
      out_uint16_le(self->out_s, y);
    }
    self->orders_state.scr_blt_y = y;
  }
  if (cx != self->orders_state.scr_blt_cx)
  {
    present |= 0x04;
    if (order_flags & RDP_ORDER_DELTA)
    {
      out_uint8(self->out_s, cx - self->orders_state.scr_blt_cx);
    }
    else
    {
      out_uint16_le(self->out_s, cx);
    }
    self->orders_state.scr_blt_cx = cx;
  }
  if (cy != self->orders_state.scr_blt_cy)
  {
    present |= 0x08;
    if (order_flags & RDP_ORDER_DELTA)
    {
      out_uint8(self->out_s, cy - self->orders_state.scr_blt_cy);
    }
    else
    {
      out_uint16_le(self->out_s, cy);
    }
    self->orders_state.scr_blt_cy = cy;
  }
  if (rop != self->orders_state.scr_blt_rop)
  {
    present |= 0x10;
    out_uint8(self->out_s, rop);
    self->orders_state.scr_blt_rop = rop;
  }
  if (srcx != self->orders_state.scr_blt_srcx)
  {
    present |= 0x20;
    if (order_flags & RDP_ORDER_DELTA)
    {
      out_uint8(self->out_s, srcx - self->orders_state.scr_blt_srcx);
    }
    else
    {
      out_uint16_le(self->out_s, srcx);
    }
    self->orders_state.scr_blt_srcx = srcx;
  }
  if (srcy != self->orders_state.scr_blt_srcy)
  {
    present |= 0x40;
    if (order_flags & RDP_ORDER_DELTA)
    {
      out_uint8(self->out_s, srcy - self->orders_state.scr_blt_srcy)
    }
    else
    {
      out_uint16_le(self->out_s, srcy)
    }
    self->orders_state.scr_blt_srcy = srcy;
  }
  present_ptr[0] = present;
  return 0;
}

/*****************************************************************************/
/* returns error */
/* send a pat blt order */
/* max size 39 */
int APP_CC
xrdp_orders_pat_blt(struct xrdp_orders* self, int x, int y,
                    int cx, int cy, int rop, int bg_color,
                    int fg_color, struct xrdp_brush* brush,
                    struct xrdp_rect* rect)
{
  int order_flags;
  int vals[8];
  int present;
  char* present_ptr;
  struct xrdp_brush blank_brush;

  xrdp_orders_check(self, 39);
  self->order_count++;
  order_flags = RDP_ORDER_STANDARD;
  if (self->orders_state.last_order != RDP_ORDER_PATBLT)
  {
    order_flags |= RDP_ORDER_CHANGE;
  }
  self->orders_state.last_order = RDP_ORDER_PATBLT;
  if (rect != 0)
  {
    /* if clip is present, still check if its needed */
    if (x < rect->left || y < rect->top ||
        x + cx > rect->right || y + cy > rect->bottom)
    {
      order_flags |= RDP_ORDER_BOUNDS;
      if (xrdp_orders_last_bounds(self, rect))
      {
        order_flags |= RDP_ORDER_LASTBOUNDS;
      }
    }
  }
  vals[0] = x;
  vals[1] = self->orders_state.pat_blt_x;
  vals[2] = y;
  vals[3] = self->orders_state.pat_blt_y;
  vals[4] = cx;
  vals[5] = self->orders_state.pat_blt_cx;
  vals[6] = cy;
  vals[7] = self->orders_state.pat_blt_cy;
  if (xrdp_orders_send_delta(self, vals, 8))
  {
    order_flags |= RDP_ORDER_DELTA;
  }
  out_uint8(self->out_s, order_flags);
  if (order_flags & RDP_ORDER_CHANGE)
  {
    out_uint8(self->out_s, self->orders_state.last_order);
  }
  present = 0;
  present_ptr = self->out_s->p; /* hold 2 byte present pointer, todo */
  out_uint8s(self->out_s, 2)    /* this can be smaller, */
                                /* see RDP_ORDER_SMALL and RDP_ORDER_TINY */
  if ((order_flags & RDP_ORDER_BOUNDS) &&
      !(order_flags & RDP_ORDER_LASTBOUNDS))
  {
    xrdp_orders_out_bounds(self, rect);
  }
  if (x != self->orders_state.pat_blt_x)
  {
    present |= 0x0001;
    if (order_flags & RDP_ORDER_DELTA)
    {
      out_uint8(self->out_s, x - self->orders_state.pat_blt_x);
    }
    else
    {
      out_uint16_le(self->out_s, x);
    }
    self->orders_state.pat_blt_x = x;
  }
  if (y != self->orders_state.pat_blt_y)
  {
    present |= 0x0002;
    if (order_flags & RDP_ORDER_DELTA)
    {
      out_uint8(self->out_s, y - self->orders_state.pat_blt_y);
    }
    else
    {
      out_uint16_le(self->out_s, y);
    }
    self->orders_state.pat_blt_y = y;
  }
  if (cx != self->orders_state.pat_blt_cx)
  {
    present |= 0x0004;
    if (order_flags & RDP_ORDER_DELTA)
    {
      out_uint8(self->out_s, cx - self->orders_state.pat_blt_cx);
    }
    else
    {
      out_uint16_le(self->out_s, cx);
    }
    self->orders_state.pat_blt_cx = cx;
  }
  if (cy != self->orders_state.pat_blt_cy)
  {
    present |= 0x0008;
    if (order_flags & RDP_ORDER_DELTA)
    {
      out_uint8(self->out_s, cy - self->orders_state.pat_blt_cy);
    }
    else
    {
      out_uint16_le(self->out_s, cy);
    }
    self->orders_state.pat_blt_cy = cy;
  }
  if (rop != self->orders_state.pat_blt_rop)
  {
    present |= 0x0010;
    /* PATCOPY PATPAINT PATINVERT DSTINVERT BLACKNESS WHITENESS */
    out_uint8(self->out_s, rop);
    self->orders_state.pat_blt_rop = rop;
  }
  if (bg_color != self->orders_state.pat_blt_bg_color)
  {
    present |= 0x0020;
    out_uint8(self->out_s, bg_color);
    out_uint8(self->out_s, bg_color >> 8);
    out_uint8(self->out_s, bg_color >> 16);
    self->orders_state.pat_blt_bg_color = bg_color;
  }
  if (fg_color != self->orders_state.pat_blt_fg_color)
  {
    present |= 0x0040;
    out_uint8(self->out_s, fg_color);
    out_uint8(self->out_s, fg_color >> 8);
    out_uint8(self->out_s, fg_color >> 16);
    self->orders_state.pat_blt_fg_color = fg_color;
  }
  if (brush == 0) /* if nil use blank one */
  {               /* todo can we just set style to zero */
    g_memset(&blank_brush, 0, sizeof(struct xrdp_brush));
    brush = &blank_brush;
  }
  if (brush->x_orgin != self->orders_state.pat_blt_brush.x_orgin)
  {
    present |= 0x0080;
    out_uint8(self->out_s, brush->x_orgin);
    self->orders_state.pat_blt_brush.x_orgin = brush->x_orgin;
  }
  if (brush->y_orgin != self->orders_state.pat_blt_brush.y_orgin)
  {
    present |= 0x0100;
    out_uint8(self->out_s, brush->y_orgin);
    self->orders_state.pat_blt_brush.y_orgin = brush->y_orgin;
  }
  if (brush->style != self->orders_state.pat_blt_brush.style)
  {
    present |= 0x0200;
    out_uint8(self->out_s, brush->style);
    self->orders_state.pat_blt_brush.style = brush->style;
  }
  if (brush->pattern[0] != self->orders_state.pat_blt_brush.pattern[0])
  {
    present |= 0x0400;
    out_uint8(self->out_s, brush->pattern[0]);
    self->orders_state.pat_blt_brush.pattern[0] = brush->pattern[0];
  }
  if (g_memcmp(brush->pattern + 1,
               self->orders_state.pat_blt_brush.pattern + 1, 7) != 0)
  {
    present |= 0x0800;
    out_uint8a(self->out_s, brush->pattern + 1, 7);
    g_memcpy(self->orders_state.pat_blt_brush.pattern + 1,
             brush->pattern + 1, 7);
  }
  present_ptr[0] = present;
  present_ptr[1] = present >> 8;
  return 0;
}

/*****************************************************************************/
/* returns error */
/* send a dest blt order */
/* max size 21 */
int APP_CC
xrdp_orders_dest_blt(struct xrdp_orders* self, int x, int y,
                     int cx, int cy, int rop,
                     struct xrdp_rect* rect)
{
  int order_flags;
  int vals[8];
  int present;
  char* present_ptr;

  xrdp_orders_check(self, 21);
  self->order_count++;
  order_flags = RDP_ORDER_STANDARD;
  if (self->orders_state.last_order != RDP_ORDER_DESTBLT)
  {
    order_flags |= RDP_ORDER_CHANGE;
  }
  self->orders_state.last_order = RDP_ORDER_DESTBLT;
  if (rect != 0)
  {
    /* if clip is present, still check if its needed */
    if (x < rect->left || y < rect->top ||
        x + cx > rect->right || y + cy > rect->bottom)
    {
      order_flags |= RDP_ORDER_BOUNDS;
      if (xrdp_orders_last_bounds(self, rect))
      {
        order_flags |= RDP_ORDER_LASTBOUNDS;
      }
    }
  }
  vals[0] = x;
  vals[1] = self->orders_state.dest_blt_x;
  vals[2] = y;
  vals[3] = self->orders_state.dest_blt_y;
  vals[4] = cx;
  vals[5] = self->orders_state.dest_blt_cx;
  vals[6] = cy;
  vals[7] = self->orders_state.dest_blt_cy;
  if (xrdp_orders_send_delta(self, vals, 8))
  {
    order_flags |= RDP_ORDER_DELTA;
  }
  out_uint8(self->out_s, order_flags);
  if (order_flags & RDP_ORDER_CHANGE)
  {
    out_uint8(self->out_s, self->orders_state.last_order)
  }
  present = 0;
  present_ptr = self->out_s->p; /* hold 1 byte present pointer */
  out_uint8s(self->out_s, 1)
  if ((order_flags & RDP_ORDER_BOUNDS) &&
      !(order_flags & RDP_ORDER_LASTBOUNDS))
  {
    xrdp_orders_out_bounds(self, rect);
  }
  if (x != self->orders_state.dest_blt_x)
  {
    present |= 0x01;
    if (order_flags & RDP_ORDER_DELTA)
    {
      out_uint8(self->out_s, x - self->orders_state.dest_blt_x);
    }
    else
    {
      out_uint16_le(self->out_s, x);
    }
    self->orders_state.dest_blt_x = x;
  }
  if (y != self->orders_state.dest_blt_y)
  {
    present |= 0x02;
    if (order_flags & RDP_ORDER_DELTA)
    {
      out_uint8(self->out_s, y - self->orders_state.dest_blt_y);
    }
    else
    {
      out_uint16_le(self->out_s, y);
    }
    self->orders_state.dest_blt_y = y;
  }
  if (cx != self->orders_state.dest_blt_cx)
  {
    present |= 0x04;
    if (order_flags & RDP_ORDER_DELTA)
    {
      out_uint8(self->out_s, cx - self->orders_state.dest_blt_cx);
    }
    else
    {
      out_uint16_le(self->out_s, cx);
    }
    self->orders_state.dest_blt_cx = cx;
  }
  if (cy != self->orders_state.dest_blt_cy)
  {
    present |= 0x08;
    if (order_flags & RDP_ORDER_DELTA)
    {
      out_uint8(self->out_s, cy - self->orders_state.dest_blt_cy);
    }
    else
    {
      out_uint16_le(self->out_s, cy);
    }
    self->orders_state.dest_blt_cy = cy;
  }
  if (rop != self->orders_state.dest_blt_rop)
  {
    present |= 0x10;
    out_uint8(self->out_s, rop);
    self->orders_state.dest_blt_rop = rop;
  }
  present_ptr[0] = present;
  return 0;
}

/*****************************************************************************/
/* returns error */
/* send a line order */
/* max size 32 */
int APP_CC
xrdp_orders_line(struct xrdp_orders* self, int mix_mode,
                 int startx, int starty,
                 int endx, int endy, int rop, int bg_color,
                 struct xrdp_pen* pen,
                 struct xrdp_rect* rect)
{
  int order_flags;
  int vals[8];
  int present;
  char* present_ptr;
  struct xrdp_pen blank_pen;

  xrdp_orders_check(self, 32);
  self->order_count++;
  order_flags = RDP_ORDER_STANDARD;
  if (self->orders_state.last_order != RDP_ORDER_LINE)
  {
    order_flags |= RDP_ORDER_CHANGE;
  }
  self->orders_state.last_order = RDP_ORDER_LINE;
  if (rect != 0)
  {
    /* if clip is present, still check if its needed */
    if (MIN(endx, startx) < rect->left ||
        MIN(endy, starty) < rect->top ||
        MAX(endx, startx) >= rect->right ||
        MAX(endy, starty) >= rect->bottom)
    {
      order_flags |= RDP_ORDER_BOUNDS;
      if (xrdp_orders_last_bounds(self, rect))
      {
        order_flags |= RDP_ORDER_LASTBOUNDS;
      }
    }
  }
  vals[0] = startx;
  vals[1] = self->orders_state.line_startx;
  vals[2] = starty;
  vals[3] = self->orders_state.line_starty;
  vals[4] = endx;
  vals[5] = self->orders_state.line_endx;
  vals[6] = endy;
  vals[7] = self->orders_state.line_endy;
  if (xrdp_orders_send_delta(self, vals, 8))
  {
    order_flags |= RDP_ORDER_DELTA;
  }
  out_uint8(self->out_s, order_flags);
  if (order_flags & RDP_ORDER_CHANGE)
  {
    out_uint8(self->out_s, self->orders_state.last_order);
  }
  present = 0;
  present_ptr = self->out_s->p; /* hold 2 byte present pointer */
  out_uint8s(self->out_s, 2)
  if ((order_flags & RDP_ORDER_BOUNDS) &&
      !(order_flags & RDP_ORDER_LASTBOUNDS))
  {
    xrdp_orders_out_bounds(self, rect);
  }
  if (mix_mode != self->orders_state.line_mix_mode)
  {
    present |= 0x0001;
    out_uint16_le(self->out_s, mix_mode)
    self->orders_state.line_mix_mode = mix_mode;
  }
  if (startx != self->orders_state.line_startx)
  {
    present |= 0x0002;
    if (order_flags & RDP_ORDER_DELTA)
    {
      out_uint8(self->out_s, startx - self->orders_state.line_startx);
    }
    else
    {
      out_uint16_le(self->out_s, startx);
    }
    self->orders_state.line_startx = startx;
  }
  if (starty != self->orders_state.line_starty)
  {
    present |= 0x0004;
    if (order_flags & RDP_ORDER_DELTA)
    {
      out_uint8(self->out_s, starty - self->orders_state.line_starty);
    }
    else
    {
      out_uint16_le(self->out_s, starty);
    }
    self->orders_state.line_starty = starty;
  }
  if (endx != self->orders_state.line_endx)
  {
    present |= 0x0008;
    if (order_flags & RDP_ORDER_DELTA)
    {
      out_uint8(self->out_s, endx - self->orders_state.line_endx);
    }
    else
    {
      out_uint16_le(self->out_s, endx);
    }
    self->orders_state.line_endx = endx;
  }
  if (endy != self->orders_state.line_endy)
  {
    present |= 0x0010;
    if (order_flags & RDP_ORDER_DELTA)
    {
      out_uint8(self->out_s, endy - self->orders_state.line_endy);
    }
    else
    {
      out_uint16_le(self->out_s, endy);
    }
    self->orders_state.line_endy = endy;
  }
  if (bg_color != self->orders_state.line_bg_color)
  {
    present |= 0x0020;
    out_uint8(self->out_s, bg_color)
    out_uint8(self->out_s, bg_color >> 8)
    out_uint8(self->out_s, bg_color >> 16)
    self->orders_state.line_bg_color = bg_color;
  }
  if (rop != self->orders_state.line_rop)
  {
    present |= 0x0040;
    out_uint8(self->out_s, rop)
    self->orders_state.line_rop = rop;
  }
  if (pen == 0)
  {
    g_memset(&blank_pen, 0, sizeof(struct xrdp_pen));
    pen = &blank_pen;
  }
  if (pen->style != self->orders_state.line_pen.style)
  {
    present |= 0x0080;
    out_uint8(self->out_s, pen->style)
    self->orders_state.line_pen.style = pen->style;
  }
  if (pen->width != self->orders_state.line_pen.width)
  {
    present |= 0x0100;
    out_uint8(self->out_s, pen->width)
    self->orders_state.line_pen.width = pen->width;
  }
  if (pen->color != self->orders_state.line_pen.color)
  {
    present |= 0x0200;
    out_uint8(self->out_s, pen->color)
    out_uint8(self->out_s, pen->color >> 8)
    out_uint8(self->out_s, pen->color >> 16)
    self->orders_state.line_pen.color = pen->color;
  }
  present_ptr[0] = present;
  present_ptr[1] = present >> 8;
  return 0;
}

/*****************************************************************************/
/* returns error */
/* send a mem blt order */
/* max size  30 */
int APP_CC
xrdp_orders_mem_blt(struct xrdp_orders* self, int cache_id,
                    int color_table, int x, int y, int cx, int cy,
                    int rop, int srcx, int srcy,
                    int cache_idx, struct xrdp_rect* rect)
{
  int order_flags;
  int vals[12];
  int present;
  char* present_ptr;

  xrdp_orders_check(self, 30);
  self->order_count++;
  order_flags = RDP_ORDER_STANDARD;
  if (self->orders_state.last_order != RDP_ORDER_MEMBLT)
  {
    order_flags |= RDP_ORDER_CHANGE;
  }
  self->orders_state.last_order = RDP_ORDER_MEMBLT;
  if (rect != 0)
  {
    /* if clip is present, still check if its needed */
    if (x < rect->left || y < rect->top ||
        x + cx > rect->right || y + cy > rect->bottom)
    {
      order_flags |= RDP_ORDER_BOUNDS;
      if (xrdp_orders_last_bounds(self, rect))
      {
        order_flags |= RDP_ORDER_LASTBOUNDS;
      }
    }
  }
  vals[0] = x;
  vals[1] = self->orders_state.mem_blt_x;
  vals[2] = y;
  vals[3] = self->orders_state.mem_blt_y;
  vals[4] = cx;
  vals[5] = self->orders_state.mem_blt_cx;
  vals[6] = cy;
  vals[7] = self->orders_state.mem_blt_cy;
  vals[8] = srcx;
  vals[9] = self->orders_state.mem_blt_srcx;
  vals[10] = srcy;
  vals[11] = self->orders_state.mem_blt_srcy;
  if (xrdp_orders_send_delta(self, vals, 12))
  {
    order_flags |= RDP_ORDER_DELTA;
  }
  out_uint8(self->out_s, order_flags);
  if (order_flags & RDP_ORDER_CHANGE)
  {
    out_uint8(self->out_s, self->orders_state.last_order)
  }
  present = 0;
  present_ptr = self->out_s->p; /* hold 2 byte present pointer, todo */
  out_uint8s(self->out_s, 2)    /* this can be smaller, */
                                /* see RDP_ORDER_SMALL and RDP_ORDER_TINY */
  if ((order_flags & RDP_ORDER_BOUNDS) &&
      !(order_flags & RDP_ORDER_LASTBOUNDS))
  {
    xrdp_orders_out_bounds(self, rect);
  }
  if (cache_id != self->orders_state.mem_blt_cache_id ||
      color_table != self->orders_state.mem_blt_color_table)
  {
    present |= 0x0001;
    out_uint8(self->out_s, cache_id);
    out_uint8(self->out_s, color_table);
    self->orders_state.mem_blt_cache_id = cache_id;
    self->orders_state.mem_blt_color_table = color_table;
  }
  if (x != self->orders_state.mem_blt_x)
  {
    present |= 0x0002;
    if (order_flags & RDP_ORDER_DELTA)
    {
      out_uint8(self->out_s, x - self->orders_state.mem_blt_x);
    }
    else
    {
      out_uint16_le(self->out_s, x);
    }
    self->orders_state.mem_blt_x = x;
  }
  if (y != self->orders_state.mem_blt_y)
  {
    present |= 0x0004;
    if (order_flags & RDP_ORDER_DELTA)
    {
      out_uint8(self->out_s, y - self->orders_state.mem_blt_y);
    }
    else
    {
      out_uint16_le(self->out_s, y);
    }
    self->orders_state.mem_blt_y = y;
  }
  if (cx != self->orders_state.mem_blt_cx)
  {
    present |= 0x0008;
    if (order_flags & RDP_ORDER_DELTA)
    {
      out_uint8(self->out_s, cx - self->orders_state.mem_blt_cx);
    }
    else
    {
      out_uint16_le(self->out_s, cx);
    }
    self->orders_state.mem_blt_cx = cx;
  }
  if (cy != self->orders_state.mem_blt_cy)
  {
    present |= 0x0010;
    if (order_flags & RDP_ORDER_DELTA)
    {
      out_uint8(self->out_s, cy - self->orders_state.mem_blt_cy);
    }
    else
    {
      out_uint16_le(self->out_s, cy);
    }
    self->orders_state.mem_blt_cy = cy;
  }
  if (rop != self->orders_state.mem_blt_rop)
  {
    present |= 0x0020;
    out_uint8(self->out_s, rop);
    self->orders_state.mem_blt_rop = rop;
  }
  if (srcx != self->orders_state.mem_blt_srcx)
  {
    present |= 0x0040;
    if (order_flags & RDP_ORDER_DELTA)
    {
      out_uint8(self->out_s, srcx - self->orders_state.mem_blt_srcx);
    }
    else
    {
      out_uint16_le(self->out_s, srcx);
    }
    self->orders_state.mem_blt_srcx = srcx;
  }
  if (srcy != self->orders_state.mem_blt_srcy)
  {
    present |= 0x0080;
    if (order_flags & RDP_ORDER_DELTA)
    {
      out_uint8(self->out_s, srcy - self->orders_state.mem_blt_srcy);
    }
    else
    {
      out_uint16_le(self->out_s, srcy);
    }
    self->orders_state.mem_blt_srcy = srcy;
  }
  if (cache_idx != self->orders_state.mem_blt_cache_idx)
  {
    present |= 0x0100;
    out_uint16_le(self->out_s, cache_idx);
    self->orders_state.mem_blt_cache_idx = cache_idx;
  }
  present_ptr[0] = present;
  present_ptr[1] = present >> 8;
  return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
xrdp_orders_text(struct xrdp_orders* self,
                 int font, int flags, int mixmode,
                 int fg_color, int bg_color,
                 int clip_left, int clip_top,
                 int clip_right, int clip_bottom,
                 int box_left, int box_top,
                 int box_right, int box_bottom,
                 int x, int y, char* data, int data_len,
                 struct xrdp_rect* rect)
{
  int order_flags;
  int present;
  char* present_ptr;

  xrdp_orders_check(self, 100);
  self->order_count++;
  order_flags = RDP_ORDER_STANDARD;
  if (self->orders_state.last_order != RDP_ORDER_TEXT2)
  {
    order_flags |= RDP_ORDER_CHANGE;
  }
  self->orders_state.last_order = RDP_ORDER_TEXT2;
  if (rect != 0)
  {
    /* if clip is present, still check if its needed */
    if ((box_right - box_left > 1 &&
              (box_left < rect->left ||
               box_top < rect->top ||
               box_right > rect->right ||
               box_bottom > rect->bottom)) ||
        (clip_left < rect->left ||
         clip_top < rect->top ||
         clip_right > rect->right ||
         clip_bottom > rect->bottom))
    {
      order_flags |= RDP_ORDER_BOUNDS;
      if (xrdp_orders_last_bounds(self, rect))
      {
        order_flags |= RDP_ORDER_LASTBOUNDS;
      }
    }
  }
  out_uint8(self->out_s, order_flags);
  if (order_flags & RDP_ORDER_CHANGE)
  {
    out_uint8(self->out_s, self->orders_state.last_order);
  }
  present = 0;
  present_ptr = self->out_s->p; /* hold 3 byte present pointer, todo */
  out_uint8s(self->out_s, 3)    /* this can be smaller, */
                                /* see RDP_ORDER_SMALL and RDP_ORDER_TINY */
  if ((order_flags & RDP_ORDER_BOUNDS) &&
      !(order_flags & RDP_ORDER_LASTBOUNDS))
  {
    xrdp_orders_out_bounds(self, rect);
  }
  if (font != self->orders_state.text_font)
  {
    present |= 0x000001;
    out_uint8(self->out_s, font);
    self->orders_state.text_font = font;
  }
  if (flags != self->orders_state.text_flags)
  {
    present |= 0x000002;
    out_uint8(self->out_s, flags);
    self->orders_state.text_flags = flags;
  }
  /* unknown */
  if (mixmode != self->orders_state.text_mixmode)
  {
    present |= 0x000008;
    out_uint8(self->out_s, mixmode);
    self->orders_state.text_mixmode = mixmode;
  }
  if (fg_color != self->orders_state.text_fg_color)
  {
    present |= 0x000010;
    out_uint8(self->out_s, fg_color)
    out_uint8(self->out_s, fg_color >> 8)
    out_uint8(self->out_s, fg_color >> 16)
    self->orders_state.text_fg_color = fg_color;
  }
  if (bg_color != self->orders_state.text_bg_color)
  {
    present |= 0x000020;
    out_uint8(self->out_s, bg_color)
    out_uint8(self->out_s, bg_color >> 8)
    out_uint8(self->out_s, bg_color >> 16)
    self->orders_state.text_bg_color = bg_color;
  }
  if (clip_left != self->orders_state.text_clip_left)
  {
    present |= 0x000040;
    out_uint16_le(self->out_s, clip_left);
    self->orders_state.text_clip_left = clip_left;
  }
  if (clip_top != self->orders_state.text_clip_top)
  {
    present |= 0x000080;
    out_uint16_le(self->out_s, clip_top);
    self->orders_state.text_clip_top = clip_top;
  }
  if (clip_right != self->orders_state.text_clip_right)
  {
    present |= 0x000100;
    out_uint16_le(self->out_s, clip_right);
    self->orders_state.text_clip_right = clip_right;
  }
  if (clip_bottom != self->orders_state.text_clip_bottom)
  {
    present |= 0x000200;
    out_uint16_le(self->out_s, clip_bottom);
    self->orders_state.text_clip_bottom = clip_bottom;
  }
  if (box_left != self->orders_state.text_box_left)
  {
    present |= 0x000400;
    out_uint16_le(self->out_s, box_left);
    self->orders_state.text_box_left = box_left;
  }
  if (box_top != self->orders_state.text_box_top)
  {
    present |= 0x000800;
    out_uint16_le(self->out_s, box_top);
    self->orders_state.text_box_top = box_top;
  }
  if (box_right != self->orders_state.text_box_right)
  {
    present |= 0x001000;
    out_uint16_le(self->out_s, box_right);
    self->orders_state.text_box_right = box_right;
  }
  if (box_bottom != self->orders_state.text_box_bottom)
  {
    present |= 0x002000;
    out_uint16_le(self->out_s, box_bottom);
    self->orders_state.text_box_bottom = box_bottom;
  }
  if (x != self->orders_state.text_x)
  {
    present |= 0x080000;
    out_uint16_le(self->out_s, x);
    self->orders_state.text_x = x;
  }
  if (y != self->orders_state.text_y)
  {
    present |= 0x100000;
    out_uint16_le(self->out_s, y);
    self->orders_state.text_y = y;
  }
  {
    /* always send text */
    present |= 0x200000;
    out_uint8(self->out_s, data_len);
    out_uint8a(self->out_s, data, data_len);
  }
  /* send 3 byte present */
  present_ptr[0] = present;
  present_ptr[1] = present >> 8;
  present_ptr[2] = present >> 16;
  return 0;
}

/*****************************************************************************/
/* returns error */
/* when a palette gets sent, send the main palette too */
int APP_CC
xrdp_orders_send_palette(struct xrdp_orders* self, int* palette,
                         int cache_id)
{
  int order_flags;
  int len;
  int i;

  xrdp_orders_check(self, 2000);
  self->order_count++;
  order_flags = RDP_ORDER_STANDARD | RDP_ORDER_SECONDARY;
  out_uint8(self->out_s, order_flags);
  len = 1027 - 7; /* length after type minus 7 */
  out_uint16_le(self->out_s, len);
  out_uint16_le(self->out_s, 0); /* flags */
  out_uint8(self->out_s, RDP_ORDER_COLCACHE); /* type */
  out_uint8(self->out_s, cache_id);
  out_uint16_le(self->out_s, 256); /* num colors */
  for (i = 0; i < 256; i++)
  {
    out_uint8(self->out_s, palette[i]);
    out_uint8(self->out_s, palette[i] >> 8);
    out_uint8(self->out_s, palette[i] >> 16);
    out_uint8(self->out_s, 0);
  }
  return 0;
}

/*****************************************************************************/
/* returns error */
/* max size width * height * Bpp + 16 */
int APP_CC
xrdp_orders_send_raw_bitmap(struct xrdp_orders* self,
                            int width, int height, int bpp, char* data,
                            int cache_id, int cache_idx)
{
  int order_flags;
  int len;
  int bufsize;
  int Bpp;
  int i;
  int j;
  int pixel;
  int e;

  if (width > 64)
  {
    g_writeln("error, width > 64");
    return 1;
  }
  if (height > 64)
  {
    g_writeln("error, height > 64");
    return 1;
  }
  e = width % 4;
  if (e != 0)
  {
    e = 4 - e;
  }
  Bpp = (bpp + 7) / 8;
  bufsize = (width + e) * height * Bpp;
  xrdp_orders_check(self, bufsize + 16);
  self->order_count++;
  order_flags = RDP_ORDER_STANDARD | RDP_ORDER_SECONDARY;
  out_uint8(self->out_s, order_flags);
  len = (bufsize + 9) - 7; /* length after type minus 7 */
  out_uint16_le(self->out_s, len);
  out_uint16_le(self->out_s, 8); /* flags */
  out_uint8(self->out_s, RDP_ORDER_RAW_BMPCACHE); /* type */
  out_uint8(self->out_s, cache_id);
  out_uint8s(self->out_s, 1); /* pad */
  out_uint8(self->out_s, width + e);
  out_uint8(self->out_s, height);
  out_uint8(self->out_s, bpp);
  out_uint16_le(self->out_s, bufsize);
  out_uint16_le(self->out_s, cache_idx);
  for (i = height - 1; i >= 0; i--)
  {
    for (j = 0; j < width; j++)
    {
      if (Bpp == 3)
      {
        pixel = GETPIXEL32(data, j, i, width);
        out_uint8(self->out_s, pixel >> 16);
        out_uint8(self->out_s, pixel >> 8);
        out_uint8(self->out_s, pixel);
      }
      else if (Bpp == 2)
      {
        pixel = GETPIXEL16(data, j, i, width);
        out_uint8(self->out_s, pixel);
        out_uint8(self->out_s, pixel >> 8);
      }
      else if (Bpp == 1)
      {
        pixel = GETPIXEL8(data, j, i, width);
        out_uint8(self->out_s, pixel);
      }
    }
    for (j = 0; j < e; j++)
    {
      out_uint8s(self->out_s, Bpp);
    }
  }
  return 0;
}

/*****************************************************************************/
/* returns error */
/* max size width * height * Bpp + 16 */
int APP_CC
xrdp_orders_send_bitmap(struct xrdp_orders* self,
                        int width, int height, int bpp, char* data,
                        int cache_id, int cache_idx)
{
  int order_flags;
  int len;
  int bufsize;
  int Bpp;
  int i;
  int lines_sending;
  int e;
  struct stream* s;
  struct stream* temp_s;
  char* p;

  if (width > 64)
  {
    g_writeln("error, width > 64");
    return 1;
  }
  if (height > 64)
  {
    g_writeln("error, height > 64");
    return 1;
  }
  e = width % 4;
  if (e != 0)
  {
    e = 4 - e;
  }
  make_stream(s);
  init_stream(s, 16384);
  make_stream(temp_s);
  init_stream(temp_s, 16384);
  p = s->p;
  i = height;
  lines_sending = xrdp_bitmap_compress(data, width, height, s, bpp, 16384,
                                       i - 1, temp_s, e);
  if (lines_sending != height)
  {
    free_stream(s);
    free_stream(temp_s);
    g_writeln("error in xrdp_orders_send_bitmap, lines_sending(%d) != \
height(%d)", lines_sending, height);
    return 1;
  }
  bufsize = s->p - p;
  Bpp = (bpp + 7) / 8;
  xrdp_orders_check(self, bufsize + 16);
  self->order_count++;
  order_flags = RDP_ORDER_STANDARD | RDP_ORDER_SECONDARY;
  out_uint8(self->out_s, order_flags);
  if (self->rdp_layer->client_info.op2)
  {
    len = (bufsize + 9) - 7; /* length after type minus 7 */
    out_uint16_le(self->out_s, len);
    out_uint16_le(self->out_s, 1024); /* flags */
  }
  else
  {
    len = (bufsize + 9 + 8) - 7; /* length after type minus 7 */
    out_uint16_le(self->out_s, len);
    out_uint16_le(self->out_s, 8); /* flags */
  }
  out_uint8(self->out_s, RDP_ORDER_BMPCACHE); /* type */
  out_uint8(self->out_s, cache_id);
  out_uint8s(self->out_s, 1); /* pad */
  out_uint8(self->out_s, width + e);
  out_uint8(self->out_s, height);
  out_uint8(self->out_s, bpp);
  out_uint16_le(self->out_s, bufsize/* + 8*/);
  out_uint16_le(self->out_s, cache_idx);
  if (!self->rdp_layer->client_info.op2)
  {
    out_uint8s(self->out_s, 2); /* pad */
    out_uint16_le(self->out_s, bufsize);
    out_uint16_le(self->out_s, (width + e) * Bpp); /* line size */
    out_uint16_le(self->out_s, (width + e) *
                                Bpp * height); /* final size */
  }
  out_uint8a(self->out_s, s->data, bufsize);
  free_stream(s);
  free_stream(temp_s);
  return 0;
}

/*****************************************************************************/
/* returns error */
/* max size datasize + 18*/
/* todo, only sends one for now */
int APP_CC
xrdp_orders_send_font(struct xrdp_orders* self,
                      struct xrdp_font_char* font_char,
                      int font_index, int char_index)
{
  int order_flags;
  int datasize;
  int len;

  datasize = FONT_DATASIZE(font_char);
  xrdp_orders_check(self, datasize + 18);
  self->order_count++;
  order_flags = RDP_ORDER_STANDARD | RDP_ORDER_SECONDARY;
  out_uint8(self->out_s, order_flags);
  len = (datasize + 12) - 7; /* length after type minus 7 */
  out_uint16_le(self->out_s, len);
  out_uint16_le(self->out_s, 8); /* flags */
  out_uint8(self->out_s, RDP_ORDER_FONTCACHE); /* type */
  out_uint8(self->out_s, font_index);
  out_uint8(self->out_s, 1); /* num of chars */
  out_uint16_le(self->out_s, char_index);
  out_uint16_le(self->out_s, font_char->offset);
  out_uint16_le(self->out_s, font_char->baseline);
  out_uint16_le(self->out_s, font_char->width);
  out_uint16_le(self->out_s, font_char->height);
  out_uint8a(self->out_s, font_char->data, datasize);
  return 0;
}

/*****************************************************************************/
/* returns error */
/* max size width * height * Bpp + 14 */
int APP_CC
xrdp_orders_send_raw_bitmap2(struct xrdp_orders* self,
                             int width, int height, int bpp, char* data,
                             int cache_id, int cache_idx)
{
  int order_flags;
  int len;
  int bufsize;
  int Bpp;
  int i;
  int j;
  int pixel;
  int e;

  if (width > 64)
  {
    g_writeln("error, width > 64");
    return 1;
  }
  if (height > 64)
  {
    g_writeln("error, height > 64");
    return 1;
  }
  e = width % 4;
  if (e != 0)
  {
    e = 4 - e;
  }
  Bpp = (bpp + 7) / 8;
  bufsize = (width + e) * height * Bpp;
  xrdp_orders_check(self, bufsize + 14);
  self->order_count++;
  order_flags = RDP_ORDER_STANDARD | RDP_ORDER_SECONDARY;
  out_uint8(self->out_s, order_flags);
  len = (bufsize + 6) - 7; /* length after type minus 7 */
  out_uint16_le(self->out_s, len);
  i = (((Bpp + 2) << 3) & 0x38) | (cache_id & 7);
  out_uint16_le(self->out_s, i); /* flags */
  out_uint8(self->out_s, RDP_ORDER_RAW_BMPCACHE2); /* type */
  out_uint8(self->out_s, width + e);
  out_uint8(self->out_s, height);
  out_uint16_be(self->out_s, bufsize | 0x4000);
  i = ((cache_idx >> 8) & 0xff) | 0x80;
  out_uint8(self->out_s, i);
  i = cache_idx & 0xff;
  out_uint8(self->out_s, i);
  for (i = height - 1; i >= 0; i--)
  {
    for (j = 0; j < width; j++)
    {
      if (Bpp == 3)
      {
        pixel = GETPIXEL32(data, j, i, width);
        out_uint8(self->out_s, pixel >> 16);
        out_uint8(self->out_s, pixel >> 8);
        out_uint8(self->out_s, pixel);
      }
      else if (Bpp == 2)
      {
        pixel = GETPIXEL16(data, j, i, width);
        out_uint8(self->out_s, pixel);
        out_uint8(self->out_s, pixel >> 8);
      }
      else if (Bpp == 1)
      {
        pixel = GETPIXEL8(data, j, i, width);
        out_uint8(self->out_s, pixel);
      }
    }
    for (j = 0; j < e; j++)
    {
      out_uint8s(self->out_s, Bpp);
    }
  }
  return 0;
}

/*****************************************************************************/
/* returns error */
/* max size width * height * Bpp + 14 */
int APP_CC
xrdp_orders_send_bitmap2(struct xrdp_orders* self,
                         int width, int height, int bpp, char* data,
                         int cache_id, int cache_idx)
{
  int order_flags;
  int len;
  int bufsize;
  int Bpp;
  int i;
  int lines_sending;
  int e;
  struct stream* s;
  struct stream* temp_s;
  char* p;

  if (width > 64)
  {
    g_writeln("error, width > 64");
    return 1;
  }
  if (height > 64)
  {
    g_writeln("error, height > 64");
    return 1;
  }
  e = width % 4;
  if (e != 0)
  {
    e = 4 - e;
  }
  make_stream(s);
  init_stream(s, 16384);
  make_stream(temp_s);
  init_stream(temp_s, 16384);
  p = s->p;
  i = height;
  lines_sending = xrdp_bitmap_compress(data, width, height, s, bpp, 16384,
                                       i - 1, temp_s, e);
  if (lines_sending != height)
  {
    free_stream(s);
    free_stream(temp_s);
    g_writeln("error in xrdp_orders_send_bitmap2, lines_sending(%d) != \
height(%d)", lines_sending, height);
    return 1;
  }
  bufsize = s->p - p;
  Bpp = (bpp + 7) / 8;
  xrdp_orders_check(self, bufsize + 14);
  self->order_count++;
  order_flags = RDP_ORDER_STANDARD | RDP_ORDER_SECONDARY;
  out_uint8(self->out_s, order_flags);
  len = (bufsize + 6) - 7; /* length after type minus 7 */
  out_uint16_le(self->out_s, len);
  i = (((Bpp + 2) << 3) & 0x38) | (cache_id & 7);
  i = i | 0x400;
  out_uint16_le(self->out_s, i); /* flags */
  out_uint8(self->out_s, RDP_ORDER_BMPCACHE2); /* type */
  out_uint8(self->out_s, width + e);
  out_uint8(self->out_s, height);
  out_uint16_be(self->out_s, bufsize | 0x4000);
  i = ((cache_idx >> 8) & 0xff) | 0x80;
  out_uint8(self->out_s, i);
  i = cache_idx & 0xff;
  out_uint8(self->out_s, i);
  out_uint8a(self->out_s, s->data, bufsize);
  free_stream(s);
  free_stream(temp_s);
  return 0;
}
