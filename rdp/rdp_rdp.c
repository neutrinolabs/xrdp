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
   Copyright (C) Jay Sorg 2005-2009

   librdp rdp layer

*/

#include "rdp.h"

/*****************************************************************************/
struct rdp_rdp* APP_CC
rdp_rdp_create(struct mod* owner)
{
  struct rdp_rdp* self;

  self = (struct rdp_rdp*)g_malloc(sizeof(struct rdp_rdp), 1);
  self->mod = owner;
  self->sec_layer = rdp_sec_create(self);
  self->bitmap_compression = 1;
  self->bitmap_cache = 1;
  self->desktop_save = 0;
  self->orders = rdp_orders_create(self);
  self->rec_mode = 0;
  return self;
}

/*****************************************************************************/
void APP_CC
rdp_rdp_delete(struct rdp_rdp* self)
{
  if (self == 0)
  {
    return;
  }
  rdp_orders_delete(self->orders);
  rdp_sec_delete(self->sec_layer);
  if (self->rec_fd != 0)
  {
    g_file_close(self->rec_fd);
    self->rec_fd = 0;
  }
  g_free(self);
}

/******************************************************************************/
/* Initialise an RDP packet */
int APP_CC
rdp_rdp_init(struct rdp_rdp* self, struct stream* s)
{
  if (rdp_sec_init(self->sec_layer, s, SEC_ENCRYPT) != 0)
  {
    return 1;
  }
  s_push_layer(s, rdp_hdr, 6);
  return 0;
}

/******************************************************************************/
/* Send an RDP packet */
int APP_CC
rdp_rdp_send(struct rdp_rdp* self, struct stream* s, int pdu_type)
{
  int len;
  int sec_flags;

  s_pop_layer(s, rdp_hdr);
  len = s->end - s->p;
  out_uint16_le(s, len);
  out_uint16_le(s, pdu_type | 0x10);
  out_uint16_le(s, self->sec_layer->mcs_layer->userid);
  sec_flags = SEC_ENCRYPT;
  if (rdp_sec_send(self->sec_layer, s, sec_flags) != 0)
  {
    return 1;
  }
  return 0;
}

/******************************************************************************/
/* Initialise an RDP data packet */
int APP_CC
rdp_rdp_init_data(struct rdp_rdp* self, struct stream* s)
{
  if (rdp_sec_init(self->sec_layer, s, SEC_ENCRYPT) != 0)
  {
    return 1;
  }
  s_push_layer(s, rdp_hdr, 18);
  return 0;
}

/******************************************************************************/
/* Send an RDP data packet */
int APP_CC
rdp_rdp_send_data(struct rdp_rdp* self, struct stream* s, int pdu_data_type)
{
  int len;
  int sec_flags;

  s_pop_layer(s, rdp_hdr);
  len = s->end - s->p;
  out_uint16_le(s, len);
  out_uint16_le(s, RDP_PDU_DATA | 0x10);
  out_uint16_le(s, self->sec_layer->mcs_layer->userid);
  out_uint32_le(s, self->share_id);
  out_uint8(s, 0);
  out_uint8(s, 1);
  out_uint16_le(s, len - 14);
  out_uint8(s, pdu_data_type);
  out_uint8(s, 0); /* compress type */
  out_uint16_le(s, 0); /* compress len */
  sec_flags = SEC_ENCRYPT;
  if (rdp_sec_send(self->sec_layer, s, sec_flags) != 0)
  {
    return 1;
  }
  return 0;
}

/******************************************************************************/
/* Output general capability set */
static int APP_CC
rdp_rdp_out_general_caps(struct rdp_rdp* self, struct stream* s)
{
  out_uint16_le(s, RDP_CAPSET_GENERAL);
  out_uint16_le(s, RDP_CAPLEN_GENERAL);
  out_uint16_le(s, 1); /* OS major type */
  out_uint16_le(s, 3); /* OS minor type */
  out_uint16_le(s, 0x200); /* Protocol version */
  out_uint16_le(s, 0); /* Pad */
  out_uint16_le(s, 0); /* Compression types */
  out_uint16_le(s, self->use_rdp5 ? 0x40d : 0);
  out_uint16_le(s, 0); /* Update capability */
  out_uint16_le(s, 0); /* Remote unshare capability */
  out_uint16_le(s, 0); /* Compression level */
  out_uint16_le(s, 0); /* Pad */
  return 0;
}

