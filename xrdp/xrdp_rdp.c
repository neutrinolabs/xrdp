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

   rdp layer

*/

#include "xrdp.h"

char unknown1[172] =
{ 0xff, 0x02, 0xb6, 0x00, 0x28, 0x00, 0x00, 0x00,
  0x27, 0x00, 0x27, 0x00, 0x03, 0x00, 0x04, 0x00,
  0x00, 0x00, 0x26, 0x00, 0x01, 0x00, 0x1e, 0x00,
  0x02, 0x00, 0x1f, 0x00, 0x03, 0x00, 0x1d, 0x00,
  0x04, 0x00, 0x27, 0x00, 0x05, 0x00, 0x0b, 0x00,
  0x06, 0x00, 0x28, 0x00, 0x08, 0x00, 0x21, 0x00,
  0x09, 0x00, 0x20, 0x00, 0x0a, 0x00, 0x22, 0x00,
  0x0b, 0x00, 0x25, 0x00, 0x0c, 0x00, 0x24, 0x00,
  0x0d, 0x00, 0x23, 0x00, 0x0e, 0x00, 0x19, 0x00,
  0x0f, 0x00, 0x16, 0x00, 0x10, 0x00, 0x15, 0x00,
  0x11, 0x00, 0x1c, 0x00, 0x12, 0x00, 0x1b, 0x00,
  0x13, 0x00, 0x1a, 0x00, 0x14, 0x00, 0x17, 0x00,
  0x15, 0x00, 0x18, 0x00, 0x16, 0x00, 0x0e, 0x00,
  0x18, 0x00, 0x0c, 0x00, 0x19, 0x00, 0x0d, 0x00,
  0x1a, 0x00, 0x12, 0x00, 0x1b, 0x00, 0x14, 0x00,
  0x1f, 0x00, 0x13, 0x00, 0x20, 0x00, 0x00, 0x00,
  0x21, 0x00, 0x0a, 0x00, 0x22, 0x00, 0x06, 0x00,
  0x23, 0x00, 0x07, 0x00, 0x24, 0x00, 0x08, 0x00,
  0x25, 0x00, 0x09, 0x00, 0x26, 0x00, 0x04, 0x00,
  0x27, 0x00, 0x03, 0x00, 0x28, 0x00, 0x02, 0x00,
  0x29, 0x00, 0x01, 0x00, 0x2a, 0x00, 0x05, 0x00,
  0x2b, 0x00, 0x2a, 0x00 };

/*****************************************************************************/
int xrdp_rdp_read_config(struct xrdp_client_info* client_info)
{
  int fd;
  int index;
  struct xrdp_list* items;
  struct xrdp_list* values;
  char* item;
  char* value;

  fd = g_file_open("xrdp.ini");
  if (fd > 0)
  {
    items = xrdp_list_create();
    items->auto_free = 1;
    values = xrdp_list_create();
    values->auto_free = 1;
    xrdp_file_read_section(fd, "globals", items, values);
    for (index = 0; index < items->count; index++)
    {
      item = (char*)xrdp_list_get_item(items, index);
      value = (char*)xrdp_list_get_item(values, index);
      if (g_strcmp(item, "bitmap_cache") == 0)
      {
        if (g_strcmp(value, "yes") == 0)
        {
          client_info->use_bitmap_cache = 1;
        }
      }
      else if (g_strcmp(item, "bitmap_compression") == 0)
      {
        if (g_strcmp(value, "yes") == 0)
        {
          client_info->use_bitmap_comp = 1;
        }
      }
    }
    xrdp_list_delete(items);
    xrdp_list_delete(values);
    g_file_close(fd);
  }
  return 0;
}

/*****************************************************************************/
struct xrdp_rdp* xrdp_rdp_create(struct xrdp_process* owner, int sck)
{
  struct xrdp_rdp* self;

