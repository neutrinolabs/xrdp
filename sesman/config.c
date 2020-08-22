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
 * @file config.c
 * @brief User authentication code
 * @author Simone Fedele @< simo [at] esseemme [dot] org @>
 *
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "arch.h"
#include "list.h"
#include "file.h"
#include "sesman.h"
#include "log.h"
#include "string_calls.h"

/***************************************************************************//**
 *
 * @brief Reads sesman [global] configuration section
 * @param file configuration file descriptor
 * @param cf pointer to a config struct
 * @param param_n parameter name list
 * @param param_v parameter value list
 * @return 0 on success, 1 on failure
 *
 */
static int
config_read_globals(int file, struct config_sesman *cf, struct list *param_n,
                    struct list *param_v)
{
    int i;
    int length;
    char *buf;

    list_clear(param_v);
    list_clear(param_n);

    /* resetting the struct */
    cf->listen_address[0] = '\0';
    cf->listen_port[0] = '\0';
    cf->enable_user_wm = 0;
    cf->user_wm[0] = '\0';
    cf->default_wm = 0;
    cf->auth_file_path = 0;
    cf->reconnect_sh = 0;

    file_read_section(file, SESMAN_CFG_GLOBALS, param_n, param_v);

    for (i = 0; i < param_n->count; i++)
    {
        buf = (char *)list_get_item(param_n, i);

        if (0 == g_strcasecmp(buf, SESMAN_CFG_DEFWM))
        {
            cf->default_wm = g_strdup((char *)list_get_item(param_v, i));
        }
        else if (0 == g_strcasecmp(buf, SESMAN_CFG_USERWM))
        {
            g_strncpy(cf->user_wm, (char *)list_get_item(param_v, i), 31);
        }
        else if (0 == g_strcasecmp(buf, SESMAN_CFG_ENABLE_USERWM))
        {
            cf->enable_user_wm = g_text2bool((char *)list_get_item(param_v, i));
        }
        else if (0 == g_strcasecmp(buf, SESMAN_CFG_PORT))
        {
            g_strncpy(cf->listen_port, (char *)list_get_item(param_v, i), 15);
        }
        else if (0 == g_strcasecmp(buf, SESMAN_CFG_ADDRESS))
        {
            g_strncpy(cf->listen_address, (char *)list_get_item(param_v, i), 31);
        }
        else if (0 == g_strcasecmp(buf, SESMAN_CFG_AUTH_FILE_PATH))
        {
            cf->auth_file_path = g_strdup((char *)list_get_item(param_v, i));
        }
        else if (g_strcasecmp(buf, SESMAN_CFG_RECONNECT_SH) == 0)
        {
            cf->reconnect_sh = g_strdup((char *)list_get_item(param_v, i));
        }
    }

    /* checking for missing required parameters */
    if ('\0' == cf->listen_address[0])
    {
        g_strncpy(cf->listen_address, "0.0.0.0", 8);
    }

    if ('\0' == cf->listen_port[0])
    {
        g_strncpy(cf->listen_port, "3350", 5);
    }

    if ('\0' == cf->user_wm[0])
    {
        cf->enable_user_wm = 0;
    }

    if (cf->default_wm == 0)
    {
        cf->default_wm = g_strdup("startwm.sh");
    }
    else if (g_strlen(cf->default_wm) == 0)
    {
        g_free(cf->default_wm);
        cf->default_wm = g_strdup("startwm.sh");
    }
    /* if default_wm doesn't begin with '/', it's a relative path to XRDP_CFG_PATH */
    if (cf->default_wm[0] != '/')
    {
        /* sizeof operator returns string length including null terminator  */
        length = sizeof(XRDP_CFG_PATH) + g_strlen(cf->default_wm) + 1; /* '/' */
        buf = (char *)g_malloc(length, 0);
        g_sprintf(buf, "%s/%s", XRDP_CFG_PATH, cf->default_wm);
        g_free(cf->default_wm);
        cf->default_wm = g_strdup(buf);
        g_free(buf);
    }

