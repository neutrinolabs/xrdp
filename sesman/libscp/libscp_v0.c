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
   Copyright (C) Jay Sorg 2005-2010
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

extern struct log_config* s_log;

/* client API */
/******************************************************************************/
enum SCP_CLIENT_STATES_E
scp_v0c_connect(struct SCP_CONNECTION* c, struct SCP_SESSION* s)
{
  tui32 version;
  tui32 size;
  tui16 sz;

  init_stream(c->in_s, c->in_s->size);
  init_stream(c->out_s, c->in_s->size);

  LOG_DBG("[v0:%d] starting connection", __LINE__);
  g_tcp_set_non_blocking(c->in_sck);
  g_tcp_set_no_delay(c->in_sck);
  s_push_layer(c->out_s, channel_hdr, 8);

  /* code */
  if (s->type == SCP_SESSION_TYPE_XVNC)
  {
    out_uint16_be(c->out_s, 0);
  }
  else if (s->type == SCP_SESSION_TYPE_XRDP)
  {
    out_uint16_be(c->out_s, 10);
  }
  else
  {
    log_message(LOG_LEVEL_WARNING, "[v0:%d] connection aborted: network error", __LINE__);
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

  /* version */
  out_uint32_be(c->out_s, 0);
  /* size */
  out_uint32_be(c->out_s, c->out_s->end - c->out_s->data);

  if (0!=scp_tcp_force_send(c->in_sck, c->out_s->data, c->out_s->end - c->out_s->data))
  {
    log_message(LOG_LEVEL_WARNING, "[v0:%d] connection aborted: network error", __LINE__);
    return SCP_CLIENT_STATE_NETWORK_ERR;
  }

  if (0!=scp_tcp_force_recv(c->in_sck, c->in_s->data, 8))
  {
    log_message(LOG_LEVEL_WARNING, "[v0:%d] connection aborted: network error", __LINE__);
    return SCP_CLIENT_STATE_NETWORK_ERR;
  }

  in_uint32_be(c->in_s, version);
  if (0 != version)
  {
    log_message(LOG_LEVEL_WARNING, "[v0:%d] connection aborted: version error", __LINE__);
    return SCP_CLIENT_STATE_VERSION_ERR;
  }

  in_uint32_be(c->in_s, size);
  if (size < 14)
  {
    log_message(LOG_LEVEL_WARNING, "[v0:%d] connection aborted: packet size error", __LINE__);
    return SCP_CLIENT_STATE_SIZE_ERR;
  }

  /* getting payload */
  init_stream(c->in_s, c->in_s->size);
  if (0!=scp_tcp_force_recv(c->in_sck, c->in_s->data, size - 8))
  {
    log_message(LOG_LEVEL_WARNING, "[v0:%d] connection aborted: network error", __LINE__);
    return SCP_CLIENT_STATE_NETWORK_ERR;
  }

  /* check code */
  in_uint16_be(c->in_s, sz);
  if (3 != sz)
  {
    log_message(LOG_LEVEL_WARNING, "[v0:%d] connection aborted: sequence error", __LINE__);
    return SCP_CLIENT_STATE_SEQUENCE_ERR;
  }

  /* message payload */
  in_uint16_be(c->in_s, sz);
  if (1 != sz)
  {
    log_message(LOG_LEVEL_WARNING, "[v0:%d] connection aborted: connection denied", __LINE__);
    return SCP_CLIENT_STATE_CONNECTION_DENIED;
  }

  in_uint16_be(c->in_s, sz);
  s->display = sz;

  LOG_DBG("[v0:%d] connection terminated", __LINE__);
  return SCP_CLIENT_STATE_END;
}

/* server API */
/******************************************************************************/
enum SCP_SERVER_STATES_E
scp_v0s_accept(struct SCP_CONNECTION* c, struct SCP_SESSION** s, int skipVchk)
{
  tui32 version = 0;
  tui32 size;
  struct SCP_SESSION* session = 0;
  tui16 sz;
  tui32 code = 0;
  char buf[257];

  if (!skipVchk)
  {
    LOG_DBG("[v0:%d] starting connection", __LINE__);
    if (0 == scp_tcp_force_recv(c->in_sck, c->in_s->data, 8))
    {
      c->in_s->end = c->in_s->data + 8;
      in_uint32_be(c->in_s, version);
      if (version != 0)
      {
        log_message(LOG_LEVEL_WARNING, "[v0:%d] connection aborted: version error", __LINE__);
        return SCP_SERVER_STATE_VERSION_ERR;
      }
    }
    else
    {
      log_message(LOG_LEVEL_WARNING, "[v0:%d] connection aborted: network error", __LINE__);
      return SCP_SERVER_STATE_NETWORK_ERR;
    }
  }

  in_uint32_be(c->in_s, size);

  init_stream(c->in_s, 8196);
  if (0 != scp_tcp_force_recv(c->in_sck, c->in_s->data, size - 8))
  {
    log_message(LOG_LEVEL_WARNING, "[v0:%d] connection aborted: network error", __LINE__);
    return SCP_SERVER_STATE_NETWORK_ERR;
  }
  c->in_s->end = c->in_s->data + (size - 8);

  in_uint16_be(c->in_s, code);

  if (code == 0 || code == 10)
  {
    session = scp_session_create();
    if (0 == session)
    {
      log_message(LOG_LEVEL_WARNING, "[v0:%d] connection aborted: network error", __LINE__);
      return SCP_SERVER_STATE_INTERNAL_ERR;
    }

    scp_session_set_version(session, version);
    if (code == 0)
    {
      scp_session_set_type(session, SCP_SESSION_TYPE_XVNC);
    }
    else
    {
      scp_session_set_type(session, SCP_SESSION_TYPE_XRDP);
    }

    /* reading username */
    in_uint16_be(c->in_s, sz);
    buf[sz]='\0';
    in_uint8a(c->in_s, buf, sz);
    if (0 != scp_session_set_username(session, buf))
    {
      scp_session_destroy(session);
      log_message(LOG_LEVEL_WARNING, "[v0:%d] connection aborted: error setting username", __LINE__);
      return SCP_SERVER_STATE_INTERNAL_ERR;
    }

    /* reading password */
    in_uint16_be(c->in_s, sz);
    buf[sz]='\0';
    in_uint8a(c->in_s, buf, sz);
    if (0 != scp_session_set_password(session, buf))
    {
      scp_session_destroy(session);
      log_message(LOG_LEVEL_WARNING, "[v0:%d] connection aborted: error setting password", __LINE__);
      return SCP_SERVER_STATE_INTERNAL_ERR;
    }

    /* width */
    in_uint16_be(c->in_s, sz);
    scp_session_set_width(session, sz);
    /* height */
    in_uint16_be(c->in_s, sz);
    scp_session_set_height(session, sz);
    /* bpp */
    in_uint16_be(c->in_s, sz);
    scp_session_set_bpp(session, (tui8)sz);
    if (s_check_rem(c->in_s, 2))
    {
      /* reading domain */
      in_uint16_be(c->in_s, sz);
      if (sz > 0)
      {
        in_uint8a(c->in_s, buf, sz);
        buf[sz] = '\0';
        scp_session_set_domain(session, buf);
      }
    }
    if (s_check_rem(c->in_s, 2))
    {
      /* reading program */
      in_uint16_be(c->in_s, sz);
      if (sz > 0)
      {
        in_uint8a(c->in_s, buf, sz);
        buf[sz] = '\0';
        scp_session_set_program(session, buf);
      }
    }
    if (s_check_rem(c->in_s, 2))
    {
      /* reading directory */
      in_uint16_be(c->in_s, sz);
      if (sz > 0)
      {
        in_uint8a(c->in_s, buf, sz);
        buf[sz] = '\0';
        scp_session_set_directory(session, buf);
      }
    }
    if (s_check_rem(c->in_s, 2))
    {
      /* reading client IP address */
      in_uint16_be(c->in_s, sz);
      if (sz > 0)
      {
        in_uint8a(c->in_s, buf, sz);
        buf[sz] = '\0';
        scp_session_set_client_ip(session, buf);
      }
    }
  }
  else
  {
    log_message(LOG_LEVEL_WARNING, "[v0:%d] connection aborted: sequence error", __LINE__);
    return SCP_SERVER_STATE_SEQUENCE_ERR;
  }

  (*s)=session;
  return SCP_SERVER_STATE_OK;
}

/******************************************************************************/
enum SCP_SERVER_STATES_E
scp_v0s_allow_connection(struct SCP_CONNECTION* c, SCP_DISPLAY d)
{
  out_uint32_be(c->out_s, 0);  /* version */
  out_uint32_be(c->out_s, 14); /* size */
  out_uint16_be(c->out_s, 3);  /* cmd */
  out_uint16_be(c->out_s, 1);  /* data */
  out_uint16_be(c->out_s, d);  /* data */
  s_mark_end(c->out_s);

  if (0 != scp_tcp_force_send(c->in_sck, c->out_s->data, c->out_s->end - c->out_s->data))
  {
    log_message(LOG_LEVEL_WARNING, "[v0:%d] connection aborted: network error", __LINE__);
    return SCP_SERVER_STATE_NETWORK_ERR;
  }

  LOG_DBG("[v0:%d] connection terminated (allowed)", __LINE__);
  return SCP_SERVER_STATE_OK;
}

/******************************************************************************/
enum SCP_SERVER_STATES_E
scp_v0s_deny_connection(struct SCP_CONNECTION* c)
{
  out_uint32_be(c->out_s, 0);  /* version */
  out_uint32_be(c->out_s, 14); /* size */
  out_uint16_be(c->out_s, 3);  /* cmd */
  out_uint16_be(c->out_s, 0);  /* data */
  out_uint16_be(c->out_s, 0);  /* data */
  s_mark_end(c->out_s);

  if (0 != scp_tcp_force_send(c->in_sck, c->out_s->data, c->out_s->end - c->out_s->data))
  {
    log_message(LOG_LEVEL_WARNING, "[v0:%d] connection aborted: network error", __LINE__);
    return SCP_SERVER_STATE_NETWORK_ERR;
  }

  LOG_DBG("[v0:%d] connection terminated (denied)", __LINE__);
  return SCP_SERVER_STATE_OK;
}