  self = (struct xrdp_rdp*)g_malloc(sizeof(struct xrdp_rdp), 1);
  self->pro_layer = owner;
  self->share_id = 66538;
  self->sec_layer = xrdp_sec_create(self, sck);
  /* read ini settings */
  xrdp_rdp_read_config(&self->client_info);
  return self;
}

/*****************************************************************************/
void xrdp_rdp_delete(struct xrdp_rdp* self)
{
  if (self == 0)
  {
    return;
  }
  xrdp_sec_delete(self->sec_layer);
  g_free(self);
}

/*****************************************************************************/
int xrdp_rdp_init(struct xrdp_rdp* self, struct stream* s)
{
  if (xrdp_sec_init(self->sec_layer, s) != 0)
  {
    return 1;
  }
  s_push_layer(s, rdp_hdr, 6);
  return 0;
}

/*****************************************************************************/
int xrdp_rdp_init_data(struct xrdp_rdp* self, struct stream* s)
{
  if (xrdp_sec_init(self->sec_layer, s) != 0)
  {
    return 1;
  }
  s_push_layer(s, rdp_hdr, 18);
  return 0;
}

/*****************************************************************************/
int xrdp_rdp_recv(struct xrdp_rdp* self, struct stream* s, int* code)
{
  int error;
  int len;
  int pdu_code;
  int chan;

  if (s->next_packet == 0 || s->next_packet >= s->end)
  {
    chan = 0;
    error = xrdp_sec_recv(self->sec_layer, s, &chan);
    if (error == -1) /* special code for send demand active */
    {
      s->next_packet = 0;
      *code = -1;
      return 0;
    }
    if (chan != MCS_GLOBAL_CHANNEL && chan > 0)
    {
      s->next_packet = 0;
      *code = 0;
      return 0;
    }
    if (error != 0)
    {
      return 1;
    }
    s->next_packet = s->p;
  }
  else
  {
    s->p = s->next_packet;
  }
  in_uint16_le(s, len);
  if (len == 0x8000)
  {
    s->next_packet += 8;
    *code = 0;
    return 0;
  }
  in_uint16_le(s, pdu_code);
  *code = pdu_code & 0xf;
  in_uint8s(s, 2); /* mcs user id */
  s->next_packet += len;
  return 0;
}

/*****************************************************************************/
int xrdp_rdp_send(struct xrdp_rdp* self, struct stream* s, int pdu_type)
{
  int len;

  DEBUG(("in xrdp_rdp_send\n\r"));
  s_pop_layer(s, rdp_hdr);
  len = s->end - s->p;
  out_uint16_le(s, len);
  out_uint16_le(s, 0x10 | pdu_type);
  out_uint16_le(s, self->mcs_channel);
  if (xrdp_sec_send(self->sec_layer, s, 0) != 0)
  {
    return 1;
  }
  DEBUG(("out xrdp_rdp_send\n\r"));
  return 0;
}

/*****************************************************************************/
int xrdp_rdp_send_data(struct xrdp_rdp* self, struct stream* s,
                       int data_pdu_type)
{
  int len;

  DEBUG(("in xrdp_rdp_send_data\n\r"));
  s_pop_layer(s, rdp_hdr);
  len = s->end - s->p;
  out_uint16_le(s, len);
  out_uint16_le(s, 0x10 | RDP_PDU_DATA);
  out_uint16_le(s, self->mcs_channel);
  out_uint32_le(s, self->share_id);
  out_uint8(s, 0);
  out_uint8(s, 1);
  out_uint16_le(s, len - 14);
  out_uint8(s, data_pdu_type);
  out_uint8(s, 0);
  out_uint16_le(s, 0);
  if (xrdp_sec_send(self->sec_layer, s, 0) != 0)
  {
    return 1;
  }
  DEBUG(("out xrdp_rdp_send_data\n\r"));
  return 0;
}