/******************************************************************************/
/* Output bitmap capability set */
static int APP_CC
rdp_rdp_out_bitmap_caps(struct rdp_rdp* self, struct stream* s)
{
  out_uint16_le(s, RDP_CAPSET_BITMAP);
  out_uint16_le(s, RDP_CAPLEN_BITMAP);
  out_uint16_le(s, self->mod->rdp_bpp); /* Preferred BPP */
  out_uint16_le(s, 1); /* Receive 1 BPP */
  out_uint16_le(s, 1); /* Receive 4 BPP */
  out_uint16_le(s, 1); /* Receive 8 BPP */
  out_uint16_le(s, 800); /* Desktop width */
  out_uint16_le(s, 600); /* Desktop height */
  out_uint16_le(s, 0); /* Pad */
  out_uint16_le(s, 1); /* Allow resize */
  out_uint16_le(s, self->bitmap_compression); /* Support compression */
  out_uint16_le(s, 0); /* Unknown */
  out_uint16_le(s, 1); /* Unknown */
  out_uint16_le(s, 0); /* Pad */
  return 0;
}

/******************************************************************************/
/* Output order capability set */
static int APP_CC
rdp_rdp_out_order_caps(struct rdp_rdp* self, struct stream* s)
{
  char order_caps[32];

  g_memset(order_caps, 0, 32);
  order_caps[0] = 1; /* dest blt */
  order_caps[1] = 1; /* pat blt */
  order_caps[2] = 1; /* screen blt */
  order_caps[3] = self->bitmap_cache; /* memblt */
  order_caps[4] = 0; /* triblt */
  order_caps[8] = 1; /* line */
  order_caps[9] = 1; /* line */
  order_caps[10] = 1; /* rect */
  order_caps[11] = self->desktop_save; /* desksave */
  order_caps[13] = 1; /* memblt another above */
  order_caps[14] = 1; /* triblt another above */
  order_caps[20] = self->polygon_ellipse_orders; /* polygon */
  order_caps[21] = self->polygon_ellipse_orders; /* polygon2 */
  order_caps[22] = 0; /* todo polyline */
  order_caps[25] = self->polygon_ellipse_orders; /* ellipse */
  order_caps[26] = self->polygon_ellipse_orders; /* ellipse2 */
  order_caps[27] = 1; /* text2 */
  out_uint16_le(s, RDP_CAPSET_ORDER);
  out_uint16_le(s, RDP_CAPLEN_ORDER);
  out_uint8s(s, 20); /* Terminal desc, pad */
  out_uint16_le(s, 1); /* Cache X granularity */
  out_uint16_le(s, 20); /* Cache Y granularity */
  out_uint16_le(s, 0); /* Pad */
  out_uint16_le(s, 1); /* Max order level */
  out_uint16_le(s, 0x147); /* Number of fonts */
  out_uint16_le(s, 0x2a); /* Capability flags */
  out_uint8p(s, order_caps, 32); /* Orders supported */
  out_uint16_le(s, 0x6a1); /* Text capability flags */
  out_uint8s(s, 6); /* Pad */
  out_uint32_le(s, self->desktop_save * 0x38400); /* Desktop cache size */
  out_uint32_le(s, 0); /* Unknown */
  out_uint32_le(s, 0x4e4); /* Unknown */
  return 0;
}

/******************************************************************************/
/* Output bitmap cache capability set */
static int APP_CC
rdp_rdp_out_bmpcache_caps(struct rdp_rdp* self, struct stream* s)
{
  int Bpp;

  out_uint16_le(s, RDP_CAPSET_BMPCACHE);
  out_uint16_le(s, RDP_CAPLEN_BMPCACHE);
  Bpp = (self->mod->rdp_bpp + 7) / 8;
  out_uint8s(s, 24); /* unused */
  out_uint16_le(s, 0x258); /* entries */
  out_uint16_le(s, 0x100 * Bpp); /* max cell size */
  out_uint16_le(s, 0x12c); /* entries */
  out_uint16_le(s, 0x400 * Bpp); /* max cell size */
  out_uint16_le(s, 0x106); /* entries */
  out_uint16_le(s, 0x1000 * Bpp); /* max cell size */
  return 0;
}

/******************************************************************************/
/* Output control capability set */
static int APP_CC
rdp_rdp_out_control_caps(struct rdp_rdp* self, struct stream* s)
{
  out_uint16_le(s, RDP_CAPSET_CONTROL);
  out_uint16_le(s, RDP_CAPLEN_CONTROL);
  out_uint16_le(s, 0); /* Control capabilities */
  out_uint16_le(s, 0); /* Remote detach */
  out_uint16_le(s, 2); /* Control interest */
  out_uint16_le(s, 2); /* Detach interest */
  return 0;
}

