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

#ifndef LOG_H
#define LOG_H

#include <pthread.h>

#include "arch.h"
#include "defines.h"
#include "list.h"

/* Check the config_ac.h file is included so we know whether to enable the
 * development macros
 */
#ifndef CONFIG_AC_H
#   error config_ac.h not visible in log.h
#endif

/* logging buffer size */
#define LOG_BUFFER_SIZE      8192
#define LOGGER_NAME_SIZE     50

/* logging levels */
enum logLevels
{
    LOG_LEVEL_ALWAYS = 0,
    LOG_LEVEL_ERROR,     /* for describing non-recoverable error states in a request or method */
    LOG_LEVEL_WARNING,   /* for describing recoverable error states in a request or method */
    LOG_LEVEL_INFO,      /* for low verbosity and high level descriptions of normal operations */
    LOG_LEVEL_DEBUG,     /* for medium verbosity and low level descriptions of normal operations */
    LOG_LEVEL_TRACE,     /* for high verbosity and low level descriptions of normal operations (eg. method or wire tracing) */
    LOG_LEVEL_NEVER,
};

/* startup return values */
enum logReturns
{
    LOG_STARTUP_OK = 0,
    LOG_ERROR_MALLOC,
    LOG_ERROR_NULL_FILE,
    LOG_ERROR_FILE_OPEN,
    LOG_ERROR_NO_CFG,
    LOG_ERROR_FILE_NOT_OPEN,
    LOG_GENERAL_ERROR
};

#define SESMAN_CFG_LOGGING            "Logging"
#define SESMAN_CFG_LOGGING_LOGGER     "LoggingPerLogger"
#define SESMAN_CFG_LOG_FILE           "LogFile"
#define SESMAN_CFG_LOG_LEVEL          "LogLevel"
#define SESMAN_CFG_LOG_ENABLE_CONSOLE "EnableConsole"
#define SESMAN_CFG_LOG_CONSOLE_LEVEL  "ConsoleLevel"
#define SESMAN_CFG_LOG_ENABLE_SYSLOG  "EnableSyslog"
#define SESMAN_CFG_LOG_SYSLOG_LEVEL   "SyslogLevel"
#define SESMAN_CFG_LOG_ENABLE_PID     "EnableProcessId"

/* enable threading */
/*#define LOG_ENABLE_THREAD*/

#ifdef USE_DEVEL_LOGGING

#define LOG_PER_LOGGER_LEVEL

/**
 * @brief Logging macro for messages that are for an XRDP developer to
 * understand and debug XRDP code.
 *
 * Note: all log levels are relevant to help a developer understand XRDP at
 *      different levels of granularity.
 *
 * Note: the logging function calls are removed when USE_DEVEL_LOGGING is
 * NOT defined.
 *
 * Note: when the build is configured with --enable-devel-logging, then
 *      the log level can be configured per the source file name or method name
 *      (with the suffix "()") in the [LoggingPerLogger]
 *      section of the configuration file.
 *
 *      For example:
 *      ```
 *      [LoggingPerLogger]
 *      xrdp.c=DEBUG
 *      main()=WARNING
 *      ```
 *
 * @param lvl, the log level
 * @param msg, the log text as a printf format c-string
 * @param ... the arguments for the printf format c-string
 */
#define LOG_DEVEL(log_level, args...) \
    log_message_with_location(__func__, __FILE__, __LINE__, log_level, args)

/**
 * @brief Logging macro for messages that are for a system administrator to
 * configure and run XRDP on their machine.
 *
 * Note: the logging function calls contain additional code location info when
 *      USE_DEVEL_LOGGING is defined.
 *
 * @param lvl, the log level
 * @param msg, the log text as a printf format c-string
 * @param ... the arguments for the printf format c-string
 */
#define LOG(log_level, args...) \
    log_message_with_location(__func__, __FILE__, __LINE__, log_level, args)

/**
 * @brief Logging macro for logging the contents of a byte array using a hex
 * dump format.
 *
 * Note: the logging function calls are removed when USE_DEVEL_LOGGING is
 * NOT defined.
 *
 * @param log_level, the log level
 * @param message, a message prefix for the hex dump. Note: no printf like
 *          formatting is done to this message.
 * @param buffer, a pointer to the byte array to log as a hex dump
 * @param length, the length of the byte array to log
 */
#define LOG_DEVEL_HEXDUMP(log_level, message, buffer, length)  \
    log_hexdump_with_location(__func__, __FILE__, __LINE__, log_level, message, buffer, length)

