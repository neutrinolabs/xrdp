/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2013
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

/**
 *
 * @file sesman_config.h
 * @brief User authentication definitions
 * @author Simone Fedele @< simo [at] esseemme [dot] org @>
 *
 */

#ifndef SESMAN_CONFIG_H
#define SESMAN_CONFIG_H

#include "arch.h"
#include "list.h"
#include "log.h"

#include "xrdp_sockets.h"

enum SESMAN_CFG_SESS_POLICY_BITS
{
    /* If these two are set, they override everything else */
    SESMAN_CFG_SESS_POLICY_DEFAULT = (1 << 0),
    SESMAN_CFG_SESS_POLICY_SEPARATE = (1 << 1),
    /* Configuration bits */
    SESMAN_CFG_SESS_POLICY_U = (1 << 2),
    SESMAN_CFG_SESS_POLICY_B = (1 << 3),
    SESMAN_CFG_SESS_POLICY_D = (1 << 4),
    SESMAN_CFG_SESS_POLICY_I = (1 << 5)
};

/**
 * Name of default sesman.ini file
 */
#define DEFAULT_SESMAN_INI XRDP_CFG_PATH "/sesman.ini"

/**
 *
 * @struct config_security
 * @brief struct that contains sesman access control configuration
 *
 */
struct config_security
{
    /**
     * @var allow_root
     * @brief allow root login on TS
     */
    int allow_root;
    /**
     * @var login_retry
     * @brief maximum login attempts
     */
    int login_retry;
    /**
     * @var ts_users
     * @brief Terminal Server Users group
     */
    char *ts_users;
    /**
     * @var ts_admins
     * @brief Terminal Server Administrators group
     */
    char *ts_admins;
    /**
     * @var ts_always_group_check
     * @brief if the Groups are not found deny access
     */
    int ts_always_group_check;
    /**
     * @var restrict_outbound_clipboard
     * @brief if the clipboard should be enforced restricted. If true only allow client -> server, not vice versa.
     */
    int restrict_outbound_clipboard;

    /**
     * @var restrict_inbound_clipboard
     * @brief if the clipboard should be enforced restricted. If true only allow server -> client, not vice versa.
     */
    int restrict_inbound_clipboard;

    /**
     * @var allow_alternate_shell
     * @brief allow an user-supplied alternate shell.
     * @details 'YES' for all programs allowed, 'NO' for disabling of alternate
     *          shells.
     *          If not specified, 'YES' is assumed.
     */
    int allow_alternate_shell;

    /*
     * @var xorg_no_new_privileges
     * @brief if the Xorg X11 server should be started with no_new_privs (Linux only)
     */
    int xorg_no_new_privileges;

    /*
     * @var session_sockdir_group
     * @brief Group to have read access to the session sockdirs
     */
    char *session_sockdir_group;
};

/**
 *
 * @struct config_sessions
 * @brief struct that contains sesman session handling configuration
 *
 */
struct config_sessions
{
    /**
     * @var x11_display_offset
     * @brief X11 TCP port offset. default value: 10
     */
    unsigned int x11_display_offset;
    /**
     * @var max_display_number
     * @brief Highest X11 display number considered for allocation
     */
    unsigned int max_display_number;
    /**
     * @var max_sessions
     * @brief maximum number of allowed sessions. 0 for unlimited
     */
    unsigned int max_sessions;
    /**
     * @var max_idle_time
     * @brief maximum idle time for each session
     */
    int max_idle_time;
    /**
     * @var max_disc_time
     * @brief maximum disconnected time for each session
     */
    int max_disc_time;
    /**
     * @var kill_disconnected
     * @brief enables automatic killing of disconnected session
     */
    int kill_disconnected;
    /**
     * @var policy
     * @brief session allocation policy
     */
    unsigned int policy;
};

/**
 *
 * @struct config_sesman
 * @brief struct that contains sesman configuration
 *
 * This struct contains all of sesman configuration parameters\n
 * Every parameter in [globals] is a member of this struct, other
 * sections options are embedded in this struct as member structures
 *
 */
struct config_sesman
{
    /**
     * @var sesman_ini
     * @brief File that these parameters are read from
     */
    char *sesman_ini;

    /**
     * @var listen_port
     * @brief Listening port
     */
    char listen_port[XRDP_SOCKETS_MAXPATH];
    /**
     * @var enable_user_wm
     * @brief Flag that enables user specific wm
     */
    int enable_user_wm;
    /**
     * @var default_wm
     * @brief Default window manager
     */
    char *default_wm;
    /**
     * @var user_wm
     * @brief Default window manager.
     */
    char *user_wm;
    /**
     * @var reconnect_sh
     * @brief Script executed when reconnected
     */
    char *reconnect_sh;
    /**
     * @var auth_file_path
     * @brief Auth file path
     */
    char *auth_file_path;
    /**
     * @var vnc_params
     * @brief Xvnc additional parameter list
     */
    struct list *vnc_params;
    /**
     * @var xorg_params
     * @brief Xorg additional parameter list
     */
    struct list *xorg_params;
    /**
     * @var log
     * @brief Log configuration struct
     */
    //struct log_config log;
    /**
     * @var sec
     * @brief Security configuration options struct
     */
    struct config_security sec;
    /**
     * @var sess
     * @brief Session configuration options struct
     */
    struct config_sessions sess;

    /**
     * @var env_names
     * @brief environment variable name list
     */
    struct list *env_names;
    /**
    * @var env_values
    * @brief environment variable value list
    */
    struct list *env_values;
};

/**
 *
 * @brief Reads sesman configuration
 * @param sesman_ini Name of configuration file to read
 * @return configuration on success, NULL on failure
 *
 * @post pass return value to config_free() to prevent memory leaks
 *
 */
struct config_sesman *
config_read(const char *sesman_ini);

/**
 *
 * @brief Dumps configuration
 * @param pointer to a config_sesman struct
 *
 */
void
config_dump(struct config_sesman *config);

/**
 *
 * @brief Frees configuration allocated by config_read()
 * @param pointer to a config_sesman struct (may be NULL)
 *
 */
void
config_free(struct config_sesman *cs);

/**
 * Converts a session allocation Policy value to a strin
 * @param value - Session allocation policy value
 * @param buff - Buffer for result
 * @param bufflen - Length of buffer
 * @return Length of string that would be required without a terminator
 *         to write the whole output (like snprintf())
 */
int
config_output_policy_string(unsigned int value,
                            char *buff, unsigned int bufflen);

#endif