/******************************************************************************/
/* Output activation capability set */
static int APP_CC
rdp_rdp_out_activate_caps(struct rdp_rdp* self, struct stream* s)
{
  out_uint16_le(s, RDP_CAPSET_ACTIVATE);
  out_uint16_le(s, RDP_CAPLEN_ACTIVATE);
  out_uint16_le(s, 0); /* Help key */
  out_uint16_le(s, 0); /* Help index key */
  out_uint16_le(s, 0); /* Extended help key */
  out_uint16_le(s, 0); /* Window activate */
  return 0;
}

/******************************************************************************/
/* Output pointer capability set */
static int APP_CC
rdp_rdp_out_pointer_caps(struct rdp_rdp* self, struct stream* s)
{
  out_uint16_le(s, RDP_CAPSET_POINTER);
  out_uint16_le(s, RDP_CAPLEN_POINTER_MONO);
  out_uint16_le(s, 0); /* Color pointer */
  out_uint16_le(s, 20); /* Cache size */
  return 0;
}

/******************************************************************************/
/* Output share capability set */
static int APP_CC
rdp_rdp_out_share_caps(struct rdp_rdp* self, struct stream* s)
{
  out_uint16_le(s, RDP_CAPSET_SHARE);
  out_uint16_le(s, RDP_CAPLEN_SHARE);
  out_uint16_le(s, 0); /* userid */
  out_uint16_le(s, 0); /* pad */
  return 0;
}

/******************************************************************************/
/* Output color cache capability set */
static int APP_CC
rdp_rdp_out_colcache_caps(struct rdp_rdp* self, struct stream* s)
{
  out_uint16_le(s, RDP_CAPSET_COLCACHE);
  out_uint16_le(s, RDP_CAPLEN_COLCACHE);
  out_uint16_le(s, 6); /* cache size */
  out_uint16_le(s, 0); /* pad */
  return 0;
}

static char caps_0x0d[] = {
0x01, 0x00, 0x00, 0x00, 0x09, 0x04, 0x00, 0x00,
0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00
};

static char caps_0x0c[] = { 0x01, 0x00, 0x00, 0x00 };

static char caps_0x0e[] = { 0x01, 0x00, 0x00, 0x00 };

static char caps_0x10[] = {
0xFE, 0x00, 0x04, 0x00, 0xFE, 0x00, 0x04, 0x00,
0xFE, 0x00, 0x08, 0x00, 0xFE, 0x00, 0x08, 0x00,
0xFE, 0x00, 0x10, 0x00, 0xFE, 0x00, 0x20, 0x00,
0xFE, 0x00, 0x40, 0x00, 0xFE, 0x00, 0x80, 0x00,
0xFE, 0x00, 0x00, 0x01, 0x40, 0x00, 0x00, 0x08,
0x00, 0x01, 0x00, 0x01, 0x02, 0x00, 0x00, 0x00
};

/******************************************************************************/
/* Output unknown capability sets */
static int APP_CC
rdp_rdp_out_unknown_caps(struct rdp_rdp* self, struct stream* s, int id,
                         int length, char* caps)
{
  out_uint16_le(s, id);
  out_uint16_le(s, length);
  out_uint8p(s, caps, length - 4);
  return 0;
}

#define RDP5_FLAG 0x0030

/******************************************************************************/
/* Send a confirm active PDU */
static int APP_CC
rdp_rdp_send_confirm_active(struct rdp_rdp* self, struct stream* s)
{
  int sec_flags;
  int caplen;

  sec_flags = SEC_ENCRYPT;
  //sec_flags = RDP5_FLAG | SEC_ENCRYPT;
  caplen = RDP_CAPLEN_GENERAL + RDP_CAPLEN_BITMAP + RDP_CAPLEN_ORDER +
           RDP_CAPLEN_BMPCACHE + RDP_CAPLEN_COLCACHE +
           RDP_CAPLEN_ACTIVATE + RDP_CAPLEN_CONTROL +
           RDP_CAPLEN_POINTER_MONO + RDP_CAPLEN_SHARE +
           0x58 + 0x08 + 0x08 + 0x34 /* unknown caps */  +
           4 /* w2k fix, why? */ ;
  if (rdp_sec_init(self->sec_layer, s, sec_flags) != 0)
  {
    return 1;
  }
  out_uint16_le(s, 2 + 14 + caplen + sizeof(RDP_SOURCE));
  out_uint16_le(s, (RDP_PDU_CONFIRM_ACTIVE | 0x10)); /* Version 1 */
  out_uint16_le(s, (self->sec_layer->mcs_layer->userid + 1001));
  out_uint32_le(s, self->share_id);
  out_uint16_le(s, 0x3ea); /* userid */
  out_uint16_le(s, sizeof(RDP_SOURCE));
  out_uint16_le(s, caplen);
  out_uint8p(s, RDP_SOURCE, sizeof(RDP_SOURCE));
  out_uint16_le(s, 0xd); /* num_caps */
  out_uint8s(s, 2); /* pad */
  rdp_rdp_out_general_caps(self, s);
  rdp_rdp_out_bitmap_caps(self, s);
  rdp_rdp_out_order_caps(self, s);
  rdp_rdp_out_bmpcache_caps(self, s);
  rdp_rdp_out_colcache_caps(self, s);
  rdp_rdp_out_activate_caps(self, s);
  rdp_rdp_out_control_caps(self, s);
  rdp_rdp_out_pointer_caps(self, s);
  rdp_rdp_out_share_caps(self, s);
  rdp_rdp_out_unknown_caps(self, s, 0x0d, 0x58, caps_0x0d); /* international? */
  rdp_rdp_out_unknown_caps(self, s, 0x0c, 0x08, caps_0x0c);
  rdp_rdp_out_unknown_caps(self, s, 0x0e, 0x08, caps_0x0e);
  rdp_rdp_out_unknown_caps(self, s, 0x10, 0x34, caps_0x10); /* glyph cache? */
  s_mark_end(s);
  if (rdp_sec_send(self->sec_layer, s, sec_flags) != 0)
  {
    return 1;
  }
  return 0;
}

