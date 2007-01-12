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
   Copyright (C) Jay Sorg 2005-2007
*/

/**
 * 
 * @file libscp_v1c.c
 * @brief libscp version 1 client api code
 * @author Simone Fedele
 * 
 */

#include "libscp_v1c.h"

static enum SCP_CLIENT_STATES_E _scp_v1c_check_response(struct SCP_CONNECTION* c, struct SCP_SESSION* s);

/* client API */
/* 001 */ 
enum SCP_CLIENT_STATES_E scp_v1c_connect(struct SCP_CONNECTION* c, struct SCP_SESSION* s)
{
  unsigned char sz;
  uint32_t size;
  //uint32_t version;
  //uint16_t cmd;
  //uint16_t dim;

  init_stream(c->out_s, c->out_s->size);
  init_stream(c->in_s, c->in_s->size);

  size=19+17+4+ g_strlen(s->hostname) + g_strlen(s->username) + g_strlen(s->password);
  if (s->addr_type==SCP_ADDRESS_TYPE_IPV4)
  {
    size=size+4;
  }
  else
  {
    size=size+16;
  }

  /* sending request */

  /* header */
  out_uint32_be(c->out_s, 1); /* version */
  out_uint32_be(c->out_s, size);
  out_uint16_be(c->out_s, SCP_COMMAND_SET_DEFAULT);
  out_uint16_be(c->out_s, 1);

  /* body */
  out_uint8(c->out_s, s->type);
  out_uint16_be(c->out_s, s->height);
  out_uint16_be(c->out_s, s->width);
  out_uint8(c->out_s, s->bpp);
  out_uint8(c->out_s, s->rsr);
  out_uint8p(c->out_s, s->locale, 17);
  out_uint8(c->out_s, s->addr_type);

  if (s->addr_type==SCP_ADDRESS_TYPE_IPV4)
  {
    out_uint32_be(c->out_s, s->ipv4addr);
  }
  else
  {
    #warning ipv6 address needed
  }

  sz=g_strlen(s->hostname);
  out_uint8(c->out_s, sz);
  out_uint8p(c->out_s, s->hostname, sz);
  sz=g_strlen(s->username);
  out_uint8(c->out_s, sz);
  out_uint8p(c->out_s, s->username, sz);
  sz=g_strlen(s->password);
  out_uint8(c->out_s, sz);
  out_uint8p(c->out_s, s->password, sz);

  if (0!=tcp_force_send(c->in_sck, c->out_s->data, size))
  {
    return SCP_CLIENT_STATE_NETWORK_ERR;
  }

  /* wait for response */ 
  return _scp_v1c_check_response(c, s);
}

/* 004 */ 
enum SCP_CLIENT_STATES_E scp_v1c_resend_credentials(struct SCP_CONNECTION* c, struct SCP_SESSION* s)
{
  unsigned char sz;
  uint32_t size;
  //uint32_t version;
  //uint16_t cmd;
  //uint16_t dim;

  init_stream(c->out_s, c->out_s->size);
  init_stream(c->in_s, c->in_s->size);

  size=12+2+g_strlen(s->username)+g_strlen(s->password);

  /* sending request */
  /* header */
  out_uint32_be(c->out_s, 1); /* version */
  out_uint32_be(c->out_s, size);
  out_uint16_be(c->out_s, SCP_COMMAND_SET_DEFAULT);
  out_uint16_be(c->out_s, 4);

  /* body */
  sz=g_strlen(s->username);
  out_uint8(c->out_s, sz);
  out_uint8p(c->out_s, s->username, sz);
  sz=g_strlen(s->password);
  out_uint8(c->out_s, sz);
  out_uint8p(c->out_s, s->password, sz);

  if (0!=tcp_force_send(c->in_sck, c->out_s->data, size))
  {
    return SCP_CLIENT_STATE_NETWORK_ERR;
  }

  /* wait for response */
  return _scp_v1c_check_response(c, s);
}

/* 021 */ enum SCP_CLIENT_STATES_E scp_v1c_pwd_change(struct SCP_CONNECTION* c, char* newpass);
/* 022 */ enum SCP_CLIENT_STATES_E scp_v1c_pwd_change_cancel(struct SCP_CONNECTION* c);