/*****************************************************************************/
int xrdp_rdp_parse_client_mcs_data(struct xrdp_rdp* self)
{
  struct stream* p;
  int i;

  p = &self->sec_layer->client_mcs_data;
  p->p = p->data;
  in_uint8s(p, 31);
  in_uint16_le(p, self->client_info.width);
  in_uint16_le(p, self->client_info.height);
  in_uint8s(p, 120);
  self->client_info.bpp = 8;
  in_uint16_le(p, i);
  switch (i)
  {
    case 0xca01:
      in_uint8s(p, 6);
      in_uint8(p, i);
      if (i > 8)
      {
        self->client_info.bpp = i;
      }
      break;
    case 0xca02:
      self->client_info.bpp = 15;
      break;
    case 0xca03:
      self->client_info.bpp = 16;
      break;
    case 0xca04:
      self->client_info.bpp = 24;
      break;
  }
  p->p = p->data;
  DEBUG(("client width %d, client height %d bpp %d\n\r",
         self->client_info.width, self->client_info.height,
         self->client_info.bpp));
  return 0;
}

/*****************************************************************************/
int xrdp_rdp_incoming(struct xrdp_rdp* self)
{
  DEBUG(("in xrdp_rdp_incoming\n\r"));
  if (xrdp_sec_incoming(self->sec_layer) != 0)
  {
    return 1;
  }
  self->mcs_channel = self->sec_layer->mcs_layer->userid +
                      MCS_USERCHANNEL_BASE;
  xrdp_rdp_parse_client_mcs_data(self);
  DEBUG(("out xrdp_rdp_incoming mcs channel %d\n\r", self->mcs_channel));
  return 0;
}

