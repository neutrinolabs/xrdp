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
   Copyright (C) Jay Sorg 2004-2010

   rdp layer

*/

#include "libxrdp.h"

#if defined(XRDP_FREERDP1)
#include <freerdp/codec/mppc_enc.h>
#endif

/* some compilers need unsigned char to avoid warnings */
static tui8 g_unknown1[172] =
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

/* some compilers need unsigned char to avoid warnings */
/*
static tui8 g_unknown2[8] =
{ 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x04, 0x00 };
*/

/*****************************************************************************/
static int APP_CC
xrdp_rdp_read_config(struct xrdp_client_info* client_info)
{
  int index = 0;
  struct list* items = (struct list *)NULL;
  struct list* values = (struct list *)NULL;
  char* item = (char *)NULL;
  char* value = (char *)NULL;
  char cfg_file[256];

  /* initialize (zero out) local variables: */
  g_memset(cfg_file,0,sizeof(char) * 256);

  items = list_create();
  items->auto_free = 1;
  values = list_create();
  values->auto_free = 1;
  g_snprintf(cfg_file, 255, "%s/xrdp.ini", XRDP_CFG_PATH);
  file_by_name_read_section(cfg_file, "globals", items, values);
  for (index = 0; index < items->count; index++)
  {
    item = (char*)list_get_item(items, index);
    value = (char*)list_get_item(values, index);
    if (g_strcasecmp(item, "bitmap_cache") == 0)
    {
      if ((g_strcasecmp(value, "yes") == 0) ||
          (g_strcasecmp(value, "true") == 0) ||
          (g_strcasecmp(value, "1") == 0))
      {
        client_info->use_bitmap_cache = 1;
      }
    }
    else if (g_strcasecmp(item, "bitmap_compression") == 0)
    {
      if (g_strcasecmp(value, "yes") == 0 ||
          g_strcasecmp(value, "true") == 0 ||
          g_strcasecmp(value, "1") == 0)
      {
        client_info->use_bitmap_comp = 1;
      }
    }
    else if (g_strcasecmp(item, "crypt_level") == 0)
    {
      if (g_strcasecmp(value, "low") == 0)
      {
        client_info->crypt_level = 1;
      }
      else if (g_strcasecmp(value, "medium") == 0)
      {
        client_info->crypt_level = 2;
      }
      else if (g_strcasecmp(value, "high") == 0)
      {
        client_info->crypt_level = 3;
      }
      else
      {
        g_writeln("Warning: Your configured crypt level is"
		  "undefined 'high' will be used");
        client_info->crypt_level = 3;  
      }	  
    }
    else if (g_strcasecmp(item, "channel_code") == 0)
    {
      if ((g_strcasecmp(value, "yes") == 0) ||
          (g_strcasecmp(value, "1") == 0) ||
          (g_strcasecmp(value, "true") == 0))      
      {
        client_info->channel_code = 1;
      }
      else
      {
        g_writeln("Info: All channels are disabled");
      }
    }
    else if (g_strcasecmp(item, "max_bpp") == 0)
    {
      client_info->max_bpp = g_atoi(value);
    }
  }
  list_delete(items);
  list_delete(values);
  return 0;
}

