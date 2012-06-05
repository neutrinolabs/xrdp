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
   Copyright (C) Jay Sorg 2005-2008
*/

/**
 *
 * @file scp_v0.c
 * @brief scp version 0 implementation
 * @author Jay Sorg, Simone Fedele
 *
 */

#include "sesman.h"

extern struct config_sesman* g_cfg; /* in sesman.c */

/******************************************************************************/
void DEFAULT_CC
scp_v0_process(struct SCP_CONNECTION* c, struct SCP_SESSION* s)
{
  int display = 0;
  tbus data;
  struct session_item* s_item;

  data = auth_userpass(s->username, s->password);
  if(s->type==SCP_GW_AUTHENTICATION)
  {
    /* this is just authentication in a gateway situation */
    /* g_writeln("SCP_GW_AUTHENTICATION message received"); */
      if(data){	 
	  if (1 == access_login_allowed(s->username))
	  {
	      /* the user is member of the correct groups. */
	      scp_v0s_replyauthentication(c,0);
	      /* g_writeln("Connection allowed"); */
	  }else{	      
              scp_v0s_replyauthentication(c,3);
	      /* g_writeln("user password ok, but group problem"); */
	  }
      }else{	  
	  /* g_writeln("username or password error"); */
          scp_v0s_replyauthentication(c,2);	  
      }
      auth_end(data);
  }
  else if (data)
  {     
    s_item = session_get_bydata(s->username, s->width, s->height, s->bpp, s->type);
    if (s_item != 0)
    {
      display = s_item->display;
      if (0 != s->client_ip)
      {
        log_message( LOG_LEVEL_INFO, "++ reconnected session: username %s, display :%d.0, session_pid %d, ip %s", s->username, display, s_item->pid, s->client_ip);
      }
      else
      {
        log_message(LOG_LEVEL_INFO, "++ reconnected session: username %s, display :%d.0, session_pid %d", s->username, display, s_item->pid);
      }
      auth_end(data);
      /* don't set data to null here */
    }
    else
    {
      LOG_DBG("pre auth");
      if (1 == access_login_allowed(s->username))
      {
        if (0 != s->client_ip)
        {
          log_message(LOG_LEVEL_INFO, "++ created session (access granted): username %s, ip %s", s->username, s->client_ip);
        }
        else
        {
          log_message(LOG_LEVEL_INFO, "++ created session (access granted): username %s", s->username);
        }

        if (SCP_SESSION_TYPE_XVNC == s->type)
        {
          log_message( LOG_LEVEL_INFO, "starting Xvnc session...");
          display = session_start(s->width, s->height, s->bpp, s->username,
                                  s->password, data, SESMAN_SESSION_TYPE_XVNC,
                                  s->domain, s->program, s->directory, s->client_ip);
        }
        else
        {
          log_message(LOG_LEVEL_INFO, "starting X11rdp session...");
          display = session_start(s->width, s->height, s->bpp, s->username,
                                  s->password, data, SESMAN_SESSION_TYPE_XRDP,
                                  s->domain, s->program, s->directory, s->client_ip);
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