    if (cf->reconnect_sh == 0)
    {
        cf->reconnect_sh = g_strdup("reconnectwm.sh");
    }
    else if (g_strlen(cf->reconnect_sh) == 0)
    {
        g_free(cf->reconnect_sh);
        cf->reconnect_sh = g_strdup("reconnectwm.sh");
    }
    /* if reconnect_sh doesn't begin with '/', it's a relative path to XRDP_CFG_PATH */
    if (cf->reconnect_sh[0] != '/')
    {
        /* sizeof operator returns string length including null terminator  */
        length = sizeof(XRDP_CFG_PATH) + g_strlen(cf->reconnect_sh) + 1; /* '/' */
        buf = (char *)g_malloc(length, 0);
        g_sprintf(buf, "%s/%s", XRDP_CFG_PATH, cf->reconnect_sh);
        g_free(cf->reconnect_sh);
        cf->reconnect_sh = g_strdup(buf);
        g_free(buf);
    }

    return 0;
}

/***************************************************************************//**
 *
 * @brief Reads sesman [Security] configuration section
 * @param file configuration file descriptor
 * @param sc pointer to a config_security struct
 * @param param_n parameter name list
 * @param param_v parameter value list
 * @return 0 on success, 1 on failure
 *
 */
static int
config_read_security(int file, struct config_security *sc,
                     struct list *param_n,
                     struct list *param_v)
{
    int i;
    int gid;
    char *buf;

    list_clear(param_v);
    list_clear(param_n);

    /* setting defaults */
    sc->allow_root = 0;
    sc->login_retry = 3;
    sc->ts_users_enable = 0;
    sc->ts_admins_enable = 0;
    sc->restrict_outbound_clipboard = 0;

    file_read_section(file, SESMAN_CFG_SECURITY, param_n, param_v);

    for (i = 0; i < param_n->count; i++)
    {
        buf = (char *)list_get_item(param_n, i);

        if (0 == g_strcasecmp(buf, SESMAN_CFG_SEC_ALLOW_ROOT))
        {
            sc->allow_root = g_text2bool((char *)list_get_item(param_v, i));
        }

        if (0 == g_strcasecmp(buf, SESMAN_CFG_SEC_LOGIN_RETRY))
        {
            sc->login_retry = g_atoi((char *)list_get_item(param_v, i));
        }

        if (0 == g_strcasecmp(buf, SESMAN_CFG_SEC_USR_GROUP))
        {
            if (g_getgroup_info((char *)list_get_item(param_v, i), &gid) == 0)
            {
                sc->ts_users_enable = 1;
                sc->ts_users = gid;
            }
        }

        if (0 == g_strcasecmp(buf, SESMAN_CFG_SEC_ADM_GROUP))
        {
            if (g_getgroup_info((char *)list_get_item(param_v, i), &gid) == 0)
            {
                sc->ts_admins_enable = 1;
                sc->ts_admins = gid;
            }
        }
        if (0 == g_strcasecmp(buf, SESMAN_CFG_SEC_ALWAYSGROUPCHECK))
        {
            sc->ts_always_group_check = g_text2bool((char *)list_get_item(param_v, i));
        }

        if (0 == g_strcasecmp(buf, SESMAN_CFG_SEC_RESTRICT_OUTBOUND_CLIPBOARD))
        {
            sc->restrict_outbound_clipboard = g_text2bool((char *)list_get_item(param_v, i));
        }

    }

    return 0;
}

/***************************************************************************//**
 *
 * @brief Reads sesman [Sessions] configuration section
 * @param file configuration file descriptor
 * @param ss pointer to a config_sessions struct
 * @param param_n parameter name list
 * @param param_v parameter value list
 * @return 0 on success, 1 on failure
 *
 */
static int
config_read_sessions(int file, struct config_sessions *se, struct list *param_n,
                     struct list *param_v)
{
    int i;
    char *buf;

    list_clear(param_v);
    list_clear(param_n);

