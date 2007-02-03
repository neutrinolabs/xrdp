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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#include "os_calls.h"

#include "log.h"

/* this gets created in log_start and freed in log_end */
static struct log_config* l_cfg;

/* threading additions */
#ifdef LOG_ENABLE_THREAD
#include "pthread.h"
/* these get initalized in log_start, they don't need
   to get freed */
static pthread_mutex_t log_lock;
static pthread_mutexattr_t log_lock_attr;
#endif

/**
 *
 * @brief Opens log file
 * @param fname log file name
 * @return see open(2) return values
 * 
 */
static int DEFAULT_CC
log_file_open(const char* fname)
{
  return open(fname, O_WRONLY | O_CREAT | O_APPEND | O_SYNC, S_IRUSR |
              S_IWUSR);
}

/**
 *
 * @brief Converts xrdp log level to syslog logging level
 * @param xrdp logging level
 * @return syslog equivalent logging level
 *
 */
static int DEFAULT_CC
log_xrdp2syslog(const int lvl)
{
  switch (lvl)
  {
    case LOG_LEVEL_ALWAYS:
      return LOG_CRIT;
    case LOG_LEVEL_ERROR:
      return LOG_ERR;
    case LOG_LEVEL_WARNING:
      return LOG_WARNING;
    case LOG_LEVEL_INFO:
      return LOG_INFO;
    /* case LOG_LEVEL_DEBUG: */
    default:
      return LOG_DEBUG;
  }
}

/**
 *ring 
 * @brief Converts xrdp log level to syslog logging level
 * @param lvl logging level
 * @param str pointer to a st
 * @return syslog equivalent logging level
 *
 */
static void DEFAULT_CC
log_lvl2str(int lvl, char* str)
{
  switch (lvl)
  {
    case LOG_LEVEL_ALWAYS:
      snprintf(str, 9, "%s", "[CORE ] ");
      break;
    case LOG_LEVEL_ERROR:
      snprintf(str, 9, "%s", "[ERROR] ");
      break;
    case LOG_LEVEL_WARNING:
      snprintf(str, 9, "%s", "[WARN ] ");
      break;
    case LOG_LEVEL_INFO:
      snprintf(str, 9, "%s", "[INFO ] ");
      break;
    /* case LOG_LEVEL_DEBUG: */
    default:
      snprintf(str, 9, "%s", "[DEBUG] ");
      break;
  }
}

/******************************************************************************/
int DEFAULT_CC
log_message(const unsigned int lvl, const char* msg, ...)
{
  char buff[LOG_BUFFER_SIZE + 31]; /* 19 (datetime) 4 (space+cr+lf+\0) */
  va_list ap;
  int len = 0;
  int rv;
  time_t now_t;
  struct tm* now;

  rv = 0;
  if (0 == l_cfg)
  {
    return LOG_ERROR_NO_CFG;
  }

  if (0 > l_cfg->fd)
  {
    return LOG_ERROR_FILE_NOT_OPEN;
  }

  now_t = time(&now_t);
  now = localtime(&now_t);

  snprintf(buff, 21, "[%.4d%.2d%.2d-%.2d:%.2d:%.2d] ", (now->tm_year) + 1900,
           (now->tm_mon) + 1, now->tm_mday, now->tm_hour, now->tm_min,
           now->tm_sec);

  log_lvl2str(lvl, buff + 20);

  va_start(ap, msg);
  len = vsnprintf(buff + 28, LOG_BUFFER_SIZE, msg, ap);
  va_end(ap);

  /* checking for truncated messages */ 
  if (len > LOG_BUFFER_SIZE)
  {
    log_message(LOG_LEVEL_WARNING, "next message will be truncated");
  }

  /* forcing the end of message string */
  #ifdef _WIN32
    buff[len + 28] = '\r';
    buff[len + 29] = '\n';
    buff[len + 30] = '\0';
  #else
    #ifdef _MACOS
      buff[len + 28] = '\r';
      buff[len + 29] = '\0';
    #else
      buff[len + 28] = '\n';
      buff[len + 29] = '\0';
    #endif
  #endif

  if (l_cfg->enable_syslog  && (lvl <= l_cfg->log_level))
  {
    /* log to syslog */
    syslog(log_xrdp2syslog(lvl), buff + 20);
  }

  if (lvl <= l_cfg->log_level)
  {
    /* log to console */
    g_printf((char*)buff);

    /* log to application logfile */
#ifdef LOG_ENABLE_THREAD
    pthread_mutex_lock(&log_lock);
#endif
    rv = g_file_write(l_cfg->fd, (char*)buff, g_strlen((char*)buff));
#ifdef LOG_ENABLE_THREAD
    pthread_mutex_unlock(&log_lock);
#endif
  }
  return rv;
}