/* ... */ enum SCP_CLIENT_STATES_E scp_v1c_get_session_list(struct SCP_CONNECTION* c, int* scount, struct SCP_DISCONNECTED_SESSION** s);
/* 041 */ enum SCP_CLIENT_STATES_E scp_v1c_select_session(struct SCP_CONNECTION* c, SCP_SID sid);
/* 042 */ enum SCP_CLIENT_STATES_E scp_v1c_select_session_cancel(struct SCP_CONNECTION* c);

/* 03x */ enum SCP_CLIENT_STATES_E scp_v1c_retrieve_session(struct SCP_CONNECTION* c, struct SCP_SESSION* s, struct SCP_DISCONNECTED_SESSION* ds);

static enum SCP_CLIENT_STATES_E _scp_v1c_check_response(struct SCP_CONNECTION* c, struct SCP_SESSION* s)
{
  uint32_t version;
  uint32_t size;
  uint16_t cmd;
  uint16_t dim;

  init_stream(c->in_s, c->in_s->size);
  if (0!=tcp_force_recv(c->in_sck, c->in_s->data, 8))
  {
    return SCP_CLIENT_STATE_NETWORK_ERR;
  }
	
  in_uint32_be(c->in_s, version);
  if (version!=1)
  {
    return SCP_CLIENT_STATE_VERSION_ERR;
  }

  in_uint32_be(c->in_s, size);

  init_stream(c->in_s, c->in_s->size);
  /* read the rest of the packet */ 
  if (0!=tcp_force_recv(c->in_sck, c->in_s->data, size-8))
  {
    return SCP_CLIENT_STATE_NETWORK_ERR;
  }

  in_uint16_be(c->in_s, cmd);
  if (cmd!=SCP_COMMAND_SET_DEFAULT)
  {
    return SCP_CLIENT_STATE_SEQUENCE_ERR;
  }

  in_uint16_be(c->in_s, cmd)
  if (cmd==2) /* connection denied */
  {
    in_uint16_be(c->in_s, dim);
    if (s->errstr!=0)
    {
      g_free(s->errstr);
    }
    s->errstr=g_malloc(dim+1,0);
    if (s->errstr==0)
    {
      return SCP_CLIENT_STATE_INTERNAL_ERR;
    }
    in_uint8a(c->in_s, s->errstr, dim);
    (s->errstr)[dim]='\0';

    return SCP_CLIENT_STATE_CONNECTION_DENIED;
  }
  else if (cmd==3) /* resend usr/pwd */
  {
    in_uint16_be(c->in_s, dim);
    if (s->errstr!=0)
    {
      g_free(s->errstr);
    }
    s->errstr=g_malloc(dim+1,0);
    if (s->errstr==0)
    {
      return SCP_CLIENT_STATE_INTERNAL_ERR;
    }
    in_uint8a(c->in_s, s->errstr, dim);
    (s->errstr)[dim]='\0';

    return SCP_CLIENT_STATE_RESEND_CREDENTIALS;
  }
  else if (cmd==20) /* password change */
  {
    in_uint16_be(c->in_s, dim);
    if (s->errstr!=0)
    {
      g_free(s->errstr);
    }
    s->errstr=g_malloc(dim+1,0);
    if (s->errstr==0)
    {
      return SCP_CLIENT_STATE_INTERNAL_ERR;
    }
    in_uint8a(c->in_s, s->errstr, dim);
    (s->errstr)[dim]='\0';

    return SCP_CLIENT_STATE_PWD_CHANGE_REQ;
  }
  else if (cmd==30) /* display */
  {
    in_uint16_be(c->in_s, s->display);

    return SCP_CLIENT_STATE_OK;
  }
  else if (cmd==32) /* display of a disconnected session */
  {
    return SCP_CLIENT_STATE_RECONNECT;
  }
  else if (cmd==40) /* session list */
  {
    return SCP_CLIENT_STATE_SESSION_LIST;
  }

  return SCP_CLIENT_STATE_SEQUENCE_ERR;
}