/**
 * @brief Logging macro for logging the contents of a byte array using a hex
 * dump format.
 *
 * @param log_level, the log level
 * @param message, a message prefix for the hex dump. Note: no printf like
 *          formatting is done to this message.
 * @param buffer, a pointer to the byte array to log as a hex dump
 * @param length, the length of the byte array to log
 */
#define LOG_HEXDUMP(log_level, message, buffer, length)  \
    log_hexdump_with_location(__func__, __FILE__, __LINE__, log_level, message, buffer, length)

#define LOG_DEVEL_LEAKING_FDS(exe,min,max) log_devel_leaking_fds(exe,min,max)

#else
#define LOG(log_level, args...) log_message(log_level, args)
#define LOG_HEXDUMP(log_level, message, buffer, length)  \
    log_hexdump(log_level, message, buffer, length)

/* Since log_message() returns a value ensure that the elided versions of
 * LOG_DEVEL and LOG_DEVEL_HEXDUMP also "fake" returning the success value
 */
#define LOG_DEVEL(log_level, args...) UNUSED_VAR(LOG_STARTUP_OK)
#define LOG_DEVEL_HEXDUMP(log_level, message, buffer, length) UNUSED_VAR(LOG_STARTUP_OK)

#define LOG_DEVEL_LEAKING_FDS(exe,min,max)
#endif

/* Flags values for log_start() */

/**
 * Dump the log config on startup
 */
#define LOG_START_DUMP_CONFIG (1<<0)

/**
 * Restart the logging system.
 *
 * On a restart, existing files are not closed. This is because the
 * files may be shared by sub-processes, and the result will not be what the
 * user expects
 */
#define LOG_START_RESTART (1<<1)

#ifdef LOG_PER_LOGGER_LEVEL
enum log_logger_type
{
    LOG_TYPE_FILE = 0,
    LOG_TYPE_FUNCTION,
};

struct log_logger_level
{
    enum logLevels log_level;
    enum log_logger_type logger_type;
    char logger_name[LOGGER_NAME_SIZE + 1];
};
#endif

struct log_config
{
    const char *program_name; /* Pointer to static storage */
    char *log_file; /* Dynamically allocated */
    int fd;
    enum logLevels log_level;
    int enable_console;
    enum logLevels console_level;
    int enable_syslog;
    enum logLevels syslog_level;
#ifdef LOG_PER_LOGGER_LEVEL
    struct list *per_logger_level;
#endif
    int dump_on_start;
    int enable_pid;
#ifdef LOG_ENABLE_THREAD
    pthread_mutex_t log_lock;
    pthread_mutexattr_t log_lock_attr;
#endif
};

/* internal functions, only used in log.c if this ifdef is defined.*/
#ifdef LOGINTERNALSTUFF

/**
 *
 * @brief Starts the logging subsystem
 * @param l_cfg logging system configuration
 * @return
 *
 */
enum logReturns
internal_log_start(struct log_config *l_cfg);

/**
 *
 * @brief Shuts down the logging subsystem
 * @param l_cfg pointer to the logging subsystem to stop
 *
 */
enum logReturns
internal_log_end(struct log_config *l_cfg);

/**
 * Converts a log level to a string
 * @param lvl, the loglevel
 * @param str pointer where the string will be stored.
 */
void
internal_log_lvl2str(const enum logLevels lvl, char *str);

/**
 *
 * @brief Converts a string to a log level
 * @param s The string to convert
 * @return The corresponding level or LOG_LEVEL_DEBUG if error
 *
 */
enum logLevels
internal_log_text2level(const char *buf);

/**
 * A function that init our struct that holds all state and
 * also init its content.
 * @return  LOG_STARTUP_OK or LOG_ERROR_MALLOC
 */
struct log_config *
internalInitAndAllocStruct(void);

/**
 * Print the contents of the logging config to stdout.
 */
void
internal_log_config_dump(struct log_config *config);

/**
 * the log function that all files use to log an event.
 * @param lvl, the loglevel
 * @param override_destination_level, if true then the destination log level is not used
 * @param override_log_level, the loglevel instead of the destination log level if override_destination_level is true
 * @param msg, the logtext.
 * @param ap, the values for the message format arguments
 * @return
 */
enum logReturns
internal_log_message(const enum logLevels lvl,
                     const bool_t override_destination_level,
                     const enum logLevels override_log_level,
                     const char *msg,
                     va_list ap);

/**
 * @param log_level, the log level
 * @param override_destination_level, if true then the destination log level is ignored.
 * @param override_log_level, the log level to use instead of the destination log level
 *      if override_destination_level is true
 * @return true if at least one log destination will accept a message logged at the given level.
 */
