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
#include "sesman_auth.h"

#include <stdio.h>
#include <security/pam_appl.h>

/* Allows the conversation function to find required items */
struct conv_func_data
{
    const char *pass;
};

struct auth_info
{
    int session_opened;
    int did_setcred;
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
 * @param[in] appdata_ptr Used to pass in a struct conv_func_data pointer
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
    struct conv_func_data *conv_func_data;
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
                    conv_func_data = (struct conv_func_data *) appdata_ptr;
                    /* Check this function isn't being called
                     * later than we expected */
                    if (conv_func_data == NULL || conv_func_data->pass == NULL)
                    {
                        LOG(LOG_LEVEL_ERROR,
                            "verify_pam_conv: Password unavailable");
                        reply[i].resp = g_strdup("????");
                    }
                    else
                    {
                        reply[i].resp = g_strdup(conv_func_data->pass);
                    }
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
#ifdef __LINUX_PAM__
            /* /usr/lib/pam.d is hardcoded into Linux-PAM */
            g_file_exist("/usr/lib/pam.d/xrdp-sesman") ||
#endif
#ifdef OPENPAM_VERSION
            /* /usr/local/etc/pam.d is hardcoded into OpenPAM */
            g_file_exist("/usr/local/etc/pam.d/xrdp-sesman") ||
#endif
            g_file_exist(XRDP_PAMCONF_PATH "/xrdp-sesman"))
    {
        g_strncpy(service_name, "xrdp-sesman", 255);
    }
    else
    {
        g_strncpy(service_name, "gdm", 255);
    }
}

/******************************************************************************/

/** Performs PAM operations common to login methods
 *
 * @param auth_info Module auth_info structure
 * @param user User name
 * @param pass Password, if needed for authentication.
 * @param client_ip Client IP if known, or NULL
 * @param authentication_required True if user must be authenticated
 *
 * For a UDS connection, the user can be assumed to be authenticated,
 * so in this instance authentication_required can be false.
 *
 * @return Code describing the success of the operation
 */
static enum scp_login_status
common_pam_login(struct auth_info *auth_info,
                 const char *user,
                 const char *pass,
                 const char *client_ip,
                 int authentication_required)
{
    int perror;
    char service_name[256];
    struct conv_func_data conv_func_data;
    struct pam_conv pamc;

    /*
     * Set up the data required by the conversation function, and the
     * structure which allows us to pass this to pam_start()
     */
    conv_func_data.pass = (authentication_required) ? pass : NULL;
    pamc.conv = verify_pam_conv;
    pamc.appdata_ptr = (void *) &conv_func_data;

    get_service_name(service_name);
    perror = pam_start(service_name, user, &pamc, &(auth_info->ph));

    if (perror != PAM_SUCCESS)
    {
        LOG(LOG_LEVEL_ERROR, "pam_start failed: %s",
            pam_strerror(auth_info->ph, perror));
        pam_end(auth_info->ph, perror);
        return E_SCP_LOGIN_GENERAL_ERROR;
    }

    if (client_ip != NULL && client_ip[0] != '\0')
    {
        perror = pam_set_item(auth_info->ph, PAM_RHOST, client_ip);
        if (perror != PAM_SUCCESS)
        {
            LOG(LOG_LEVEL_ERROR, "pam_set_item(PAM_RHOST) failed: %s",
                pam_strerror(auth_info->ph, perror));
        }
    }

    perror = pam_set_item(auth_info->ph, PAM_TTY, service_name);
    if (perror != PAM_SUCCESS)
    {
        LOG(LOG_LEVEL_ERROR, "pam_set_item(PAM_TTY) failed: %s",
            pam_strerror(auth_info->ph, perror));
    }

    if (authentication_required)
    {
        perror = pam_authenticate(auth_info->ph, 0);

        if (perror != PAM_SUCCESS)
        {
            LOG(LOG_LEVEL_ERROR, "pam_authenticate failed: %s",
                pam_strerror(auth_info->ph, perror));
            pam_end(auth_info->ph, perror);
            return E_SCP_LOGIN_NOT_AUTHENTICATED;
        }
    }
    /* From man page:
       The pam_acct_mgmt function is used to determine if the users account is
       valid. It checks for authentication token and account expiration and
       verifies access restrictions. It is typically called after the user has
       been authenticated.
     */
    perror = pam_acct_mgmt(auth_info->ph, 0);

    if (perror != PAM_SUCCESS)
    {
        LOG(LOG_LEVEL_ERROR, "pam_acct_mgmt failed: %s",
            pam_strerror(auth_info->ph, perror));
        pam_end(auth_info->ph, perror);
        return E_SCP_LOGIN_NOT_AUTHORIZED;
    }

    /* Set the appdata_ptr passed to the conversation function to
     * NULL, as the existing value is going out of scope */
    pamc.appdata_ptr = NULL;
    perror = pam_set_item(auth_info->ph, PAM_CONV, &pamc);
    if (perror != PAM_SUCCESS)
    {
        LOG(LOG_LEVEL_ERROR, "pam_set_item(PAM_CONV) failed: %s",
            pam_strerror(auth_info->ph, perror));
    }

    return E_SCP_LOGIN_OK;
}