    /* setting defaults */
    se->x11_display_offset = 10;
    se->max_sessions = 0;
    se->max_idle_time = 0;
    se->max_disc_time = 0;
    se->kill_disconnected = 0;
    se->policy = SESMAN_CFG_SESS_POLICY_DFLT;

    file_read_section(file, SESMAN_CFG_SESSIONS, param_n, param_v);

    for (i = 0; i < param_n->count; i++)
    {
        buf = (char *)list_get_item(param_n, i);

        if (0 == g_strcasecmp(buf, SESMAN_CFG_SESS_X11DISPLAYOFFSET))
        {
            se->x11_display_offset = g_atoi((char *)list_get_item(param_v, i));
        }

        if (0 == g_strcasecmp(buf, SESMAN_CFG_SESS_MAX))
        {
            se->max_sessions = g_atoi((char *)list_get_item(param_v, i));
        }

        if (0 == g_strcasecmp(buf, SESMAN_CFG_SESS_KILL_DISC))
        {
            se->kill_disconnected = g_text2bool((char *)list_get_item(param_v, i));
        }

        if (0 == g_strcasecmp(buf, SESMAN_CFG_SESS_IDLE_LIMIT))
        {
            se->max_idle_time = g_atoi((char *)list_get_item(param_v, i));
        }

        if (0 == g_strcasecmp(buf, SESMAN_CFG_SESS_DISC_LIMIT))
        {
            se->max_disc_time = g_atoi((char *)list_get_item(param_v, i));
        }

        if (0 == g_strcasecmp(buf, SESMAN_CFG_SESS_POLICY_S))
        {
            char *value = (char *)list_get_item(param_v, i);
            if (0 == g_strcasecmp(value, SESMAN_CFG_SESS_POLICY_DFLT_S))
            {
                se->policy = SESMAN_CFG_SESS_POLICY_DFLT;
            }
            else if (0 == g_strcasecmp(value, SESMAN_CFG_SESS_POLICY_UBD_S))
            {
                se->policy = SESMAN_CFG_SESS_POLICY_UBD;
            }
            else if (0 == g_strcasecmp(value, SESMAN_CFG_SESS_POLICY_UBI_S))
            {
                se->policy = SESMAN_CFG_SESS_POLICY_UBI;
            }
            else if (0 == g_strcasecmp(value, SESMAN_CFG_SESS_POLICY_UBC_S))
            {
                se->policy = SESMAN_CFG_SESS_POLICY_UBC;
            }
            else if (0 == g_strcasecmp(value, SESMAN_CFG_SESS_POLICY_UBDI_S))
            {
                se->policy = SESMAN_CFG_SESS_POLICY_UBDI;
            }
            else if (0 == g_strcasecmp(value, SESMAN_CFG_SESS_POLICY_UBDC_S))
            {
                se->policy = SESMAN_CFG_SESS_POLICY_UBDC;
            }
            else /* silently ignore typos */
            {
                se->policy = SESMAN_CFG_SESS_POLICY_DFLT;
            }
        }
    }

    return 0;
}

/***************************************************************************//**
 *
 * @brief Reads sesman [X11rdp] configuration section
 * @param file configuration file descriptor
 * @param cs pointer to a config_sesman struct
 * @param param_n parameter name list
 * @param param_v parameter value list
 * @return 0 on success, 1 on failure
 *
 */
static int
config_read_rdp_params(int file, struct config_sesman *cs, struct list *param_n,
                       struct list *param_v)
{
    int i;

    list_clear(param_v);
    list_clear(param_n);

    cs->rdp_params = list_create();
    cs->rdp_params->auto_free = 1;

    file_read_section(file, SESMAN_CFG_RDP_PARAMS, param_n, param_v);

    for (i = 0; i < param_n->count; i++)
    {
        list_add_item(cs->rdp_params, (long)g_strdup((char *)list_get_item(param_v, i)));
    }

    return 0;
}

/***************************************************************************//**
 *
 * @brief Reads sesman [Xorg] configuration section
 * @param file configuration file descriptor
 * @param cs pointer to a config_sesman struct
 * @param param_n parameter name list
 * @param param_v parameter value list
 * @return 0 on success, 1 on failure
 *
 */
