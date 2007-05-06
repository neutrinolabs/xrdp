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
 * @file libscp_v0.c
 * @brief libscp version 0 code
 * @author Simone Fedele
 *
 */

#include "libscp_v0.h"

#include "os_calls.h"
/* client API */
/******************************************************************************/
enum SCP_CLIENT_STATES_E scp_v0c_connect(struct SCP_CONNECTION* c, struct SCP_SESSION* s)
{
  tui32 version;
  tui32 size;
  tui16 sz;

  init_stream(c->in_s, c->in_s->size);
  init_stream(c->out_s, c->in_s->size);

  g_tcp_set_non_blocking(c->in_sck);
  g_tcp_set_no_delay(c->in_sck);
  s_push_layer(c->out_s, channel_hdr, 8);

  if (s->type==SCP_SESSION_TYPE_XVNC)
  {
    out_uint16_be(c->out_s, 0); // code
  }
  else if (s->type==SCP_SESSION_TYPE_XRDP)
  {
    out_uint16_be(c->out_s, 10); // code
  }
  else
  {
    return SCP_CLIENT_STATE_INTERNAL_ERR;
  }
  sz = g_strlen(s->username);
  out_uint16_be(c->out_s, sz);
  out_uint8a(c->out_s, s->username, sz);

  sz = g_strlen(s->password);
  out_uint16_be(c->out_s,sz);
  out_uint8a(c->out_s, s->password, sz);
  out_uint16_be(c->out_s, s->width);
  out_uint16_be(c->out_s, s->height);
  out_uint16_be(c->out_s, s->bpp);

  s_mark_end(c->out_s);
  s_pop_layer(c->out_s, channel_hdr);

  out_uint32_be(c->out_s, 0);                // version
  out_uint32_be(c->out_s, c->out_s->end - c->out_s->data); // size

  if (0!=scp_tcp_force_send(c->in_sck, c->out_s->data, c->out_s->end - c->out_s->data))
  {
    return SCP_CLIENT_STATE_NETWORK_ERR;
  }

  if (0!=scp_tcp_force_recv(c->in_sck, c->in_s->data, 8))
  {
    return SCP_CLIENT_STATE_NETWORK_ERR;
  }

  in_uint32_be(c->in_s, version);
  if (0 != version)
  {
    return SCP_CLIENT_STATE_VERSION_ERR;
  }

  in_uint32_be(c->in_s, size);
  if (size < 14)
  {
    return SCP_CLIENT_STATE_SIZE_ERR;
  }

  init_stream(c->in_s, c->in_s->size);
  if (0!=scp_tcp_force_recv(c->in_sck, c->in_s->data, size - 8))
  {
    return SCP_CLIENT_STATE_NETWORK_ERR;
  }

  in_uint16_be(c->in_s, sz);
  if (3!=sz)
  {
    return SCP_CLIENT_STATE_SEQUENCE_ERR;
  }

  in_uint16_be(c->in_s, sz);
  if (1!=sz)
  {
    return SCP_CLIENT_STATE_CONNECTION_DENIED;
  }

  in_uint16_be(c->in_s, sz);
  s->display=sz;

  return SCP_CLIENT_STATE_END;
}

/* server API */
/******************************************************************************/
enum SCP_SERVER_STATES_E scp_v0s_accept(struct SCP_CONNECTION* c, struct SCP_SESSION** s, int skipVchk)
{
  tui32 version=0;
  tui32 size;
  struct SCP_SESSION* session=0;
  tui16 sz;
  tui32 code=0;

  if (!skipVchk)
  {
    if (0==scp_tcp_force_recv(c->in_sck, c->in_s->data, 8))
    {
      in_uint32_be(c->in_s, version);
      if (version != 0)
      {
        return SCP_SERVER_STATE_VERSION_ERR;
      }
    }
    else
    {
      return SCP_SERVER_STATE_NETWORK_ERR;
    }
  }

  in_uint32_be(c->in_s, size);

  init_stream(c->in_s, 8196);
  if (0!=scp_tcp_force_recv(c->in_sck, c->in_s->data, size-8))
  {
    return SCP_SERVER_STATE_NETWORK_ERR;
  }

  in_uint16_be(c->in_s, code);

  if (code == 0 || code == 10)
  {
    session = g_malloc(sizeof(struct SCP_SESSION),1);
    if (0 == session)
    {
      return SCP_SERVER_STATE_INTERNAL_ERR;
    }

    session->version=version;

    if (code == 0)
    {
      session->type=SCP_SESSION_TYPE_XVNC;
    }
    else
    {
      session->type=SCP_SESSION_TYPE_XRDP;
    }

    /* reading username */
    in_uint16_be(c->in_s, sz);
    session->username=g_malloc(sz+1,0);
    if (0==session->username)
    {
      g_free(session);
      return SCP_SERVER_STATE_INTERNAL_ERR;
    }
    session->username[sz]='\0';
    in_uint8a(c->in_s, session->username, sz);

    /* reading password */
    in_uint16_be(c->in_s, sz);
    session->password=g_malloc(sz+1,0);
    if (0==session->password)
    {
      g_free(session->username);
      g_free(session);
      return SCP_SERVER_STATE_INTERNAL_ERR;
    }
    session->password[sz]='\0';
    in_uint8a(c->in_s, session->password, sz);

    in_uint16_be(c->in_s, session->width);
    in_uint16_be(c->in_s, session->height);
    in_uint16_be(c->in_s, sz);
    session->bpp=(unsigned char)sz;
  }
  else
  {
    return SCP_SERVER_STATE_SEQUENCE_ERR;
  }

  (*s)=session;
  return SCP_SERVER_STATE_OK;
}

/******************************************************************************/
enum SCP_SERVER_STATES_E scp_v0s_allow_connection(struct SCP_CONNECTION* c, SCP_DISPLAY d)
{
  out_uint32_be(c->out_s, 0);  /* version */
  out_uint32_be(c->out_s, 14); /* size */
  out_uint16_be(c->out_s, 3);  /* cmd */
  out_uint16_be(c->out_s, 1);  /* data */
  out_uint16_be(c->out_s, d);  /* data */
  s_mark_end(c->out_s);

  if (0!=scp_tcp_force_send(c->in_sck, c->out_s->data, c->out_s->end - c->out_s->data))
  {
    return SCP_SERVER_STATE_NETWORK_ERR;
  }

  return SCP_SERVER_STATE_OK;
}

/******************************************************************************/
enum SCP_SERVER_STATES_E scp_v0s_deny_connection(struct SCP_CONNECTION* c)
{
  out_uint32_be(c->out_s, 0);  /* version */
  out_uint32_be(c->out_s, 14); /* size */
  out_uint16_be(c->out_s, 3);  /* cmd */
  out_uint16_be(c->out_s, 0);  /* data */
  out_uint16_be(c->out_s, 0);  /* data */
  s_mark_end(c->out_s);

  if (0!=scp_tcp_force_send(c->in_sck, c->out_s->data, c->out_s->end - c->out_s->data))
  {
    return SCP_SERVER_STATE_NETWORK_ERR;
  }

  return SCP_SERVER_STATE_OK;
}
