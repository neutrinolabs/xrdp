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
   Copyright (C) Jay Sorg 2005-2006
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

#include <sys/types.h>
#include <inttypes.h>

#include "os_calls.h"
#include "parse.h"
#include "arch.h"

//#warning sesman requires its own tcp streaming functions for threading safety
#include "tcp.h"

#define SCP_SID      uint32_t
#define SCP_DISPLAY  uint16_t

//#warning this should be an INT16 on every platform...
//typedef unsigned int SCP_DISPLAY_PORT; --> uint16_t is it portable?

#define SCP_RESOURCE_SHARING_REQUEST_YES 0x01
#define SCP_RESOURCE_SHARING_REQUEST_NO  0x00

#define SCP_SESSION_TYPE_XVNC 0x00
#define SCP_SESSION_TYPE_XRDP 0x01

#define SCP_ADDRESS_TYPE_IPV4 0x00
#define SCP_ADDRESS_TYPE_IPV6 0x01

#define SCP_COMMAND_SET_DEFAULT 0x0000
#define SCP_COMMAND_SET_MANAGE  0x0001
#define SCP_COMMAND_SET_RSR     0x0002

struct SCP_CONNECTION
{
  int in_sck;
  struct stream* in_s;
  struct stream* out_s;
};

struct SCP_SESSION
{
  unsigned char type;
  uint32_t version;
  uint16_t height;
  uint16_t width;
  unsigned char bpp;
  unsigned char rsr;
  char locale[18];
  char* username;
  char* password;
  char* hostname;
  unsigned char addr_type;
  uint32_t ipv4addr; //htons
  uint32_t ipv6addr; //should be 128bit
  uint16_t display;
  char* errstr;
};

struct SCP_DISCONNECTED_SESSION
{
  uint32_t SID;
  unsigned char type;
  uint16_t height;
  uint16_t width;
  unsigned char bpp;
  unsigned char idle_days;
  unsigned char idle_hours;
  unsigned char idle_minutes;
};

enum SCP_CLIENT_STATES_E
{
  SCP_CLIENT_STATE_OK,
  SCP_CLIENT_STATE_NETWORK_ERR
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
  SCP_SERVER_STATE_START_MANAGE,
  SCP_SERVER_STATE_END
};

#endif
