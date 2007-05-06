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
 * @file libscp_v1s.c
 * @brief libscp version 1 server api code
 * @author Simone Fedele
 *
 */

#ifndef LIBSCP_V1S_C
#define LIBSCP_V1S_C

#include "libscp_v1s.h"

/* server API */
enum SCP_SERVER_STATES_E scp_v1s_accept(struct SCP_CONNECTION* c, struct SCP_SESSION** s, int skipVchk)
{
  struct SCP_SESSION* session;
  tui32 version;
  tui32 size;
  tui16 cmdset;
  tui16 cmd;
  tui8 sz;

  if (!skipVchk)
  {

    if (0==scp_tcp_force_recv(c->in_sck, c->in_s->data, 8))
    {
      in_uint32_be(c->in_s, version);
      if (version != 1)
      {
        return SCP_SERVER_STATE_VERSION_ERR;
      }
    }
    else
    {
      return SCP_SERVER_STATE_NETWORK_ERR;
    }
  }
  else
  {
    version=1;
  }

  in_uint32_be(c->in_s, size);
  if (size<12)
  {
    return SCP_SERVER_STATE_SIZE_ERR;
  }

  init_stream(c->in_s, c->in_s->size);
  if (0!=scp_tcp_force_recv(c->in_sck, c->in_s->data, (size-8)))
  {
    return SCP_SERVER_STATE_NETWORK_ERR;
  }

  /* reading command set */
  in_uint16_be(c->in_s, cmdset);

  /* if we are starting a management session */
  if (cmdset==SCP_COMMAND_SET_MANAGE)
  {
    return SCP_SERVER_STATE_START_MANAGE;
  }

  /* if we started with resource sharing... */
  if (cmdset==SCP_COMMAND_SET_RSR)
  {
    return SCP_SERVER_STATE_SEQUENCE_ERR;
  }

  /* reading command */
  in_uint16_be(c->in_s, cmd);
  if (cmd != 1)
  {
    return SCP_SERVER_STATE_SEQUENCE_ERR;
  }

  session = g_malloc(sizeof(struct SCP_SESSION),1);
  if (0 == session)
  {
     return SCP_SERVER_STATE_INTERNAL_ERR;
  }
  session->version=1;

  in_uint8(c->in_s, session->type);
  if ((session->type != SCP_SESSION_TYPE_XVNC) && (session->type != SCP_SESSION_TYPE_XRDP))
  {
    g_free(session);
    return SCP_SERVER_STATE_SESSION_TYPE_ERR;
  }

  in_uint16_be(c->in_s,session->height);
  in_uint16_be(c->in_s, session->width);
  in_uint8(c->in_s, session->bpp);
  in_uint8(c->in_s, session->rsr);
  in_uint8a(c->in_s, session->locale, 17);
    session->locale[17]='\0';

  in_uint8(c->in_s, session->addr_type);
  if (session->addr_type==SCP_ADDRESS_TYPE_IPV4)
  {
    in_uint32_be(c->in_s, session->ipv4addr);
  }
  else if (session->addr_type==SCP_ADDRESS_TYPE_IPV6)
  {
    #warning how to handle ipv6 addresses?
  }

  /* reading hostname */
  in_uint8(c->in_s, sz);
  session->hostname=g_malloc(sz+1,1);
  if (0==session->hostname)
  {
    g_free(session);
    return SCP_SERVER_STATE_INTERNAL_ERR;
  }
  session->hostname[sz]='\0';
  in_uint8a(c->in_s, session->hostname, sz);

  /* reading username */
  in_uint8(c->in_s, sz);
  session->username=g_malloc(sz+1,1);
  if (0==session->username)
  {
    g_free(session->hostname);
    g_free(session);
    return SCP_SERVER_STATE_INTERNAL_ERR;
  }
  session->username[sz]='\0';
  in_uint8a(c->in_s, session->username, sz);

  /* reading password */
  in_uint8(c->in_s, sz);
  session->password=g_malloc(sz+1,1);
  if (0==session->password)
  {
    g_free(session->username);
    g_free(session->hostname);
    g_free(session);
    return SCP_SERVER_STATE_INTERNAL_ERR;
  }
  session->password[sz]='\0';
  in_uint8a(c->in_s, session->password, sz);

  /* returning the struct */
  (*s)=session;

  return SCP_SERVER_STATE_OK;
}

