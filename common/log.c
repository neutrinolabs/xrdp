/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2014
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include "list.h"
#include "file.h"
#include "os_calls.h"
#include "thread_calls.h"
#include "string_calls.h"

/* Add a define here so that the log.h will hold more information
 * when compiled from this C file.
 * When compiled normally the log.h file only contain the public parts
 * of the operators in this file. */
#define LOGINTERNALSTUFF
#include "log.h"

/* Here we store the current state and configuration of the log */
static struct log_config *g_staticLogConfig = NULL;

/* This file first start with all private functions.
   In the end of the file the public functions is defined */

/**
 *
 * @brief Opens log file
 * @param fname log file name
 * @return see open(2) return values
 *
 */
int
internal_log_file_open(const char *fname)
{
    int ret = -1;

    if (fname != NULL)
    {
        ret = open(fname, O_WRONLY | O_CREAT | O_APPEND | O_SYNC,
                   S_IRUSR | S_IWUSR);
    }

#ifdef FD_CLOEXEC
    if (ret != -1)
    {
        fcntl(ret, F_SETFD, FD_CLOEXEC);
    }
#endif

    return ret;
}

/**
 *
 * @brief Converts xrdp log level to syslog logging level
 * @param xrdp logging level
 * @return syslog equivalent logging level
 *
 */
int
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
        case LOG_LEVEL_TRACE:
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
void
internal_log_lvl2str(const enum logLevels lvl, char *str)
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
        case LOG_LEVEL_TRACE:
            snprintf(str, 9, "%s", "[TRACE] ");
            break;
        default:
            snprintf(str, 9, "%s", "PRG ERR!");
            g_writeln("Programming error - undefined log level!!!");
    }
}

