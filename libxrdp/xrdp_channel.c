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
   Copyright (C) Jay Sorg 2006-2010

   channel layer

*/

#include "libxrdp.h"

/* todo, move these to constants.h */
//#define CHANNEL_CHUNK_LENGTH 1600 /* todo, why is this so small? */
#define CHANNEL_CHUNK_LENGTH 8192
#define CHANNEL_FLAG_FIRST 0x01
#define CHANNEL_FLAG_LAST 0x02
#define CHANNEL_FLAG_SHOW_PROTOCOL 0x10

/*****************************************************************************/
/* returns pointer or nil on error */
static struct mcs_channel_item* APP_CC
xrdp_channel_get_item(struct xrdp_channel* self, int channel_id)
{
  struct mcs_channel_item* channel;
  if(self->mcs_layer->channel_list==NULL)
  {
    g_writeln("xrdp_channel_get_item - No channel initialized");
    return NULL ;
  }
  channel = (struct mcs_channel_item*)
               list_get_item(self->mcs_layer->channel_list, channel_id);
  return channel;
}

/*****************************************************************************/
struct xrdp_channel* APP_CC
xrdp_channel_create(struct xrdp_sec* owner, struct xrdp_mcs* mcs_layer)
{
  struct xrdp_channel* self;

  self = (struct xrdp_channel*)g_malloc(sizeof(struct xrdp_channel), 1);
  self->sec_layer = owner;
  self->mcs_layer = mcs_layer;
  return self;
}

/*****************************************************************************/
/* returns error */
void APP_CC
xrdp_channel_delete(struct xrdp_channel* self)
{
  if (self == 0)
  {
    return;
  }
  memset(self,0,sizeof(struct xrdp_channel));
  g_free(self);
}

/*****************************************************************************/
/* returns error */
int APP_CC
xrdp_channel_init(struct xrdp_channel* self, struct stream* s)
{
  if (xrdp_sec_init(self->sec_layer, s) != 0)
  {
    return 1;
  }
  s_push_layer(s, channel_hdr, 8);
  return 0;
}

/*****************************************************************************/
/* returns error */
/* This sends data out to the secure layer. */
int APP_CC
xrdp_channel_send(struct xrdp_channel* self, struct stream* s, int channel_id,
                  int total_data_len, int flags)
{
  struct mcs_channel_item* channel;

  channel = xrdp_channel_get_item(self, channel_id);
  if (channel == NULL)
  {
    g_writeln("xrdp_channel_send - no such channel");  
    return 1;
  }
  s_pop_layer(s, channel_hdr);
  out_uint32_le(s, total_data_len);
  if (channel->flags & CHANNEL_OPTION_SHOW_PROTOCOL)
  {
    flags |= CHANNEL_FLAG_SHOW_PROTOCOL;
  }
  out_uint32_le(s, flags);
  if (xrdp_sec_send(self->sec_layer, s, channel->chanid) != 0)
  {
    g_writeln("xrdp_channel_send - failure sending data");  
    return 1;
  }
  return 0;
}

/*****************************************************************************/
/* returns error */
/* this will inform the callback, whatever it is that some channel data is
   ready.  the default for this is a call to xrdp_wm.c. */
static int APP_CC
xrdp_channel_call_callback(struct xrdp_channel* self, struct stream* s,
                           int channel_id,
                           int total_data_len, int flags)
{
  struct xrdp_session* session;
  int rv;
  int size;

  rv = 0;
  session = self->sec_layer->rdp_layer->session;
  if (session != 0)
  {
    if (session->callback != 0)
    {
      size = (int)(s->end - s->p);
      /* in xrdp_wm.c */
      rv = session->callback(session->id, 0x5555,
                             MAKELONG(channel_id, flags),
                             size, (tbus)(s->p), total_data_len);
    }
    else
    {
      g_writeln("in xrdp_channel_call_callback, session->callback is nil");
    }
  }
  else
  {
    g_writeln("in xrdp_channel_call_callback, session is nil");
  }
  return rv;
}

/*****************************************************************************/
/* returns error */
/* This is called from the secure layer to process an incomming non global
   channel packet.
   'chanid' passed in here is the mcs channel id so it MCS_GLOBAL_CHANNEL
   plus something. */
int APP_CC
xrdp_channel_process(struct xrdp_channel* self, struct stream* s,
                     int chanid)
{
  int length;
  int flags;
  int rv;
  int channel_id;
  struct mcs_channel_item* channel;


  /* this assumes that the channels are in order of chanid(mcs channel id)
     but they should be, see xrdp_sec_process_mcs_data_channels
     the first channel should be MCS_GLOBAL_CHANNEL + 1, second
     one should be MCS_GLOBAL_CHANNEL + 2, and so on */
  channel_id = (chanid - MCS_GLOBAL_CHANNEL) - 1;
  channel = xrdp_channel_get_item(self, channel_id);
  if (channel == NULL)
  {
    g_writeln("xrdp_channel_process, channel not found");
    return 1;
  }
  rv = 0;
  in_uint32_le(s, length);
  in_uint32_le(s, flags);
  rv = xrdp_channel_call_callback(self, s, channel_id, length, flags);
  return rv;
}
