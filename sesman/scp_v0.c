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
 * @file scp_v0.c
 * @brief scp version 0 implementation
 * @author Jay Sorg, Simone Fedele
 * 
 */

#include "sesman.h"

/******************************************************************************/
void DEFAULT_CC 
scp_v0_process(int in_sck, struct stream* in_s, struct stream* out_s)
{
  int code;
  int i;
  int width;
  int height;
  int bpp;
  int display;
  char user[256];
  char pass[256];
  long data;
  struct session_item* s_item;
      
  in_uint16_be(in_s, code);
  if (code == 0 || code == 10) /* check username - password, */
  {                            /* start session */
    in_uint16_be(in_s, i);
    in_uint8a(in_s, user, i);
    user[i] = 0;
    in_uint16_be(in_s, i);
    in_uint8a(in_s, pass, i);
    pass[i] = 0;
    in_uint16_be(in_s, width);
    in_uint16_be(in_s, height);
    in_uint16_be(in_s, bpp);
    data = auth_userpass(user, pass);
    display = 0;
    if (data)
    {
      s_item = session_get_bydata(user, width, height, bpp);
      if (s_item != 0)
      {
        display = s_item->display;
        auth_end(data);
        /* don't set data to null here */
      }
      else
      {
        g_printf("pre auth");
        if (1 == access_login_allowed(user))
        {
          log_message(LOG_LEVEL_INFO,
                      "granted TS access to user %s", user);
          if (0 == code)
          {
            log_message(LOG_LEVEL_INFO, "starting Xvnc session...");
            display = session_start(width, height, bpp, user, pass,
                                    data, SESMAN_SESSION_TYPE_XVNC);
          }
          else
          {
            log_message(LOG_LEVEL_INFO, "starting Xrdp session...");
            display = session_start(width, height, bpp, user, pass,
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
      }
    }
    init_stream(out_s, 8192);
    out_uint32_be(out_s, 0); /* version */
    out_uint32_be(out_s, 14); /* size */
    out_uint16_be(out_s, 3); /* cmd */
    out_uint16_be(out_s, data != 0); /* data */
    out_uint16_be(out_s, display); /* data */
    s_mark_end(out_s);
    tcp_force_send(in_sck, out_s->data,
                   out_s->end - out_s->data);
  }
}
