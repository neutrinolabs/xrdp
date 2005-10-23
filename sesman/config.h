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

#ifndef CONFIG_H
#define CONFIG_H

#include "arch.h"
#include "list.h"
#include "log.h"

#define SESMAN_CFG_FILE              "./sesman.ini"

#define SESMAN_CFG_GLOBALS           "Globals"
#define SESMAN_CFG_DEFWM             "DefaultWindowManager"
#define SESMAN_CFG_PORT              "ListenPort"
#define SESMAN_CFG_ENABLE_USERWM     "EnableUserWindowManager"
#define SESMAN_CFG_USERWM            "UserWindowManager"

#define SESMAN_CFG_LOGGING           "Logging"
#define SESMAN_CFG_LOG_FILE          "LogFile"
#define SESMAN_CFG_LOG_LEVEL         "LogLevel"
#define SESMAN_CFG_LOG_ENABLE_SYSLOG "EnableSyslog"
#define SESMAN_CFG_LOG_SYSLOG_LEVEL  "SyslogLevel"

struct config_sesman
{
  char listen_port[16];
  int enable_user_wm;
  char default_wm[32];
  char user_wm[32];
  struct log_config log;
};

/**
 *
 * Reads sesman configuration
 *
 * @param cfg pointer to configuration object to be replaced
 *
 * @return 0 on success, 1 on failure
 * 
 */
int DEFAULT_CC
config_read(struct config_sesman* cfg);

/**
 *
 * Reads sesman configuration
 *
 * @param cfg pointer to configuration object to be replaced
 *
 * @return 0 on success, 1 on failure
 * 
 */
int DEFAULT_CC
config_read_globals(int file, struct config_sesman* cf, struct list* param_n, struct list* param_v);

/**
 *
 * Reads sesman configuration
 *
 * @param cfg pointer to configuration object to be replaced
 *
 * @return 0 on success, 1 on failure
 * 
 */
int DEFAULT_CC
config_read_logging(int file, struct log_config* lc, struct list* param_n, struct list* param_v);

#endif
