/**
 * xrdp: A Remote Desktop Protocol server.
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
 *
 * This file implements the interface in chansrv_config.h
 */
#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "arch.h"

#include "list.h"
#include "log.h"
#include "file.h"
#include "os_calls.h"

#include "chansrv_config.h"
#include "string_calls.h"

/* Default settings */
#define DEFAULT_USE_UNIX_SOCKET             0
#define DEFAULT_RESTRICT_OUTBOUND_CLIPBOARD 0
#define DEFAULT_ENABLE_FUSE_MOUNT           1
#define DEFAULT_FUSE_MOUNT_NAME             "xrdp-client"
#define DEFAULT_FILE_UMASK                  077

/**
 * Type used for passing a logging function about
 */
typedef
printflike(2, 3)
enum logReturns (*log_func_t)(const enum logLevels lvl,
                              const char *msg, ...);

/***************************************************************************//**
 * @brief Error logging function to use to log to stdout
 *
 * Has the same signature as the log_message() function
 */
static enum logReturns
log_to_stdout(const enum logLevels lvl, const char *msg, ...)
{
    char buff[256];
    va_list ap;

    va_start(ap, msg);
    vsnprintf(buff, sizeof(buff), msg, ap);
    va_end(ap);
    g_writeln("%s", buff);

    return LOG_STARTUP_OK;
}

/***************************************************************************//**
 * Reads the config values we need from the [Globals] section
 *
 * @param logmsg Function to use to log messages
 * @param names List of definitions in the section
 * @params values List of corresponding values for the names
 * @params cfg Pointer to structure we're filling in
 *
 * @return 0 for success
 */
static int
read_config_globals(log_func_t logmsg,
                    struct list *names, struct list *values,
                    struct config_chansrv *cfg)
{
    int error = 0;
    int index;

    for (index = 0; index < names->count; ++index)
    {
        const char *name = (const char *)list_get_item(names, index);
        const char *value = (const char *)list_get_item(values, index);

        if (g_strcasecmp(name, "ListenAddress") == 0)
        {
            if (g_strcasecmp(value, "127.0.0.1") == 0)
            {
                cfg->use_unix_socket = 1;
            }
        }
    }

    return error;
}

/***************************************************************************//**
 * Reads the config values we need from the [Security] section
 *
 * @param logmsg Function to use to log messages
 * @param names List of definitions in the section
 * @params values List of corresponding values for the names
 * @params cfg Pointer to structure we're filling in
 *
 * @return 0 for success
 */
static int
read_config_security(log_func_t logmsg,
                     struct list *names, struct list *values,
                     struct config_chansrv *cfg)
{
    int error = 0;
    int index;

    for (index = 0; index < names->count; ++index)
    {
        const char *name = (const char *)list_get_item(names, index);
        const char *value = (const char *)list_get_item(values, index);

        if (g_strcasecmp(name, "RestrictOutboundClipboard") == 0)
        {
            cfg->restrict_outbound_clipboard = g_text2bool(value);
        }
    }

    return error;
}

/***************************************************************************//**
 * Reads the config values we need from the [Chansrv] section
 *
 * @param logmsg Function to use to log messages
 * @param names List of definitions in the section
 * @params values List of corresponding values for the names
 * @params cfg Pointer to structure we're filling in
 *
 * @return 0 for success
 */
static int
read_config_chansrv(log_func_t logmsg,
                    struct list *names, struct list *values,
                    struct config_chansrv *cfg)
{
    int error = 0;
    int index;

