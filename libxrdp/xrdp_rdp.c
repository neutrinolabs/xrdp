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

   rdp layer

*/

#include "libxrdp.h"

static char unknown1[172] =
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
static int APP_CC
xrdp_rdp_read_config(struct xrdp_client_info* client_info)
{
  int fd;
  int index;
  struct list* items;
  struct list* values;
  char* item;
  char* value;

  fd = g_file_open(XRDP_CFG_FILE); /* xrdp.ini */
  if (fd > 0)
  {
    items = list_create();
    items->auto_free = 1;
    values = list_create();
    values->auto_free = 1;
    file_read_section(fd, "globals", items, values);
    for (index = 0; index < items->count; index++)
    {
      item = (char*)list_get_item(items, index);
      value = (char*)list_get_item(values, index);
      if (g_strncasecmp(item, "bitmap_cache", 255) == 0)
      {
        if (g_strncasecmp(value, "yes", 255) == 0 ||
            g_strncasecmp(value, "true", 255) == 0 ||
            g_strncasecmp(value, "1", 255) == 0)
        {
          client_info->use_bitmap_cache = 1;
        }
      }
      else if (g_strncasecmp(item, "bitmap_compression", 255) == 0)
      {
        if (g_strncasecmp(value, "yes", 255) == 0 ||
            g_strncasecmp(value, "true", 255) == 0 ||
            g_strncasecmp(value, "1", 255) == 0)
        {
          client_info->use_bitmap_comp = 1;
        }
      }
      else if (g_strncasecmp(item, "crypt_level", 255) == 0)
      {
        if (g_strncasecmp(value, "low", 255) == 0)
        {
          client_info->crypt_level = 1;
        }
        else if (g_strncasecmp(value, "medium", 255) == 0)
        {
          client_info->crypt_level = 2;
        }
        else if (g_strncasecmp(value, "high", 255) == 0)
        {
          client_info->crypt_level = 3;
        }
      }
    }
    list_delete(items);
    list_delete(values);
    g_file_close(fd);
  }
  return 0;
}

/*****************************************************************************/
struct xrdp_rdp* APP_CC
xrdp_rdp_create(struct xrdp_session* session, int sck)
{
  struct xrdp_rdp* self;

  self = (struct xrdp_rdp*)g_malloc(sizeof(struct xrdp_rdp), 1);
  self->session = session;
  self->share_id = 66538;
  /* read ini settings */
  xrdp_rdp_read_config(&self->client_info);
  /* create sec layer */
  self->sec_layer = xrdp_sec_create(self, sck, self->client_info.crypt_level);
  /* default 8 bit v1 color bitmap cache entries and size */
  self->client_info.cache1_entries = 600;
  self->client_info.cache1_size = 256;
  self->client_info.cache2_entries = 300;
  self->client_info.cache2_size = 1024;
  self->client_info.cache3_entries = 262;
  self->client_info.cache3_size = 4096;
  return self;
}

/*****************************************************************************/
void APP_CC
xrdp_rdp_delete(struct xrdp_rdp* self)
{
  if (self == 0)
  {
    return;
  }
  xrdp_sec_delete(self->sec_layer);
  g_free(self);
}

/*****************************************************************************/
int APP_CC
xrdp_rdp_init(struct xrdp_rdp* self, struct stream* s)
{
  if (xrdp_sec_init(self->sec_layer, s) != 0)
  {
    return 1;
  }
  s_push_layer(s, rdp_hdr, 6);
  return 0;
}

/*****************************************************************************/
int APP_CC
xrdp_rdp_init_data(struct xrdp_rdp* self, struct stream* s)
{
  if (xrdp_sec_init(self->sec_layer, s) != 0)
  {
    return 1;
  }
  s_push_layer(s, rdp_hdr, 18);
  return 0;
}

