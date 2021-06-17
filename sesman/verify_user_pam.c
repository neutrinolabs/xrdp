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
 * @file verify_user_pam.c
 * @brief Authenticate user using pam
 * @author Jay Sorg
 *
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "arch.h"
#include "os_calls.h"
#include "log.h"
#include "string_calls.h"

#include <stdio.h>
#include <security/pam_appl.h>

/* Defines the maximum size of a username or password. With pam there is no real limit */
#define MAX_BUF 8192

struct t_user_pass
{
    char user[MAX_BUF];
    char pass[MAX_BUF];
};

struct t_auth_info
{
    struct t_user_pass user_pass;
    int session_opened;
    int did_setcred;
    struct pam_conv pamc;
    pam_handle_t *ph;
};

/***************************************************************************//**
 * Returns a string representing a pam_conv message style
 *
 * @param msg_style PAM msg_style (pam_conv(3))
 * @param buff      Buffer for conversion of unrecognised values
 * @param bufflen   Total length of above
 *
 * The buffer described by buff is only written to if required.
 */
static const char *
msg_style_to_str(int msg_style, char *buff, unsigned int bufflen)
{
    const char *result;
    switch (msg_style)
    {
        case PAM_PROMPT_ECHO_OFF:
            result = "PAM_PROMPT_ECHO_OFF";
            break;

        case PAM_PROMPT_ECHO_ON:
            result = "PAM_PROMPT_ECHO_ON";
            break;

        case PAM_ERROR_MSG:
            result = "PAM_ERROR_MSG";
            break;

        case PAM_TEXT_INFO:
            result = "PAM_TEXT_INFO";
            break;

        default:
            g_snprintf(buff, bufflen, "UNKNOWN_0x%x", msg_style);
            result = buff;
    }

    return result;
}

/***************************************************************************//**
 * Provides the PAM conversation callback function
 *
 * At present, the main purpose of this function is to supply the
 * user's password to the PAM stack, although some module logging is
 * implemented here.
 *
 * @param[in] num_msg Count of messages in the msg array
 * @param[in] msg Messages from the PAM stack to the application
 * @param[out] resp Message replies from the application to the PAM stack
 * @param[in] appdata_ptr Used to pass in a struct t_user_pass pointer
 *
 * @result PAM_SUCCESS if the messages were all processed successfully.
 *
 * @post If PAM_SUCCESS is returned, resp and its contents are allocated here
 *       and must be freed by the caller
 * @post If PAM_SUCCESS is not returned, resp is not allocated and must not
 *       be not freed by the caller
 *
 * @note See pam_conv(3) for more information
 * @note A basic example conversation function can be found in OSF RFC
         86.0 (1995)
 */

static int
verify_pam_conv(int num_msg, const struct pam_message **msg,
                struct pam_response **resp, void *appdata_ptr)
{
    int i;
    struct pam_response *reply = NULL;
    struct t_user_pass *user_pass;
    char sb[64];
    int rv = PAM_SUCCESS;

    if (num_msg <= 0 || num_msg > PAM_MAX_NUM_MSG)
    {
        rv = PAM_CONV_ERR;
    }
    else if ((reply = g_new0(struct pam_response, num_msg)) == NULL)
    {
        rv = PAM_BUF_ERR;
    }
    else
    {
        for (i = 0; i < num_msg && rv == PAM_SUCCESS; i++)
        {
            LOG_DEVEL(LOG_LEVEL_INFO, "Handling struct pam_message"
                      " { style = %s, msg = \"%s\" }",
                      msg_style_to_str(msg[i]->msg_style, sb, sizeof (sb)),
                      msg[i]->msg == NULL ? "<null>" : msg[i]->msg);

            switch (msg[i]->msg_style)
            {
                case PAM_PROMPT_ECHO_OFF: /* password */
                    user_pass = (struct t_user_pass *) appdata_ptr;
                    reply[i].resp = g_strdup(user_pass->pass);
                    break;

                case PAM_ERROR_MSG:
                    LOG(LOG_LEVEL_ERROR, "PAM: %s", msg[i]->msg);
                    break;

                case PAM_TEXT_INFO:
                    LOG(LOG_LEVEL_INFO, "PAM: %s", msg[i]->msg);
                    break;

                default:
                {
                    LOG(LOG_LEVEL_ERROR, "Unhandled message in verify_pam_conv"
                        " { style = %s, msg = \"%s\" }",
                        msg_style_to_str(msg[i]->msg_style, sb, sizeof (sb)),
                        msg[i]->msg == NULL ? "<null>" : msg[i]->msg);
                    rv = PAM_CONV_ERR;
                }
            }
        }
    }

    if (rv == PAM_SUCCESS)
    {
        *resp = reply;
    }
    else if (reply != NULL)
    {
        for (i = 0; i < num_msg; i++)
        {
            if (reply[i].resp != NULL)
            {
                g_free(reply[i].resp);
            }
        }
        g_free(reply);
    }

    return rv;
}

/******************************************************************************/
static void
get_service_name(char *service_name)
{
    service_name[0] = 0;

    if (g_file_exist("/etc/pam.d/xrdp-sesman") ||
            g_file_exist(XRDP_SYSCONF_PATH "/pam.d/xrdp-sesman"))
    {
        g_strncpy(service_name, "xrdp-sesman", 255);
    }
    else
    {
        g_strncpy(service_name, "gdm", 255);
    }
}

/******************************************************************************/
/* returns long, zero is no go
 Stores the detailed error code in the errorcode variable*/