/******************************************************************************/
/* Process a color pointer PDU */
static int APP_CC
rdp_rdp_process_color_pointer_pdu(struct rdp_rdp* self, struct stream* s)
{
  int cache_idx;
  int dlen;
  int mlen;
  struct rdp_cursor* cursor;

  in_uint16_le(s, cache_idx);
  if (cache_idx >= sizeof(self->cursors) / sizeof(cursor))
  {
    return 1;
  }
  cursor = self->cursors + cache_idx;
  in_uint16_le(s, cursor->x);
  in_uint16_le(s, cursor->y);
  in_uint16_le(s, cursor->width);
  in_uint16_le(s, cursor->height);
  in_uint16_le(s, mlen); /* mask length */
  in_uint16_le(s, dlen); /* data length */
  if ((mlen > sizeof(cursor->mask)) || (dlen > sizeof(cursor->data)))
  {
    return 1;
  }
  in_uint8a(s, cursor->data, dlen);
  in_uint8a(s, cursor->mask, mlen);
  self->mod->server_set_cursor(self->mod, cursor->x, cursor->y,
                               cursor->data, cursor->mask);
  return 0;
}

/******************************************************************************/
/* Process a cached pointer PDU */
static int APP_CC
rdp_rdp_process_cached_pointer_pdu(struct rdp_rdp* self, struct stream* s)
{
  int cache_idx;
  struct rdp_cursor* cursor;

  in_uint16_le(s, cache_idx);
  if (cache_idx >= sizeof(self->cursors) / sizeof(cursor))
  {
    return 1;
  }
  cursor = self->cursors + cache_idx;
  self->mod->server_set_cursor(self->mod, cursor->x, cursor->y,
                               cursor->data, cursor->mask);
  return 0;
}

/******************************************************************************/
/* Process a system pointer PDU */
static int APP_CC
rdp_rdp_process_system_pointer_pdu(struct rdp_rdp* self, struct stream* s)
{
  int system_pointer_type;
  struct rdp_cursor* cursor;

  in_uint16_le(s, system_pointer_type);
  switch (system_pointer_type)
  {
    case RDP_NULL_POINTER:
      cursor = (struct rdp_cursor*)g_malloc(sizeof(struct rdp_cursor), 1);
      g_memset(cursor->mask, 0xff, sizeof(cursor->mask));
      self->mod->server_set_cursor(self->mod, cursor->x, cursor->y,
                                   cursor->data, cursor->mask);
      g_free(cursor);
      break;
    default:
      break;
  }
  return 0;
}

/******************************************************************************/
/* Process a pointer PDU */
static int APP_CC
rdp_rdp_process_pointer_pdu(struct rdp_rdp* self, struct stream* s)
{
  int message_type;
  int x;
  int y;
  int rv;

  rv = 0;
  in_uint16_le(s, message_type);
  in_uint8s(s, 2); /* pad */
  switch (message_type)
  {
    case RDP_POINTER_MOVE:
      in_uint16_le(s, x);
      in_uint16_le(s, y);
      break;
    case RDP_POINTER_COLOR:
      rv = rdp_rdp_process_color_pointer_pdu(self, s);
      break;
    case RDP_POINTER_CACHED:
      rv = rdp_rdp_process_cached_pointer_pdu(self, s);
      break;
    case RDP_POINTER_SYSTEM:
      rv = rdp_rdp_process_system_pointer_pdu(self, s);
      break;
    default:
      break;
  }
  return rv;
}

