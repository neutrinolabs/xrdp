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
 * @file libscp_init.c
 * @brief libscp initialization code
 * @author Simone Fedele
 *
 */

#include "libscp_init.h"

//struct log_config* s_log;

/* server API */
int DEFAULT_CC
scp_init()
{
/*
  if (0 == log)
  {
    return 1;
  }
*/

  //s_log = log;

  scp_lock_init();

  log_message(LOG_LEVEL_WARNING, "[init:%d] libscp initialized", __LINE__);

  return 0;
}

