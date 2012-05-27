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
   Copyright (C) Jay Sorg 2005-2010
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include "list.h"
#include "os_calls.h"

/* Add a define here so that the log.h will hold more information 
 * when compiled from this C file.
 * When compiled normally the log.h file only contain the public parts 
 * of the operators in this file. */
#define LOGINTERNALSTUFF
#include "log.h"

/* Here we store the current state and configuration of the log */
static struct log_config* staticLogConfig = NULL; 

/*This file first start with all private functions.
 In the end of the file the public functions is defined*/

/**
 *
 * @brief Opens log file
 * @param fname log file name
 * @return see open(2) return values
 * 
 */
int DEFAULT_CC
internal_log_file_open(const char* fname)
{
  int ret = -1 ;
  if(fname!=NULL)
  {
    ret = open(fname, O_WRONLY | O_CREAT | O_APPEND | O_SYNC, S_IRUSR |S_IWUSR);
  }
  return ret ;
}

/**
 *
 * @brief Converts xrdp log level to syslog logging level
 * @param xrdp logging level
 * @return syslog equivalent logging level
 *
 */
int DEFAULT_CC
internal_log_xrdp2syslog(const enum logLevels lvl)
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
    case LOG_LEVEL_DEBUG:
      return LOG_DEBUG;  
    default:
      g_writeln("Undefined log level - programming error");
      return LOG_DEBUG;
  }
}

/**
 * @brief Converts xrdp log levels to textual logging levels
 * @param lvl logging level
 * @param str pointer to a string, must be allocated before
 * @return The log string in str pointer.
 *
 */
void DEFAULT_CC
internal_log_lvl2str(const enum logLevels lvl, char* str)
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
    case LOG_LEVEL_DEBUG:   
      snprintf(str, 9, "%s", "[DEBUG] ");
      break;
    default:
      snprintf(str, 9, "%s", "PRG ERR!");  
      g_writeln("Programming error - undefined log level!!!");    
  }
}

/******************************************************************************/

/******************************************************************************/
enum logReturns DEFAULT_CC
internal_log_start(struct log_config* l_cfg)
{
  enum logReturns ret = LOG_GENERAL_ERROR;
  if (0 == l_cfg)
  {
    ret = LOG_ERROR_MALLOC;
    return ret;
  }

  /* if logfile is NULL, we return error */
  if (0 == l_cfg->log_file)
  {
    g_writeln("log_file not properly assigned");
    return ret ;    
  }

  /* if progname is NULL, we ureturn error */
  if (0 == l_cfg->program_name)
  {
    g_writeln("program_name not properly assigned");
    return ret ;   
  }

  /* open file */
  l_cfg->fd = internal_log_file_open(l_cfg->log_file);

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
  pthread_mutexattr_init(&(l_cfg->log_lock_attr));
  pthread_mutex_init(&(l_cfg->log_lock), &(l_cfg->log_lock_attr));
#endif

  return LOG_STARTUP_OK;
}

/******************************************************************************/
enum logReturns DEFAULT_CC
internal_log_end(struct log_config* l_cfg)
{
  enum logReturns ret = LOG_GENERAL_ERROR ;  
  /* if log is closed, quit silently */
  if (0 == l_cfg)
  {
    return ret;
  }

  /* closing log file */
  log_message(LOG_LEVEL_ALWAYS, "shutting down log subsystem...");

  if (0 > l_cfg->fd)
  {
     /* closing logfile... */
     g_file_close(l_cfg->fd);
  } 

  /* if syslog is enabled, close it */
  if (l_cfg->enable_syslog)
  {
    closelog();
  }

  /* freeing allocated memory */
  if (0 != l_cfg->log_file)
  {
    g_free(l_cfg->log_file);
    l_cfg->log_file = 0;
  }
  if (0 != l_cfg->program_name)
  {
    g_free(l_cfg->program_name);
    l_cfg->program_name = 0;
  }
  ret = LOG_STARTUP_OK ;
  return ret ;
}

/**
 * Converts a string representing th log level to a value
 * @param buf
 * @return 
 */
enum logLevels DEFAULT_CC
internal_log_text2level(char* buf)
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
  else if (0 == g_strcasecmp(buf, "4") ||
           0 == g_strcasecmp(buf, "debug"))
  {
    return LOG_LEVEL_DEBUG;
  }
  g_writeln("Your configured log level is corrupt - we use debug log level");
  return LOG_LEVEL_DEBUG;
}

