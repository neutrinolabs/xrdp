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
   Copyright (C) Jay Sorg 2005

   session manager - read config file
*/

#include "list.h"
#include "file.h"
#include "sesman.h"

/******************************************************************************/
/* returns error */
int DEFAULT_CC
config_read(struct sesman_config* cfg)
{
  int i;
  int fd;
  struct list* sec;
  struct list* param_n;
  struct list* param_v;
  char* buf;

  fd = g_file_open(SESMAN_CFG_FILE);
  if (-1 == fd)
  {
    g_printf("sesman: error reading config: %s\n\r", SESMAN_CFG_FILE);
    return 1;
  }
  g_memset(cfg, 0, sizeof(struct sesman_config));
  sec = list_create();
  sec->auto_free = 1;
  file_read_sections(fd, sec);
  param_n = list_create();
  param_n->auto_free = 1;
  param_v = list_create();
  param_v->auto_free = 1;
  file_read_section(fd, SESMAN_CFG_GLOBALS, param_n, param_v);
  for (i = 0; i < param_n->count; i++)
  {
    buf = (char*)list_get_item(param_n, i);
    if (0 == g_strncasecmp(buf, SESMAN_CFG_DEFWM, 20))
    {
      g_strncpy(cfg->default_wm, (char*)list_get_item(param_v, i), 31);
    }
    else if (0 == g_strncasecmp(buf, SESMAN_CFG_USERWM, 20))
    {
      g_strncpy(cfg->user_wm, (char*)list_get_item(param_v, i), 31);
    }
    else if (0 == g_strncasecmp(buf, SESMAN_CFG_ENABLE_USERWM, 20))
    {
      buf = (char*)list_get_item(param_v, i);
      if (0 == g_strncasecmp(buf, "1", 1) ||
          0 == g_strncasecmp(buf, "true", 4) ||
          0 == g_strncasecmp(buf, "yes", 3))
      {
        cfg->enable_user_wm = 1;
      }
    }
    else if (0 == g_strncasecmp(buf, SESMAN_CFG_PORT, 20))
    {
      g_strncpy(cfg->listen_port, (char*)list_get_item(param_v, i), 15);
    }
  }
  g_printf("sesman config:\n\r");
  g_printf("\tListenPort:               %s\n\r", cfg->listen_port);
  g_printf("\tEnableUserWindowManager:  %i\n\r", cfg->enable_user_wm);
  g_printf("\tUserWindowManager:        %s\n\r", cfg->user_wm);
  g_printf("\tDefaultWindowManager:     %s\n\r", cfg->default_wm);
  /* cleanup */
  list_delete(sec);
  list_delete(param_v);
  list_delete(param_n);
  return 0;
}