/*****************************************************************************/
/* returns erros */
int APP_CC
xrdp_rdp_recv(struct xrdp_rdp* self, struct stream* s, int* code)
{
  int error;
  int len;
  int pdu_code;
  int chan;

  DEBUG(("in xrdp_rdp_recv\r\n"));
  if (s->next_packet == 0 || s->next_packet >= s->end)
  {
    chan = 0;
    error = xrdp_sec_recv(self->sec_layer, s, &chan);
    if (error == -1) /* special code for send demand active */
    {
      s->next_packet = 0;
      *code = -1;
      DEBUG(("out xrdp_rdp_recv\r\n"));
      return 0;
    }
    if (error != 0)
    {
      DEBUG(("out xrdp_rdp_recv error\r\n"));
      return 1;
    }
    if (chan != MCS_GLOBAL_CHANNEL && chan > 0)
    {
      s->next_packet = 0;
      *code = 0;
      DEBUG(("out xrdp_rdp_recv\r\n"));
      return 0;
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
    DEBUG(("out xrdp_rdp_recv\r\n"));
    return 0;
  }
  in_uint16_le(s, pdu_code);
  *code = pdu_code & 0xf;
  in_uint8s(s, 2); /* mcs user id */
  s->next_packet += len;
  DEBUG(("out xrdp_rdp_recv\r\n"));
  return 0;
}

/*****************************************************************************/
int APP_CC
xrdp_rdp_send(struct xrdp_rdp* self, struct stream* s, int pdu_type)
{
  int len;

  DEBUG(("in xrdp_rdp_send\r\n"));
  s_pop_layer(s, rdp_hdr);
  len = s->end - s->p;
  out_uint16_le(s, len);
  out_uint16_le(s, 0x10 | pdu_type);
  out_uint16_le(s, self->mcs_channel);
  if (xrdp_sec_send(self->sec_layer, s) != 0)
  {
    DEBUG(("out xrdp_rdp_send error\r\n"));
    return 1;
  }
  DEBUG(("out xrdp_rdp_send\r\n"));
  return 0;
}

/*****************************************************************************/
int APP_CC
xrdp_rdp_send_data(struct xrdp_rdp* self, struct stream* s,
                   int data_pdu_type)
{
  int len;

  DEBUG(("in xrdp_rdp_send_data\r\n"));
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
  if (xrdp_sec_send(self->sec_layer, s) != 0)
  {
    DEBUG(("out xrdp_rdp_send_data error\r\n"));
    return 1;
  }
  DEBUG(("out xrdp_rdp_send_data\r\n"));
  return 0;
}

/*****************************************************************************/
static int APP_CC
xrdp_rdp_parse_client_mcs_data(struct xrdp_rdp* self)
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
  /* todo - for now, don't allow unsupported bpp connections
     xrdp_rdp_send_demand_active will tell the client what bpp to use */
  if (self->client_info.bpp == 24)
  {
    self->client_info.bpp = 16;
  }
  if (self->client_info.bpp == 15)
  {
    self->client_info.bpp = 16;
  }
  p->p = p->data;
  DEBUG(("client width %d, client height %d bpp %d\r\n",
         self->client_info.width, self->client_info.height,
         self->client_info.bpp));
  return 0;
}

/*****************************************************************************/
int APP_CC
xrdp_rdp_incoming(struct xrdp_rdp* self)
{
  DEBUG(("in xrdp_rdp_incoming\r\n"));
  if (xrdp_sec_incoming(self->sec_layer) != 0)
  {
    return 1;
  }
  self->mcs_channel = self->sec_layer->mcs_layer->userid +
                      MCS_USERCHANNEL_BASE;
  xrdp_rdp_parse_client_mcs_data(self);
  DEBUG(("out xrdp_rdp_incoming mcs channel %d\r\n", self->mcs_channel));
  return 0;
}

/*****************************************************************************/
int APP_CC
xrdp_rdp_send_demand_active(struct xrdp_rdp* self)
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

  out_uint16_le(s, 4); /* 4 chars for RDP\0 */
  out_uint16_le(s, 0x0104); /* size after num caps */
  out_uint8a(s, "RDP", 4);
  out_uint32_le(s, 8); /* num caps 8 */

  /* Output share capability set */
  out_uint16_le(s, RDP_CAPSET_SHARE);
  out_uint16_le(s, RDP_CAPLEN_SHARE);
  out_uint16_le(s, self->mcs_channel);
  out_uint16_be(s, 0xb5e2); /* 0x73e1 */

  /* Output general capability set */
  out_uint16_le(s, RDP_CAPSET_GENERAL); /* 1 */
  out_uint16_le(s, RDP_CAPLEN_GENERAL); /* 24(0x18) */
  out_uint16_le(s, 1); /* OS major type */
  out_uint16_le(s, 3); /* OS minor type */
  out_uint16_le(s, 0x200); /* Protocol version */
  out_uint16_le(s, 0); /* pad */
  out_uint16_le(s, 0); /* Compression types */
  out_uint16_le(s, 0); /* pad use 0x40d for rdp packets, 0 for not */
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
  out_uint8s(s, 2); /* ? */
  out_uint32_le(s, 0x0f4240); /* desk save */
  out_uint32_le(s, 0x0f4240); /* desk save */
  out_uint32_le(s, 1); /* ? */
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
static int APP_CC
xrdp_process_capset_general(struct xrdp_rdp* self, struct stream* s,
                            int len)
{
  int i;

  in_uint8s(s, 10);
  in_uint16_le(s, i);
  /* use_compact_packets is pretty much 'use rdp5' */
  self->client_info.use_compact_packets = (i != 0);
  /* op2 is a boolean to use compact bitmap headers in bitmap cache */
  /* set it to same as 'use rdp5' boolean */
  self->client_info.op2 = self->client_info.use_compact_packets;
  return 0;
}

