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
 * @file scp_v0.c
 * @brief scp version 0 implementation
 * @author Jay Sorg, Simone Fedele
 * 
 */

#include "sesman.h"

/******************************************************************************/
void DEFAULT_CC 
scp_v0_process(struct SCP_CONNECTION* c, struct SCP_SESSION* s)
{
  int display = 0;
  long data;
  struct session_item* s_item;

  data = auth_userpass(s->username, s->password);

  if (data)
  {
    s_item = session_get_bydata(s->username, s->width, s->height, s->bpp);
    if (s_item != 0)
    {
      display = s_item->display;
      auth_end(data);
      /* don't set data to null here */
    }
    else
    {
      g_printf("pre auth");
      if (1 == access_login_allowed(s->username))
      {
        log_message(LOG_LEVEL_INFO, "granted TS access to user %s", s->username);
        if (SCP_SESSION_TYPE_XVNC == s->type)
        {
          log_message(LOG_LEVEL_INFO, "starting Xvnc session...");
          display = session_start(s->width, s->height, s->bpp, s->username, s->password,
                                  data, SESMAN_SESSION_TYPE_XVNC);
        }
        else
        {
          log_message(LOG_LEVEL_INFO, "starting Xrdp session...");
          display = session_start(s->width, s->height, s->bpp, s->username, s->password,
                                  data, SESMAN_SESSION_TYPE_XRDP);
        }
      }
      else
      {
        display = 0;
      }
    }
    if (display == 0)
    {
      auth_end(data);
      data = 0;
      scp_v0s_deny_connection(c);
    }
    else
    {
      scp_v0s_allow_connection(c, display);
    }
  }
  else
  {
    scp_v0s_deny_connection(c);
  }
}

