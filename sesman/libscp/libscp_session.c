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
 * @file libscp_session.c
 * @brief SCP_SESSION handling code
 * @author Simone Fedele
 *
 */

#include "libscp_session.h"

struct SCP_SESSION*
scp_session_create()
{
  struct SCP_SESSION* s;

  s = g_malloc(sizeof(struct SCP_SESSION), 0);

  if (0 == s)
  {
    return 0;
  }

  s->username=0;
  s->password=0;
  s->hostname=0;
  s->errstr=0;
#warning usare scp_session_set* per inizializzare la sessione!!!!!!

  return s;
}

/*******************************************************************/
int
scp_session_set_type(struct SCP_SESSION* s, tui8 type)
{
  switch (type)
  {
    case SCP_SESSION_TYPE_XVNC:
      s->type = SCP_SESSION_TYPE_XVNC;
      break;
    case SCP_SESSION_TYPE_XRDP:
      s->type = SCP_SESSION_TYPE_XRDP;
      break;
    default:
      return 1;
  }
  return 0;
}

/*******************************************************************/
int
scp_session_set_version(struct SCP_SESSION* s, tui32 version)
{
  switch (version)
  {
    case 0:
      s->version = 0;
      break;
    case 1:
      s->version = 1;
      break;
    default:
      return 1;
  }
  return 0;
}

/*******************************************************************/
int
scp_session_set_height(struct SCP_SESSION* s, tui16 h)
{
  s->height = h;
  return 0;
}

/*******************************************************************/
int
scp_session_set_width(struct SCP_SESSION* s, tui16 w)
{
  s->width = w;
  return 0;
}

/*******************************************************************/
int
scp_session_set_bpp(struct SCP_SESSION* s, tui8 bpp)
{
  switch (bpp)
  {
    case 8:
    case 16:
    case 24:
      s->bpp = bpp;
    default:
      return 1;
  }
  return 0;
}

/*******************************************************************/
int
scp_session_set_rsr(struct SCP_SESSION* s, tui8 rsr)
{
  if (s->rsr)
  {
    s->rsr = 1;
  }
  else
  {
    s->rsr = 0;
  }
  return 0;
}

/*******************************************************************/
int
scp_session_set_locale(struct SCP_SESSION* s, char* str)
{
  if (0 == str) return 1;
  g_strncpy(s->locale, str, 17);
  s->locale[17]='\0';
  return 0;
}

/*******************************************************************/
int
scp_session_set_username(struct SCP_SESSION* s, char* str)
{
  if (0 == str) return 1;
  if (0 != s->username) g_free(s->username);
  s->username = g_strdup(str);
  return 0;
}

/*******************************************************************/
int
scp_session_set_password(struct SCP_SESSION* s, char* str)
{
  if (0 == str) return 1;
  if (0 != s->password) g_free(s->password);
  s->password = g_strdup(str);
  return 0;
}

/*******************************************************************/
int
scp_session_set_hostname(struct SCP_SESSION* s, char* str)
{
  if (0 == str) return 1;
  if (0 != s->hostname) g_free(s->hostname);
  s->hostname = g_strdup(str);
  return 0;
}

/*******************************************************************/
int
scp_session_set_errstr(struct SCP_SESSION* s, char* str)
{
  if (0 == str) return 1;
  if (0 != s->errstr) g_free(s->errstr);
  s->errstr = g_strdup(str);
  return 0;
}

/*******************************************************************/
int
scp_session_set_display(struct SCP_SESSION* s, SCP_DISPLAY display)
{
  s->display=display;
  return 0;
}

/*******************************************************************/
int
scp_session_set_addr(struct SCP_SESSION* s, int type, char* addr)
{

}

/*******************************************************************/
void
scp_session_destroy(struct SCP_SESSION* s)
{
  if (s->username)
  {
    g_free(s->username);
    s->username=0;
  }

  if (s->password)
  {
    g_free(s->password);
    s->password=0;
  }

  if (s->hostname)
  {
    g_free(s->hostname);
    s->hostname=0;
  }

  if (s->errstr)
  {
    g_free(s->errstr);
    s->errstr=0;
  }

  g_free(s);
}
