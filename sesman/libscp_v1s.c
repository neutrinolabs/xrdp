
#ifndef LIBSCP_V1S_C
#define LIBSCP_V1S_C

#include "libscp_v1s.h"

/* server API */
enum SCP_SERVER_STATES_E scp_v1s_accept(struct SCP_CONNECTION* c, struct SCP_SESSION** s, int skipVchk)
{
  struct SCP_SESSION* session;
  uint32_t version;
  uint32_t size;
  uint16_t cmdset;
  uint16_t cmd;
  unsigned char sz;
  
  if (!skipVchk)
  {

    if (0==tcp_force_recv(c->in_sck, c->in_s->data, 8))
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
  
  in_uint32_be(c->in_s, size);
  if (size<12) 
  {
    return SCP_SERVER_STATE_SIZE_ERR;
  }
  
  if (0!=tcp_force_recv(c->in_sck, c->in_s->data, (size-8)))
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
  if (cmd != 0)
  {
    return SCP_SERVER_STATE_SEQUENCE_ERR;
  }
	  
  session = g_malloc(sizeof(struct SCP_SESSION),1);
  if (0 == session) return SCP_SERVER_STATE_INTERNAL_ERR;

  in_uint8(c->in_s, session->type);
  if ((session->type != SCP_SESSION_TYPE_XVNC) && (session->type != SCP_SESSION_TYPE_XRDP))
  {
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
  if (0==session->hostname) return SCP_SERVER_STATE_INTERNAL_ERR;
  session->hostname[sz]='\0';
  in_uint8a(c->in_s, session->hostname, sz);

  /* reading username */
  in_uint8(c->in_s, sz);
  session->username=g_malloc(sz+1,1);
  if (0==session->username) return SCP_SERVER_STATE_INTERNAL_ERR;
  session->username[sz]='\0';
  in_uint8a(c->in_s, session->username, sz);
  
  /* reading password */
  in_uint8(c->in_s, sz);
  session->password=g_malloc(sz+1,1);
  if (0==session->password) return SCP_SERVER_STATE_INTERNAL_ERR;
  session->password[sz]='\0';
  in_uint8a(c->in_s, session->password, sz);

  //leggo lo stream e ritorno la struttura
  *s=session;
  
  return SCP_SERVER_STATE_OK;
}

enum SCP_SERVER_STATES_E scp_v1s_deny_connection(struct SCP_CONNECTION* c, char* reason)
{
  int rlen;
  
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

  if (0!=tcp_force_send(c->in_sck, c->out_s->data, rlen+14))
  {
    return SCP_SERVER_STATE_NETWORK_ERR;
  }

  return SCP_SERVER_STATE_END;
}

enum SCP_SERVER_STATES_E scp_v1s_request_password(struct SCP_CONNECTION* c, char** pwd, char** user)
{
  unsigned char sz;
  uint32_t version;
  uint32_t size;
  uint16_t cmdset;
  uint16_t cmd;

  version=1;
  size=12;
  cmdset=0;
  cmd=3;  
  
  /* send password request */
  out_uint32_be(c->out_s, 1);                       /* version */
  out_uint32_be(c->out_s, 12);                      /* size    */
  out_uint16_be(c->out_s, SCP_COMMAND_SET_DEFAULT); /* cmdset  */
  out_uint16_be(c->out_s, 3);                       /* cmd     */

  if (0!=tcp_force_send(c->in_sck, c->out_s->data, 12))
  {
    return SCP_SERVER_STATE_NETWORK_ERR;
  }
  
  /* receive password & username */
#warning check cmd seq
/*  tcp_force_recv()
  in_uint32_be()
  in_uint32_be
  in_uint16_be
  in_uint16_be*/
  
  /* reading username */
  in_uint8(c->in_s, sz);
  (*user)=g_malloc(sz+1,1);
  if (0==(*user)) return SCP_SERVER_STATE_INTERNAL_ERR;
  (*user)[sz]='\0';
  in_uint8a(c->in_s, (*user), sz);
  
  /* reading password */
  in_uint8(c->in_s, sz);
  (*pwd)=g_malloc(sz+1,1);
  if (0==(*pwd)) return SCP_SERVER_STATE_INTERNAL_ERR;
  (*pwd)[sz]='\0';
  in_uint8a(c->in_s, (*pwd), sz);
  
  return SCP_SERVER_STATE_INTERNAL_ERR;
}

/* 020 */ 
enum SCP_SERVER_STATES_E scp_v1s_request_pwd_change(struct SCP_CONNECTION* c, char* reason, char* npw)
{
  return SCP_SERVER_STATE_INTERNAL_ERR;
}
/* 023 */ 
enum SCP_SERVER_STATES_E scp_v1s_pwd_change_error(struct SCP_CONNECTION* s, char* error, int retry, char* npw)
{
  return SCP_SERVER_STATE_INTERNAL_ERR;
}
/* 030 */ 
enum SCP_SERVER_STATES_E scp_v1s_connect_new_session(struct SCP_CONNECTION* s, SCP_DISPLAY d)
{
  return SCP_SERVER_STATE_INTERNAL_ERR;
}
/* 031 */ 
enum SCP_SERVER_STATES_E scp_v1s_reconnect_session(struct SCP_CONNECTION* s, SCP_DISPLAY d)
{
  return SCP_SERVER_STATE_INTERNAL_ERR;
}
/* 032 */ 
enum SCP_SERVER_STATES_E scp_v1s_connection_error(struct SCP_CONNECTION* s, char* error)
{
  return SCP_SERVER_STATE_INTERNAL_ERR;
}
/* 040 */ 
enum SCP_SERVER_STATES_E scp_v1s_list_sessions(struct SCP_CONNECTION* s, int sescnt, struct SCP_DISCONNECTED_SESSION** ds, SCP_SID* sid)
{
  return SCP_SERVER_STATE_INTERNAL_ERR;
}

#endif