bool_t
internal_log_is_enabled_for_level(const enum logLevels log_level,
                                  const bool_t override_destination_level,
                                  const enum logLevels override_log_level);

/**
 * @param function_name, the function name (typically the __func__ macro)
 * @param file_name, the file name (typically the __FILE__ macro)
 * @param[out] log_level_return, the log level to use instead of the destination log level
 * @return true if the logger location overrides the destination log levels
 */
bool_t
internal_log_location_overrides_level(const char *function_name,
                                      const char *file_name,
                                      enum logLevels *log_level_return);

/*End of internal functions*/
#endif

/**
 * This function initialize the log facilities according to the configuration
 * file, that is described by the in parameter.
 * @param iniFile
 * @param applicationName the name that is used in the log for the running
 *                        application
 * @param flags Flags to affect the operation of the call
 * @return LOG_STARTUP_OK on success
 */
enum logReturns
log_start(const char *iniFile, const char *applicationName,
          unsigned int flags);

/**
 * An alternative log_start where the caller gives the params directly.
 * @param config
 * @return
 *
 * @post to avoid memory leaks, the config argument must be free'ed using
 * `log_config_free()`
 */
enum logReturns
log_start_from_param(const struct log_config *src_log_config);

/**
 * Sets up a suitable log config for writing to the console only
 * (i.e. for a utility)
 *
 * The config can be customised by the caller before calling
 * log_start_from_param()
 *
 * @param Default log level
 * @param Log level name, or NULL. This can be used to provide an
 *        override to the default log level, by environment variable or
 *        argument.
 *
 * @return pointer to struct log_config.
 */
struct log_config *
log_config_init_for_console(enum logLevels lvl, const char *override_name);

/**
 * Read configuration from a file and store the values in the returned
 * log_config.
 * @param file
 * @param applicationName, the application name used in the log events.
 * @param section_prefix, prefix for the logging sections to parse
 * @return
 */
struct log_config *
log_config_init_from_config(const char *iniFilename,
                            const char *applicationName,
                            const char *section_prefix);

/**
 * Free the memory for the log_config struct.
 */
enum logReturns
log_config_free(struct log_config *config);

/**
 * Function that terminates all logging
 * @return
 */
enum logReturns
log_end(void);

/**
 * the log function that all files use to log an event.
 *
 * Please prefer to use the LOG and LOG_DEVEL macros instead of this function directly.
 *
 * @param lvl, the loglevel
 * @param msg, the logtext.
 * @param ...
 * @return
 */
enum logReturns
log_message(const enum logLevels lvl, const char *msg, ...) printflike(2, 3);

enum logReturns
log_hexdump(const enum logLevels log_level,
            const char *msg,
            const char *p,
            int len);

/**
 * the log function that all files use to log an event,
 * with the function name and file line.
 *
 * Please prefer to use the LOG and LOG_DEVEL macros instead of this function directly.
 *
 * @param function_name, the function name (typically the __func__ macro)
 * @param file_name, the file name (typically the __FILE__ macro)
 * @param line_number, the line number in the file (typically the __LINE__ macro)
 * @param lvl, the loglevel
 * @param msg, the logtext.
 * @param ...
 * @return
 */
enum logReturns
log_message_with_location(const char *function_name,
                          const char *file_name,
                          const int line_number,
                          const enum logLevels lvl,
                          const char *msg,
                          ...) printflike(5, 6);

enum logReturns
log_hexdump_with_location(const char *function_name,
                          const char *file_name,
                          const int line_number,
                          const enum logLevels log_level,
                          const char *msg,
                          const char *p,
                          int len);

/**
 * This function returns the configured file name for the logfile
 * @param replybuf the buffer where the reply is stored
 * @param bufsize how big is the reply buffer.
 * @return
 */
char *getLogFile(char *replybuf, int bufsize);

/**
 * Returns formatted datetime for log
 * @return
 */
char *getFormattedDateTime(char *replybuf, int bufsize);

#ifdef USE_DEVEL_LOGGING
/**
 * Log open file descriptors not cloexec before execing another program
 *
 * Used to ensure file descriptors aren't leaking when running
 * non-privileged executables
 *
 * Use the LOG_DEVEL_LEAKING_FDS() macro to invoke this function
 *
 * @param exe Executable we're about to launch
 * @param min Minimum FD to consider
 * @param max Maximum FD to consider + 1, or -1 for no upper FD
 */
void
log_devel_leaking_fds(const char *exe, int min, int max);
#endif

#endif