/*****************************************************************************/
int xrdp_rdp_send_demand_active(struct xrdp_rdp* self)
{
  struct stream* s;

  make_stream(s);
  init_stream(s, 8192);
  if (xrdp_rdp_init(self, s) != 0)
  {
    free_stream(s);
    return 1;
  }

  out_uint32_le(s, self->share_id);
  out_uint32_be(s, 0x04000401);
  out_uint32_be(s, 0x524e5300);
  out_uint32_be(s, 0x08000000);
  out_uint32_be(s, 0x09000800);
  out_uint16_le(s, self->mcs_channel);
  out_uint16_be(s, 0xb5e2);

  /* Output general capability set */
  out_uint16_le(s, RDP_CAPSET_GENERAL); /* 1 */
  out_uint16_le(s, RDP_CAPLEN_GENERAL); /* 24(0x18) */
  out_uint16_le(s, 1); /* OS major type */
  out_uint16_le(s, 3); /* OS minor type */
  out_uint16_le(s, 0x200); /* Protocol version */
  out_uint16_le(s, 0); /* pad */
  out_uint16_le(s, 0); /* Compression types */
  out_uint16_le(s, 0); /* pad */
  out_uint16_le(s, 0); /* Update capability */
  out_uint16_le(s, 0); /* Remote unshare capability */
  out_uint16_le(s, 0); /* Compression level */
  out_uint16_le(s, 0); /* Pad */

  /* Output bitmap capability set */
  out_uint16_le(s, RDP_CAPSET_BITMAP); /* 2 */
  out_uint16_le(s, RDP_CAPLEN_BITMAP); /* 28(0x1c) */
  out_uint16_le(s, self->client_info.bpp); /* Preferred BPP */
  out_uint16_le(s, 1); /* Receive 1 BPP */
  out_uint16_le(s, 1); /* Receive 4 BPP */
  out_uint16_le(s, 1); /* Receive 8 BPP */
  out_uint16_le(s, self->client_info.width); /* width */
  out_uint16_le(s, self->client_info.height); /* height */
  out_uint16_le(s, 0); /* Pad */
  out_uint16_le(s, 1); /* Allow resize */
  out_uint16_le(s, 1); /* bitmap compression */
  out_uint16_le(s, 0); /* unknown */
  out_uint16_le(s, 0); /* unknown */
  out_uint16_le(s, 0); /* pad */

  /* Output ? */
  out_uint16_le(s, 14);
  out_uint16_le(s, 4);

  /* Output order capability set */
  out_uint16_le(s, RDP_CAPSET_ORDER); /* 3 */
  out_uint16_le(s, RDP_CAPLEN_ORDER); /* 88(0x58) */
  out_uint8s(s, 16);
  out_uint32_be(s, 0x40420f00);
  out_uint16_le(s, 1); /* Cache X granularity */
  out_uint16_le(s, 20); /* Cache Y granularity */
  out_uint16_le(s, 0); /* Pad */
  out_uint16_le(s, 1); /* Max order level */
  out_uint16_le(s, 0x2f); /* Number of fonts */
  out_uint16_le(s, 0x22); /* Capability flags */
  /* caps */
  out_uint8(s, 1); /* dest blt */
  out_uint8(s, 1); /* pat blt */
  out_uint8(s, 1); /* screen blt */
  out_uint8(s, 1); /* memblt */
  out_uint8(s, 1);
  out_uint8(s, 1);
  out_uint8(s, 1);
  out_uint8(s, 1);
  out_uint8(s, 1);
  out_uint8(s, 1);
  out_uint8(s, 1);
  out_uint8(s, 1);
  out_uint8(s, 1);
  out_uint8(s, 0);
  out_uint8(s, 0);
  out_uint8(s, 1);
  out_uint8(s, 1);
  out_uint8(s, 1);
  out_uint8(s, 1);
  out_uint8(s, 1);
  out_uint8(s, 1);
  out_uint8(s, 1);
  out_uint8(s, 1);
  out_uint8(s, 1);
  out_uint8(s, 1);
  out_uint8(s, 1);
  out_uint8(s, 1);
  out_uint8(s, 1);
  out_uint8(s, 1);
  out_uint8(s, 0);
  out_uint8(s, 0);
  out_uint8(s, 0);
  out_uint16_le(s, 0x6a1);
  out_uint8s(s, 6); /* ? */
  out_uint32_le(s, 0x0f4240); /* desk save */
  out_uint32_le(s, 0); /* ? */
  out_uint32_le(s, 0); /* ? */

  /* Output color cache capability set */
  out_uint16_le(s, RDP_CAPSET_COLCACHE);
  out_uint16_le(s, RDP_CAPLEN_COLCACHE);
  out_uint16_le(s, 6); /* cache size */
  out_uint16_le(s, 0); /* pad */

  /* Output pointer capability set */
  out_uint16_le(s, RDP_CAPSET_POINTER);
  out_uint16_le(s, RDP_CAPLEN_POINTER);
  out_uint16_le(s, 1); /* Colour pointer */
  out_uint16_le(s, 0x19); /* Cache size */

  /* Output ? */
  out_uint16_le(s, 0xd);
  out_uint16_le(s, 0x58); /* 88 */
  out_uint8(s, 1);
  out_uint8s(s, 83);

  s_mark_end(s);

  if (xrdp_rdp_send(self, s, RDP_PDU_DEMAND_ACTIVE) != 0)
  {
    free_stream(s);
    return 1;
  }

  free_stream(s);
  return 0;
}

/*****************************************************************************/
int xrdp_process_capset_bmpcache(struct xrdp_rdp* self, struct stream* s,
                                 int len)
{
  //g_hexdump(s->p, len);
  in_uint8s(s, 24);
  in_uint16_le(s, self->client_info.cache1_entries);
  in_uint16_le(s, self->client_info.cache1_size);
  in_uint16_le(s, self->client_info.cache2_entries);
  in_uint16_le(s, self->client_info.cache2_size);
  in_uint16_le(s, self->client_info.cache3_entries);
  in_uint16_le(s, self->client_info.cache3_size);
  //g_printf("%d %d %d %d %d %d\n", self->cache1_entries, self->cache1_size,
  //         self->cache2_entries, self->cache2_size,
  //         self->cache3_entries, self->cache3_size);
  return 0;
}