static int
config_read_xorg_params(int file, struct config_sesman *cs,
                        struct list *param_n, struct list *param_v)
{
    int i;

    list_clear(param_v);
    list_clear(param_n);

    cs->xorg_params = list_create();
    cs->xorg_params->auto_free = 1;

    file_read_section(file, SESMAN_CFG_XORG_PARAMS, param_n, param_v);

    for (i = 0; i < param_n->count; i++)
    {
        list_add_item(cs->xorg_params,
                      (long) g_strdup((char *) list_get_item(param_v, i)));
    }

    return 0;
}

/***************************************************************************//**
 *
 * @brief Reads sesman [Xvnc] configuration section
 * @param file configuration file descriptor
 * @param cs pointer to a config_sesman struct
 * @param param_n parameter name list
 * @param param_v parameter value list
 * @return 0 on success, 1 on failure
 *
 */
static int
config_read_vnc_params(int file, struct config_sesman *cs, struct list *param_n,
                       struct list *param_v)
{
    int i;

    list_clear(param_v);
    list_clear(param_n);

    cs->vnc_params = list_create();
    cs->vnc_params->auto_free = 1;

    file_read_section(file, SESMAN_CFG_VNC_PARAMS, param_n, param_v);

    for (i = 0; i < param_n->count; i++)
    {
        list_add_item(cs->vnc_params, (long)g_strdup((char *)list_get_item(param_v, i)));
    }

    return 0;
}

/******************************************************************************/
static int
config_read_session_variables(int file, struct config_sesman *cs,
                              struct list *param_n, struct list *param_v)
{
    int i;

    list_clear(param_v);
    list_clear(param_n);

    cs->env_names = list_create();
    cs->env_names->auto_free = 1;
    cs->env_values = list_create();
    cs->env_values->auto_free = 1;

    file_read_section(file, SESMAN_CFG_SESSION_VARIABLES, param_n, param_v);

    for (i = 0; i < param_n->count; i++)
    {
        list_add_item(cs->env_names,
                      (tintptr) g_strdup((char *) list_get_item(param_n, i)));
        list_add_item(cs->env_values,
                      (tintptr) g_strdup((char *) list_get_item(param_v, i)));
    }

    return 0;
}

/******************************************************************************/
struct config_sesman *
config_read(const char *sesman_ini)
{
    struct config_sesman *cfg;
    int all_ok = 0;

    if ((cfg = g_new0(struct config_sesman, 1)) != NULL)
    {
        if ((cfg->sesman_ini = g_strdup(sesman_ini)) != NULL)
        {
            int fd;
            if ((fd = g_file_open_ex(cfg->sesman_ini, 1, 0, 0, 0)) != -1)
            {
                struct list *sec;
                struct list *param_n;
                struct list *param_v;
                sec = list_create();
                sec->auto_free = 1;
                file_read_sections(fd, sec);
                param_n = list_create();
                param_n->auto_free = 1;
                param_v = list_create();
                param_v->auto_free = 1;

                /* read global config */
                config_read_globals(fd, cfg, param_n, param_v);

                /* read Xvnc/X11rdp/Xorg parameter list */
                config_read_vnc_params(fd, cfg, param_n, param_v);
                config_read_rdp_params(fd, cfg, param_n, param_v);
                config_read_xorg_params(fd, cfg, param_n, param_v);

                /* read security config */
                config_read_security(fd, &(cfg->sec), param_n, param_v);

                /* read session config */
                config_read_sessions(fd, &(cfg->sess), param_n, param_v);

                config_read_session_variables(fd, cfg, param_n, param_v);

                /* cleanup */
                list_delete(sec);
                list_delete(param_v);
                list_delete(param_n);
                g_file_close(fd);
                all_ok = 1;
            }
        }
    }

    if (!all_ok)
    {
        config_free(cfg);
        cfg = NULL;
    }

    return cfg;
}