/******************************************************************************/
int DEFAULT_CC
log_start(const char* progname, const char* logfile, const unsigned int loglvl,
          const int syslog, const unsigned int syslvl)
{
  /* setup log struct */
  l_cfg = (struct log_config*)g_malloc(sizeof(struct log_config), 1);

  if (0 == l_cfg)
  {
    return LOG_ERROR_MALLOC;
  }

  /* if logfile is NULL, we use a default logfile */
  if (0 == logfile)
  {
    l_cfg->log_file = g_strdup("./myprogram.log");
  }
  else
  {
    l_cfg->log_file = g_strdup(logfile);
  }

  /* if progname is NULL, we use a default name */
  if (0 == progname)
  {
    l_cfg->program_name = g_strdup("myprogram");
  }
  else
  {
    l_cfg->program_name = g_strdup(progname);
  }

  /* setting log level */
  l_cfg->log_level = loglvl;

  /* 0 disables syslog, everything else enables it */
  l_cfg->enable_syslog = (syslog ? 1 : 0);
  /* forcing syslog_level to be always <= app logging level */
  l_cfg->syslog_level = (syslvl>loglvl ? loglvl : syslvl);

  /* open file */
  l_cfg->fd = log_file_open(l_cfg->log_file);

  if (-1 == l_cfg->fd)
  {
    return LOG_ERROR_FILE_OPEN;
  }

  /* if syslog is enabled, open it */
  if (l_cfg->enable_syslog)
  {
    openlog(l_cfg->program_name, LOG_CONS | LOG_PID, LOG_DAEMON);
  }

#ifdef LOG_ENABLE_THREAD
  pthread_mutexattr_init(&log_lock_attr);
  pthread_mutex_init(&log_lock, &log_lock_attr);
#endif

  return LOG_STARTUP_OK;
}

/******************************************************************************/
void DEFAULT_CC
log_end(void)
{
  /* if log is closed, quit silently */
  if (0 == l_cfg)
  {
    return;
  }

  /* closing log file */
  log_message(LOG_LEVEL_ALWAYS, "shutting down log subsystem...");

  if (0 > l_cfg->fd)
  {
    /* if syslog is enabled, close it */
    if (l_cfg->enable_syslog)
    {
      closelog();
    }
  }

  /* closing logfile... */
  g_file_close(l_cfg->fd);

  /* if syslog is enabled, close it */
  if (l_cfg->enable_syslog)
  {
    closelog();
  }

  /* freeing allocated memory */
  g_free(l_cfg->log_file);
  g_free(l_cfg->program_name);
  g_free(l_cfg);

  l_cfg = 0;
}

/******************************************************************************/
int DEFAULT_CC
log_text2level(char* buf)
{
  if (0 == g_strcasecmp(buf, "0") ||
      0 == g_strcasecmp(buf, "core"))
  {
    return LOG_LEVEL_ALWAYS;
  }
  else if (0 == g_strcasecmp(buf, "1") ||
           0 == g_strcasecmp(buf, "error"))
  {
    return LOG_LEVEL_ERROR;
  }
  else if (0 == g_strcasecmp(buf, "2") ||
           0 == g_strcasecmp(buf, "warn") ||
           0 == g_strcasecmp(buf, "warning"))
  {
    return LOG_LEVEL_WARNING;
  }
  else if (0 == g_strcasecmp(buf, "3") ||
           0 == g_strcasecmp(buf, "info"))
  {
    return LOG_LEVEL_INFO;
  }
  return LOG_LEVEL_DEBUG;
}