/******************************************************************************/
/* returns non-NULL for success
 * Detailed error code is in the errorcode variable */

struct auth_info *
auth_userpass(const char *user, const char *pass,
              const char *client_ip, enum scp_login_status *errorcode)
{
    struct auth_info *auth_info;
    enum scp_login_status status;

    auth_info = g_new0(struct auth_info, 1);
    if (auth_info == NULL)
    {
        status = E_SCP_LOGIN_NO_MEMORY;
    }
    else
    {
        status = common_pam_login(auth_info, user, pass, client_ip, 1);

        if (status != E_SCP_LOGIN_OK)
        {
            g_free(auth_info);
            auth_info = NULL;
        }
    }

    if (errorcode != NULL)
    {
        *errorcode = status;
    }

    return auth_info;
}

/******************************************************************************/

struct auth_info *
auth_uds(const char *user, enum scp_login_status *errorcode)
{
    struct auth_info *auth_info;
    enum scp_login_status status;

    auth_info = g_new0(struct auth_info, 1);
    if (auth_info == NULL)
    {
        status = E_SCP_LOGIN_NO_MEMORY;
    }
    else
    {
        status = common_pam_login(auth_info, user, NULL, NULL, 0);

        if (status != E_SCP_LOGIN_OK)
        {
            g_free(auth_info);
            auth_info = NULL;
        }
    }

    if (errorcode != NULL)
    {
        *errorcode = status;
    }

    return auth_info;
}

/******************************************************************************/

/* returns error */
static int
auth_start_session_private(struct auth_info *auth_info, int display_num)
{
    int error;
    char display[256];

    g_sprintf(display, ":%d", display_num);
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
/**
 * Main routine to start a session
 *
 * Calls the private routine and logs an additional error if the private
 * routine fails
 */
int
auth_start_session(struct auth_info *auth_info, int display_num)
{
    int result = auth_start_session_private(auth_info, display_num);
    if (result != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "Can't start PAM session. See PAM logging for more info");
    }

    return result;
}

/******************************************************************************/
/* returns error */
static int
auth_stop_session(struct auth_info *auth_info)
{
    int rv = 0;
    int error;

    if (auth_info->session_opened)
    {
        error = pam_close_session(auth_info->ph, 0);
        if (error != PAM_SUCCESS)
        {
            LOG(LOG_LEVEL_ERROR, "pam_close_session failed: %s",
                pam_strerror(auth_info->ph, error));
            rv = 1;
        }
        else
        {
            auth_info->session_opened = 0;
        }
    }

    if (auth_info->did_setcred)
    {
        pam_setcred(auth_info->ph, PAM_DELETE_CRED);
        auth_info->did_setcred = 0;
    }

    return rv;
}

/******************************************************************************/
/* returns error */
/* cleanup */
int
auth_end(struct auth_info *auth_info)
{
    if (auth_info != NULL)
    {
        if (auth_info->ph != 0)
        {
            auth_stop_session(auth_info);

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
auth_set_env(struct auth_info *auth_info)
{
    char **pam_envlist;
    char **pam_env;

    if (auth_info != NULL)
    {
        /* export PAM environment */
        pam_envlist = pam_getenvlist(auth_info->ph);

        if (pam_envlist != NULL)
        {
            for (pam_env = pam_envlist; *pam_env != NULL; ++pam_env)
            {
                char *str = *pam_env;
                int eq_pos = g_pos(str, "=");

                if (eq_pos > 0)
                {
                    str[eq_pos] = '\0';
                    g_setenv(str, str + eq_pos + 1, 1);
                }

                g_free(str);
            }

            g_free(pam_envlist);
        }
    }

    return 0;
}