/******************************************************************************/
/* Process bitmap updates */
static void APP_CC
rdp_rdp_process_bitmap_updates(struct rdp_rdp* self, struct stream* s)
{
  int num_updates;
  int left;
  int top;
  int right;
  int bottom;
  int width;
  int height;
  int cx;
  int cy;
  int bpp;
  int Bpp;
  int compress;
  int bufsize;
  int size;
  int i;
  int x;
  int y;
  char* data;
  char* bmpdata0;
  char* bmpdata1;

  in_uint16_le(s, num_updates);
  for (i = 0; i < num_updates; i++)
  {
    in_uint16_le(s, left);
    in_uint16_le(s, top);
    in_uint16_le(s, right);
    in_uint16_le(s, bottom);
    in_uint16_le(s, width);
    in_uint16_le(s, height);
    in_uint16_le(s, bpp);
    Bpp = (bpp + 7) / 8;
    in_uint16_le(s, compress);
    in_uint16_le(s, bufsize);
    cx = (right - left) + 1;
    cy = (bottom - top) + 1;
    bmpdata0 = (char*)g_malloc(width * height * Bpp, 0);
    if (compress)
    {
      if (compress & 0x400)
      {
        size = bufsize;
      }
      else
      {
        in_uint8s(s, 2); /* pad */
        in_uint16_le(s, size);
        in_uint8s(s, 4); /* line_size, final_size */
      }
      in_uint8p(s, data, size);
      rdp_bitmap_decompress(bmpdata0, width, height, data, size, Bpp);
      bmpdata1 = rdp_orders_convert_bitmap(bpp, self->mod->xrdp_bpp,
                                           bmpdata0, width, height,
                                           self->colormap.colors);
      self->mod->server_paint_rect(self->mod, left, top, cx, cy, bmpdata1,
                                   width, height, 0, 0);
    }
    else /* not compressed */
    {
      for (y = 0; y < height; y++)
      {
        data = bmpdata0 + ((height - y) - 1) * (width * Bpp);
        if (Bpp == 1)
        {
          for (x = 0; x < width; x++)
          {
            in_uint8(s, data[x]);
          }
        }
        else if (Bpp == 2)
        {
          for (x = 0; x < width; x++)
          {
            in_uint16_le(s, ((tui16*)data)[x]);
          }
        }
        else if (Bpp == 3)
        {
          for (x = 0; x < width; x++)
          {
            in_uint8(s, data[x * 3 + 0]);
            in_uint8(s, data[x * 3 + 1]);
            in_uint8(s, data[x * 3 + 2]);
          }
        }
      }
      bmpdata1 = rdp_orders_convert_bitmap(bpp, self->mod->xrdp_bpp,
                                           bmpdata0, width, height,
                                           self->colormap.colors);
      self->mod->server_paint_rect(self->mod, left, top, cx, cy, bmpdata1,
                                   width, height, 0, 0);
    }
    if (bmpdata0 != bmpdata1)
    {
      g_free(bmpdata1);
    }
    g_free(bmpdata0);
  }
}

/******************************************************************************/
/* Process a palette update */
static void APP_CC
rdp_rdp_process_palette(struct rdp_rdp* self, struct stream* s)
{
  int i;
  int r;
  int g;
  int b;

  in_uint8s(s, 2); /* pad */
  in_uint16_le(s, self->colormap.ncolors);
  in_uint8s(s, 2); /* pad */
  for (i = 0; i < self->colormap.ncolors; i++)
  {
    in_uint8(s, r);
    in_uint8(s, g);
    in_uint8(s, b);
    self->colormap.colors[i] = (r << 16) | (g << 8) | b;
  }
  //ui_set_colormap(hmap);
}

/******************************************************************************/
/* Process an update PDU */
static int APP_CC
rdp_rdp_process_update_pdu(struct rdp_rdp* self, struct stream* s)
{
  int update_type;
  int count;

  in_uint16_le(s, update_type);
  self->mod->server_begin_update(self->mod);
  switch (update_type)
  {
    case RDP_UPDATE_ORDERS:
      in_uint8s(s, 2); /* pad */
      in_uint16_le(s, count);
      in_uint8s(s, 2); /* pad */
      rdp_orders_process_orders(self->orders, s, count);
      break;
    case RDP_UPDATE_BITMAP:
      rdp_rdp_process_bitmap_updates(self, s);
      break;
    case RDP_UPDATE_PALETTE:
      rdp_rdp_process_palette(self, s);
      break;
    case RDP_UPDATE_SYNCHRONIZE:
      break;
    default:
      break;
  }
  self->mod->server_end_update(self->mod);
  return 0;
}


