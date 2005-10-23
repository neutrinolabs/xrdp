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

   session manager
   linux only

   log.c: logging code

*/

#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "syslog.h"
#include "stdarg.h"
#include "stdio.h"
#include "time.h"

#include "os_calls.h"

#include "log.h"

static struct log_config *l_cfg;

/**
 *
 * Opens log file
 *
 * @param fname log file name
 * 
 * @return see open(2) return values
 * 
 */
static int log_file_open(const char* fname)
{
  return open(fname, O_WRONLY | O_CREAT | O_APPEND | O_SYNC, S_IRUSR | S_IWUSR);
}

/**
 *
 * Converts xrdp log level to syslog logging level
 *
 * @param xrdp logging level
 *
 * @return syslog equivalent logging level
 *
 */
static int log_xrdp2syslog(const int lvl)
{
  switch (lvl)
  {
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
 *
 * Converts xrdp log level to syslog logging level
 *
 * @param xrdp logging level
 *
 * @return syslog equivalent logging level
 *
 */
void log_lvl2str(int lvl, char* str)
{
  switch (lvl)
  {
    case LOG_LEVEL_ERROR:
	    snprintf(str, 9, "%s", "[ERROR] ");
    case LOG_LEVEL_WARNING:
	    snprintf(str, 9, "%s", "[WARN ] ");
    case LOG_LEVEL_INFO:
	    snprintf(str, 9, "%s", "[INFO ] ");
    /* case LOG_LEVEL_DEBUG: */
    default:
	    snprintf(str, 9, "%s", "[DEBUG] ");
  }
}
/******************************************************************************/
int DEFAULT_CC
log_message(const unsigned int lvl, const char* msg, ...)
{
  char buff[LOG_BUFFER_SIZE+31]; /* 19 (datetime) 4 (space+cr+lf+\0) */
  va_list ap;
  int len = 0;
  time_t now_t;
  struct tm* now;

  if (0 == l_cfg) 
  {
    return LOG_ERROR_NO_CFG;
  }

  if (0 > l_cfg->fd) 
  {
    return LOG_ERROR_FILE_NOT_OPEN;
  }

  log_lvl2str(lvl, buff);
  
  now_t = time(&now_t);
  now = localtime(&now_t);
  
  snprintf(buff+8, 21, "[%.4d%.2d%.2d-%.2d:%.2d:%.2d] ", (now->tm_year)+1900, (now->tm_mon)+1, 
           now->tm_mday, now->tm_hour, now->tm_min, now->tm_sec);

  va_start(ap, msg);
  len = vsnprintf(buff+28, LOG_BUFFER_SIZE, msg, ap);
  va_end(ap);
 
  /* checking for truncated messages */ 
  if (len > LOG_BUFFER_SIZE)
  {
    log_message(LOG_LEVEL_WARNING, "next message will be truncated");
  }  
  
  /* forcing the end of message string */
  buff[len+28] = '\r';
  buff[len+29] = '\n';
  buff[len+30] = '\0';
  
  if ( l_cfg->enable_syslog  && (lvl <= l_cfg->log_level) )
  {
    /* log to syslog */
    syslog(log_xrdp2syslog(lvl), msg);
  }
  
  if (lvl <= l_cfg->log_level)
  {
    /* log to application logfile */
    return g_file_write(l_cfg->fd, (char*) buff, g_strlen((char*) buff));
  }
  return 0;
}

/******************************************************************************/
int DEFAULT_CC
log_start(const char* progname, const char* logfile, const unsigned int loglvl, 
          const int syslog, const unsigned int syslvl)
{
  /* setup log struct */
  l_cfg = (struct log_config*) g_malloc(sizeof(struct log_config),1);

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
    l_cfg->log_file = g_strdup((char*) logfile);
  }
 
  /* if progname is NULL, we use a default name */
  if (0 == progname)
  {
    l_cfg->program_name = g_strdup("myprogram");
  }
  else
  {
    l_cfg->program_name = g_strdup((char*) progname);
  }
  
  /* setting log level */
  l_cfg->log_level = loglvl;

  /* 0 disables syslog, everything else enables it */
  l_cfg->enable_syslog = (syslog ? 1 : 0);
  /* forcing syslog_level to be always <= app logging level */
  l_cfg->syslog_level = (syslvl>loglvl ? loglvl : syslvl );
  
  /* open file */
  l_cfg->fd = log_file_open(l_cfg->log_file);

  if (-1 == l_cfg->fd)
  {
    return LOG_ERROR_FILE_OPEN;
  }

  /* if syslog is enabled, open it */
  if (l_cfg->enable_syslog) openlog(l_cfg->program_name, LOG_CONS | LOG_PID, LOG_DAEMON);

  return LOG_STARTUP_OK;
}

/******************************************************************************/
void DEFAULT_CC
log_end()
{
  /* if log is closed, quit silently */
  if (0 == l_cfg)
  {
    return;
  }
  
  /* closing log file */
  log_message(LOG_LEVEL_ALWAYS,"shutting down log subsystem...");

  if (0 > l_cfg->fd)
  {
    /* if syslog is enabled, close it */
    if (l_cfg->enable_syslog) closelog();
  }

  /* closing logfile... */
  g_file_close(l_cfg->fd);
  
  /* if syslog is enabled, close it */
  if (l_cfg->enable_syslog) closelog();

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
  if (0 == g_strncasecmp(buf, "1", 1) ||
      0 == g_strncasecmp(buf, "error", 4))
  {
    return LOG_LEVEL_ERROR;
  } 
  else if (0 == g_strncasecmp(buf, "2", 1) ||
      0 == g_strncasecmp(buf, "warn", 4) ||
      0 == g_strncasecmp(buf, "warning", 3))
  {
    return LOG_LEVEL_WARNING;
  } 
  else if (0 == g_strncasecmp(buf, "3", 1) ||
      0 == g_strncasecmp(buf, "info", 4))
  {
    return LOG_LEVEL_INFO;
  } 
  /* else if (0 == g_strncasecmp(buf, "1", 1) ||
      0 == g_strncasecmp(buf, "true", 4) ||
      0 == g_strncasecmp(buf, "yes", 3))
  {
    return LOG_LEVEL_DEBUG;
  }*/
  
  return LOG_LEVEL_DEBUG;
}