enum SCP_SERVER_STATES_E
scp_v1s_deny_connection(struct SCP_CONNECTION* c, char* reason)
{
  int rlen;

  init_stream(c->out_s,c->out_s->size);

  /* forcing message not to exceed 64k */
  rlen = g_strlen(reason);
  if (rlen > 65535)
  {
    rlen = 65535;
  }

  out_uint32_be(c->out_s, 1);
  /* packet size: 4 + 4 + 2 + 2 + 2 + strlen(reason)*/
  /* version + size + cmdset + cmd + msglen + msg */
  out_uint32_be(c->out_s, rlen+14);
  out_uint16_be(c->out_s, SCP_COMMAND_SET_DEFAULT);
  out_uint16_be(c->out_s, 2);
  out_uint16_be(c->out_s, rlen)
  out_uint8p(c->out_s, reason, rlen);

  if (0!=scp_tcp_force_send(c->in_sck, c->out_s->data, rlen+14))
  {
    return SCP_SERVER_STATE_NETWORK_ERR;
  }

  return SCP_SERVER_STATE_END;
}

enum SCP_SERVER_STATES_E
scp_v1s_request_password(struct SCP_CONNECTION* c, struct SCP_SESSION* s, char* reason)
{
  tui8 sz;
  char *ubuf;
  char *pbuf;
  tui32 version;
  tui32 size;
  tui16 cmdset;
  tui16 cmd;
  int rlen;

  init_stream(c->in_s, c->in_s->size);
  init_stream(c->out_s, c->out_s->size);

  /* forcing message not to exceed 64k */
  rlen = g_strlen(reason);
  if (rlen > 65535)
  {
    rlen = 65535;
  }

  /* send password request */
  version=1;
  size=12;
  cmdset=0;
  cmd=3;

  out_uint32_be(c->out_s, version);                 /* version */
  out_uint32_be(c->out_s, 14+rlen);                 /* size    */
  out_uint16_be(c->out_s, SCP_COMMAND_SET_DEFAULT); /* cmdset  */
  out_uint16_be(c->out_s, cmd);                     /* cmd     */

  out_uint16_be(c->out_s, rlen);
  out_uint8p(c->out_s, reason, rlen);

  if (0!=scp_tcp_force_send(c->in_sck, c->out_s->data, 14+rlen))
  {
    return SCP_SERVER_STATE_NETWORK_ERR;
  }

  /* receive password & username */
  if (0!=scp_tcp_force_recv(c->in_sck, c->in_s->data, 8))
  {
    return SCP_SERVER_STATE_NETWORK_ERR;
  }

  in_uint32_be(c->in_s, version);
  if (version!=1)
  {
    return SCP_SERVER_STATE_VERSION_ERR;
  }

  in_uint32_be(c->in_s, size);
  if (size<12)
  {
    return SCP_SERVER_STATE_SIZE_ERR;
  }

  init_stream(c->in_s, c->in_s->size);
  if (0!=scp_tcp_force_recv(c->in_sck, c->in_s->data, (size-8)))
  {
    return SCP_SERVER_STATE_NETWORK_ERR;
  }

  in_uint16_be(c->in_s, cmdset);
  if (cmdset != SCP_COMMAND_SET_DEFAULT)
  {
    return SCP_SERVER_STATE_SEQUENCE_ERR;
  }

  in_uint16_be(c->in_s, cmd);
  if (cmd != 4)
  {
    return SCP_SERVER_STATE_SEQUENCE_ERR;
  }

  /* reading username */
  in_uint8(c->in_s, sz);
  ubuf=g_malloc(sz+1,1);
  if (0==ubuf)
  {
    return SCP_SERVER_STATE_INTERNAL_ERR;
  }
  ubuf[sz]='\0';
  in_uint8a(c->in_s, ubuf, sz);

  /* reading password */
  in_uint8(c->in_s, sz);
  pbuf=g_malloc(sz+1,1);
  if (0==pbuf)
  {
    g_free(ubuf);
    return SCP_SERVER_STATE_INTERNAL_ERR;
  }
  pbuf[sz]='\0';
  in_uint8a(c->in_s, pbuf, sz);

  /* replacing username and password */
  g_free(s->username);
  g_free(s->password);
  s->username=ubuf;
  s->password=pbuf;

  return SCP_SERVER_STATE_OK;
}

/* 020 */
enum SCP_SERVER_STATES_E
scp_v1s_request_pwd_change(struct SCP_CONNECTION* c, char* reason, char* npw)
{
  return SCP_SERVER_STATE_INTERNAL_ERR;
}

/* 023 */
enum SCP_SERVER_STATES_E
scp_v1s_pwd_change_error(struct SCP_CONNECTION* c, char* error, int retry, char* npw)
{
  return SCP_SERVER_STATE_INTERNAL_ERR;
}