/******************************************************************************/
void APP_CC
rdp_rdp_out_unistr(struct stream* s, char* text)
{
  int i;

  i = 0;
  while (text[i] != 0)
  {
    out_uint8(s, text[i]);
    out_uint8(s, 0);
    i++;
  }
  out_uint8(s, 0);
  out_uint8(s, 0);
}

/******************************************************************************/
int APP_CC
rdp_rdp_send_login_info(struct rdp_rdp* self, int flags)
{
  int len_domain;
  int len_username;
  int len_password;
  int len_program;
  int len_directory;
  int sec_flags;
  struct stream* s;

  DEBUG(("in rdp_rdp_send_login_info"));
  make_stream(s);
  init_stream(s, 8192);
  len_domain = 2 * g_strlen(self->mod->domain);
  len_username = 2 * g_strlen(self->mod->username);
  len_password = 2 * g_strlen(self->mod->password);
  len_program = 2 * g_strlen(self->mod->program);
  len_directory = 2 * g_strlen(self->mod->directory);
  sec_flags = SEC_LOGON_INFO | SEC_ENCRYPT;
  if (rdp_sec_init(self->sec_layer, s, sec_flags) != 0)
  {
    free_stream(s);
    DEBUG(("out rdp_rdp_send_login_info error 1"));
    return 1;
  }
  out_uint32_le(s, 0);
  out_uint32_le(s, flags);
  out_uint16_le(s, len_domain);
  out_uint16_le(s, len_username);
  out_uint16_le(s, len_password);
  out_uint16_le(s, len_program);
  out_uint16_le(s, len_directory);
  rdp_rdp_out_unistr(s, self->mod->domain);
  rdp_rdp_out_unistr(s, self->mod->username);
  rdp_rdp_out_unistr(s, self->mod->password);
  rdp_rdp_out_unistr(s, self->mod->program);
  rdp_rdp_out_unistr(s, self->mod->directory);
  s_mark_end(s);
  if (rdp_sec_send(self->sec_layer, s, sec_flags) != 0)
  {
    free_stream(s);
    DEBUG(("out rdp_rdp_send_login_info error 2"));
    return 1;
  }
  free_stream(s);
  DEBUG(("out rdp_rdp_send_login_info"));
  return 0;
}

/******************************************************************************/
int APP_CC
rdp_rdp_connect(struct rdp_rdp* self, char* ip, char* port)
{
  int flags;

  DEBUG(("in rdp_rdp_connect"));
  flags = RDP_LOGON_NORMAL;
  if (g_strlen(self->mod->password) > 0)
  {
    flags |= RDP_LOGON_AUTO;
  }
  if (rdp_sec_connect(self->sec_layer, ip, port) != 0)
  {
    DEBUG(("out rdp_rdp_connect error rdp_sec_connect failed"));
    return 1;
  }
  if (rdp_rdp_send_login_info(self, flags) != 0)
  {
    DEBUG(("out rdp_rdp_connect error rdp_rdp_send_login_info failed"));
    return 1;
  }
  DEBUG(("out rdp_rdp_connect"));
  return 0;
}

/******************************************************************************/
int APP_CC
rdp_rdp_send_input(struct rdp_rdp* self, struct stream* s,
                   int time, int message_type,
                   int device_flags, int param1, int param2)
{
  if (rdp_rdp_init_data(self, s) != 0)
  {
    return 1;
  }
  out_uint16_le(s, 1); /* number of events */
  out_uint16_le(s, 0);
  out_uint32_le(s, time);
  out_uint16_le(s, message_type);
  out_uint16_le(s, device_flags);
  out_uint16_le(s, param1);
  out_uint16_le(s, param2);
  s_mark_end(s);
  if (rdp_rdp_send_data(self, s, RDP_DATA_PDU_INPUT) != 0)
  {
    return 1;
  }
  return 0;
}

/******************************************************************************/
int APP_CC
rdp_rdp_send_invalidate(struct rdp_rdp* self, struct stream* s,
                        int left, int top, int width, int height)
{
  if (rdp_rdp_init_data(self, s) != 0)
  {
    return 1;
  }
  out_uint32_le(s, 1);
  out_uint16_le(s, left);
  out_uint16_le(s, top);
  out_uint16_le(s, (left + width) - 1);
  out_uint16_le(s, (top + height) - 1);
  s_mark_end(s);
  if (rdp_rdp_send_data(self, s, 33) != 0)
  {
    return 1;
  }
  return 0;
}