enum logReturns DEFAULT_CC 
internalReadConfiguration(const char *inFilename,const char *applicationName)
{
  int fd;
  enum logReturns ret = LOG_GENERAL_ERROR ;
  struct list* sec;
  struct list* param_n;
  struct list* param_v; 
  
  if(inFilename==NULL)
  {
    g_writeln("The inifile is null to readConfiguration!");
    return ret ;
  }
  fd = g_file_open(inFilename);
  if (-1 == fd)
  {
    ret = LOG_ERROR_NO_CFG ;  
    g_writeln("We could not open the configuration file to read log parameters");
    return ret ;   
  }
  // we initialize the memory for the configuration and set all content to zero. 
  ret = internalInitAndAllocStruct();  
  if(ret!=LOG_STARTUP_OK){
      return ret ;
  }

  sec = list_create();
  sec->auto_free = 1;
  file_read_sections(fd, sec);
  param_n = list_create();
  param_n->auto_free = 1;
  param_v = list_create();
  param_v->auto_free = 1;
 
  /* read logging config */
  ret = internal_config_read_logging(fd, staticLogConfig, param_n, 
    param_v, applicationName); 
  if(ret!=LOG_STARTUP_OK)
  {
    return ret ;
  }
  /* cleanup */
  list_delete(sec);
  list_delete(param_v);
  list_delete(param_n);
  g_file_close(fd);  
  return ret ;
}


/******************************************************************************/
enum logReturns DEFAULT_CC
internal_config_read_logging(int file, struct log_config* lc, struct list* param_n,
                    struct list* param_v,const char *applicationName)
{
  int i;
  char* buf;

  list_clear(param_v);
  list_clear(param_n);

  /* setting defaults */
  lc->program_name = g_strdup(applicationName);
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
      lc->log_level = internal_log_text2level((char*)list_get_item(param_v, i));
    }
    if (0 == g_strcasecmp(buf, SESMAN_CFG_LOG_ENABLE_SYSLOG))
    {
      lc->enable_syslog = text2bool((char*)list_get_item(param_v, i));
    }
    if (0 == g_strcasecmp(buf, SESMAN_CFG_LOG_SYSLOG_LEVEL))
    {
      lc->syslog_level = internal_log_text2level((char*)list_get_item(param_v, i));
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
  return LOG_STARTUP_OK;
}

enum logReturns DEFAULT_CC 
internalInitAndAllocStruct()
{
    enum logReturns ret = LOG_GENERAL_ERROR ;
    staticLogConfig = g_malloc(sizeof(struct log_config),1);
    if(staticLogConfig!=NULL)
    {
      staticLogConfig->fd = -1;
      staticLogConfig->enable_syslog = 0 ;  
      ret = LOG_STARTUP_OK ;
    }
    else
    {
      g_writeln("could not allocate memory for log struct") ;
      ret = LOG_ERROR_MALLOC ;
    }
    return ret ;    
}

/*
 * Here below the public functions 
 */


/**
 *
 * @brief Reads sesman configuration
 * @param s translates the strings "1", "true" and "yes" in 1 (true) and other strings in 0
 * @return 0 on false, 1 on 1,true, yes
 *
 */
int APP_CC
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


enum logReturns DEFAULT_CC 
log_start_from_param(const struct log_config* iniParams)
{
    enum logReturns ret = LOG_GENERAL_ERROR ;
    if(staticLogConfig!=NULL)
    {
      log_message(LOG_LEVEL_ALWAYS,"Log already initialized");
      return ret ;
    }
    if(iniParams==NULL)
    {
      g_writeln("inparam to log_start_from_param is NULL");
      return ret ;        
    }
    else
    {
      /*Copy the struct information*/
      ret = internalInitAndAllocStruct();      
      if(ret!=LOG_STARTUP_OK)
      {
          return ret ;
      }
      staticLogConfig->enable_syslog = iniParams->enable_syslog;
      staticLogConfig->fd = iniParams->fd;
      staticLogConfig->log_file = g_strdup(iniParams->log_file);
      staticLogConfig->log_level = iniParams->log_level ;
      staticLogConfig->log_lock = iniParams->log_lock ;
      staticLogConfig->log_lock_attr = iniParams->log_lock_attr ;
      staticLogConfig->program_name = g_strdup(iniParams->program_name);
      staticLogConfig->syslog_level = iniParams->syslog_level;
      ret = internal_log_start(staticLogConfig);     
      if(ret!=LOG_STARTUP_OK)
      {
        g_writeln("Could not start log");
        if(staticLogConfig!=NULL)
        {
          g_free(staticLogConfig);
          staticLogConfig = NULL ;
        }      
      }    
    }
    return ret ;          
}