/*****************************************************************************/
struct xrdp_rdp* APP_CC
xrdp_rdp_create(struct xrdp_session* session, struct trans* trans)
{
  struct xrdp_rdp* self = (struct xrdp_rdp *)NULL;
  int bytes;

  DEBUG(("in xrdp_rdp_create"));
  self = (struct xrdp_rdp*)g_malloc(sizeof(struct xrdp_rdp), 1);
  self->session = session;
  self->share_id = 66538;
  /* read ini settings */
  xrdp_rdp_read_config(&self->client_info);
  /* create sec layer */
  self->sec_layer = xrdp_sec_create(self, trans, self->client_info.crypt_level,
                                    self->client_info.channel_code);
  /* default 8 bit v1 color bitmap cache entries and size */
  self->client_info.cache1_entries = 600;
  self->client_info.cache1_size = 256;
  self->client_info.cache2_entries = 300;
  self->client_info.cache2_size = 1024;
  self->client_info.cache3_entries = 262;
  self->client_info.cache3_size = 4096;
  /* load client ip info */
  bytes = sizeof(self->client_info.client_ip) - 1;
  g_write_ip_address(trans->sck, self->client_info.client_ip, bytes);
#if defined(XRDP_FREERDP1)
  self->mppc_enc = mppc_enc_new(PROTO_RDP_50);
#endif
  self->client_info.size = sizeof(self->client_info);
  DEBUG(("out xrdp_rdp_create"));
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
#if defined(XRDP_FREERDP1)
  mppc_enc_free((struct rdp_mppc_enc*)(self->mppc_enc));
#endif
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
  int error = 0;
  int len = 0;
  int pdu_code = 0;
  int chan = 0;

  DEBUG(("in xrdp_rdp_recv"));
  if (s->next_packet == 0 || s->next_packet >= s->end)
  {
    chan = 0;
    error = xrdp_sec_recv(self->sec_layer, s, &chan);
    if (error == -1) /* special code for send demand active */
    {
      s->next_packet = 0;
      *code = -1;
      DEBUG(("out (1) xrdp_rdp_recv"));
      return 0;
    }
    if (error != 0)
    {
      DEBUG(("out xrdp_rdp_recv error"));
      return 1;
    }
    if ((chan != MCS_GLOBAL_CHANNEL) && (chan > 0))
    {
      if (chan > MCS_GLOBAL_CHANNEL)
      {
        if(xrdp_channel_process(self->sec_layer->chan_layer, s, chan)!=0)
        {
          g_writeln("xrdp_channel_process returned unhandled error") ;
        }
      }else{
        g_writeln("Wrong channel Id to be handled by xrdp_channel_process %d",chan);
      }
      s->next_packet = 0;
      *code = 0;
      DEBUG(("out (2) xrdp_rdp_recv"));
      return 0;
    }
    s->next_packet = s->p;
  }
  else
  {
    DEBUG(("xrdp_rdp_recv stream not touched"))
    s->p = s->next_packet;
  }
  if (!s_check_rem(s, 6))
  {
    s->next_packet = 0;
    *code = 0;
    DEBUG(("out (3) xrdp_rdp_recv"));
    len = (int)(s->end - s->p);
    g_writeln("xrdp_rdp_recv: bad RDP packet, length [%d]", len);
    return 0;
  }else{
    in_uint16_le(s, len);
    /*g_writeln("New len received : %d next packet: %d s_end: %d",len,s->next_packet,s->end); */
    in_uint16_le(s, pdu_code);
    *code = pdu_code & 0xf;
    in_uint8s(s, 2); /* mcs user id */
    s->next_packet += len;
    DEBUG(("out (4) xrdp_rdp_recv"));
    return 0;
  }
}

/*****************************************************************************/
int APP_CC
xrdp_rdp_send(struct xrdp_rdp* self, struct stream* s, int pdu_type)
{
  int len = 0;

  DEBUG(("in xrdp_rdp_send"));
  s_pop_layer(s, rdp_hdr);
  len = s->end - s->p;
  out_uint16_le(s, len);
  out_uint16_le(s, 0x10 | pdu_type);
  out_uint16_le(s, self->mcs_channel);
  if (xrdp_sec_send(self->sec_layer, s, MCS_GLOBAL_CHANNEL) != 0)
  {
    DEBUG(("out xrdp_rdp_send error"));
    return 1;
  }
  DEBUG(("out xrdp_rdp_send"));
  return 0;
}