/*****************************************************************************/
int xrdp_rdp_process_confirm_active(struct xrdp_rdp* self, struct stream* s)
{
  int cap_len;
  int source_len;
  int num_caps;
  int index;
  int type;
  int len;
  char* p;

  in_uint8s(s, 4); /* rdp_shareid */
  in_uint8s(s, 2); /* userid */
  in_uint16_le(s, source_len); /* sizeof RDP_SOURCE */
  in_uint16_le(s, cap_len);
  in_uint8s(s, source_len);
  in_uint16_le(s, num_caps);
  in_uint8s(s, 2); /* pad */
  for (index = 0; index < num_caps; index++)
  {
    p = s->p;
    in_uint16_le(s, type);
    in_uint16_le(s, len);
    //g_printf("%d %d\n", type, len);
    switch (type)
    {
      case RDP_CAPSET_BMPCACHE:
        xrdp_process_capset_bmpcache(self, s, len);
        break;
    }
    s->p = p + len;
  }
  //g_hexdump(s->p, s->end - s->p);
  return 0;
}

/*****************************************************************************/
int xrdp_rdp_process_data_pointer(struct xrdp_rdp* self, struct stream* s)
{
  return 0;
}

/*****************************************************************************/
/* RDP_INPUT_SYNCHRONIZE */
int xrdp_rdp_process_input_sync(struct xrdp_rdp* self, int device_flags,
                                int key_flags)
{
  DEBUG(("sync event flags %d key %d\n\r", device_flags, key_flags));
  if (!self->up_and_running)
  {
    return 0;
  }
  xrdp_wm_key_sync(self->pro_layer->wm, device_flags, key_flags);
  return 0;
}

/*****************************************************************************/
/* RDP_INPUT_SCANCODE */
int xrdp_rdp_process_input_scancode(struct xrdp_rdp* self, int device_flags,
                                    int scan_code)
{
  DEBUG(("key event flags %4.4x scan_code %d\n\r", device_flags, scan_code));
  if (!self->up_and_running)
  {
    return 0;
  }
  xrdp_wm_key(self->pro_layer->wm, device_flags, scan_code);
  return 0;
}

/*****************************************************************************/
/* RDP_INPUT_MOUSE */
int xrdp_rdp_process_input_mouse(struct xrdp_rdp* self, int device_flags,
                                 int x, int y)
{
  DEBUG(("mouse event flags %4.4x x %d y %d\n\r", device_flags, x, y));
  if (!self->up_and_running)
  {
    return 0;
  }
  if (device_flags & MOUSE_FLAG_MOVE) /* 0x0800 */
  {
    xrdp_wm_mouse_move(self->pro_layer->wm, x, y);
  }
  if (device_flags & MOUSE_FLAG_BUTTON1) /* 0x1000 */
  {
    if (device_flags & MOUSE_FLAG_DOWN) /* 0x8000 */
    {
      xrdp_wm_mouse_click(self->pro_layer->wm, x, y, 1, 1);
    }
    else
    {
      xrdp_wm_mouse_click(self->pro_layer->wm, x, y, 1, 0);
    }
  }
  if (device_flags & MOUSE_FLAG_BUTTON2) /* 0x2000 */
  {
    if (device_flags & MOUSE_FLAG_DOWN) /* 0x8000 */
    {
      xrdp_wm_mouse_click(self->pro_layer->wm, x, y, 2, 1);
    }
    else
    {
      xrdp_wm_mouse_click(self->pro_layer->wm, x, y, 2, 0);
    }
  }
  if (device_flags & MOUSE_FLAG_BUTTON3) /* 0x4000 */
  {
    if (device_flags & MOUSE_FLAG_DOWN) /* 0x8000 */
    {
      xrdp_wm_mouse_click(self->pro_layer->wm, x, y, 3, 1);
    }
    else
    {
      xrdp_wm_mouse_click(self->pro_layer->wm, x, y, 3, 0);
    }
  }
  if (device_flags == MOUSE_FLAG_BUTTON4 || /* 0x0280 */
      device_flags == 0x0278)
  {
    xrdp_wm_mouse_click(self->pro_layer->wm, 0, 0, 4, 0);
  }
  if (device_flags == MOUSE_FLAG_BUTTON5 || /* 0x0380 */
      device_flags == 0x0388)
  {
    xrdp_wm_mouse_click(self->pro_layer->wm, 0, 0, 5, 0);
  }
  return 0;
}

