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
 * @file libscp_commands_mng.h
 * @brief libscp data types definitions
 * @author Simone Fedele
 *
 */

#ifndef LIBSCP_COMMANDS_MNG_H
#define LIBSCP_COMMANDS_MNG_H

#define SCP_CMD_MNG_LOGIN        0x0001
#define SCP_CMD_MNG_LOGIN_ALLOW  0x0002
#define SCP_CMD_MNG_LOGIN_DENY   0x0003
#define SCP_CMD_MNG_CMD_ERROR    0x0004
#define SCP_CMD_MNG_LIST_REQ     0x0005
#define SCP_CMD_MNG_LIST         0x0006
#define SCP_CMD_MNG_ACTION       0x0007

#endif