/******************************************************************************/
int APP_CC
rdp_rdp_recv(struct rdp_rdp* self, struct stream* s, int* type)
{
  int len;
  int pdu_type;
  int chan;

  chan = 0;
  DEBUG(("in rdp_rdp_recv"));
  if (s->next_packet >= s->end || s->next_packet == 0)
  {
    if (rdp_sec_recv(self->sec_layer, s, &chan) != 0)
    {
      DEBUG(("error in rdp_rdp_recv, rdp_sec_recv failed"));
      return 1;
    }
    s->next_packet = s->p;
  }
  else
  {
    chan = MCS_GLOBAL_CHANNEL;
    s->p = s->next_packet;
  }
  if (chan == MCS_GLOBAL_CHANNEL)
  {
    in_uint16_le(s, len);
    DEBUG(("rdp_rdp_recv got %d len", len));
    if (len == 0x8000)
    {
      s->next_packet += 8;
      DEBUG(("out rdp_rdp_recv"));
      return 0;
    }
    in_uint16_le(s, pdu_type);
    in_uint8s(s, 2);
    *type = pdu_type & 0xf;
    s->next_packet += len;
  }
  else
  {
    /* todo, process channel data */
    DEBUG(("got channel data channel %d", chan));
    s->next_packet = s->end;
  }
  DEBUG(("out rdp_rdp_recv"));
  return 0;
}


/******************************************************************************/
static int APP_CC
rdp_rdp_process_disconnect_pdu(struct rdp_rdp* self, struct stream* s)
{
  return 0;
}

/******************************************************************************/
int APP_CC
rdp_rdp_process_data_pdu(struct rdp_rdp* self, struct stream* s)
{
  int data_pdu_type;
  int ctype;
  int clen;
  int len;
  int rv;

  rv = 0;
  in_uint8s(s, 6); /* shareid, pad, streamid */
  in_uint16_le(s, len);
  in_uint8(s, data_pdu_type);
  in_uint8(s, ctype);
  in_uint16_le(s, clen);
  clen -= 18;
  switch (data_pdu_type)
  {
    case RDP_DATA_PDU_UPDATE:
      rv = rdp_rdp_process_update_pdu(self, s);
      break;
    case RDP_DATA_PDU_CONTROL:
      break;
    case RDP_DATA_PDU_SYNCHRONISE:
      break;
    case RDP_DATA_PDU_POINTER:
      rv = rdp_rdp_process_pointer_pdu(self, s);
      break;
    case RDP_DATA_PDU_BELL:
      break;
    case RDP_DATA_PDU_LOGON:
      break;
    case RDP_DATA_PDU_DISCONNECT:
      rv = rdp_rdp_process_disconnect_pdu(self, s);
      break;
    default:
      break;
  }
  return rv;
}

/******************************************************************************/
/* Process a bitmap capability set */
static void APP_CC
rdp_rdp_process_general_caps(struct rdp_rdp* self, struct stream* s)
{
}

/******************************************************************************/
/* Process a bitmap capability set */
static void APP_CC
rdp_rdp_process_bitmap_caps(struct rdp_rdp* self, struct stream* s)
{
  int width;
  int height;
  int bpp;

  in_uint16_le(s, bpp);
  in_uint8s(s, 6);
  in_uint16_le(s, width);
  in_uint16_le(s, height);
  self->mod->rdp_bpp = bpp;
  /* todo, call reset if needed and use width and height */
}

/******************************************************************************/
/* Process server capabilities */
/* returns error */
static int APP_CC
rdp_rdp_process_server_caps(struct rdp_rdp* self, struct stream* s, int len)
{
  int n;
  int ncapsets;
  int capset_type;
  int capset_length;
  char* next;
  char* start;

  start = s->p;
  in_uint16_le(s, ncapsets);
  in_uint8s(s, 2); /* pad */
  for (n = 0; n < ncapsets; n++)
  {
    if (s->p > start + len)
    {
      return 0;
    }
    in_uint16_le(s, capset_type);
    in_uint16_le(s, capset_length);
    next = (s->p + capset_length) - 4;
    switch (capset_type)
    {
      case RDP_CAPSET_GENERAL:
        rdp_rdp_process_general_caps(self, s);
        break;
      case RDP_CAPSET_BITMAP:
        rdp_rdp_process_bitmap_caps(self, s);
        break;
      default:
        break;
    }
    s->p = next;
  }
  return 0;
}

