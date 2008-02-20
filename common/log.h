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

#ifndef LOG_H
#define LOG_H

#include <pthread.h>

#include "arch.h"

/* logging buffer size */
#define LOG_BUFFER_SIZE      1024

/* logging levels */
#define LOG_LEVEL_ALWAYS        0
#define LOG_LEVEL_ERROR         1
#define LOG_LEVEL_WARNING       2
#define LOG_LEVEL_INFO          3
#define LOG_LEVEL_DEBUG         4

/* startup return values */
#define LOG_STARTUP_OK          0
#define LOG_ERROR_MALLOC        1
#define LOG_ERROR_NULL_FILE     2
#define LOG_ERROR_FILE_OPEN     3
#define LOG_ERROR_NO_CFG        4
#define LOG_ERROR_FILE_NOT_OPEN 5

/* enable threading */
/*#define LOG_ENABLE_THREAD*/

#ifdef DEBUG
  #define LOG_DBG(lcfg,args...) log_message((lcfg), LOG_LEVEL_DEBUG, args);
#else
  #define LOG_DBG(lcfg,args...)
#endif

struct log_config
{
  char* program_name;
  char* log_file;
  int fd;
  unsigned int log_level;
  int enable_syslog;
  unsigned int syslog_level;
  pthread_mutex_t log_lock;
  pthread_mutexattr_t log_lock_attr;
};

/**
 *
 * @brief Logs a message. Optionally logs the same message on syslog
 * @param lvl The level of the logged message
 * @param msg The message to be logged
 * @return
 *
 */
int DEFAULT_CC
log_message(struct log_config* l_cfg, const unsigned int lvl, const char* msg, ...);

/**
 *
 * @brief Starts the logging subsystem
 * @param l_cfg loggging system configuration
 * @return
 *
 */
int DEFAULT_CC
log_start(struct log_config* l_cfg);

/**
 *
 * @brief Shuts down the logging subsystem
 * @param l_cfg pointer to the logging subsystem to stop
 *
 */
void DEFAULT_CC
log_end(struct log_config* l_cfg);

/**
 *
 * @brief Converts a string to a log level
 * @param s The string to convert
 * @return The corresponding level or LOG_LEVEL_DEBUG if error
 *
 */
int DEFAULT_CC
log_text2level(char* s);

#endif