/*****************************************************************************/
/* RDP_DATA_PDU_INPUT */
int xrdp_rdp_process_data_input(struct xrdp_rdp* self, struct stream* s)
{
  int num_events;
  int index;
  int msg_type;
  int device_flags;
  int param1;
  int param2;

  in_uint16_le(s, num_events);
  in_uint8s(s, 2); /* pad */
  DEBUG(("xrdp_rdp_process_data_input %d events\n\r", num_events));
  for (index = 0; index < num_events; index++)
  {
    in_uint8s(s, 4); /* time */
    in_uint16_le(s, msg_type);
    in_uint16_le(s, device_flags);
    in_sint16_le(s, param1);
    in_sint16_le(s, param2);
    switch (msg_type)
    {
      case RDP_INPUT_SYNCHRONIZE: /* 0 */
        xrdp_rdp_process_input_sync(self, device_flags, param1);
        break;
      case RDP_INPUT_SCANCODE: /* 4 */
        xrdp_rdp_process_input_scancode(self, device_flags, param1);
        break;
      case RDP_INPUT_MOUSE: /* 8001 */
        xrdp_rdp_process_input_mouse(self, device_flags, param1, param2);
        break;
      default:
        g_printf("unknown in xrdp_rdp_process_data_input\n\r");
        break;
    }
  }
  return 0;
}

/*****************************************************************************/
int xrdp_rdp_send_synchronise(struct xrdp_rdp* self)
{
  struct stream* s;

  make_stream(s);
  init_stream(s, 8192);
  if (xrdp_rdp_init_data(self, s) != 0)
  {
    free_stream(s);
    return 1;
  }
  out_uint16_le(s, 1);
  out_uint16_le(s, 1002);
  s_mark_end(s);
  if (xrdp_rdp_send_data(self, s, RDP_DATA_PDU_SYNCHRONISE) != 0)
  {
    free_stream(s);
    return 1;
  }
  free_stream(s);
  return 0;
}

/*****************************************************************************/
int xrdp_rdp_send_control(struct xrdp_rdp* self, int action)
{
  struct stream* s;

  make_stream(s);
  init_stream(s, 8192);
  if (xrdp_rdp_init_data(self, s) != 0)
  {
    free_stream(s);
    return 1;
  }
  out_uint16_le(s, action);
  out_uint16_le(s, 0); /* userid */
  out_uint32_le(s, 1002); /* control id */
  s_mark_end(s);
  if (xrdp_rdp_send_data(self, s, RDP_DATA_PDU_CONTROL) != 0)
  {
    free_stream(s);
    return 1;
  }
  free_stream(s);
  return 0;
}

/*****************************************************************************/
int xrdp_rdp_process_data_control(struct xrdp_rdp* self, struct stream* s)
{
  int action;

  in_uint16_le(s, action);
  in_uint8s(s, 2); /* user id */
  in_uint8s(s, 4); /* control id */
  if (action == RDP_CTL_REQUEST_CONTROL)
  {
    xrdp_rdp_send_synchronise(self);
    xrdp_rdp_send_control(self, RDP_CTL_COOPERATE);
    xrdp_rdp_send_control(self, RDP_CTL_GRANT_CONTROL);
  }
  return 0;
}

