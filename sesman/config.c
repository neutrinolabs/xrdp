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
 * @file config.c
 * @brief User authentication code
 * @author Simone Fedele @< simo [at] esseemme [dot] org @>
 *
 */

#include "arch.h"
#include "list.h"
#include "file.h"
#include "sesman.h"

/******************************************************************************/
/**
 *
 * @brief Reads sesman configuration
 * @param s translates the strings "1", "true" and "yes" in 1 (true) and other strings in 0
 * @return 0 on success, 1 on failure
 *
 */
static int APP_CC
text2bool(char* s)
{
  if (0 == g_strcasecmp(s, "1") ||
      0 == g_strcasecmp(s, "true") ||
      0 == g_strcasecmp(s, "yes"))
  {
    return 1;
  }
  return 0;
}

/******************************************************************************/
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
    log_message(LOG_LEVEL_ALWAYS, "error opening %s in \
config_read", SESMAN_CFG_FILE);
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

  /* read Xvnc/X11rdp parameter list */
  config_read_vnc_params(fd, cfg, param_n, param_v);
  config_read_rdp_params(fd, cfg, param_n, param_v);

  /* read logging config */
  config_read_logging(fd, &(cfg->log), param_n, param_v);

  /* read security config */
  config_read_security(fd, &(cfg->sec), param_n, param_v);

  /* read session config */
  config_read_sessions(fd, &(cfg->sess), param_n, param_v);

  /* cleanup */
  list_delete(sec);
  list_delete(param_v);
  list_delete(param_n);
  g_file_close(fd);
  return 0;
}

/******************************************************************************/
int DEFAULT_CC
config_read_globals(int file, struct config_sesman* cf, struct list* param_n,
                    struct list* param_v)
{
  int i;
  char* buf;

  list_clear(param_v);
  list_clear(param_n);

  /* resetting the struct */
  cf->listen_address[0] = '\0';
  cf->listen_port[0] = '\0';
  cf->enable_user_wm = 0;
  cf->user_wm[0] = '\0';
  cf->default_wm[0] = '\0';
  cf->auth_file_path = 0;

  file_read_section(file, SESMAN_CFG_GLOBALS, param_n, param_v);
  for (i = 0; i < param_n->count; i++)
  {
    buf = (char*)list_get_item(param_n, i);
    if (0 == g_strcasecmp(buf, SESMAN_CFG_DEFWM))
    {
      g_strncpy(cf->default_wm, (char*)list_get_item(param_v, i), 31);
    }
    else if (0 == g_strcasecmp(buf, SESMAN_CFG_USERWM))
    {
      g_strncpy(cf->user_wm, (char*)list_get_item(param_v, i), 31);
    }
    else if (0 == g_strcasecmp(buf, SESMAN_CFG_ENABLE_USERWM))
    {
      cf->enable_user_wm = text2bool((char*)list_get_item(param_v, i));
    }
    else if (0 == g_strcasecmp(buf, SESMAN_CFG_PORT))
    {
      g_strncpy(cf->listen_port, (char*)list_get_item(param_v, i), 15);
    }
    else if (0 == g_strcasecmp(buf, SESMAN_CFG_ADDRESS))
    {
      g_strncpy(cf->listen_address, (char*)list_get_item(param_v, i), 31);
    }
    else if (0 == g_strcasecmp(buf, SESMAN_CFG_AUTH_FILE_PATH))
    {
      cf->auth_file_path = g_strdup((char*)list_get_item(param_v, i));
    }
  }

  /* checking for missing required parameters */
  if ('\0' == cf->listen_address[0])
  {
    g_strncpy(cf->listen_address, "0.0.0.0", 8);
  }
  if ('\0' == cf->listen_port[0])
  {
    g_strncpy(cf->listen_port, "3350", 5);
  }
  if ('\0' == cf->user_wm[0])
  {
    cf->enable_user_wm = 0;
  }
  if ('\0' == cf->default_wm[0])
  {
    g_strncpy(cf->default_wm, "startwm.sh", 11);
  }