/*****************************************************************************/
static int APP_CC
xrdp_process_capset_order(struct xrdp_rdp* self, struct stream* s,
                          int len)
{
  int i;

  in_uint8s(s, 72);
  in_uint32_le(s, i); /* desktop cache size, usually 0x38400 */
  self->client_info.desktop_cache = i;
  return 0;
}

/*****************************************************************************/
/* get the bitmap cache size */
static int APP_CC
xrdp_process_capset_bmpcache(struct xrdp_rdp* self, struct stream* s,
                             int len)
{
  in_uint8s(s, 24);
  in_uint16_le(s, self->client_info.cache1_entries);
  in_uint16_le(s, self->client_info.cache1_size);
  in_uint16_le(s, self->client_info.cache2_entries);
  in_uint16_le(s, self->client_info.cache2_size);
  in_uint16_le(s, self->client_info.cache3_entries);
  in_uint16_le(s, self->client_info.cache3_size);
  DEBUG(("cache1 entries %d size %d\r\n", self->client_info.cache1_entries,
         self->client_info.cache1_size));
  DEBUG(("cache2 entries %d size %d\r\n", self->client_info.cache2_entries,
         self->client_info.cache2_size));
  DEBUG(("cache3 entries %d size %d\r\n", self->client_info.cache3_entries,
         self->client_info.cache3_size));
  return 0;
}

/*****************************************************************************/
/* get the bitmap cache size */
static int APP_CC
xrdp_process_capset_bmpcache2(struct xrdp_rdp* self, struct stream* s,
                              int len)
{
  int Bpp;
  int i;

  self->client_info.bitmap_cache_version = 2;
  Bpp = (self->client_info.bpp + 7) / 8;
  in_uint16_le(s, i);
  self->client_info.bitmap_cache_persist_enable = i;
  in_uint8s(s, 2); /* number of caches in set, 3 */
  in_uint32_le(s, i);
  i = MIN(i, 2000);
  self->client_info.cache1_entries = i;
  self->client_info.cache1_size = 256 * Bpp;
  in_uint32_le(s, i);
  i = MIN(i, 2000);
  self->client_info.cache2_entries = i;
  self->client_info.cache2_size = 1024 * Bpp;
  in_uint32_le(s, i);
  i = i & 0x7fffffff;
  i = MIN(i, 2000);
  self->client_info.cache3_entries = i;
  self->client_info.cache3_size = 4096 * Bpp;
  DEBUG(("cache1 entries %d size %d\r\n", self->client_info.cache1_entries,
         self->client_info.cache1_size));
  DEBUG(("cache2 entries %d size %d\r\n", self->client_info.cache2_entries,
         self->client_info.cache2_size));
  DEBUG(("cache3 entries %d size %d\r\n", self->client_info.cache3_entries,
         self->client_info.cache3_size));
  return 0;
}

/*****************************************************************************/
/* get the number of client cursor cache */
static int APP_CC
xrdp_process_capset_pointercache(struct xrdp_rdp* self, struct stream* s,
                                 int len)
{
  int i;

  in_uint8s(s, 2); /* color pointer */
  in_uint16_le(s, i);
  i = MIN(i, 32);
  self->client_info.pointer_cache_entries = i;
  return 0;
}