/*****************************************************************************/
int APP_CC
xrdp_rdp_send_data(struct xrdp_rdp* self, struct stream* s,
                   int data_pdu_type)
{
  int len;
  int ctype;
  int clen;
  int dlen;
  int pdulen;
  int pdutype;
  int tocomplen;
  int iso_offset;
  int mcs_offset;
  int sec_offset;
  int rdp_offset;
  struct stream ls;
#if defined(XRDP_FREERDP1)
  struct rdp_mppc_enc* mppc_enc;
#endif

  DEBUG(("in xrdp_rdp_send_data"));
  s_pop_layer(s, rdp_hdr);
  len = (int)(s->end - s->p);
  pdutype = 0x10 | RDP_PDU_DATA;
  pdulen = len;
  dlen = len;
  ctype = 0;
  clen = len;
  tocomplen = pdulen - 18;
#if defined(XRDP_FREERDP1)
  if (self->client_info.rdp_compression && self->session->up_and_running)
  {
    mppc_enc = (struct rdp_mppc_enc*)(self->mppc_enc);
    if (compress_rdp(mppc_enc, s->p + 18, tocomplen))
    {
      DEBUG(("mppc_encode ok flags 0x%x bytes_in_opb %d historyOffset %d "
             "tocomplen %d", mppc_enc->flags, mppc_enc->bytes_in_opb,
             mppc_enc->historyOffset, tocomplen));
      if (mppc_enc->flags & RDP_MPPC_COMPRESSED)
      {
        clen = mppc_enc->bytes_in_opb + 18;
        pdulen = clen;
        ctype = mppc_enc->flags;
        iso_offset = (int)(s->iso_hdr - s->data);
        mcs_offset = (int)(s->mcs_hdr - s->data);
        sec_offset = (int)(s->sec_hdr - s->data);
        rdp_offset = (int)(s->rdp_hdr - s->data);

        /* outputBuffer has 64 bytes preceding it */
        ls.data = mppc_enc->outputBuffer - (rdp_offset + 18);
        ls.p = ls.data + rdp_offset;
        ls.end = ls.p + clen;
        ls.size = clen;
        ls.iso_hdr = ls.data + iso_offset;
        ls.mcs_hdr = ls.data + mcs_offset;
        ls.sec_hdr = ls.data + sec_offset;
        ls.rdp_hdr = ls.data + rdp_offset;
        ls.channel_hdr = 0;
        ls.next_packet = 0;
        s = &ls;
      }
    }
    else
    {
      g_writeln("mppc_encode not ok");
    }
  }
#endif
  out_uint16_le(s, pdulen);
  out_uint16_le(s, pdutype);
  out_uint16_le(s, self->mcs_channel);
  out_uint32_le(s, self->share_id);
  out_uint8(s, 0);
  out_uint8(s, 1);
  out_uint16_le(s, dlen);
  out_uint8(s, data_pdu_type);
  out_uint8(s, ctype);
  out_uint16_le(s, clen);

  if (xrdp_sec_send(self->sec_layer, s, MCS_GLOBAL_CHANNEL) != 0)
  {
    DEBUG(("out xrdp_rdp_send_data error"));
    return 1;
  }
  DEBUG(("out xrdp_rdp_send_data"));
  return 0;
}

/*****************************************************************************/
int APP_CC
xrdp_rdp_send_data_update_sync(struct xrdp_rdp* self)
{
  struct stream * s = (struct stream *)NULL;

  make_stream(s);
  init_stream(s, 8192);
  DEBUG(("in xrdp_rdp_send_data_update_sync"));
  if (xrdp_rdp_init_data(self, s) != 0)
  {
    DEBUG(("out xrdp_rdp_send_data_update_sync error"));
    free_stream(s);
    return 1;
  }
  out_uint16_le(s, RDP_UPDATE_SYNCHRONIZE);
  out_uint8s(s, 2);
  s_mark_end(s);
  if (xrdp_rdp_send_data(self, s, RDP_DATA_PDU_UPDATE) != 0)
  {
    DEBUG(("out xrdp_rdp_send_data_update_sync error"));
    free_stream(s);
    return 1;
  }
  DEBUG(("out xrdp_rdp_send_data_update_sync"));
  free_stream(s);
  return 0;
}

