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
static int text2bool(char* s)
{	
  if (0 == g_strncasecmp(s, "1", 1) ||
      0 == g_strncasecmp(s, "true", 4) ||
      0 == g_strncasecmp(s, "yes", 3))
  {
    return 1;
  }
  return 0;
}

/******************************************************************************/
/* returns error */
int DEFAULT_CC
config_read(struct config_sesman* cfg)
{
  int fd;
  struct list* sec;
  struct list* param_n;
  struct list* param_v;

  fd = g_file_open(SESMAN_CFG_FILE);
  if (-1 == fd)
  {
    g_printf("sesman: error reading config: %s\n\r", SESMAN_CFG_FILE);
    return 1;
  }
  g_memset(cfg, 0, sizeof(struct config_sesman));
  sec = list_create();
  sec->auto_free = 1;
  file_read_sections(fd, sec);
  param_n = list_create();
  param_n->auto_free = 1;
  param_v = list_create();
  param_v->auto_free = 1;

  /* read global config */
  config_read_globals(fd, cfg, param_n, param_v);
 
  /* read logging config */
  config_read_logging(fd, &(cfg->log), param_n, param_v);
  
  /* cleanup */
  list_delete(sec);
  list_delete(param_v);
  list_delete(param_n);
  return 0;
}

int DEFAULT_CC
config_read_globals(int file, struct config_sesman* cf, struct list* param_n, struct list* param_v)
{
  int i;
  char* buf;

  list_clear(param_v);
  list_clear(param_n);
	  
  file_read_section(file, SESMAN_CFG_GLOBALS, param_n, param_v);
  for (i = 0; i < param_n->count; i++)
  {
    buf = (char*)list_get_item(param_n, i);
    if (0 == g_strncasecmp(buf, SESMAN_CFG_DEFWM, 20))
    {
      g_strncpy(cf->default_wm, (char*)list_get_item(param_v, i), 31);
    }
    else if (0 == g_strncasecmp(buf, SESMAN_CFG_USERWM, 20))
    {
      g_strncpy(cf->user_wm, (char*)list_get_item(param_v, i), 31);
    }
    else if (0 == g_strncasecmp(buf, SESMAN_CFG_ENABLE_USERWM, 20))
    {
      cf->enable_user_wm = text2bool((char*) list_get_item(param_v, i));
    }
    else if (0 == g_strncasecmp(buf, SESMAN_CFG_PORT, 20))
    {
      g_strncpy(cf->listen_port, (char*)list_get_item(param_v, i), 15);
    }
  }
  
  g_printf("sesman config:\n\r");
  g_printf("\tListenPort:               %s\n\r", cf->listen_port);
  g_printf("\tEnableUserWindowManager:  %i\n\r", cf->enable_user_wm);
  g_printf("\tUserWindowManager:        %s\n\r", cf->user_wm);
  g_printf("\tDefaultWindowManager:     %s\n\r", cf->default_wm);
  
  return 0;
}

int DEFAULT_CC
config_read_logging(int file, struct log_config* lc, struct list* param_n, struct list* param_v)
{
  int i;
  char* buf;

  list_clear(param_v);
  list_clear(param_n);

  /* setting defaults */
  lc->program_name = g_strdup("sesman");
  lc->log_file = 0;
  lc->fd = 0;
  lc->log_level = LOG_LEVEL_DEBUG;
  lc->enable_syslog = 0;
  lc->syslog_level = LOG_LEVEL_DEBUG;
  
  file_read_section(file, SESMAN_CFG_LOGGING, param_n, param_v);
  for (i = 0; i < param_n->count; i++)
  {
    buf = (char*)list_get_item(param_n, i);
    if (0 == g_strncasecmp(buf, SESMAN_CFG_LOG_FILE, 20))
    {
      lc->log_file = g_strdup((char*)list_get_item(param_v, i));
    }
    if (0 == g_strncasecmp(buf, SESMAN_CFG_LOG_LEVEL, 20))
    {
      lc->log_level = log_text2level((char*)list_get_item(param_v, i));
    }
    if (0 == g_strncasecmp(buf, SESMAN_CFG_LOG_ENABLE_SYSLOG, 20))
    {
      lc->enable_syslog = text2bool((char*)list_get_item(param_v, i));
    }
    if (0 == g_strncasecmp(buf, SESMAN_CFG_LOG_SYSLOG_LEVEL, 20))
    {
      lc->syslog_level = log_text2level((char*)list_get_item(param_v, i));
    }
  }
  
  g_printf("logging configuration:\n\r");
  g_printf("\tLogFile:       %s\n\r",lc->log_file);
  g_printf("\tLogLevel:      %i\n\r", lc->log_level);
  g_printf("\tEnableSyslog:  %i\n\r", lc->enable_syslog);
  g_printf("\tSyslogLevel:   %i\n\r", lc->syslog_level);	  
  
  return 0;
}