/******************************************************************************/
void
config_dump(struct config_sesman *config)
{
    int i;
    struct config_sessions *se;
    struct config_security *sc;
    se = &(config->sess);
    sc = &(config->sec);

    /* Global sesman configuration */
    g_writeln("Filename:                     %s", config->sesman_ini);
    g_writeln("Global configuration:");
    g_writeln("    ListenAddress:            %s", config->listen_address);
    g_writeln("    ListenPort:               %s", config->listen_port);
    g_writeln("    EnableUserWindowManager:  %d", config->enable_user_wm);
    g_writeln("    UserWindowManager:        %s", config->user_wm);
    g_writeln("    DefaultWindowManager:     %s", config->default_wm);
    g_writeln("    ReconnectScript:          %s", config->reconnect_sh);
    g_writeln("    AuthFilePath:             %s",
              ((config->auth_file_path) ? (config->auth_file_path) : ("disabled")));

    /* Session configuration */
    g_writeln("Session configuration:");
    g_writeln("    MaxSessions:              %d", se->max_sessions);
    g_writeln("    X11DisplayOffset:         %d", se->x11_display_offset);
    g_writeln("    KillDisconnected:         %d", se->kill_disconnected);
    g_writeln("    IdleTimeLimit:            %d", se->max_idle_time);
    g_writeln("    DisconnectedTimeLimit:    %d", se->max_disc_time);
    g_writeln("    Policy:                   %d", se->policy);

    /* Security configuration */
    g_writeln("Security configuration:");
    g_writeln("    AllowRootLogin:           %d", sc->allow_root);
    g_writeln("    MaxLoginRetry:            %d", sc->login_retry);
    g_writeln("    AlwaysGroupCheck:         %d", sc->ts_always_group_check);
    g_writeln("    RestrictOutboundClipboard: %d", sc->restrict_outbound_clipboard);

    g_printf( "    TSUsersGroup:             ");
    if (sc->ts_users_enable)
    {
        g_printf("%d", sc->ts_users);
    }
    else
    {
        g_printf("(not defined)");
    }
    g_writeln("%s", "");

    g_printf( "    TSAdminsGroup:            ");
    if (sc->ts_admins_enable)
    {
        g_printf("%d", sc->ts_admins);
    }
    else
    {
        g_printf("(not defined)");
    }
    g_writeln("%s", "");


    /* Xorg */
    if (config->xorg_params->count)
    {
        g_writeln("Xorg parameters:");
    }

    for (i = 0; i < config->xorg_params->count; i++)
    {
        g_writeln("    Parameter %02d              %s",
                  i, (char *) list_get_item(config->xorg_params, i));
    }

    /* Xvnc */
    if (config->vnc_params->count)
    {
        g_writeln("Xvnc parameters:");
    }

    for (i = 0; i < config->vnc_params->count; i++)
    {
        g_writeln("    Parameter %02d              %s",
                  i, (char *)list_get_item(config->vnc_params, i));
    }

    /* X11rdp */
    if (config->rdp_params->count)
    {
        g_writeln("X11rdp parameters:");
    }

    for (i = 0; i < config->rdp_params->count; i++)
    {
        g_writeln("    Parameter %02d              %s",
                  i, (char *)list_get_item(config->rdp_params, i));
    }

    /* SessionVariables */
    if (config->env_names->count)
    {
        g_writeln("%s parameters:", SESMAN_CFG_SESSION_VARIABLES);
    }

    for (i = 0; i < config->env_names->count; i++)
    {
        g_writeln("    Parameter %02d              %s=%s",
                  i, (char *) list_get_item(config->env_names, i),
                  (char *) list_get_item(config->env_values, i));
    }
}

/******************************************************************************/
void
config_free(struct config_sesman *cs)
{
    if (cs != NULL)
    {
        g_free(cs->sesman_ini);
        g_free(cs->default_wm);
        g_free(cs->reconnect_sh);
        g_free(cs->auth_file_path);
        list_delete(cs->rdp_params);
        list_delete(cs->vnc_params);
        list_delete(cs->xorg_params);
        list_delete(cs->env_names);
        list_delete(cs->env_values);
        g_free(cs);
    }
}