/* 030 */
enum SCP_SERVER_STATES_E
scp_v1s_connect_new_session(struct SCP_CONNECTION* c, SCP_DISPLAY d)
{
  /* send password request */
  tui32 version=1;
  tui32 size=14;
  tui16 cmd=30;

  init_stream(c->out_s, c->out_s->size);

  out_uint32_be(c->out_s, version);                 /* version */
  out_uint32_be(c->out_s, size);                    /* size    */
  out_uint16_be(c->out_s, SCP_COMMAND_SET_DEFAULT); /* cmdset  */
  out_uint16_be(c->out_s, cmd);                     /* cmd     */

  out_uint16_be(c->out_s, d);                       /* display */

  if (0!=scp_tcp_force_send(c->in_sck, c->out_s->data, 14))
  {
    return SCP_SERVER_STATE_NETWORK_ERR;
  }

  return SCP_SERVER_STATE_OK;
}

/* 032 */
enum SCP_SERVER_STATES_E
scp_v1s_connection_error(struct SCP_CONNECTION* c, char* error)
{
  return SCP_SERVER_STATE_INTERNAL_ERR;
  return SCP_SERVER_STATE_END;
}

/* 040 */
enum SCP_SERVER_STATES_E
scp_v1s_list_sessions(struct SCP_CONNECTION* c, int sescnt, struct SCP_DISCONNECTED_SESSION* ds, SCP_SID* sid)
{
  tui32 version=1;
  tui32 size=12;
  tui16 cmd=40;
  int pktcnt;
  int idx;
  int sidx;
  int pidx;
  struct SCP_DISCONNECTED_SESSION* cds;

  /* first we send a notice that we have some disconnected sessions */
  init_stream(c->out_s, c->out_s->size);

  out_uint32_be(c->out_s, version);                 /* version */
  out_uint32_be(c->out_s, size);                    /* size    */
  out_uint16_be(c->out_s, SCP_COMMAND_SET_DEFAULT); /* cmdset  */
  out_uint16_be(c->out_s, cmd);                     /* cmd     */

  if (0!=scp_tcp_force_send(c->in_sck, c->out_s->data, size))
  {
    return SCP_SERVER_STATE_NETWORK_ERR;
  }

  /* then we wait for client ack */
#warning maybe this message could say if the session should be resized on
#warning server side or client side
  init_stream(c->in_s, c->in_s->size);
  if (0!=scp_tcp_force_recv(c->in_sck, c->in_s->data, 8))
  {
    return SCP_SERVER_STATE_NETWORK_ERR;
  }

  in_uint32_be(c->in_s, version);
  if (version!=1)
  {
    return SCP_SERVER_STATE_VERSION_ERR;
  }

  in_uint32_be(c->in_s, size);
  if (size<12)
  {
    return SCP_SERVER_STATE_SIZE_ERR;
  }

  init_stream(c->in_s, c->in_s->size);
  if (0!=scp_tcp_force_recv(c->in_sck, c->in_s->data, (size-8)))
  {
    return SCP_SERVER_STATE_NETWORK_ERR;
  }

  in_uint16_be(c->in_s, cmd);
  if (cmd != SCP_COMMAND_SET_DEFAULT)
  {
    return SCP_SERVER_STATE_SEQUENCE_ERR;
  }

  in_uint16_be(c->in_s, cmd);
  if (cmd != 41)
  {
    return SCP_SERVER_STATE_SEQUENCE_ERR;
  }

  /* calculating the number of packets to send */
  pktcnt=sescnt/SCP_SERVER_MAX_LIST_SIZE;
  if ((sescnt%SCP_SERVER_MAX_LIST_SIZE)!=0)
  {
    pktcnt++;
  }

  for (idx=0; idx<pktcnt; idx++)
  {
    /* ok, we send session session list */
    init_stream(c->out_s, c->out_s->size);

    /* size: ver+size+cmdset+cmd+sescnt+continue+count */
    size=4+4+2+2+4+1+1;

    /* header */
    cmd=42;
    s_push_layer(c->out_s, channel_hdr, 8);
    out_uint16_be(c->out_s, SCP_COMMAND_SET_DEFAULT);
    out_uint16_be(c->out_s, cmd);

    /* session count */
    out_uint32_be(c->out_s, sescnt);

    /* setting the continue flag */
    if ((idx+1)*SCP_SERVER_MAX_LIST_SIZE >= sescnt)
    {
      out_uint8(c->out_s, 0);
      /* setting session count for this packet */
      pidx=sescnt-(idx*SCP_SERVER_MAX_LIST_SIZE);
      out_uint8(c->out_s, pidx);
    }
    else
    {
      out_uint8(c->out_s, 1);
      /* setting session count for this packet */
      pidx=SCP_SERVER_MAX_LIST_SIZE;
      out_uint8(c->out_s, pidx);
    }

    /* adding session descriptors */
    for (sidx=0; sidx<pidx; sidx++)
    {
      /* shortcut to the current session to send */
      cds=ds+((idx)*SCP_SERVER_MAX_LIST_SIZE)+sidx;

      /* session data */
      out_uint32_be(c->out_s, cds->SID); /* session id */
      out_uint8(c->out_s, cds->type);
      out_uint16_be(c->out_s, cds->height);
      out_uint16_be(c->out_s, cds->width);
      out_uint8(c->out_s, cds->bpp);
      out_uint8(c->out_s, cds->idle_days);
      out_uint8(c->out_s, cds->idle_hours);
      out_uint8(c->out_s, cds->idle_minutes);

      size = size + 13;
    }

    s_pop_layer(c->out_s, channel_hdr);
    out_uint32_be(c->out_s, version);
    out_uint32_be(c->out_s, size);

    if (0!=scp_tcp_force_send(c->in_sck, c->out_s->data, size))
    {
      return SCP_SERVER_STATE_NETWORK_ERR;
    }
  }

  /* we get the response */
  init_stream(c->in_s, c->in_s->size);
  if (0!=scp_tcp_force_recv(c->in_sck, c->in_s->data, (8)))
  {
    return SCP_SERVER_STATE_NETWORK_ERR;
  }

  in_uint32_be(c->in_s, version);
  if (version!=1)
  {
    return SCP_SERVER_STATE_VERSION_ERR;
  }

  in_uint32_be(c->in_s, size);
  if (size<12)
  {
    return SCP_SERVER_STATE_SIZE_ERR;
  }

  /* rest of the packet */
  init_stream(c->in_s, c->in_s->size);
  if (0!=scp_tcp_force_recv(c->in_sck, c->in_s->data, (size-8)))
  {
    return SCP_SERVER_STATE_NETWORK_ERR;
  }

  in_uint16_be(c->in_s, cmd);
  if (cmd != SCP_COMMAND_SET_DEFAULT)
  {
    return SCP_SERVER_STATE_SEQUENCE_ERR;
  }

  in_uint16_be(c->in_s, cmd);
  if (cmd == 43)
  {
    /* select session */
    in_uint32_be(c->in_s, (*sid));

    /* checking sid value */
    for (idx=0; idx<sescnt; idx++)
    {
      /* the sid is valid */
      if (ds[idx].SID==(*sid))
      {
        /* ok, session selected */
        return SCP_SERVER_STATE_OK;
      }
    }

    /* if we got here, the requested sid wasn't one from the list we sent */
    /* we should kill the connection                                      */
    return SCP_CLIENT_STATE_INTERNAL_ERR;
  }
  else if (cmd == 44)
  {
    /* cancel connection */
    return SCP_SERVER_STATE_SELECTION_CANCEL;
  }
//  else if (cmd == 45)
//  {
//    /* force new connection */
//    return SCP_SERVER_STATE_FORCE_NEW;
//  }
  else
  {
    /* wrong response */
    return SCP_SERVER_STATE_SEQUENCE_ERR;
  }

  return SCP_SERVER_STATE_OK;
}