/*****************************************************************************/
static int APP_CC
xrdp_rdp_parse_client_mcs_data(struct xrdp_rdp* self)
{
  struct stream* p = (struct stream *)NULL;
  int i = 0;

  p = &(self->sec_layer->client_mcs_data);
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
  if (self->client_info.max_bpp > 0)
  {
    if (self->client_info.bpp > self->client_info.max_bpp)
    {
      self->client_info.bpp = self->client_info.max_bpp;
    }
  }
  p->p = p->data;
  DEBUG(("client width %d, client height %d bpp %d",
         self->client_info.width, self->client_info.height,
         self->client_info.bpp));
  return 0;
}

/*****************************************************************************/
int APP_CC
xrdp_rdp_incoming(struct xrdp_rdp* self)
{
  DEBUG(("in xrdp_rdp_incoming"));
  if (xrdp_sec_incoming(self->sec_layer) != 0)
  {
    return 1;
  }
  self->mcs_channel = self->sec_layer->mcs_layer->userid +
                      MCS_USERCHANNEL_BASE;
  xrdp_rdp_parse_client_mcs_data(self);
  DEBUG(("out xrdp_rdp_incoming mcs channel %d", self->mcs_channel));
  return 0;
}

/*****************************************************************************/
int APP_CC
xrdp_rdp_send_demand_active(struct xrdp_rdp* self)
{
  struct stream* s;
  int caps_count;
  int caps_size;
  char* caps_count_ptr;
  char* caps_size_ptr;
  char* caps_ptr;

  make_stream(s);
  init_stream(s, 8192);
  if (xrdp_rdp_init(self, s) != 0)
  {
    free_stream(s);
    return 1;
  }

  caps_count = 0;
  out_uint32_le(s, self->share_id);
  out_uint16_le(s, 4); /* 4 chars for RDP\0 */
  /* 2 bytes size after num caps, set later */
  caps_size_ptr = s->p;
  out_uint8s(s, 2);
  out_uint8a(s, "RDP", 4);
  /* 4 byte num caps, set later */
  caps_count_ptr = s->p;
  out_uint8s(s, 4);
  caps_ptr = s->p;

  /* Output share capability set */
  caps_count++;
  out_uint16_le(s, RDP_CAPSET_SHARE);
  out_uint16_le(s, RDP_CAPLEN_SHARE);
  out_uint16_le(s, self->mcs_channel);
  out_uint16_be(s, 0xb5e2); /* 0x73e1 */

  /* Output general capability set */
  caps_count++;
  out_uint16_le(s, RDP_CAPSET_GENERAL); /* 1 */
  out_uint16_le(s, RDP_CAPLEN_GENERAL); /* 24(0x18) */
  out_uint16_le(s, 1); /* OS major type */
  out_uint16_le(s, 3); /* OS minor type */
  out_uint16_le(s, 0x200); /* Protocol version */
  out_uint16_le(s, 0); /* pad */
  out_uint16_le(s, 0); /* Compression types */
  //out_uint16_le(s, 0); /* pad use 0x40d for rdp packets, 0 for not */
  out_uint16_le(s, 0x40d); /* pad use 0x40d for rdp packets, 0 for not */
  out_uint16_le(s, 0); /* Update capability */
  out_uint16_le(s, 0); /* Remote unshare capability */
  out_uint16_le(s, 0); /* Compression level */
  out_uint16_le(s, 0); /* Pad */

  /* Output bitmap capability set */
  caps_count++;
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

  /* Output font capability set */
  caps_count++;
  out_uint16_le(s, RDP_CAPSET_FONT); /* 14 */
  out_uint16_le(s, RDP_CAPLEN_FONT); /* 4 */

  /* Output order capability set */
  caps_count++;
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
  out_uint8(s, 1); /* mem blt */
  out_uint8(s, 0); /* tri blt */
  out_uint8(s, 0); /* unused */
  out_uint8(s, 0); /* unused */
  out_uint8(s, 0); /* nine grid */
  out_uint8(s, 1); /* line to */
  out_uint8(s, 0); /* multi nine grid */
  out_uint8(s, 1); /* rect */
  out_uint8(s, 0); /* desk save */
  out_uint8(s, 0); /* unused */
  out_uint8(s, 0); /* unused */
  out_uint8(s, 0); /* unused */
  out_uint8(s, 0); /* multi dest blt */
  out_uint8(s, 0); /* multi pat blt */
  out_uint8(s, 0); /* multi screen blt */
  out_uint8(s, 1); /* multi rect */
  out_uint8(s, 0); /* fast index */
  out_uint8(s, 0); /* polygonSC ([MS-RDPEGDI], 2.2.2.2.1.1.2.16) */
  out_uint8(s, 0); /* polygonCB ([MS-RDPEGDI], 2.2.2.2.1.1.2.17) */
  out_uint8(s, 0); /* polyline */
  out_uint8(s, 0); /* unused */
  out_uint8(s, 0); /* fast glyph */
  out_uint8(s, 0); /* ellipse */
  out_uint8(s, 0); /* ellipse */
  out_uint8(s, 0); /* ? */
  out_uint8(s, 0); /* unused */
  out_uint8(s, 0); /* unused */
  out_uint8(s, 0); /* unused */
  out_uint8(s, 0); /* unused */
  out_uint16_le(s, 0x6a1);
  out_uint8s(s, 2); /* ? */
  out_uint32_le(s, 0x0f4240); /* desk save */
  out_uint32_le(s, 0x0f4240); /* desk save */
  out_uint32_le(s, 1); /* ? */
  out_uint32_le(s, 0); /* ? */

  /* Output color cache capability set */
  caps_count++;
  out_uint16_le(s, RDP_CAPSET_COLCACHE);
  out_uint16_le(s, RDP_CAPLEN_COLCACHE);
  out_uint16_le(s, 6); /* cache size */
  out_uint16_le(s, 0); /* pad */

  /* Output pointer capability set */
  caps_count++;
  out_uint16_le(s, RDP_CAPSET_POINTER);
  out_uint16_le(s, RDP_CAPLEN_POINTER);
  out_uint16_le(s, 1); /* Colour pointer */
  out_uint16_le(s, 0x19); /* Cache size */
  out_uint16_le(s, 0x19); /* Cache size */

  /* Output input capability set */
  caps_count++;
  out_uint16_le(s, RDP_CAPSET_INPUT); /* 13(0xd) */
  out_uint16_le(s, RDP_CAPLEN_INPUT); /* 88(0x58) */
  out_uint8(s, 1);
  out_uint8s(s, 83);

  out_uint8s(s, 4); /* pad */

  s_mark_end(s);

  caps_size = (int)(s->end - caps_ptr);
  caps_size_ptr[0] = caps_size;
  caps_size_ptr[1] = caps_size >> 8;

  caps_count_ptr[0] = caps_count;
  caps_count_ptr[1] = caps_count >> 8;
  caps_count_ptr[2] = caps_count >> 16;
  caps_count_ptr[3] = caps_count >> 24;

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
  char order_caps[32];

  DEBUG(("order capabilities"));
  in_uint8s(s, 20); /* Terminal desc, pad */
  in_uint8s(s, 2); /* Cache X granularity */
  in_uint8s(s, 2); /* Cache Y granularity */
  in_uint8s(s, 2); /* Pad */
  in_uint8s(s, 2); /* Max order level */
  in_uint8s(s, 2); /* Number of fonts */
  in_uint8s(s, 2); /* Capability flags */
  in_uint8a(s, order_caps, 32); /* Orders supported */
  DEBUG(("dest blt-0 %d", order_caps[0]));
  DEBUG(("pat blt-1 %d", order_caps[1]));
  DEBUG(("screen blt-2 %d", order_caps[2]));
  DEBUG(("memblt-3-13 %d %d", order_caps[3], order_caps[13]));
  DEBUG(("triblt-4-14 %d %d", order_caps[4], order_caps[14]));
  DEBUG(("line-8 %d", order_caps[8]));
  DEBUG(("line-9 %d", order_caps[9]));
  DEBUG(("rect-10 %d", order_caps[10]));
  DEBUG(("desksave-11 %d", order_caps[11]));
  DEBUG(("polygon-20 %d", order_caps[20]));
  DEBUG(("polygon2-21 %d", order_caps[21]));
  DEBUG(("polyline-22 %d", order_caps[22]));
  DEBUG(("ellipse-25 %d", order_caps[25]));
  DEBUG(("ellipse2-26 %d", order_caps[26]));
  DEBUG(("text2-27 %d", order_caps[27]));
  DEBUG(("order_caps dump"));
#if defined(XRDP_DEBUG)
  g_hexdump(order_caps, 32);
#endif
  in_uint8s(s, 2); /* Text capability flags */
  in_uint8s(s, 6); /* Pad */
  in_uint32_le(s, i); /* desktop cache size, usually 0x38400 */
  self->client_info.desktop_cache = i;
  DEBUG(("desktop cache size %d", i));
  in_uint8s(s, 4); /* Unknown */
  in_uint8s(s, 4); /* Unknown */
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
  DEBUG(("cache1 entries %d size %d", self->client_info.cache1_entries,
         self->client_info.cache1_size));
  DEBUG(("cache2 entries %d size %d", self->client_info.cache2_entries,
         self->client_info.cache2_size));
  DEBUG(("cache3 entries %d size %d", self->client_info.cache3_entries,
         self->client_info.cache3_size));
  return 0;
}

