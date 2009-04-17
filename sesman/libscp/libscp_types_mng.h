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
   Copyright (C) Jay Sorg 2005-2009
*/

/**
 *
 * @file libscp_types_mng.h
 * @brief libscp data types definitions
 * @author Simone Fedele
 *
 */

#ifndef LIBSCP_TYPES_MNG_H
#define LIBSCP_TYPES_MNG_H

#include "os_calls.h"
#include "parse.h"
#include "arch.h"
#include "log.h"

enum SCP_MNG_COMMAND
{
  SCP_MNG_CMD_KILL,
  SCP_MNG_CMD_DISCONNECT
};

struct SCP_MNG_DATA
{
  enum SCP_MNG_COMMAND cmd;
  SCP_SID sid;
};

#endif
