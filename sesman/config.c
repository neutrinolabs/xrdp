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
#include "config.h"

#include "list.h"
#include "file.h"
#include "log.h"
#include "string_calls.h"
#include "chansrv/chansrv_common.h"
#include "scp.h"

static const struct bitmask_char policy_bits[] =
{
    { SESMAN_CFG_SESS_POLICY_U, 'U'  },
    { SESMAN_CFG_SESS_POLICY_B, 'B'  },
    { SESMAN_CFG_SESS_POLICY_D, 'D'  },
    { SESMAN_CFG_SESS_POLICY_I, 'I'  },
    BITMASK_CHAR_END_OF_LIST
};

/***************************************************************************//**
 * Parse a session allocation policy string
 */
static unsigned int
parse_policy_string(const char *value)
{
    unsigned int rv;
    char unrecognised[16];

    if (0 == g_strcasecmp(value, SESMAN_CFG_SESS_POLICY_DFLT_S))
    {
        rv = SESMAN_CFG_SESS_POLICY_DEFAULT;
    }
    else if (0 == g_strcasecmp(value, SESMAN_CFG_SESS_POLICY_SEP_S))
    {
        rv = SESMAN_CFG_SESS_POLICY_SEPARATE;
    }
    else
    {
        unrecognised[0] = '\0';
        rv = g_charstr_to_bitmask(value, policy_bits, unrecognised,
                                  sizeof(unrecognised));
        if (unrecognised[0] != '\0')
        {
            LOG(LOG_LEVEL_WARNING, "Character(s) '%s' in the session"
                " allocation policy are not recognised", unrecognised);

            if (g_strchr(unrecognised, 'C') != NULL ||
                    g_strchr(unrecognised, 'c') != NULL)
            {
                /* Change from xrdp v0.9.x */
                LOG(LOG_LEVEL_WARNING, "Character 'C' is no longer used"
                    " in session allocation policies - use '%s'",
                    SESMAN_CFG_SESS_POLICY_SEP_S);
            }
        }
    }

    return rv;
}

/******************************************************************************/
int
config_output_policy_string(unsigned int value,
                            char *buff, unsigned int bufflen)
{
    int rv = 0;
    if (bufflen > 0)
    {
        if (value & SESMAN_CFG_SESS_POLICY_DEFAULT)
        {
            rv = g_snprintf(buff, bufflen, "Default");
        }
        else if (value & SESMAN_CFG_SESS_POLICY_SEPARATE)
        {
            rv = g_snprintf(buff, bufflen, "Separate");
        }
        else
        {
            rv = g_bitmask_to_charstr(value, policy_bits, buff, bufflen, NULL);
        }
    }

    return rv;
}

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

    list_clear(param_v);
    list_clear(param_n);

    /* resetting the struct */
    cf->listen_port[0] = '\0';
    cf->enable_user_wm = 0;
    cf->user_wm[0] = '\0';
    cf->default_wm = 0;
    cf->auth_file_path = 0;
    cf->reconnect_sh = 0;

    file_read_section(file, SESMAN_CFG_GLOBALS, param_n, param_v);

    for (i = 0; i < param_n->count; i++)
    {
        const char *param = (const char *)list_get_item(param_n, i);
        const char *val = (const char *)list_get_item(param_v, i);

        if (0 == g_strcasecmp(param, SESMAN_CFG_DEFWM))
        {
            cf->default_wm = g_strdup(val);
        }
        else if (0 == g_strcasecmp(param, SESMAN_CFG_USERWM))
        {
            g_strncpy(cf->user_wm, val, sizeof(cf->user_wm) - 1);
        }
        else if (0 == g_strcasecmp(param, SESMAN_CFG_ENABLE_USERWM))
        {
            cf->enable_user_wm = g_text2bool(val);
        }
        else if (0 == g_strcasecmp(param, SESMAN_CFG_PORT))
        {
            scp_port_to_unix_domain_path(val, cf->listen_port,
                                         sizeof(cf->listen_port));
        }
        else if (0 == g_strcasecmp(param, SESMAN_CFG_AUTH_FILE_PATH))
        {
            cf->auth_file_path = g_strdup(val);
        }
        else if (g_strcasecmp(param, SESMAN_CFG_RECONNECT_SH) == 0)
        {
            cf->reconnect_sh = g_strdup(val);
        }
        else if (0 == g_strcasecmp(param, SESMAN_CFG_ADDRESS))
        {
            /* Config must be updated for Unix Domain Sockets */
            LOG(LOG_LEVEL_WARNING, "Obsolete setting' " SESMAN_CFG_ADDRESS
                "' in [" SESMAN_CFG_GLOBALS "] should be removed.");
            LOG(LOG_LEVEL_WARNING, "Review setting' " SESMAN_CFG_PORT "' in ["
                SESMAN_CFG_GLOBALS "]");
        }
    }

    /* checking for missing required parameters */
    if ('\0' == cf->listen_port[0])
    {
        /* Load the default value */
        scp_port_to_unix_domain_path(NULL, cf->listen_port,
                                     sizeof(cf->listen_port));
    }

    if ('\0' == cf->user_wm[0])
    {
        cf->enable_user_wm = 0;
    }

    if (cf->default_wm == 0 || cf->default_wm[0] == '\0')
    {
        g_free(cf->default_wm);
        cf->default_wm = g_strdup("startwm.sh");
    }
    /* if default_wm doesn't begin with '/', it's a relative path to
     * XRDP_CFG_PATH */
    if (cf->default_wm[0] != '/')
    {
        /* sizeof operator returns string length including null terminator  */
        int length = (sizeof(XRDP_CFG_PATH) +
                      g_strlen(cf->default_wm) + 1); /* '/' */
        char *buf = (char *)g_malloc(length, 0);
        g_sprintf(buf, "%s/%s", XRDP_CFG_PATH, cf->default_wm);
        g_free(cf->default_wm);
        cf->default_wm = buf;
    }

    if (cf->reconnect_sh == 0 || cf->reconnect_sh[0] == '\0')
    {
        g_free(cf->reconnect_sh);
        cf->reconnect_sh = g_strdup("reconnectwm.sh");
    }
    /* if reconnect_sh doesn't begin with '/', it's a relative path to
     * XRDP_CFG_PATH */
    if (cf->reconnect_sh[0] != '/')
    {
        /* sizeof operator returns string length including null terminator  */
        int length = (sizeof(XRDP_CFG_PATH) +
                      g_strlen(cf->reconnect_sh) + 1); /* '/' */
        char *buf = (char *)g_malloc(length, 0);
        g_sprintf(buf, "%s/%s", XRDP_CFG_PATH, cf->reconnect_sh);
        g_free(cf->reconnect_sh);
        cf->reconnect_sh = buf;
    }

    return 0;
}