/*****************************************************************************/
/* get the bitmap cache size */
static int APP_CC
xrdp_process_capset_bmpcache2(struct xrdp_rdp* self, struct stream* s,
                              int len)
{
  int Bpp = 0;
  int i = 0;

  self->client_info.bitmap_cache_version = 2;
  Bpp = (self->client_info.bpp + 7) / 8;
  in_uint16_le(s, i); /* cache flags */
#if defined(XRDP_JPEG)
  if (i & 0x80)
  {
    g_writeln("xrdp_process_capset_bmpcache2: client supports jpeg");
    self->client_info.jpeg = 1;
  }
#endif
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
  DEBUG(("cache1 entries %d size %d", self->client_info.cache1_entries,
         self->client_info.cache1_size));
  DEBUG(("cache2 entries %d size %d", self->client_info.cache2_entries,
         self->client_info.cache2_size));
  DEBUG(("cache3 entries %d size %d", self->client_info.cache3_entries,
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
/* get the type of client brush cache */
static int APP_CC
xrdp_process_capset_brushcache(struct xrdp_rdp* self, struct stream* s,
                               int len)
{
  int code;

  in_uint32_le(s, code);
  self->client_info.brush_cache_code = code;
  return 0;
}

/*****************************************************************************/
static int APP_CC
xrdp_process_offscreen_bmpcache(struct xrdp_rdp* self, struct stream* s,
                                int len)
{
  int i32;

  if (len - 4 < 8)
  {
    g_writeln("xrdp_process_offscreen_bmpcache: bad len");
    return 1;
  }
  in_uint32_le(s, i32);
  self->client_info.offscreen_support_level = i32;
  in_uint16_le(s, i32);
  self->client_info.offscreen_cache_size = i32 * 1024;
  in_uint16_le(s, i32);
  self->client_info.offscreen_cache_entries = i32;
  g_writeln("xrdp_process_offscreen_bmpcache: support level %d "
            "cache size %d MB cache entries %d",
            self->client_info.offscreen_support_level,
            self->client_info.offscreen_cache_size,
            self->client_info.offscreen_cache_entries);
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

  DEBUG(("in xrdp_rdp_process_confirm_active"));
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
        DEBUG(("RDP_CAPSET_GENERAL"));
        xrdp_process_capset_general(self, s, len);
        break;
      case RDP_CAPSET_BITMAP: /* 2 */
        DEBUG(("RDP_CAPSET_BITMAP"));
        break;
      case RDP_CAPSET_ORDER: /* 3 */
        DEBUG(("RDP_CAPSET_ORDER"));
        xrdp_process_capset_order(self, s, len);
        break;
      case RDP_CAPSET_BMPCACHE: /* 4 */
        DEBUG(("RDP_CAPSET_BMPCACHE"));
        xrdp_process_capset_bmpcache(self, s, len);
        break;
      case RDP_CAPSET_CONTROL: /* 5 */
        DEBUG(("RDP_CAPSET_CONTROL"));
        break;
      case RDP_CAPSET_ACTIVATE: /* 7 */
        DEBUG(("RDP_CAPSET_ACTIVATE"));
        break;
      case RDP_CAPSET_POINTER: /* 8 */
        DEBUG(("RDP_CAPSET_POINTER"));
        xrdp_process_capset_pointercache(self, s, len);
        break;
      case RDP_CAPSET_SHARE: /* 9 */
        DEBUG(("RDP_CAPSET_SHARE"));
        break;
      case RDP_CAPSET_COLCACHE: /* 10 */
        DEBUG(("RDP_CAPSET_COLCACHE"));
        break;
      case 12: /* 12 */
        DEBUG(("--12"));
        break;
      case 13: /* 13 */
        DEBUG(("--13"));
        break;
      case 14: /* 14 */
        DEBUG(("--14"));
        break;
      case RDP_CAPSET_BRUSHCACHE: /* 15 */
        xrdp_process_capset_brushcache(self, s, len);
        break;
      case 16: /* 16 */
        DEBUG(("--16"));
        break;
      case 17: /* 17 */
        DEBUG(("CAPSET_TYPE_OFFSCREEN_CACHE"));
        xrdp_process_offscreen_bmpcache(self, s, len);
        break;
      case RDP_CAPSET_BMPCACHE2: /* 19 */
        DEBUG(("RDP_CAPSET_BMPCACHE2"));
        xrdp_process_capset_bmpcache2(self, s, len);
        break;
      case 20: /* 20 */
        DEBUG(("--20"));
        break;
      case 21: /* 21 */
        DEBUG(("--21"));
        break;
      case 22: /* 22 */
        DEBUG(("--22"));
        break;
      case 26: /* 26 */
        DEBUG(("--26"));
        break;
      default:
        g_writeln("unknown in xrdp_rdp_process_confirm_active %d", type);
        break;
    }
    s->p = p + len;
  }
  DEBUG(("out xrdp_rdp_process_confirm_active"));
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
  DEBUG(("in xrdp_rdp_process_data_input %d events", num_events));
  for (index = 0; index < num_events; index++)
  {
    in_uint32_le(s, time);
    in_uint16_le(s, msg_type);
    in_uint16_le(s, device_flags);
    in_sint16_le(s, param1);
    in_sint16_le(s, param2);
    DEBUG(("xrdp_rdp_process_data_input event %4.4x flags %4.4x param1 %d \
param2 %d time %d", msg_type, device_flags, param1, param2, time));
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
  DEBUG(("out xrdp_rdp_process_data_input"));
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

  DEBUG(("xrdp_rdp_process_data_control"));
  in_uint16_le(s, action);
  in_uint8s(s, 2); /* user id */
  in_uint8s(s, 4); /* control id */
  if (action == RDP_CTL_REQUEST_CONTROL)
  {
    DEBUG(("xrdp_rdp_process_data_control got RDP_CTL_REQUEST_CONTROL"));
    DEBUG(("xrdp_rdp_process_data_control calling xrdp_rdp_send_synchronise"));
    xrdp_rdp_send_synchronise(self);
    DEBUG(("xrdp_rdp_process_data_control sending RDP_CTL_COOPERATE"));
    xrdp_rdp_send_control(self, RDP_CTL_COOPERATE);
    DEBUG(("xrdp_rdp_process_data_control sending RDP_CTL_GRANT_CONTROL"));
    xrdp_rdp_send_control(self, RDP_CTL_GRANT_CONTROL);
  }
  else
  {
    DEBUG(("xrdp_rdp_process_data_control unknown action"));
  }
  return 0;
}

/*****************************************************************************/
static int APP_CC
xrdp_rdp_process_data_sync(struct xrdp_rdp* self)
{
  DEBUG(("xrdp_rdp_process_data_sync"));
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
  out_uint8a(s, g_unknown1, 172);
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

  DEBUG(("in xrdp_rdp_process_data_font"));
  in_uint8s(s, 2); /* num of fonts */
  in_uint8s(s, 2); /* unknown */
  in_uint16_le(s, seq);
  /* 419 client sends Seq 1, then 2 */
  /* 2600 clients sends only Seq 3 */
  if (seq == 2 || seq == 3) /* after second font message, we are up and */
  {                                           /* running */
    DEBUG(("sending unknown1"));
    xrdp_rdp_send_unknown1(self);
    self->session->up_and_running = 1;
    DEBUG(("up_and_running set"));
    xrdp_rdp_send_data_update_sync(self);
  }
  DEBUG(("out xrdp_rdp_process_data_font"));
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
  DEBUG(("xrdp_rdp_process_data code %d", data_type));
  switch (data_type)
  {
    case RDP_DATA_PDU_POINTER: /* 27(0x1b) */
      xrdp_rdp_process_data_pointer(self, s);
      break;
    case RDP_DATA_PDU_INPUT: /* 28(0x1c) */
      xrdp_rdp_process_data_input(self, s);
      break;
    case RDP_DATA_PDU_CONTROL: /* 20(0x14) */
      xrdp_rdp_process_data_control(self, s);
      break;
    case RDP_DATA_PDU_SYNCHRONISE: /* 31(0x1f) */
      xrdp_rdp_process_data_sync(self);
      break;
    case 33: /* 33(0x21) ?? Invalidate an area I think */
      xrdp_rdp_process_screen_update(self, s);
      break;
    case 35: /* 35(0x23) */
      /* 35 ?? this comes when minimuzing a full screen mstsc.exe 2600 */
      /* I think this is saying the client no longer wants screen */
      /* updates and it will issue a 33 above to catch up */
      /* so minimized apps don't take bandwidth */
      break;
    case 36: /* 36(0x24) ?? disconnect query? */
      /* when this message comes, send a 37 back so the client */
      /* is sure the connection is alive and it can ask if user */
      /* really wants to disconnect */
      xrdp_rdp_send_disconnect_query_response(self); /* send a 37 back */
      break;
    case RDP_DATA_PDU_FONT2: /* 39(0x27) */
      xrdp_rdp_process_data_font(self, s);
      break;
    default:
      g_writeln("unknown in xrdp_rdp_process_data %d", data_type);
      break;
  }
  return 0;
}

/*****************************************************************************/
int APP_CC
xrdp_rdp_disconnect(struct xrdp_rdp* self)
{
  int rv;

  DEBUG(("in xrdp_rdp_disconnect"));
  rv = xrdp_sec_disconnect(self->sec_layer);
  DEBUG(("out xrdp_rdp_disconnect"));
  return rv;
}

/*****************************************************************************/
int APP_CC
xrdp_rdp_send_deactive(struct xrdp_rdp* self)
{
  struct stream* s;

  DEBUG(("in xrdp_rdp_send_deactive"));
  make_stream(s);
  init_stream(s, 8192);
  if (xrdp_rdp_init(self, s) != 0)
  {
    free_stream(s);
    DEBUG(("out xrdp_rdp_send_deactive error"));
    return 1;
  }
  s_mark_end(s);
  if (xrdp_rdp_send(self, s, RDP_PDU_DEACTIVATE) != 0)
  {
    free_stream(s);
    DEBUG(("out xrdp_rdp_send_deactive error"));
    return 1;
  }
  free_stream(s);
  DEBUG(("out xrdp_rdp_send_deactive"));
  return 0;
}