/*****************************************************************************/
int xrdp_rdp_process_data_sync(struct xrdp_rdp* self)
{
  return 0;
}

/*****************************************************************************/
int xrdp_rdp_process_screen_update(struct xrdp_rdp* self, struct stream* s)
{
  int op;
  int left;
  int top;
  int right;
  int bottom;
  int cx;
  int cy;
  struct xrdp_rect rect;

  in_uint32_le(s, op);
  in_uint16_le(s, left);
  in_uint16_le(s, top);
  in_uint16_le(s, right);
  in_uint16_le(s, bottom);
  cx = (right - left) + 1;
  cy = (bottom - top) + 1;
  if (self->up_and_running && self->pro_layer->wm != 0)
  {
    MAKERECT(rect, left, top, cx, cy);
    xrdp_bitmap_invalidate(self->pro_layer->wm->screen, &rect);
  }
  return 0;
}

/*****************************************************************************/
int xrdp_rdp_send_unknown1(struct xrdp_rdp* self)
{
  struct stream* s;

  make_stream(s);
  init_stream(s, 8192);
  if (xrdp_rdp_init_data(self, s) != 0)
  {
    free_stream(s);
    return 1;
  }
  out_uint8a(s, unknown1, 172);
  s_mark_end(s);
  if (xrdp_rdp_send_data(self, s, 0x28) != 0)
  {
    free_stream(s);
    return 1;
  }
  free_stream(s);
  return 0;
}

/*****************************************************************************/
int xrdp_rdp_process_data_font(struct xrdp_rdp* self, struct stream* s)
{
  int seq;

  in_uint8s(s, 2); /* num of fonts */
  in_uint8s(s, 2); // unknown
  in_uint16_le(s, seq);
  /* 419 client sends Seq 1, then 2 */
  /* 2600 clients sends only Seq 3 */
  if (seq == 2 || seq == 3) /* after second font message, we are up and */
  {                                           /* running */
    xrdp_rdp_send_unknown1(self);
    self->up_and_running = 1;
  }
  return 0;
}

/*****************************************************************************/
/* RDP_PDU_DATA */
int xrdp_rdp_process_data(struct xrdp_rdp* self, struct stream* s)
{
  int len;
  int data_type;
  int ctype;
  int clen;

  in_uint8s(s, 6);
  in_uint16_le(s, len);
  in_uint8(s, data_type);
  in_uint8(s, ctype);
  in_uint16_le(s, clen);
  DEBUG(("xrdp_rdp_process_data code %d\n\r", data_type));
  switch (data_type)
  {
    case RDP_DATA_PDU_POINTER: /* 27 */
      xrdp_rdp_process_data_pointer(self, s);
      break;
    case RDP_DATA_PDU_INPUT: /* 28 */
      xrdp_rdp_process_data_input(self, s);
      break;
    case RDP_DATA_PDU_CONTROL: /* 20 */
      xrdp_rdp_process_data_control(self, s);
      break;
    case RDP_DATA_PDU_SYNCHRONISE: /* 31 */
      xrdp_rdp_process_data_sync(self);
      break;
    case 33: /* 33 ?? */
      xrdp_rdp_process_screen_update(self, s);
      break;

    /*case 35:*/
      /* 35 ?? this comes when minimuzing a full screen mstsc.exe 2600 */
      /* I think this is saying the client no longer wants screen */
      /* updates and it will issue a 33 above to catch up */
      /* so minumized apps don't take bandwidth */

    case 36: /* 36 ?? disconnect? */
      return 1;
      break;
    case RDP_DATA_PDU_FONT2: /* 39 */
      xrdp_rdp_process_data_font(self, s);
      break;
    default:
      g_printf("unknown in xrdp_rdp_process_data %d\n\r", data_type);
      break;
  }
  return 0;
}

/*****************************************************************************/
int xrdp_rdp_disconnect(struct xrdp_rdp* self)
{
  return xrdp_sec_disconnect(self->sec_layer);
}