/******************************************************************************/
enum logReturns
internal_log_start(struct log_config *l_cfg)
{
    enum logReturns ret = LOG_GENERAL_ERROR;

    if (0 == l_cfg)
    {
        ret = LOG_ERROR_MALLOC;
        return ret;
    }

    /* if progname is NULL, we return error */
    if (0 == l_cfg->program_name)
    {
        g_writeln("program_name not properly assigned");
        return ret;
    }

    if (l_cfg->dump_on_start)
    {
        internal_log_config_dump(l_cfg);
    }

    /* open file */
    if (l_cfg->log_file != NULL)
    {
        l_cfg->fd = internal_log_file_open(l_cfg->log_file);

        if (-1 == l_cfg->fd)
        {
            return LOG_ERROR_FILE_OPEN;
        }
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
enum logReturns
internal_log_end(struct log_config *l_cfg)
{
    enum logReturns ret = LOG_GENERAL_ERROR;

    /* if log is closed, quit silently */
    if (0 == l_cfg)
    {
        return ret;
    }

    if (-1 != l_cfg->fd)
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

    ret = LOG_STARTUP_OK;
    return ret;
}

/**
 * Converts a string representing th log level to a value
 * @param buf
 * @return
 */
enum logLevels
internal_log_text2level(const char *buf)
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
    else if (0 == g_strcasecmp(buf, "5") ||
             0 == g_strcasecmp(buf, "trace"))
    {
        return LOG_LEVEL_TRACE;
    }

    g_writeln("Your configured log level is corrupt - we use debug log level");
    return LOG_LEVEL_DEBUG;
}

/******************************************************************************/
struct log_config *
internal_config_read_logging(int file,
                             const char *applicationName,
                             const char *section_prefix)
{
    int i;
    char *buf;
    char *temp_buf;
    char section_name[512];
    struct log_config *lc;
    struct list *param_n;
    struct list *param_v;

    lc = internalInitAndAllocStruct();
    if (lc == NULL)
    {
        return NULL;
    }

    param_n = list_create();
    param_n->auto_free = 1;
    param_v = list_create();
    param_v->auto_free = 1;

    list_clear(param_v);
    list_clear(param_n);

    /* setting defaults */
    lc->program_name = applicationName;
    lc->log_file = 0;
    lc->fd = -1;
    lc->log_level = LOG_LEVEL_INFO;
    lc->enable_console = 0;
    lc->console_level = LOG_LEVEL_INFO;
    lc->enable_syslog = 0;
    lc->syslog_level = LOG_LEVEL_INFO;
    lc->dump_on_start = 0;
    lc->enable_pid = 0;

    g_snprintf(section_name, 511, "%s%s", section_prefix, SESMAN_CFG_LOGGING);
    file_read_section(file, section_name, param_n, param_v);

    for (i = 0; i < param_n->count; i++)
    {
        buf = (char *)list_get_item(param_n, i);

        if (0 == g_strcasecmp(buf, SESMAN_CFG_LOG_FILE))
        {
            lc->log_file = g_strdup((char *)list_get_item(param_v, i));

            if (lc->log_file != NULL)
            {
                if (lc->log_file[0] != '/')
                {
                    temp_buf = (char *)g_malloc(512, 0);
                    g_snprintf(temp_buf, 511, "%s/%s", XRDP_LOG_PATH, lc->log_file);
                    g_free(lc->log_file);
                    lc->log_file = temp_buf;
                }
            }
        }

        if (0 == g_strcasecmp(buf, SESMAN_CFG_LOG_LEVEL))
        {
            lc->log_level = internal_log_text2level((char *)list_get_item(param_v, i));
        }

        if (0 == g_strcasecmp(buf, SESMAN_CFG_LOG_ENABLE_SYSLOG))
        {
            lc->enable_syslog = g_text2bool((char *)list_get_item(param_v, i));
        }

        if (0 == g_strcasecmp(buf, SESMAN_CFG_LOG_SYSLOG_LEVEL))
        {
            lc->syslog_level = internal_log_text2level((char *)list_get_item(param_v, i));
        }

        if (0 == g_strcasecmp(buf, SESMAN_CFG_LOG_ENABLE_CONSOLE))
        {
            lc->enable_console = g_text2bool((char *)list_get_item(param_v, i));
        }

        if (0 == g_strcasecmp(buf, SESMAN_CFG_LOG_CONSOLE_LEVEL))
        {
            lc->console_level = internal_log_text2level((char *)list_get_item(param_v, i));
        }

        if (0 == g_strcasecmp(buf, SESMAN_CFG_LOG_ENABLE_PID))
        {
            lc->enable_pid = g_text2bool((char *)list_get_item(param_v, i));
        }
    }

    if (0 == lc->log_file)
    {
        lc->log_file = g_strdup("./sesman.log");
    }

    /* try to create path if not exist */
    g_create_path(lc->log_file);

#ifdef LOG_PER_LOGGER_LEVEL
    int len;
    struct log_logger_level *logger;

    list_clear(param_v);
    list_clear(param_n);
    g_snprintf(section_name, 511, "%s%s", section_prefix, SESMAN_CFG_LOGGING_LOGGER);
    file_read_section(file, section_name, param_n, param_v);
    for (i = 0; i < param_n->count; i++)
    {
        logger = (struct log_logger_level *)g_malloc(sizeof(struct log_logger_level), 1);
        list_add_item(lc->per_logger_level, (tbus) logger);
        logger->log_level = internal_log_text2level((char *)list_get_item(param_v, i));

        g_strncpy(logger->logger_name, (char *)list_get_item(param_n, i), LOGGER_NAME_SIZE);
        logger->logger_name[LOGGER_NAME_SIZE] = '\0';
        len = g_strlen(logger->logger_name);
        if (len >= 2
                && logger->logger_name[len - 2] == '('
                && logger->logger_name[len - 1] == ')' )
        {
            logger->logger_type = LOG_TYPE_FUNCTION;
            logger->logger_name[len - 2] = '\0';
        }
        else
        {
            logger->logger_type = LOG_TYPE_FILE;
        }
    }
#endif

    list_delete(param_v);
    list_delete(param_n);
    return lc;
}

void
internal_log_config_dump(struct log_config *config)
{
    char str_level[20];
#ifdef LOG_PER_LOGGER_LEVEL
    struct log_logger_level *logger;
    int i;
#endif

    g_printf("logging configuration:\r\n");
    if (config->log_file)
    {
        internal_log_lvl2str(config->log_level, str_level);
        g_printf("\tLogFile:       %s\r\n", config->log_file);
        g_printf("\tLogLevel:      %s\r\n", str_level);
    }
    else
    {
        g_printf("\tLogFile:       %s\r\n", "<disabled>");
    }

    if (config->enable_console)
    {
        internal_log_lvl2str(config->console_level, str_level);
    }
    else
    {
        g_strcpy(str_level, "<disabled>");
    }
    g_printf("\tConsoleLevel:  %s\r\n", str_level);

    if (config->enable_syslog)
    {
        internal_log_lvl2str(config->syslog_level, str_level);
    }
    else
    {
        g_strcpy(str_level, "<disabled>");
    }
    g_printf("\tSyslogLevel:   %s\r\n", str_level);

#ifdef LOG_PER_LOGGER_LEVEL
    g_printf("per logger configuration:\r\n");
    for (i = 0; i < config->per_logger_level->count; i++)
    {
        logger = (struct log_logger_level *)list_get_item(config->per_logger_level, i);
        internal_log_lvl2str(logger->log_level, str_level);
        g_printf("\t%-*s: %s\r\n", LOGGER_NAME_SIZE, logger->logger_name, str_level);
    }
    if (config->per_logger_level->count == 0)
    {
        g_printf("\tNone\r\n");
    }
#endif
}

struct log_config *
internalInitAndAllocStruct(void)
{
    struct log_config *ret = g_new0(struct log_config, 1);

    if (ret != NULL)
    {
        ret->fd = -1;
        ret->enable_syslog = 0;
        ret->per_logger_level = list_create();
        if (ret->per_logger_level != NULL)
        {
            ret->per_logger_level->auto_free = 1;
        }
        else
        {
            g_writeln("could not allocate memory for log struct");
            g_free(ret);
            ret = NULL;
        }
    }
    else
    {
        g_writeln("could not allocate memory for log struct");
    }

    return ret;
}

void
internal_log_config_copy(struct log_config *dest, const struct log_config *src)
{
    int i;

    dest->enable_syslog = src->enable_syslog;
    dest->fd = src->fd;
    dest->log_file = g_strdup(src->log_file);
    dest->log_level = src->log_level;
    dest->log_lock = src->log_lock;
    dest->log_lock_attr = src->log_lock_attr;
    dest->program_name = src->program_name;
    dest->enable_syslog = src->enable_syslog;
    dest->syslog_level = src->syslog_level;
    dest->enable_console = src->enable_console;
    dest->console_level = src->console_level;
    dest->enable_pid = src->enable_pid;
    dest->dump_on_start = src->dump_on_start;
    for (i = 0; i < src->per_logger_level->count; ++i)
    {
        struct log_logger_level *dst_logger =
            (struct log_logger_level *)g_malloc(sizeof(struct log_logger_level), 1);

        g_memcpy(dst_logger,
                 (struct log_logger_level *) list_get_item(src->per_logger_level, i),
                 sizeof(struct log_logger_level));

        list_add_item(dest->per_logger_level, (tbus) dst_logger);
    }
}

bool_t
internal_log_is_enabled_for_level(const enum logLevels log_level,
                                  const bool_t override_destination_level,
                                  const enum logLevels override_log_level)
{
    /* Is log initialized? */
    if (g_staticLogConfig == NULL)
    {
        return 0;
    }
    else if (g_staticLogConfig->fd < 0
             && !g_staticLogConfig->enable_syslog
             && !g_staticLogConfig->enable_console)
    {
        /* all logging outputs are disabled */
        return 0;
    }
    else if (override_destination_level)
    {
        /* Override is enabled - should the message should be logged? */
        return log_level <= override_log_level;
    }
    /* Override is disabled - Is there at least one log destination
     * which will accept the message based on the log level? */
    else if (g_staticLogConfig->fd >= 0
             && log_level <= g_staticLogConfig->log_level)
    {
        return 1;
    }
    else if (g_staticLogConfig->enable_syslog
             && log_level <= g_staticLogConfig->syslog_level)
    {
        return 1;
    }
    else if (g_staticLogConfig->enable_console
             && log_level <= g_staticLogConfig->console_level)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

bool_t
internal_log_location_overrides_level(const char *function_name,
                                      const char *file_name,
                                      enum logLevels *log_level_return)
{
    struct log_logger_level *logger = NULL;
    int i;

    if (g_staticLogConfig == NULL)
    {
        return 0;
    }
    for (i = 0; i < g_staticLogConfig->per_logger_level->count; i++)
    {
        logger = (struct log_logger_level *)list_get_item(g_staticLogConfig->per_logger_level, i);

        if ((logger->logger_type == LOG_TYPE_FILE
                && 0 == g_strncmp(logger->logger_name, file_name, LOGGER_NAME_SIZE))
                || (logger->logger_type == LOG_TYPE_FUNCTION
                    && 0 == g_strncmp(logger->logger_name, function_name, LOGGER_NAME_SIZE)))
        {
            *log_level_return = logger->log_level;
            return 1;
        }
    }

    return 0;
}

/*
 * Here below the public functions
 */

struct log_config *
log_config_init_for_console(enum logLevels lvl, const char *override_name)
{
    struct log_config *config = internalInitAndAllocStruct();

    if (config != NULL)
    {
        config->program_name = "<null>";
        config->enable_console = 1;
        if (override_name != NULL && override_name[0] != '\0')
        {
            config->console_level = internal_log_text2level(override_name);
        }
        else
        {
            config->console_level = lvl;
        }
    }
    return config;
}


struct log_config *
log_config_init_from_config(const char *iniFilename,
                            const char *applicationName,
                            const char *section_prefix)
{
    int fd;
    struct log_config *config;

    if (applicationName == NULL)
    {
        g_writeln("Programming error your application name cannot be null");
        return NULL;
    }

    if (iniFilename == NULL)
    {
        g_writeln("The inifile is null to log_config_init_from_config!");
        return NULL;
    }

    fd = g_file_open_ex(iniFilename, 1, 0, 0, 0);

    if (-1 == fd)
    {
        g_writeln("We could not open the configuration file to read log parameters");
        return NULL;
    }

    /* read logging config */
    config = internal_config_read_logging(fd, applicationName, section_prefix);

    /* cleanup */
    g_file_close(fd);
    return config;
}

enum logReturns
log_config_free(struct log_config *config)
{
    if (config != NULL)
    {
        if (config->per_logger_level != NULL)
        {
            list_delete(config->per_logger_level);
            config->per_logger_level = NULL;
        }
        g_free(config);
    }

    return LOG_STARTUP_OK;
}

enum logReturns
log_start_from_param(const struct log_config *src_log_config)
{
    enum logReturns ret = LOG_GENERAL_ERROR;

    if (g_staticLogConfig != NULL)
    {
        log_message(LOG_LEVEL_ALWAYS, "Log already initialized");
        return ret;
    }

    if (src_log_config == NULL)
    {
        g_writeln("src_log_config to log_start_from_param is NULL");
        return ret;
    }
    else
    {
        g_staticLogConfig = internalInitAndAllocStruct();
        if (g_staticLogConfig == NULL)
        {
            g_writeln("internalInitAndAllocStruct failed");
            return LOG_ERROR_MALLOC;
        }
        internal_log_config_copy(g_staticLogConfig, src_log_config);

        ret = internal_log_start(g_staticLogConfig);
        if (ret != LOG_STARTUP_OK)
        {
            g_writeln("Could not start log");

            log_config_free(g_staticLogConfig);
            g_staticLogConfig = NULL;
        }
    }

    return ret;
}

/**
 * This function initialize the log facilities according to the configuration
 * file, that is described by the in parameter.
 * @param iniFile
 * @param applicationName, the name that is used in the log for the running application
 * @return 0 on success
 */
enum logReturns
log_start(const char *iniFile, const char *applicationName,
          bool_t dump_on_start)
{
    enum logReturns ret = LOG_GENERAL_ERROR;
    struct log_config *config;

    config = log_config_init_from_config(iniFile, applicationName, "");

    if (config != NULL)
    {
        config->dump_on_start = dump_on_start;
        ret = log_start_from_param(config);
        log_config_free(config);

        if (ret != LOG_STARTUP_OK)
        {
            g_writeln("Could not start log");
        }
    }
    else
    {
        g_writeln("Error reading configuration for log based on config: %s",
                  iniFile);
    }

    return ret;
}

/**
 * Function that terminates all logging
 * @return
 */
enum logReturns
log_end(void)
{
    enum logReturns ret = LOG_GENERAL_ERROR;
    ret = internal_log_end(g_staticLogConfig);
    log_config_free(g_staticLogConfig);
    g_staticLogConfig = NULL;

    return ret;
}

/*****************************************************************************/
/* log a hex dump */
enum logReturns
log_hexdump(const enum logLevels log_level,
            const char *message,
            const char *src,
            int len)
{
    return log_hexdump_with_location("", "", 0, log_level, message, src, len);
}

/*****************************************************************************/
/* log a hex dump */
enum logReturns
log_hexdump_with_location(const char *function_name,
                          const char *file_name,
                          const int line_number,
                          const enum logLevels log_level,
                          const char *message,
                          const char *src,
                          int len)
{
    char *dump_buffer;
    enum logReturns rv = LOG_STARTUP_OK;
    enum logLevels override_log_level;
    bool_t override_destination_level = 0;

    override_destination_level = internal_log_location_overrides_level(
        function_name,
        file_name,
        &override_log_level);
    if (!internal_log_is_enabled_for_level(log_level, override_destination_level, override_log_level))
    {
        return LOG_STARTUP_OK;
    }

    /* Start the dump on a new line so that the first line of the dump is
       aligned to the first column instead of to after the log message
       preamble (eg. time, log level, ...)
    */
#ifdef _WIN32
#define HEX_DUMP_HEADER ("Hex Dump:\r\n")
#else
#ifdef _MACOS
#define HEX_DUMP_HEADER ("Hex Dump:\r")
#else
#define HEX_DUMP_HEADER ("Hex Dump:\n")
#endif
#endif

    dump_buffer = g_bytes_to_hexdump(src, len);

    if (dump_buffer != NULL)
    {
        if (g_strlen(file_name) > 0)
        {
            rv = log_message_with_location(function_name, file_name, line_number,
                                           log_level, "%s %s%s",
                                           message, HEX_DUMP_HEADER, dump_buffer);
        }
        else
        {
            rv = log_message(log_level, "%s %s%s",
                             message, HEX_DUMP_HEADER, dump_buffer);
        }
        g_free(dump_buffer);
    }
    return rv;
}

enum logReturns
log_message_with_location(const char *function_name,
                          const char *file_name,
                          const int line_number,
                          const enum logLevels level,
                          const char *msg,
                          ...)
{
    va_list ap;
    enum logReturns rv;
    char buff[LOG_BUFFER_SIZE];
    enum logLevels override_log_level = LOG_LEVEL_NEVER;
    bool_t override_destination_level = 0;

    if (g_staticLogConfig == NULL)
    {
        g_writeln("The log reference is NULL - log not initialized properly "
                  "when called from [%s(%s:%d)]",
                  (function_name != NULL ? function_name : "unknown_function"),
                  (file_name != NULL ? file_name : "unknown_file"),
                  line_number);
        return LOG_ERROR_NO_CFG;
    }

    override_destination_level = internal_log_location_overrides_level(
        function_name,
        file_name,
        &override_log_level);
    if (!internal_log_is_enabled_for_level(level, override_destination_level, override_log_level))
    {
        return LOG_STARTUP_OK;
    }

    g_snprintf(buff, LOG_BUFFER_SIZE, "[%s(%s:%d)] %s",
               function_name, file_name, line_number, msg);

    va_start(ap, msg);
    rv = internal_log_message(level, override_destination_level, override_log_level, buff, ap);
    va_end(ap);
    return rv;
}

enum logReturns
log_message(const enum logLevels lvl, const char *msg, ...)
{
    va_list ap;
    enum logReturns rv;

    va_start(ap, msg);
    rv = internal_log_message(lvl, 0, LOG_LEVEL_NEVER, msg, ap);
    va_end(ap);
    return rv;
}

enum logReturns
internal_log_message(const enum logLevels lvl,
                     const bool_t override_destination_level,
                     const enum logLevels override_log_level,
                     const char *msg,
                     va_list ap)
{
    char buff[LOG_BUFFER_SIZE + 31]; /* 19 (datetime) 4 (space+cr+lf+\0) */
    int len = 0;
    enum logReturns rv = LOG_STARTUP_OK;
    int writereply = 0;
    time_t now_t;
    struct tm *now;

    if (g_staticLogConfig == NULL)
    {
        g_writeln("The log reference is NULL - log not initialized properly");
        return LOG_ERROR_NO_CFG;
    }

    if (0 > g_staticLogConfig->fd
            && g_staticLogConfig->enable_syslog == 0
            && g_staticLogConfig->enable_console == 0)
    {
        return LOG_ERROR_FILE_NOT_OPEN;
    }

    if (!internal_log_is_enabled_for_level(lvl, override_destination_level, override_log_level))
    {
        return LOG_STARTUP_OK;
    }

    now_t = time(&now_t);
    now = localtime(&now_t);

    strftime(buff, 21, "[%Y%m%d-%H:%M:%S] ", now);

    internal_log_lvl2str(lvl, buff + 20);

    if (g_staticLogConfig->enable_pid)
    {
        g_snprintf(buff + 28, LOG_BUFFER_SIZE, "[pid:%d tid:%lld] ",
                   g_getpid(), (long long) tc_get_threadid());
        len = g_strlen(buff + 28);
    }
    len += vsnprintf(buff + 28 + len, LOG_BUFFER_SIZE - len, msg, ap);

    /* checking for truncated messages */
    if (len > LOG_BUFFER_SIZE)
    {
        log_message(LOG_LEVEL_WARNING, "next message will be truncated");
        len = LOG_BUFFER_SIZE;
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

    if (g_staticLogConfig->enable_syslog
            && ((override_destination_level && lvl <= override_log_level)
                || (!override_destination_level && lvl <= g_staticLogConfig->syslog_level)))
    {
        /* log to syslog*/
        /* %s fix compiler warning 'not a string literal' */
        syslog(internal_log_xrdp2syslog(lvl), "%s", buff + 20);
    }

    if (g_staticLogConfig->enable_console
            && ((override_destination_level && lvl <= override_log_level)
                || (!override_destination_level && lvl <= g_staticLogConfig->console_level)))
    {
        /* log to console */
        g_printf("%s", buff);
    }

    if ((override_destination_level && lvl <= override_log_level)
            || (!override_destination_level && lvl <= g_staticLogConfig->log_level))
    {
        /* log to application logfile */
        if (g_staticLogConfig->fd >= 0)
        {
#ifdef LOG_ENABLE_THREAD
            pthread_mutex_lock(&(g_staticLogConfig->log_lock));
#endif

            writereply = g_file_write(g_staticLogConfig->fd, buff, g_strlen(buff));

            if (writereply <= 0)
            {
                rv = LOG_ERROR_NULL_FILE;
            }

#ifdef LOG_ENABLE_THREAD
            pthread_mutex_unlock(&(g_staticLogConfig->log_lock));
#endif
        }
    }

    return rv;
}

/**
 * Return the configured log file name
 * @return
 */
char *
getLogFile(char *replybuf, int bufsize)
{
    if (g_staticLogConfig)
    {
        if (g_staticLogConfig->log_file)
        {
            g_strncpy(replybuf, g_staticLogConfig->log_file, bufsize);
        }
        else
        {
            g_sprintf(replybuf, "The log_file name is NULL");
        }
    }
    else
    {
        g_snprintf(replybuf, bufsize, "The log is not properly started");
    }

    return replybuf;
}
