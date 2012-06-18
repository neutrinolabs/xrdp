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
 * @file libscp_types.h
 * @brief libscp data types definitions
 * @author Simone Fedele
 *
 */

#ifndef LIBSCP_TYPES_H
#define LIBSCP_TYPES_H

#include "os_calls.h"
#include "parse.h"
#include "arch.h"
#include "log.h"

#define SCP_SID      tui32
#define SCP_DISPLAY  tui16

#define SCP_RESOURCE_SHARING_REQUEST_YES 0x01
#define SCP_RESOURCE_SHARING_REQUEST_NO  0x00

#define SCP_SESSION_TYPE_XVNC    0x00
#define SCP_SESSION_TYPE_XRDP    0x01
#define SCP_SESSION_TYPE_MANAGE  0x02
/* SCP_GW_AUTHENTICATION can be used when XRDP + sesman act as a gateway
 * XRDP sends this command to let sesman verify if the user is allowed 
 * to use the gateway */
#define SCP_GW_AUTHENTICATION    0x04

#define SCP_ADDRESS_TYPE_IPV4 0x00
#define SCP_ADDRESS_TYPE_IPV6 0x01

/* used in scp_session_set_addr() */
#define SCP_ADDRESS_TYPE_IPV4_BIN 0x80
#define SCP_ADDRESS_TYPE_IPV6_BIN 0x81

#define SCP_COMMAND_SET_DEFAULT 0x0000
#define SCP_COMMAND_SET_MANAGE  0x0001
#define SCP_COMMAND_SET_RSR     0x0002

#define SCP_SERVER_MAX_LIST_SIZE 100

#include "libscp_types_mng.h"

struct SCP_CONNECTION
{
  int in_sck;
  struct stream* in_s;
  struct stream* out_s;
};

struct SCP_SESSION
{
  tui8  type;
  tui32 version;
  tui16 height;
  tui16 width;
  tui8  bpp;
  tui8  rsr;
  char  locale[18];
  char* username;
  char* password;
  char* hostname;
  tui8  addr_type;
  tui32 ipv4addr;
  tui8  ipv6addr[16];
  SCP_DISPLAY display;
  char* errstr;
  struct SCP_MNG_DATA* mng;
  char* domain;
  char* program;
  char* directory;
  char* client_ip;
};

struct SCP_DISCONNECTED_SESSION
{
  tui32 SID;
  tui8  type;
  tui8  status;
  tui16 height;
  tui16 width;
  tui8  bpp;
  tui8  idle_days;
  tui8  idle_hours;
  tui8  idle_minutes;
  tui16 conn_year;
  tui8  conn_month;
  tui8  conn_day;
  tui8  conn_hour;
  tui8  conn_minute;
  tui8  addr_type;
  tui32 ipv4addr;
  tui8  ipv6addr[16];
};

enum SCP_CLIENT_STATES_E
{
  SCP_CLIENT_STATE_OK,
  SCP_CLIENT_STATE_NETWORK_ERR,
  SCP_CLIENT_STATE_VERSION_ERR,
  SCP_CLIENT_STATE_SEQUENCE_ERR,
  SCP_CLIENT_STATE_SIZE_ERR,
  SCP_CLIENT_STATE_INTERNAL_ERR,
  SCP_CLIENT_STATE_SESSION_LIST,
  SCP_CLIENT_STATE_LIST_OK,
  SCP_CLIENT_STATE_RESEND_CREDENTIALS,
  SCP_CLIENT_STATE_CONNECTION_DENIED,
  SCP_CLIENT_STATE_PWD_CHANGE_REQ,
  SCP_CLIENT_STATE_RECONNECT_SINGLE,
  SCP_CLIENT_STATE_SELECTION_CANCEL,
  SCP_CLIENT_STATE_END
};

enum SCP_SERVER_STATES_E
{
  SCP_SERVER_STATE_OK,
  SCP_SERVER_STATE_VERSION_ERR,
  SCP_SERVER_STATE_NETWORK_ERR,
  SCP_SERVER_STATE_SEQUENCE_ERR,
  SCP_SERVER_STATE_INTERNAL_ERR,
  SCP_SERVER_STATE_SESSION_TYPE_ERR,
  SCP_SERVER_STATE_SIZE_ERR,
  SCP_SERVER_STATE_SELECTION_CANCEL,
  /*SCP_SERVER_STATE_FORCE_NEW,*/
  SCP_SERVER_STATE_START_MANAGE,
  SCP_SERVER_STATE_MNG_LISTREQ,
  SCP_SERVER_STATE_MNG_ACTION,
  SCP_SERVER_STATE_END
};

#endif