/* 046 was: 031 struct SCP_DISCONNECTED_SESSION* ds, */
enum SCP_SERVER_STATES_E
scp_v1s_reconnect_session(struct SCP_CONNECTION* c, SCP_DISPLAY d)
{
  tui32 version = 1;
  tui32 size = 14;
  tui16 cmd = 46;

  /* ok, we send session data and display */
  init_stream(c->out_s, c->out_s->size);

  /* header */
  out_uint32_be(c->out_s, version);
  out_uint32_be(c->out_s, size);
  out_uint16_be(c->out_s, SCP_COMMAND_SET_DEFAULT);
  out_uint16_be(c->out_s, cmd);

  /* session data */
  out_uint16_be(c->out_s, d); /* session display */
  /*out_uint8(c->out_s, ds->type);
  out_uint16_be(c->out_s, ds->height);
  out_uint16_be(c->out_s, ds->width);
  out_uint8(c->out_s, ds->bpp);
  out_uint8(c->out_s, ds->idle_days);
  out_uint8(c->out_s, ds->idle_hours);
  out_uint8(c->out_s, ds->idle_minutes);*/
  /* these last three are not really needed... */

  if (0!=scp_tcp_force_send(c->in_sck, c->out_s->data, size))
  {
    return SCP_SERVER_STATE_NETWORK_ERR;
  }

  return SCP_SERVER_STATE_OK;
}

#endif