  /* showing read config */
  g_printf("sesman config:\r\n");
  g_printf("\tListenAddress:            %s\r\n", cf->listen_address);
  g_printf("\tListenPort:               %s\r\n", cf->listen_port);
  g_printf("\tEnableUserWindowManager:  %i\r\n", cf->enable_user_wm);
  g_printf("\tUserWindowManager:        %s\r\n", cf->user_wm);
  g_printf("\tDefaultWindowManager:     %s\r\n", cf->default_wm);
  g_printf("\tAuthFilePath:             %s\r\n", ((cf->auth_file_path) ? (cf->auth_file_path) : ("disabled")));

  return 0;
}

/******************************************************************************/
int DEFAULT_CC
config_read_logging(int file, struct log_config* lc, struct list* param_n,
                    struct list* param_v)
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
    if (0 == g_strcasecmp(buf, SESMAN_CFG_LOG_FILE))
    {
      lc->log_file = g_strdup((char*)list_get_item(param_v, i));
    }
    if (0 == g_strcasecmp(buf, SESMAN_CFG_LOG_LEVEL))
    {
      lc->log_level = log_text2level((char*)list_get_item(param_v, i));
    }
    if (0 == g_strcasecmp(buf, SESMAN_CFG_LOG_ENABLE_SYSLOG))
    {
      lc->enable_syslog = text2bool((char*)list_get_item(param_v, i));
    }
    if (0 == g_strcasecmp(buf, SESMAN_CFG_LOG_SYSLOG_LEVEL))
    {
      lc->syslog_level = log_text2level((char*)list_get_item(param_v, i));
    }
  }

  if (0 == lc->log_file)
  {
    lc->log_file=g_strdup("./sesman.log");
  }

  g_printf("logging configuration:\r\n");
  g_printf("\tLogFile:       %s\r\n",lc->log_file);
  g_printf("\tLogLevel:      %i\r\n", lc->log_level);
  g_printf("\tEnableSyslog:  %i\r\n", lc->enable_syslog);
  g_printf("\tSyslogLevel:   %i\r\n", lc->syslog_level);

  return 0;
}

/******************************************************************************/
int DEFAULT_CC
config_read_security(int file, struct config_security* sc,
                     struct list* param_n,
                     struct list* param_v)
{
  int i;
  int gid;
  char* buf;

  list_clear(param_v);
  list_clear(param_n);

  /* setting defaults */
  sc->allow_root = 0;
  sc->login_retry = 3;
  sc->ts_users_enable = 0;
  sc->ts_admins_enable = 0;

  file_read_section(file, SESMAN_CFG_SECURITY, param_n, param_v);
  for (i = 0; i < param_n->count; i++)
  {
    buf = (char*)list_get_item(param_n, i);
    if (0 == g_strcasecmp(buf, SESMAN_CFG_SEC_ALLOW_ROOT))
    {
      sc->allow_root = text2bool((char*)list_get_item(param_v, i));
    }
    if (0 == g_strcasecmp(buf, SESMAN_CFG_SEC_LOGIN_RETRY))
    {
      sc->login_retry = g_atoi((char*)list_get_item(param_v, i));
    }
    if (0 == g_strcasecmp(buf, SESMAN_CFG_SEC_USR_GROUP))
    {
      if (g_getgroup_info((char*)list_get_item(param_v, i), &gid) == 0)
      {
        sc->ts_users_enable = 1;
        sc->ts_users = gid;
      }
    }
    if (0 == g_strcasecmp(buf, SESMAN_CFG_SEC_ADM_GROUP))
    {
      if (g_getgroup_info((char*)list_get_item(param_v, i), &gid) == 0)
      {
        sc->ts_admins_enable = 1;
        sc->ts_admins = gid;
      }
    }
  }