/*****************************************************************************/
int APP_CC
xrdp_rdp_process_confirm_active(struct xrdp_rdp* self, struct stream* s)
{
  int cap_len;
  int source_len;
  int num_caps;
  int index;
  int type;
  int len;
  char* p;

  DEBUG(("in xrdp_rdp_process_confirm_active\r\n"));
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
    switch (type)
    {
      case RDP_CAPSET_GENERAL: /* 1 */
        DEBUG(("RDP_CAPSET_GENERAL\r\n"));
        xrdp_process_capset_general(self, s, len);
        break;
      case RDP_CAPSET_BITMAP: /* 2 */
        DEBUG(("RDP_CAPSET_BITMAP\r\n"));
        break;
      case RDP_CAPSET_ORDER: /* 3 */
        DEBUG(("RDP_CAPSET_ORDER\r\n"));
        xrdp_process_capset_order(self, s, len);
        break;
      case RDP_CAPSET_BMPCACHE: /* 4 */
        DEBUG(("RDP_CAPSET_BMPCACHE\r\n"));
        xrdp_process_capset_bmpcache(self, s, len);
        break;
      case RDP_CAPSET_CONTROL: /* 5 */
        DEBUG(("RDP_CAPSET_CONTROL\r\n"));
        break;
      case RDP_CAPSET_ACTIVATE: /* 7 */
        DEBUG(("RDP_CAPSET_ACTIVATE\r\n"));
        break;
      case RDP_CAPSET_POINTER: /* 8 */
        DEBUG(("RDP_CAPSET_POINTER\r\n"));
        xrdp_process_capset_pointercache(self, s, len);
        break;
      case RDP_CAPSET_SHARE: /* 9 */
        DEBUG(("RDP_CAPSET_SHARE\r\n"));
        break;
      case RDP_CAPSET_COLCACHE: /* 10 */
        DEBUG(("RDP_CAPSET_COLCACHE\r\n"));
        break;
      case 12: /* 12 */
        DEBUG(("--12\r\n"));
        break;
      case 13: /* 13 */
        DEBUG(("--13\r\n"));
        break;
      case 14: /* 14 */
        DEBUG(("--14\r\n"));
        break;
      case 15: /* 15 */
        DEBUG(("--15\r\n"));
        break;
      case 16: /* 16 */
        DEBUG(("--16\r\n"));
        break;
      case 17: /* 17 */
        DEBUG(("--16\r\n"));
        break;
      case RDP_CAPSET_BMPCACHE2: /* 19 */
        DEBUG(("RDP_CAPSET_BMPCACHE2\r\n"));
        xrdp_process_capset_bmpcache2(self, s, len);
        break;
      case 20: /* 20 */
        DEBUG(("--20\r\n"));
        break;
      case 21: /* 21 */
        DEBUG(("--21\r\n"));
        break;
      default:
        g_printf("unknown in xrdp_rdp_process_confirm_active %d\r\n", type);
        break;
    }
    s->p = p + len;
  }
  DEBUG(("out xrdp_rdp_process_confirm_active\r\n"));
  return 0;
}

/*****************************************************************************/
static int APP_CC
xrdp_rdp_process_data_pointer(struct xrdp_rdp* self, struct stream* s)
{
  return 0;
}

/*****************************************************************************/
/* RDP_DATA_PDU_INPUT */
static int APP_CC
xrdp_rdp_process_data_input(struct xrdp_rdp* self, struct stream* s)
{
  int num_events;
  int index;
  int msg_type;
  int device_flags;
  int param1;
  int param2;
  int time;

  in_uint16_le(s, num_events);
  in_uint8s(s, 2); /* pad */
  DEBUG(("in xrdp_rdp_process_data_input %d events\r\n", num_events));
  for (index = 0; index < num_events; index++)
  {
    in_uint32_le(s, time);
    in_uint16_le(s, msg_type);
    in_uint16_le(s, device_flags);
    in_sint16_le(s, param1);
    in_sint16_le(s, param2);
    DEBUG(("xrdp_rdp_process_data_input event %4.4x flags %4.4x param1 %d \
param2 %d time %d\r\n", msg_type, device_flags, param1, param2, time));
    if (self->session->callback != 0)
    {
      /* msg_type can be
         RDP_INPUT_SYNCHRONIZE - 0
         RDP_INPUT_SCANCODE - 4
         RDP_INPUT_MOUSE - 0x8001 */
      /* call to xrdp_wm.c : callback */
      self->session->callback(self->session->id, msg_type, param1, param2,
                              device_flags, time);
    }
  }
  DEBUG(("out xrdp_rdp_process_data_input\r\n"));
  return 0;
}

/*****************************************************************************/
static int APP_CC
xrdp_rdp_send_synchronise(struct xrdp_rdp* self)
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
static int APP_CC
xrdp_rdp_send_control(struct xrdp_rdp* self, int action)
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
static int APP_CC
xrdp_rdp_process_data_control(struct xrdp_rdp* self, struct stream* s)
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
static int APP_CC
xrdp_rdp_process_data_sync(struct xrdp_rdp* self)
{
  return 0;
}