/**
 * This function initialize the log facilities according to the configuration 
 * file, that is described by the in parameter.
 * @param iniFile
 * @param applicationName, the name that is used in the log for the running application
 * @return 0 on success
 */
enum logReturns DEFAULT_CC
log_start(const char *iniFile, const char *applicationName)
{
    enum logReturns ret = LOG_GENERAL_ERROR ;
    if(applicationName==NULL)
    {
      g_writeln("Programming error your application name cannot be null");
      return ret ;
    }
    ret = internalReadConfiguration(iniFile, applicationName) ;
    if(ret==LOG_STARTUP_OK)
    {
      ret = internal_log_start(staticLogConfig);     
      if(ret!=LOG_STARTUP_OK)
      {
        g_writeln("Could not start log");
        if(staticLogConfig!=NULL){
          g_free(staticLogConfig);
          staticLogConfig = NULL ;
        }
      }
    }
    else
    {
      g_writeln("Error reading configuration for log based on config: %s",iniFile);
    }
    return ret ;          
}

/**
 * Function that terminates all logging
 * @return 
 */
enum logReturns DEFAULT_CC
log_end()
{
    enum logReturns ret = LOG_GENERAL_ERROR;
    ret = internal_log_end(staticLogConfig); 
    if(staticLogConfig!=NULL)
    {
      g_free(staticLogConfig);
      staticLogConfig = NULL ;
    }    
    return ret ;
}

enum logReturns DEFAULT_CC
log_message( const enum logLevels lvl, 
const char* msg, ...)
{
  char buff[LOG_BUFFER_SIZE + 31]; /* 19 (datetime) 4 (space+cr+lf+\0) */
  va_list ap;
  int len = 0;
  enum logReturns rv = LOG_STARTUP_OK;
  int writereply = 0 ;
  time_t now_t;
  struct tm* now;
  enum logReturns ret = LOG_GENERAL_ERROR;  
  if(staticLogConfig==NULL)
  {         
    g_writeln("The log reference is NULL - log not initialized properly");   
    return LOG_ERROR_NO_CFG;
  }
  if (0 > staticLogConfig->fd && staticLogConfig->enable_syslog==0)
  {
    return LOG_ERROR_FILE_NOT_OPEN;
  }

  now_t = time(&now_t);
  now = localtime(&now_t);

  snprintf(buff, 21, "[%.4d%.2d%.2d-%.2d:%.2d:%.2d] ", (now->tm_year) + 1900,
           (now->tm_mon) + 1, now->tm_mday, now->tm_hour, now->tm_min,
           now->tm_sec);

  internal_log_lvl2str(lvl, buff + 20);

  va_start(ap, msg);
  len = vsnprintf(buff + 28, LOG_BUFFER_SIZE, msg, ap);
  va_end(ap);

  /* checking for truncated messages */ 
  if (len > LOG_BUFFER_SIZE)
  {
    log_message(LOG_LEVEL_WARNING,"next message will be truncated");
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

  if (staticLogConfig->enable_syslog  && (lvl <= staticLogConfig->syslog_level))
  {
    /* log to syslog*/
    /* %s fix compiler warning 'not a string literal' */      
    syslog(internal_log_xrdp2syslog(lvl), "(%d)(%d)%s",g_getpid(),g_gettid(),(char *)(buff + 20));
  }

  if (lvl <= staticLogConfig->log_level)
  {
    /* log to console */
    g_printf((char*)buff);

    /* log to application logfile */
#ifdef LOG_ENABLE_THREAD
    pthread_mutex_lock(&(staticLogConfig->log_lock));
#endif
    if(staticLogConfig->fd>0)
    {
      writereply = g_file_write(staticLogConfig->fd, (char*)buff, g_strlen((char*)buff));
      if(writereply <= 0)
      {
          rv = LOG_ERROR_NULL_FILE;
      }
    }
#ifdef LOG_ENABLE_THREAD
    pthread_mutex_unlock(&(staticLogConfig->log_lock));
#endif
  }
  return rv;
}


/**
 * Return the configured log file name
 * @return 
 */
char *getLogFile(char *replybuf, int bufsize)
{
    if(staticLogConfig )
    {
      if(staticLogConfig->log_file)
      {
        g_strncpy(replybuf, staticLogConfig->log_file,bufsize);
      }
      else
      {
        g_sprintf(replybuf,"The log_file name is NULL");
      }
    }
    else
    {
      g_snprintf(replybuf,bufsize,"The log is not properly started");          
    }
    return replybuf ;
}