/******************************************************************************/
/* Send a control PDU */
/* returns error */
static int APP_CC
rdp_rdp_send_control(struct rdp_rdp* self, struct stream* s, int action)
{
  if (rdp_rdp_init_data(self, s) != 0)
  {
    return 1;
  }
  out_uint16_le(s, action);
  out_uint16_le(s, 0); /* userid */
  out_uint32_le(s, 0); /* control id */
  s_mark_end(s);
  if (rdp_rdp_send_data(self, s, RDP_DATA_PDU_CONTROL) != 0)
  {
    return 1;
  }
  return 0;
}

/******************************************************************************/
/* Send a synchronisation PDU */
/* returns error */
static int APP_CC
rdp_rdp_send_synchronise(struct rdp_rdp* self, struct stream* s)
{
  if (rdp_rdp_init_data(self, s) != 0)
  {
    return 1;
  }
  out_uint16_le(s, 1); /* type */
  out_uint16_le(s, 1002);
  s_mark_end(s);
  if (rdp_rdp_send_data(self, s, RDP_DATA_PDU_SYNCHRONISE) != 0)
  {
    return 1;
  }
  return 0;
}

/******************************************************************************/
/* Send an (empty) font information PDU */
static int APP_CC
rdp_rdp_send_fonts(struct rdp_rdp* self, struct stream* s, int seq)
{
  if (rdp_rdp_init_data(self, s) != 0)
  {
    return 1;
  }
  out_uint16_le(s, 0); /* number of fonts */
  out_uint16_le(s, 0); /* pad? */
  out_uint16_le(s, seq); /* unknown */
  out_uint16_le(s, 0x32); /* entry size */
  s_mark_end(s);
  if (rdp_rdp_send_data(self, s, RDP_DATA_PDU_FONT2) != 0)
  {
    return 1;
  }
  return 0;
}

/******************************************************************************/
/* Respond to a demand active PDU */
int APP_CC
rdp_rdp_process_demand_active(struct rdp_rdp* self, struct stream* s)
{
  int type;
  int len_src_descriptor;
  int len_combined_caps;

  in_uint32_le(s, self->share_id);
  in_uint16_le(s, len_src_descriptor);
  in_uint16_le(s, len_combined_caps);
  in_uint8s(s, len_src_descriptor);
  rdp_rdp_process_server_caps(self, s, len_combined_caps);
  rdp_rdp_send_confirm_active(self, s);
  rdp_rdp_send_synchronise(self, s);
  rdp_rdp_send_control(self, s, RDP_CTL_COOPERATE);
  rdp_rdp_send_control(self, s, RDP_CTL_REQUEST_CONTROL);
  rdp_rdp_recv(self, s, &type); /* RDP_PDU_SYNCHRONIZE */
  rdp_rdp_recv(self, s, &type); /* RDP_CTL_COOPERATE */
  rdp_rdp_recv(self, s, &type); /* RDP_CTL_GRANT_CONTROL */
  rdp_rdp_send_input(self, s, 0, RDP_INPUT_SYNCHRONIZE, 0, 0, 0);
  rdp_rdp_send_fonts(self, s, 1);
  rdp_rdp_send_fonts(self, s, 2);
  rdp_rdp_recv(self, s, &type); /* RDP_PDU_UNKNOWN 0x28 (Fonts?) */
  rdp_orders_reset_state(self->orders);
  return 0;
}

/******************************************************************************/
int APP_CC
rdp_rec_check_file(struct rdp_rdp* self)
{
  char file_name[256];
  int index;
  int len;
  struct stream* s;

  if (self->rec_fd == 0)
  {
    index = 1;
    g_sprintf(file_name, "rec%8.8d.rec", index);
    while (g_file_exist(file_name))
    {
      index++;
      if (index >= 9999)
      {
        return 1;
      }
      g_sprintf(file_name, "rec%8.8d.rec", index);
    }
    self->rec_fd = g_file_open(file_name);
    make_stream(s);
    init_stream(s, 8192);
    out_uint8a(s, "XRDPREC1", 8);
    out_uint8s(s, 8);
    s_mark_end(s);
    len = s->end - s->data;
    g_file_write(self->rec_fd, s->data, len);
    free_stream(s);
  }
  return 0;
}

/******************************************************************************/
int APP_CC
rdp_rec_write_item(struct rdp_rdp* self, struct stream* s)
{
  int len;
  int time;

  if (self->rec_fd == 0)
  {
    return 1;
  }
  time = g_time1();
  out_uint32_le(s, time);
  s_mark_end(s);
  len = s->end - s->data;
  s_pop_layer(s, iso_hdr);
  out_uint32_le(s, len);
  g_file_write(self->rec_fd, s->data, len);
  return 0;
}