/*****************************************************************************/
static int APP_CC
xrdp_rdp_process_screen_update(struct xrdp_rdp* self, struct stream* s)
{
  int op;
  int left;
  int top;
  int right;
  int bottom;
  int cx;
  int cy;

  in_uint32_le(s, op);
  in_uint16_le(s, left);
  in_uint16_le(s, top);
  in_uint16_le(s, right);
  in_uint16_le(s, bottom);
  cx = (right - left) + 1;
  cy = (bottom - top) + 1;
  if (self->session->callback != 0)
  {
    self->session->callback(self->session->id, 0x4444, left, top, cx, cy);
  }
  return 0;
}

/*****************************************************************************/
static int APP_CC
xrdp_rdp_send_unknown1(struct xrdp_rdp* self)
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
static int APP_CC
xrdp_rdp_process_data_font(struct xrdp_rdp* self, struct stream* s)
{
  int seq;

  in_uint8s(s, 2); /* num of fonts */
  in_uint8s(s, 2); /* unknown */
  in_uint16_le(s, seq);
  /* 419 client sends Seq 1, then 2 */
  /* 2600 clients sends only Seq 3 */
  if (seq == 2 || seq == 3) /* after second font message, we are up and */
  {                                           /* running */
    xrdp_rdp_send_unknown1(self);
    self->session->up_and_running = 1;
  }
  return 0;
}

/*****************************************************************************/
/* sent 37 pdu */
static int APP_CC
xrdp_rdp_send_disconnect_query_response(struct xrdp_rdp* self)
{
  struct stream* s;

  make_stream(s);
  init_stream(s, 8192);
  if (xrdp_rdp_init_data(self, s) != 0)
  {
    free_stream(s);
    return 1;
  }
  s_mark_end(s);
  if (xrdp_rdp_send_data(self, s, 37) != 0)
  {
    free_stream(s);
    return 1;
  }
  free_stream(s);
  return 0;
}

#if 0 /* not used */
/*****************************************************************************/
/* sent RDP_DATA_PDU_DISCONNECT 47 pdu */
static int APP_CC
xrdp_rdp_send_disconnect_reason(struct xrdp_rdp* self, int reason)
{
  struct stream* s;

  make_stream(s);
  init_stream(s, 8192);
  if (xrdp_rdp_init_data(self, s) != 0)
  {
    free_stream(s);
    return 1;
  }
  out_uint32_le(s, reason);
  s_mark_end(s);
  if (xrdp_rdp_send_data(self, s, RDP_DATA_PDU_DISCONNECT) != 0)
  {
    free_stream(s);
    return 1;
  }
  free_stream(s);
  return 0;
}
#endif

/*****************************************************************************/
/* RDP_PDU_DATA */
int APP_CC
xrdp_rdp_process_data(struct xrdp_rdp* self, struct stream* s)
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
  DEBUG(("xrdp_rdp_process_data code %d\r\n", data_type));
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
    case 33: /* 33 ?? Invalidate an area I think */
      xrdp_rdp_process_screen_update(self, s);
      break;
    case 35:
      /* 35 ?? this comes when minimuzing a full screen mstsc.exe 2600 */
      /* I think this is saying the client no longer wants screen */
      /* updates and it will issue a 33 above to catch up */
      /* so minimized apps don't take bandwidth */
      break;
    case 36: /* 36 ?? disconnect query? */
      /* when this message comes, send a 37 back so the client */
      /* is sure the connection is alive and it can ask if user */
      /* really wants to disconnect */
      xrdp_rdp_send_disconnect_query_response(self); /* send a 37 back */
      break;
    case RDP_DATA_PDU_FONT2: /* 39 */
      xrdp_rdp_process_data_font(self, s);
      break;
    default:
      g_printf("unknown in xrdp_rdp_process_data %d\r\n", data_type);
      break;
  }
  return 0;
}

/*****************************************************************************/
int APP_CC
xrdp_rdp_disconnect(struct xrdp_rdp* self)
{
  return xrdp_sec_disconnect(self->sec_layer);
}

/*****************************************************************************/
int APP_CC
xrdp_rdp_send_deactive(struct xrdp_rdp* self)
{
  struct stream* s;

  make_stream(s);
  init_stream(s, 8192);
  if (xrdp_rdp_init(self, s) != 0)
  {
    free_stream(s);
    return 1;
  }
  s_mark_end(s);
  if (xrdp_rdp_send(self, s, RDP_PDU_DEACTIVATE) != 0)
  {
    free_stream(s);
    return 1;
  }
  free_stream(s);
  return 0;
}