long
auth_userpass(const char *user, const char *pass, int *errorcode)
{
    int error;
    struct t_auth_info *auth_info;
    char service_name[256];

    get_service_name(service_name);
    auth_info = g_new0(struct t_auth_info, 1);
    g_strncpy(auth_info->user_pass.user, user, MAX_BUF - 1);
    g_strncpy(auth_info->user_pass.pass, pass, MAX_BUF - 1);
    auth_info->pamc.conv = &verify_pam_conv;
    auth_info->pamc.appdata_ptr = &(auth_info->user_pass);
    error = pam_start(service_name, user, &(auth_info->pamc), &(auth_info->ph));

    if (error != PAM_SUCCESS)
    {
        if (errorcode != NULL)
        {
            *errorcode = error;
        }
        LOG(LOG_LEVEL_ERROR, "pam_start failed: %s",
            pam_strerror(auth_info->ph, error));
        pam_end(auth_info->ph, error);
        g_free(auth_info);
        return 0;
    }

    error = pam_set_item(auth_info->ph, PAM_TTY, service_name);
    if (error != PAM_SUCCESS)
    {
        LOG(LOG_LEVEL_ERROR, "pam_set_item failed: %s",
            pam_strerror(auth_info->ph, error));
    }

    error = pam_authenticate(auth_info->ph, 0);

    if (error != PAM_SUCCESS)
    {
        if (errorcode != NULL)
        {
            *errorcode = error;
        }
        LOG(LOG_LEVEL_ERROR, "pam_authenticate failed: %s",
            pam_strerror(auth_info->ph, error));
        pam_end(auth_info->ph, error);
        g_free(auth_info);
        return 0;
    }
    /* From man page:
       The pam_acct_mgmt function is used to determine if the users account is
       valid. It checks for authentication token and account expiration and
       verifies access restrictions. It is typically called after the user has
       been authenticated.
     */
    error = pam_acct_mgmt(auth_info->ph, 0);

    if (error != PAM_SUCCESS)
    {
        if (errorcode != NULL)
        {
            *errorcode = error;
        }
        LOG(LOG_LEVEL_ERROR, "pam_acct_mgmt failed: %s",
            pam_strerror(auth_info->ph, error));
        pam_end(auth_info->ph, error);
        g_free(auth_info);
        return 0;
    }

    return (long)auth_info;
}

/******************************************************************************/
/* returns error */
int
auth_start_session(long in_val, int in_display)
{
    struct t_auth_info *auth_info;
    int error;
    char display[256];

    g_sprintf(display, ":%d", in_display);
    auth_info = (struct t_auth_info *)in_val;
    error = pam_set_item(auth_info->ph, PAM_TTY, display);

    if (error != PAM_SUCCESS)
    {
        LOG(LOG_LEVEL_ERROR, "pam_set_item failed: %s",
            pam_strerror(auth_info->ph, error));
        return 1;
    }

    error = pam_setcred(auth_info->ph, PAM_ESTABLISH_CRED);

    if (error != PAM_SUCCESS)
    {
        LOG(LOG_LEVEL_ERROR, "pam_setcred failed: %s",
            pam_strerror(auth_info->ph, error));
        return 1;
    }

    auth_info->did_setcred = 1;
    error = pam_open_session(auth_info->ph, 0);

    if (error != PAM_SUCCESS)
    {
        LOG(LOG_LEVEL_ERROR, "pam_open_session failed: %s",
            pam_strerror(auth_info->ph, error));
        return 1;
    }

    auth_info->session_opened = 1;
    return 0;
}

/******************************************************************************/
/* returns error */
int
auth_stop_session(long in_val)
{
    struct t_auth_info *auth_info;
    int error;

    auth_info = (struct t_auth_info *)in_val;
    error = pam_close_session(auth_info->ph, 0);
    if (error != PAM_SUCCESS)
    {
        LOG(LOG_LEVEL_ERROR, "pam_close_session failed: %s",
            pam_strerror(auth_info->ph, error));
        return 1;
    }
    auth_info->session_opened = 0;
    return 0;
}

/******************************************************************************/
/* returns error */
/* cleanup */
int
auth_end(long in_val)
{
    struct t_auth_info *auth_info;

    auth_info = (struct t_auth_info *)in_val;

    if (auth_info != 0)
    {
        if (auth_info->ph != 0)
        {
            if (auth_info->session_opened)
            {
                pam_close_session(auth_info->ph, 0);
            }

            if (auth_info->did_setcred)
            {
                pam_setcred(auth_info->ph, PAM_DELETE_CRED);
            }

            pam_end(auth_info->ph, PAM_SUCCESS);
            auth_info->ph = 0;
        }
    }

    g_free(auth_info);
    return 0;
}

/******************************************************************************/
/* returns error */
/* set any pam env vars */
int
auth_set_env(long in_val)
{
    struct t_auth_info *auth_info;
    char **pam_envlist;
    char **pam_env;
    char item[256];
    char value[256];
    int eq_pos;

    auth_info = (struct t_auth_info *)in_val;

    if (auth_info != 0)
    {
        /* export PAM environment */
        pam_envlist = pam_getenvlist(auth_info->ph);

        if (pam_envlist != NULL)
        {
            for (pam_env = pam_envlist; *pam_env != NULL; ++pam_env)
            {
                eq_pos = g_pos(*pam_env, "=");

                if (eq_pos >= 0 && eq_pos < 250)
                {
                    g_strncpy(item, *pam_env, eq_pos);
                    g_strncpy(value, (*pam_env) + eq_pos + 1, 255);
                    g_setenv(item, value, 1);
                }

                g_free(*pam_env);
            }

            g_free(pam_envlist);
        }
    }

    return 0;
}