  /* printing security config */
  g_printf("security configuration:\r\n");
  g_printf("\tAllowRootLogin:       %i\r\n",sc->allow_root);
  g_printf("\tMaxLoginRetry:        %i\r\n",sc->login_retry);
  if (sc->ts_users_enable)
  {
    g_printf("\tTSUsersGroup:         %i\r\n", sc->ts_users);
  }
  else
  {
    g_printf("\tNo TSUsersGroup defined\r\n");
  }
  if (sc->ts_admins_enable)
  {
    g_printf("\tTSAdminsGroup:        %i\r\n", sc->ts_admins);
  }
  else
  {
    g_printf("\tNo TSAdminsGroup defined\r\n");
  }

  return 0;
}

/******************************************************************************/
int DEFAULT_CC
config_read_sessions(int file, struct config_sessions* se, struct list* param_n,
                    struct list* param_v)
{
  int i;
  char* buf;

  list_clear(param_v);
  list_clear(param_n);

  /* setting defaults */
  se->max_sessions=0;
  se->max_idle_time=0;
  se->max_disc_time=0;
  se->kill_disconnected=0;

  file_read_section(file, SESMAN_CFG_SESSIONS, param_n, param_v);
  for (i = 0; i < param_n->count; i++)
  {
    buf = (char*)list_get_item(param_n, i);
    if (0 == g_strcasecmp(buf, SESMAN_CFG_SESS_MAX))
    {
      se->max_sessions = g_atoi((char*)list_get_item(param_v, i));
    }
    if (0 == g_strcasecmp(buf, SESMAN_CFG_SESS_KILL_DISC))
    {
      se->kill_disconnected = text2bool((char*)list_get_item(param_v, i));
    }
    if (0 == g_strcasecmp(buf, SESMAN_CFG_SESS_IDLE_LIMIT))
    {
      se->max_idle_time=g_atoi((char*)list_get_item(param_v, i));
    }
    if (0 == g_strcasecmp(buf, SESMAN_CFG_SESS_DISC_LIMIT))
    {
      se->max_disc_time=g_atoi((char*)list_get_item(param_v, i));
    }
  }

  /* printing security config */
  g_printf("session configuration:\r\n");
  g_printf("\tMaxSessions:                 %i\r\n", se->max_sessions);
  g_printf("\tKillDisconnected:            %i\r\n", se->kill_disconnected);
  g_printf("\tIdleTimeLimit:               %i\r\n", se->max_idle_time);
  g_printf("\tDisconnectedTimeLimit:       %i\r\n", se->max_idle_time);

  return 0;
}

/******************************************************************************/
int DEFAULT_CC
config_read_rdp_params(int file, struct config_sesman* cs, struct list* param_n,
                       struct list* param_v)
{
  int i;

  list_clear(param_v);
  list_clear(param_n);

  cs->rdp_params=list_create();

  file_read_section(file, SESMAN_CFG_RDP_PARAMS, param_n, param_v);
  for (i = 0; i < param_n->count; i++)
  {
    list_add_item(cs->rdp_params, (long)g_strdup((char*)list_get_item(param_v, i)));
  }

  /* printing security config */
  g_printf("X11rdp parameters:\r\n");
  for (i = 0; i < cs->rdp_params->count; i++)
  {
    g_printf("\tParameter %02d                   %s\r\n", i, (char*)list_get_item(cs->rdp_params, i));
  }

  return 0;
}

/******************************************************************************/
int DEFAULT_CC
config_read_vnc_params(int file, struct config_sesman* cs, struct list* param_n,
                       struct list* param_v)
{
  int i;

  list_clear(param_v);
  list_clear(param_n);

  cs->vnc_params=list_create();

  file_read_section(file, SESMAN_CFG_VNC_PARAMS, param_n, param_v);
  for (i = 0; i < param_n->count; i++)
  {
    list_add_item(cs->vnc_params, (long)g_strdup((char*)list_get_item(param_v, i)));
  }

  /* printing security config */
  g_printf("Xvnc parameters:\r\n");
  for (i = 0; i < cs->vnc_params->count; i++)
  {
    g_printf("\tParameter %02d                   %s\r\n", i, (char*)list_get_item(cs->vnc_params, i));
  }

  return 0;
}

