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

#include "chansrv_common.h"
#include "chansrv_config.h"
#include "string_calls.h"
#include "sesman_clip_restrict.h"

/* Default settings */
#define DEFAULT_RESTRICT_OUTBOUND_CLIPBOARD 0
#define DEFAULT_RESTRICT_INBOUND_CLIPBOARD  0
#define DEFAULT_ENABLE_FUSE_MOUNT           1
#define DEFAULT_FUSE_MOUNT_NAME             "xrdp-client"
#define DEFAULT_FUSE_MOUNT_NAME_COLON_CHAR_REPLACEMENT ':'
#define DEFAULT_FUSE_DIRECT_IO              0
#define DEFAULT_FILE_UMASK                  077
#define DEFAULT_USE_NAUTILUS3_FLIST_FORMAT  0
#define DEFAULT_NUM_SILENT_FRAMES_AAC       4
#define DEFAULT_NUM_SILENT_FRAMES_MP3       2
#define DEFAULT_MSEC_DO_NOT_SEND            1000
#define DEFAULT_LOG_FILE_PATH               ""
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

        char unrecognised[256];
        if (g_strcasecmp(name, "ListenPort") == 0)
        {
            char *listen_port = strdup(value);
            if (listen_port == NULL)
            {
                LOG(LOG_LEVEL_WARNING,
                    "Can't allocate config memory for ListenPort");
            }
            else
            {
                g_free(cfg->listen_port);
                cfg->listen_port = listen_port;
            }
        }
        if (g_strcasecmp(name, "RestrictInboundClipboard") == 0)
        {
            cfg->restrict_inbound_clipboard =
                sesman_clip_restrict_string_to_bitmask(
                    value, unrecognised, sizeof(unrecognised));
            if (unrecognised[0] != '\0')
            {
                LOG(LOG_LEVEL_WARNING,
                    "Unrecognised tokens parsing 'RestrictInboundClipboard' %s",
                    unrecognised);
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

        char unrecognised[256];
        if (g_strcasecmp(name, "RestrictOutboundClipboard") == 0)
        {
            cfg->restrict_outbound_clipboard =
                sesman_clip_restrict_string_to_bitmask(
                    value, unrecognised, sizeof(unrecognised));
            if (unrecognised[0] != '\0')
            {
                LOG(LOG_LEVEL_WARNING,
                    "Unrecognised tokens parsing 'RestrictOutboundClipboard' %s",
                    unrecognised);
            }
        }
        if (g_strcasecmp(name, "RestrictInboundClipboard") == 0)
        {
            cfg->restrict_inbound_clipboard =
                sesman_clip_restrict_string_to_bitmask(
                    value, unrecognised, sizeof(unrecognised));
            if (unrecognised[0] != '\0')
            {
                LOG(LOG_LEVEL_WARNING,
                    "Unrecognised tokens parsing 'RestrictInboundClipboard' %s",
                    unrecognised);
            }
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
        else if (g_strcasecmp(name, "FuseMountName") == 0)
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
        else if (g_strcasecmp(name, "FuseMountNameColonCharReplacement") == 0)
        {
            size_t vallen = g_strlen(value);
            if (vallen < 1)
            {
                cfg->fuse_mount_name_colon_char_replacement = '\0';
            }
            else
            {
                if (vallen > 1)
                {
                    logmsg(LOG_LEVEL_WARNING, "FuseMountNameColonCharReplacement "
                           "must be 1 character length, now it is '%s'."
                           "Only first char will be used!",
                           value);
                }
                cfg->fuse_mount_name_colon_char_replacement = value[0];
            }
        }
        else if (g_strcasecmp(name, "FuseDirectIO") == 0)
        {
            cfg->fuse_direct_io = g_text2bool(value);
        }
        else if (g_strcasecmp(name, "FileUmask") == 0)
        {
            cfg->file_umask = strtol(value, NULL, 0);
        }
        else if (g_strcasecmp(name, "UseNautilus3FlistFormat") == 0)
        {
            cfg->use_nautilus3_flist_format = g_text2bool(value);
        }
        else if (g_strcasecmp(name, "SoundNumSilentFramesAAC") == 0)
        {
            cfg->num_silent_frames_aac = strtoul(value, NULL, 0);
        }
        else if (g_strcasecmp(name, "SoundNumSilentFramesMP3") == 0)
        {
            cfg->num_silent_frames_mp3 = strtoul(value, NULL, 0);
        }
        else if (g_strcasecmp(name, "SoundMsecDoNotSend") == 0)
        {
            cfg->msec_do_not_send = strtoul(value, NULL, 0);
        }
    }

    return error;
}

/***************************************************************************//**
 * Reads the config values we need from the [ChansrvLogging] section
 *
 * @param logmsg Function to use to log messages
 * @param names List of definitions in the section
 * @params values List of corresponding values for the names
 * @params cfg Pointer to structure we're filling in
 *
 * @return 0 for success
 */
static int
read_config_chansrv_logging(log_func_t logmsg,
                            struct list *names, struct list *values,
                            struct config_chansrv *cfg)
{
    int error = 0;
    int index;

    for (index = 0; index < names->count; ++index)
    {
        const char *name = (const char *)list_get_item(names, index);
        const char *value = (const char *)list_get_item(values, index);

        if (g_strcasecmp(name, "LogFilePath") == 0)
        {
            g_free(cfg->log_file_path);
            cfg->log_file_path = g_strdup(value);
            if (cfg->log_file_path == NULL)
            {
                logmsg(LOG_LEVEL_ERROR, "Can't alloc LogFilePath");
                error = 1;
                break;
            }
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
    char *log_file_path = g_strdup(DEFAULT_LOG_FILE_PATH);
    if (cfg == NULL || fuse_mount_name == NULL || log_file_path == NULL)
    {
        /* At least one memory allocation failed */
        g_free(log_file_path);
        g_free(fuse_mount_name);
        g_free(cfg);
        cfg = NULL;
    }
    else
    {
        cfg->listen_port = NULL;
        cfg->enable_fuse_mount = DEFAULT_ENABLE_FUSE_MOUNT;
        cfg->restrict_outbound_clipboard = DEFAULT_RESTRICT_OUTBOUND_CLIPBOARD;
        cfg->restrict_inbound_clipboard = DEFAULT_RESTRICT_INBOUND_CLIPBOARD;
        cfg->fuse_mount_name = fuse_mount_name;
        cfg->fuse_mount_name_colon_char_replacement = DEFAULT_FUSE_MOUNT_NAME_COLON_CHAR_REPLACEMENT;
        cfg->fuse_direct_io = DEFAULT_FUSE_DIRECT_IO;
        cfg->file_umask = DEFAULT_FILE_UMASK;
        cfg->use_nautilus3_flist_format = DEFAULT_USE_NAUTILUS3_FLIST_FORMAT;
        cfg->num_silent_frames_aac = DEFAULT_NUM_SILENT_FRAMES_AAC;
        cfg->num_silent_frames_mp3 = DEFAULT_NUM_SILENT_FRAMES_MP3;
        cfg->msec_do_not_send = DEFAULT_MSEC_DO_NOT_SEND;
        cfg->log_file_path = log_file_path;
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

    fd = g_file_open_ro(sesman_ini);
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

            if (!error &&
                    file_read_section(fd, "ChansrvLogging", names, values) == 0)
            {
                error = read_config_chansrv_logging(logmsg, names, values, cfg);
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
    char buf[256];

    g_writeln("Global configuration:");
    g_writeln("    xrdp-sesman ListenPort:    %s",
              (config->listen_port) ? config->listen_port : "<default>");

    g_writeln("\nSecurity configuration:");
    sesman_clip_restrict_mask_to_string(
        config->restrict_outbound_clipboard,
        buf, sizeof(buf));
    g_writeln("    RestrictOutboundClipboard: %s", buf);

    sesman_clip_restrict_mask_to_string(
        config->restrict_inbound_clipboard,
        buf, sizeof(buf));
    g_writeln("    RestrictInboundClipboard:  %s", buf);

    g_writeln("\nChansrv configuration:");
    g_writeln("    EnableFuseMount            %s",
              g_bool2text(config->enable_fuse_mount));
    g_writeln("    FuseMountName:             %s", config->fuse_mount_name);
    g_writeln("    FuseMountNameColonCharReplacement:             %c", config->fuse_mount_name_colon_char_replacement);
    g_writeln("    FuseDirectIO:              %s",
              g_bool2text(config->fuse_direct_io));
    g_writeln("    FileMask:                  0%o", config->file_umask);
    g_writeln("    Nautilus 3 Flist Format:   %s",
              g_bool2text(config->use_nautilus3_flist_format));
    g_writeln("    LogFilePath            :   %s",
              (config->log_file_path[0]) ? config->log_file_path : "<default>");
}

/******************************************************************************/
void
config_free(struct config_chansrv *cc)
{
    if (cc != NULL)
    {
        g_free(cc->listen_port);
        g_free(cc->fuse_mount_name);
        g_free(cc->log_file_path);
        g_free(cc);
    }
}
