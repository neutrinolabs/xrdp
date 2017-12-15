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

extern struct config_sesman *g_cfg; /* in sesman.c */



/******************************************************************************/
int
config_read(struct config_sesman *cfg)
{
    int fd;
    struct list *sec;
    struct list *param_n;
    struct list *param_v;
    char cfg_file[256];

    g_snprintf(cfg_file, 255, "%s/sesman.ini", XRDP_CFG_PATH);
    fd = g_file_open(cfg_file);

    if (-1 == fd)
    {
        //if (g_cfg->log.fd >= 0)
        //{
        /* logging is already active */
        log_message(LOG_LEVEL_ALWAYS, "error opening %s in \
                  config_read", cfg_file);
        //}
        //else
        //{
        g_printf("error opening %s in config_read", cfg_file);
        //}
        return 1;
    }

    g_memset(cfg, 0, sizeof(struct config_sesman));
    sec = list_create();
    sec->auto_free = 1;
    file_read_sections(fd, sec);
    param_n = list_create();
    param_n->auto_free = 1;
    param_v = list_create();
    param_v->auto_free = 1;

    /* read global config */
    config_read_globals(fd, cfg, param_n, param_v);

    /* read Xvnc/X11rdp/XOrg parameter list */
    config_read_vnc_params(fd, cfg, param_n, param_v);
    config_read_rdp_params(fd, cfg, param_n, param_v);
    config_read_xorg_params(fd, cfg, param_n, param_v);

    /* read logging config */
    // config_read_logging(fd, &(cfg->log), param_n, param_v);

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
    return 0;
}

/******************************************************************************/
int
config_read_globals(int file, struct config_sesman *cf, struct list *param_n,
                    struct list *param_v)
{
    int i;
    char *buf;

    list_clear(param_v);
    list_clear(param_n);

    /* resetting the struct */
    cf->listen_address[0] = '\0';
    cf->listen_port[0] = '\0';
    cf->enable_user_wm = 0;
    cf->user_wm[0] = '\0';
    cf->default_wm[0] = '\0';
    cf->auth_file_path = 0;

    file_read_section(file, SESMAN_CFG_GLOBALS, param_n, param_v);

    for (i = 0; i < param_n->count; i++)
    {
        buf = (char *)list_get_item(param_n, i);

        if (0 == g_strcasecmp(buf, SESMAN_CFG_DEFWM))
        {
            g_strncpy(cf->default_wm, (char *)list_get_item(param_v, i), 31);
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

    if ('\0' == cf->default_wm[0])
    {
        g_strncpy(cf->default_wm, "startwm.sh", 11);
    }

    /* showing read config */
    g_printf("sesman config:\r\n");
    g_printf("\tListenAddress:            %s\r\n", cf->listen_address);
    g_printf("\tListenPort:               %s\r\n", cf->listen_port);
    g_printf("\tEnableUserWindowManager:  %i\r\n", cf->enable_user_wm);
    g_printf("\tUserWindowManager:        %s\r\n", cf->user_wm);
    g_printf("\tDefaultWindowManager:     %s\r\n", cf->default_wm);
    g_printf("\tAuthFilePath:             %s\r\n", ((cf->auth_file_path) ? (cf->auth_file_path) : ("disabled")));

    return 0;
}

/******************************************************************************/
int
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
    }

    /* printing security config */
    g_printf("security configuration:\r\n");
    g_printf("\tAllowRootLogin:       %i\r\n", sc->allow_root);
    g_printf("\tMaxLoginRetry:        %i\r\n", sc->login_retry);
    g_printf("\tAlwaysGroupCheck:     %i\r\n", sc->ts_always_group_check);

    if (sc->ts_users_enable)
    {
        g_printf("\tTSUsersGroup:         %i\r\n", sc->ts_users);
    }
    else
    {
        g_printf("\tNo TSUsersGroup defined\r\n");
    }

    if (sc->ts_admins_enable)
    {
        g_printf("\tTSAdminsGroup:        %i\r\n", sc->ts_admins);
    }
    else
    {
        g_printf("\tNo TSAdminsGroup defined\r\n");
    }

    return 0;
}

/******************************************************************************/
int
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

    /* printing session config */
    g_printf("session configuration:\r\n");
    g_printf("\tMaxSessions:                 %i\r\n", se->max_sessions);
    g_printf("\tX11DisplayOffset:            %i\r\n", se->x11_display_offset);
    g_printf("\tKillDisconnected:            %i\r\n", se->kill_disconnected);
    g_printf("\tIdleTimeLimit:               %i\r\n", se->max_idle_time);
    g_printf("\tDisconnectedTimeLimit:       %i\r\n", se->max_disc_time);
    g_printf("\tPolicy:       %i\r\n", se->policy);

    return 0;
}

/******************************************************************************/
int
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

    /* printing X11rdp parameters */
    g_printf("X11rdp parameters:\r\n");

    for (i = 0; i < cs->rdp_params->count; i++)
    {
        g_printf("\tParameter %02d                   %s\r\n", i, (char *)list_get_item(cs->rdp_params, i));
    }

    return 0;
}

/******************************************************************************/
int
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

    /* printing XOrg parameters */
    g_printf("XOrg parameters:\r\n");

    for (i = 0; i < cs->xorg_params->count; i++)
    {
        g_printf("\tParameter %02d                   %s\r\n",
                 i, (char *) list_get_item(cs->xorg_params, i));
    }

    return 0;
}

/******************************************************************************/
int
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

    /* printing Xvnc parameters */
    g_printf("Xvnc parameters:\r\n");

    for (i = 0; i < cs->vnc_params->count; i++)
    {
        g_printf("\tParameter %02d                   %s\r\n", i, (char *)list_get_item(cs->vnc_params, i));
    }

    return 0;
}

/******************************************************************************/
int
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

    /* printing session variables */
    g_writeln("%s parameters:", SESMAN_CFG_SESSION_VARIABLES);

    for (i = 0; i < cs->env_names->count; i++)
    {
        g_writeln("  Parameter %02d                   %s=%s", i,
               (char *) list_get_item(cs->env_names, i),
               (char *) list_get_item(cs->env_values, i));
    }

    return 0;
}

void
config_free(struct config_sesman *cs)
{
    list_delete(cs->rdp_params);
    list_delete(cs->vnc_params);
    list_delete(cs->xorg_params);
    list_delete(cs->env_names);
    list_delete(cs->env_values);
    g_free(cs);
}
