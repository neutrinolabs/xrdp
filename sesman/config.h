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
 * @file config.h
 * @brief User authentication definitions
 * @author Simone Fedele @< simo [at] esseemme [dot] org @>
 *
 */

#ifndef CONFIG_H
#define CONFIG_H

#include "arch.h"
#include "list.h"
#include "log.h"

#define SESMAN_CFG_GLOBALS           "Globals"
#define SESMAN_CFG_DEFWM             "DefaultWindowManager"
#define SESMAN_CFG_ADDRESS           "ListenAddress"
#define SESMAN_CFG_PORT              "ListenPort"
#define SESMAN_CFG_ENABLE_USERWM     "EnableUserWindowManager"
#define SESMAN_CFG_USERWM            "UserWindowManager"
#define SESMAN_CFG_MAX_SESSION       "MaxSessions"
#define SESMAN_CFG_AUTH_FILE_PATH    "AuthFilePath"
#define SESMAN_CFG_RECONNECT_SH      "ReconnectScript"

#define SESMAN_CFG_RDP_PARAMS        "X11rdp"
#define SESMAN_CFG_XORG_PARAMS       "Xorg"
#define SESMAN_CFG_VNC_PARAMS        "Xvnc"

#define SESMAN_CFG_SESSION_VARIABLES "SessionVariables"

/*
#define SESMAN_CFG_LOGGING           "Logging"
#define SESMAN_CFG_LOG_FILE          "LogFile"
#define SESMAN_CFG_LOG_LEVEL         "LogLevel"
#define SESMAN_CFG_LOG_ENABLE_SYSLOG "EnableSyslog"
#define SESMAN_CFG_LOG_SYSLOG_LEVEL  "SyslogLevel"
*/
#define SESMAN_CFG_SECURITY                        "Security"
#define SESMAN_CFG_SEC_LOGIN_RETRY                 "MaxLoginRetry"
#define SESMAN_CFG_SEC_ALLOW_ROOT                  "AllowRootLogin"
#define SESMAN_CFG_SEC_USR_GROUP                   "TerminalServerUsers"
#define SESMAN_CFG_SEC_ADM_GROUP                   "TerminalServerAdmins"
#define SESMAN_CFG_SEC_ALWAYSGROUPCHECK            "AlwaysGroupCheck"
#define SESMAN_CFG_SEC_RESTRICT_OUTBOUND_CLIPBOARD "RestrictOutboundClipboard"

#define SESMAN_CFG_SESSIONS          "Sessions"
#define SESMAN_CFG_SESS_MAX          "MaxSessions"
#define SESMAN_CFG_SESS_KILL_DISC    "KillDisconnected"
#define SESMAN_CFG_SESS_IDLE_LIMIT   "IdleTimeLimit"
#define SESMAN_CFG_SESS_DISC_LIMIT   "DisconnectedTimeLimit"
#define SESMAN_CFG_SESS_X11DISPLAYOFFSET "X11DisplayOffset"

#define SESMAN_CFG_SESS_POLICY_S "Policy"
#define SESMAN_CFG_SESS_POLICY_DFLT_S "Default"
#define SESMAN_CFG_SESS_POLICY_UBD_S "UBD"
#define SESMAN_CFG_SESS_POLICY_UBI_S "UBI"
#define SESMAN_CFG_SESS_POLICY_UBC_S "UBC"
#define SESMAN_CFG_SESS_POLICY_UBDI_S "UBDI"
#define SESMAN_CFG_SESS_POLICY_UBDC_S "UBDC"

enum SESMAN_CFG_SESS_POLICY_BITS
{
    SESMAN_CFG_SESS_POLICY_D = 0x01,
    SESMAN_CFG_SESS_POLICY_I = 0x02,
    SESMAN_CFG_SESS_POLICY_C = 0x04
};

enum SESMAN_CFG_SESS_POLICY
{
    SESMAN_CFG_SESS_POLICY_DFLT = 0,
    SESMAN_CFG_SESS_POLICY_UBD = SESMAN_CFG_SESS_POLICY_D,
    SESMAN_CFG_SESS_POLICY_UBI = SESMAN_CFG_SESS_POLICY_I,
    SESMAN_CFG_SESS_POLICY_UBC = SESMAN_CFG_SESS_POLICY_C,
    SESMAN_CFG_SESS_POLICY_UBDI = SESMAN_CFG_SESS_POLICY_D | SESMAN_CFG_SESS_POLICY_I,
    SESMAN_CFG_SESS_POLICY_UBDC = SESMAN_CFG_SESS_POLICY_D | SESMAN_CFG_SESS_POLICY_C
};

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
    int ts_users_enable;
    int ts_users;
    /**
     * @var ts_admins
     * @brief Terminal Server Administrators group
     */
    int ts_admins_enable;
    int ts_admins;
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
    int x11_display_offset;
    /**
     * @var max_sessions
     * @brief maximum number of allowed sessions. 0 for unlimited
     */
    int max_sessions;
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
    enum SESMAN_CFG_SESS_POLICY policy;
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
     * @var listen_address
     * @brief Listening address
     */
    char listen_address[32];
    /**
     * @var listen_port
     * @brief Listening port
     */
    char listen_port[16];
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
     * @brief Default window manager
     */
    char user_wm[32];
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
     * @var rdp_params
     * @brief X11rdp additional parameter list
     */
    struct list *rdp_params;
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

#endif