/*
  Map clipboard strings into bitmask values.
  Duplicated definition exists in chansrv_config,
  because it avoids build failure for xrdp-sesman and xrdp-sesrun.
  It should be unified in the future.
*/
static const struct bitmask_string clip_restrict_map[] =
{
    { CLIP_RESTRICT_TEXT, "text"},
    { CLIP_RESTRICT_FILE, "file"},
    { CLIP_RESTRICT_IMAGE, "image"},
    { CLIP_RESTRICT_ALL, "all"},
    { CLIP_RESTRICT_NONE, "none"},
    /* Compatibility values */
    { CLIP_RESTRICT_ALL, "true"},
    { CLIP_RESTRICT_ALL, "yes"},
    { CLIP_RESTRICT_NONE, "false"},
    BITMASK_STRING_END_OF_LIST
};

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
    sc->restrict_inbound_clipboard = 0;

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
            char unrecognised[256];
            sc->restrict_outbound_clipboard =
                g_str_to_bitmask((const char *)list_get_item(param_v, i),
                                 clip_restrict_map, ",",
                                 unrecognised, sizeof(unrecognised));
            if (unrecognised[0] != '\0')
            {
                LOG(LOG_LEVEL_WARNING,
                    "Unrecognised tokens parsing 'RestrictOutboundClipboard' %s",
                    unrecognised);
            }
        }
        if (0 == g_strcasecmp(buf, SESMAN_CFG_SEC_RESTRICT_INBOUND_CLIPBOARD))
        {
            char unrecognised[256];
            sc->restrict_inbound_clipboard =
                g_str_to_bitmask((const char *)list_get_item(param_v, i),
                                 clip_restrict_map, ",",
                                 unrecognised, sizeof(unrecognised));
            if (unrecognised[0] != '\0')
            {
                LOG(LOG_LEVEL_WARNING,
                    "Unrecognised tokens parsing 'RestrictInboundClipboard' %s",
                    unrecognised);
            }
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
    const char *buf;
    const char *value;

    list_clear(param_v);
    list_clear(param_n);

    /* setting defaults */
    se->x11_display_offset = 10;
    se->max_sessions = 0;
    se->max_idle_time = 0;
    se->max_disc_time = 0;
    se->kill_disconnected = 0;
    se->policy = SESMAN_CFG_SESS_POLICY_DEFAULT;

    file_read_section(file, SESMAN_CFG_SESSIONS, param_n, param_v);

    for (i = 0; i < param_n->count; i++)
    {
        buf = (const char *)list_get_item(param_n, i);
        value = (const char *)list_get_item(param_v, i);

        if (0 == g_strcasecmp(buf, SESMAN_CFG_SESS_X11DISPLAYOFFSET))
        {
            se->x11_display_offset = g_atoi(value);
        }

        else if (0 == g_strcasecmp(buf, SESMAN_CFG_SESS_MAX))
        {
            se->max_sessions = g_atoi(value);
        }

        else if (0 == g_strcasecmp(buf, SESMAN_CFG_SESS_KILL_DISC))
        {
            se->kill_disconnected = g_text2bool(value);
        }

        else if (0 == g_strcasecmp(buf, SESMAN_CFG_SESS_IDLE_LIMIT))
        {
            se->max_idle_time = g_atoi(value);
        }

        else if (0 == g_strcasecmp(buf, SESMAN_CFG_SESS_DISC_LIMIT))
        {
            se->max_disc_time = g_atoi(value);
        }

        else if (0 == g_strcasecmp(buf, SESMAN_CFG_SESS_POLICY_S))
        {
            se->policy = parse_policy_string(value);
        }
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
        list_add_strdup(cs->xorg_params,
                        (const char *) list_get_item(param_v, i));
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
        list_add_strdup(cs->vnc_params,
                        (const char *)list_get_item(param_v, i));
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
        list_add_strdup(cs->env_names,
                        (const char *) list_get_item(param_n, i));
        list_add_strdup(cs->env_values,
                        (const char *) list_get_item(param_v, i));
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
                all_ok = 1;

                /* read global config */
                config_read_globals(fd, cfg, param_n, param_v);

                /* read Xvnc/Xorg parameter list */
                config_read_vnc_params(fd, cfg, param_n, param_v);
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
    char policy_s[64];

    /* Global sesman configuration */
    g_writeln("Filename:                     %s", config->sesman_ini);
    g_writeln("Global configuration:");
    g_writeln("    ListenPort:               %s", config->listen_port);
    g_writeln("    EnableUserWindowManager:  %d", config->enable_user_wm);
    g_writeln("    UserWindowManager:        %s", config->user_wm);
    g_writeln("    DefaultWindowManager:     %s", config->default_wm);
    g_writeln("    ReconnectScript:          %s", config->reconnect_sh);
    g_writeln("    AuthFilePath:             %s",
              (config->auth_file_path ? config->auth_file_path : "disabled"));

    /* Session configuration */
    config_output_policy_string(se->policy, policy_s, sizeof(policy_s));

    g_writeln("Session configuration:");
    g_writeln("    MaxSessions:              %d", se->max_sessions);
    g_writeln("    X11DisplayOffset:         %d", se->x11_display_offset);
    g_writeln("    KillDisconnected:         %d", se->kill_disconnected);
    g_writeln("    IdleTimeLimit:            %d", se->max_idle_time);
    g_writeln("    DisconnectedTimeLimit:    %d", se->max_disc_time);
    g_writeln("    Policy:                   %s", policy_s);

    /* Security configuration */
    g_writeln("Security configuration:");
    g_writeln("    AllowRootLogin:            %d", sc->allow_root);
    g_writeln("    MaxLoginRetry:             %d", sc->login_retry);
    g_writeln("    AlwaysGroupCheck:          %d", sc->ts_always_group_check);
    if (sc->restrict_outbound_clipboard == CLIP_RESTRICT_NONE)
    {
        g_writeln("    RestrictOutboundClipboard: %s", "none");
    }
    else if (sc->restrict_outbound_clipboard == CLIP_RESTRICT_ALL)
    {
        g_writeln("    RestrictOutboundClipboard: %s", "all");
    }
    else
    {
        char buf[256];
        g_bitmask_to_str(sc->restrict_outbound_clipboard,
                         clip_restrict_map, ',', buf, sizeof(buf));
        g_writeln("    RestrictOutboundClipboard: %s", buf);
    }
    if (sc->restrict_inbound_clipboard == CLIP_RESTRICT_NONE)
    {
        g_writeln("    RestrictInboundClipboard:  %s", "none");
    }
    else if (sc->restrict_inbound_clipboard == CLIP_RESTRICT_ALL)
    {
        g_writeln("    RestrictInboundClipboard:  %s", "all");
    }
    else
    {
        char buf[256];
        g_bitmask_to_str(sc->restrict_inbound_clipboard,
                         clip_restrict_map, ',', buf, sizeof(buf));
        g_writeln("    RestrictInboundClipboard:  %s", buf);
    }

    g_printf( "    TSUsersGroup:              ");
    if (sc->ts_users_enable)
    {
        g_printf("%d", sc->ts_users);
    }
    else
    {
        g_printf("(not defined)");
    }
    g_writeln("%s", "");

    g_printf( "    TSAdminsGroup:             ");
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
        list_delete(cs->vnc_params);
        list_delete(cs->xorg_params);
        list_delete(cs->env_names);
        list_delete(cs->env_values);
        g_free(cs);
    }
}
