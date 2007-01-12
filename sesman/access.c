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
 * @file access.c
 * @brief User access control code
 * @author Simone Fedele
 * 
 */

#include "sesman.h"

extern struct config_sesman g_cfg;

/******************************************************************************/
int DEFAULT_CC
access_login_allowed(char* user)
{
  int gid;
  int ok;

  if ((0 == g_strncmp(user, "root", 5)) && (0 == g_cfg.sec.allow_root))
  {
    log_message(LOG_LEVEL_WARNING,
                "ROOT login attempted, but root login is disabled");
    return 0;
  }

  if (0 == g_cfg.sec.ts_users_enable)
  {
    LOG_DBG("Terminal Server Users group is disabled, allowing authentication",
            1);
    return 1;
  }

  if (0 != g_getuser_info(user, &gid, 0, 0, 0, 0))
  {
    log_message(LOG_LEVEL_ERROR, "Cannot read user info! - login denied");
    return 0;
  }

  if (g_cfg.sec.ts_users == gid)
  {
    LOG_DBG("ts_users is user's primary group", 1);
    return 1;
  }

  if (0 != g_check_user_in_group(user, g_cfg.sec.ts_users, &ok))
  {
    log_message(LOG_LEVEL_ERROR, "Cannot read group info! - login denied");
    return 0;
  }

  if (ok)
  {
    return 1;
  }

  log_message(LOG_LEVEL_INFO, "login denied for user %s", user);

  return 0;
}