    for (index = 0; index < names->count; ++index)
    {
        const char *name = (const char *)list_get_item(names, index);
        const char *value = (const char *)list_get_item(values, index);

        if (g_strcasecmp(name, "EnableFuseMount") == 0)
        {
            cfg->enable_fuse_mount = g_text2bool(value);
        }
        if (g_strcasecmp(name, "FuseMountName") == 0)
        {
            g_free(cfg->fuse_mount_name);
            cfg->fuse_mount_name = g_strdup(value);
            if (cfg->fuse_mount_name == NULL)
            {
                logmsg(LOG_LEVEL_ERROR, "Can't alloc FuseMountName");
                error = 1;
                break;
            }
        }
        else if (g_strcasecmp(name, "FileUmask") == 0)
        {
            cfg->file_umask = strtol(value, NULL, 0);
        }
    }

    return error;
}

/***************************************************************************//**
 * @brief returns a config block with default values
 *
 * @return Block, or NULL for no memory
 */
static struct config_chansrv *
new_config(void)
{
    /* Do all the allocations at the beginning, then check them together */
    struct config_chansrv *cfg = g_new0(struct config_chansrv, 1);
    char *fuse_mount_name = g_strdup(DEFAULT_FUSE_MOUNT_NAME);
    if (cfg == NULL || fuse_mount_name == NULL)
    {
        /* At least one memory allocation failed */
        g_free(fuse_mount_name);
        g_free(cfg);
        cfg = NULL;
    }
    else
    {
        cfg->use_unix_socket = DEFAULT_USE_UNIX_SOCKET;
        cfg->enable_fuse_mount = DEFAULT_ENABLE_FUSE_MOUNT;
        cfg->restrict_outbound_clipboard = DEFAULT_RESTRICT_OUTBOUND_CLIPBOARD;
        cfg->fuse_mount_name = fuse_mount_name;
        cfg->file_umask = DEFAULT_FILE_UMASK;
    }

    return cfg;
}

/******************************************************************************/
struct config_chansrv *
config_read(int use_logger, const char *sesman_ini)
{
    int error = 0;
    struct config_chansrv *cfg = NULL;
    log_func_t logmsg = (use_logger) ? log_message : log_to_stdout;
    int fd;

    fd = g_file_open_ex(sesman_ini, 1, 0, 0, 0);
    if (fd < 0)
    {
        logmsg(LOG_LEVEL_ERROR, "Can't open config file %s", sesman_ini);
        error = 1;
    }
    else
    {
        if ((cfg = new_config()) == NULL)
        {
            logmsg(LOG_LEVEL_ERROR, "Can't alloc config block");
            error = 1;
        }
        else
        {
            struct list *names = list_create();
            struct list *values = list_create();

            names->auto_free = 1;
            values->auto_free = 1;

            if (!error && file_read_section(fd, "Globals", names, values) == 0)
            {
                error = read_config_globals(logmsg, names, values, cfg);
            }


            if (!error && file_read_section(fd, "Security", names, values) == 0)
            {
                error = read_config_security(logmsg, names, values, cfg);
            }

            if (!error && file_read_section(fd, "Chansrv", names, values) == 0)
            {
                error = read_config_chansrv(logmsg, names, values, cfg);
            }

            list_delete(names);
            list_delete(values);
        }

        g_file_close(fd);
    }

    if (error)
    {
        config_free(cfg);
        cfg = NULL;
    }

    return cfg;
}

/******************************************************************************/
void
config_dump(struct config_chansrv *config)
{
    g_writeln("Global configuration:");
    g_writeln("    UseUnixSocket (derived):   %s",
              g_bool2text(config->use_unix_socket));

    g_writeln("\nSecurity configuration:");
    g_writeln("    RestrictOutboundClipboard: %s",
              g_bool2text(config->restrict_outbound_clipboard));

    g_writeln("\nChansrv configuration:");
    g_writeln("    EnableFuseMount            %s",
              g_bool2text(config->enable_fuse_mount));
    g_writeln("    FuseMountName:             %s", config->fuse_mount_name);
    g_writeln("    FileMask:                  0%o", config->file_umask);
}

/******************************************************************************/
void
config_free(struct config_chansrv *cc)
{
    if (cc != NULL)
    {
        g_free(cc->fuse_mount_name);
        g_free(cc);
    }
}
