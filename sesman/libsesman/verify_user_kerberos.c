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
 * @file verify_user_kerberos.c
 * @brief Authenticate user using kerberos
 * @author Jay Sorg
 *
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "arch.h"
#include "sesman_auth.h"
#include "os_calls.h"
#include "string_calls.h"
#include "log.h"

#include <krb5.h>

struct auth_info
{
    krb5_context ctx;
    krb5_ccache cc;
    krb5_principal me;
};

/******************************************************************************/
/* Logs a kerberos error code */
static void
log_kerberos_failure(krb5_context ctx, krb5_error_code code, const char *where)
{
    const char *errstr = krb5_get_error_message(ctx, code);
    LOG(LOG_LEVEL_ERROR, "Kerberos call to %s failed [%s]", where, errstr);
    krb5_free_error_message(ctx, errstr);
}

/******************************************************************************/
int
auth_end(struct auth_info *auth_info)
{
    if (auth_info != NULL)
    {
        if (auth_info->me)
        {
            krb5_free_principal(auth_info->ctx, auth_info->me);
        }

        if (auth_info->cc)
        {
            krb5_cc_close(auth_info->ctx, auth_info->cc);
        }

        if (auth_info->ctx)
        {
            krb5_free_context(auth_info->ctx);
        }

        g_memset(auth_info, 0, sizeof(*auth_info));
        g_free(auth_info);
    }
    return 0;
}

/******************************************************************************/
/* Checks Kerberos can be used
 *
 * If all is well, an auth_info struct is returned */
static struct auth_info *
k5_begin(const char *username)
{
    int ok = 0;
    struct auth_info *auth_info = g_new0(struct auth_info, 1);
    krb5_error_code code;

    if (auth_info == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "Out of memory in k5_begin()");
    }
    else if ((code = krb5_init_context(&auth_info->ctx)) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "Can't init Kerberos context");
    }
    /* Determine the credentials cache to use */
    else if ((code = krb5_cc_default(auth_info->ctx, &auth_info->cc)) != 0)
    {
        log_kerberos_failure(auth_info->ctx, code, "krb5_cc_default");
    }
    /* Parse the username into a full principal */
    else if ((code = krb5_parse_name(auth_info->ctx,
                                     username, &auth_info->me)) != 0)
    {
        log_kerberos_failure(auth_info->ctx, code, "krb5_parse_name");
    }
    else
    {
        ok = 1;
    }

    if (!ok)
    {
        auth_end(auth_info);
        auth_info = NULL;
    }

    return auth_info;
}


/******************************************************************************/
/* returns boolean */
static enum scp_login_status
k5_kinit(struct auth_info *auth_info, const char *password)
{
    enum scp_login_status status = E_SCP_LOGIN_GENERAL_ERROR;
    krb5_creds my_creds;
    krb5_error_code code = 0;

    code = krb5_get_init_creds_password(auth_info->ctx,
                                        &my_creds, auth_info->me,
                                        password, NULL, NULL,
                                        0,
                                        NULL,
                                        NULL);
    if (code != 0)
    {
        log_kerberos_failure(auth_info->ctx, code,
                             "krb5_get_init_creds_password");
        status = E_SCP_LOGIN_NOT_AUTHENTICATED;
    }
    else
    {
        /*
         * Try to store the creds in the credentials cache
         */
        if ((code = krb5_cc_initialize(auth_info->ctx, auth_info->cc,
                                       auth_info->me)) != 0)
        {
            log_kerberos_failure(auth_info->ctx, code, "krb5_cc_initialize");
        }
        else if ((code = krb5_cc_store_cred(auth_info->ctx, auth_info->cc,
                                            &my_creds)) != 0)
        {
            log_kerberos_failure(auth_info->ctx, code, "krb5_cc_store_cred");
        }
        else
        {
            status = E_SCP_LOGIN_OK;
        }

        /* Prevent double-free of the client principal */
        if (my_creds.client == auth_info->me)
        {
            my_creds.client = NULL;
        }

        krb5_free_cred_contents(auth_info->ctx, &my_creds);
    }

    return status;
}

/******************************************************************************/
/* returns non-NULL for success */
struct auth_info *
auth_userpass(const char *user, const char *pass,
              const char *client_ip, enum scp_login_status *errorcode)
{
    enum scp_login_status status = E_SCP_LOGIN_GENERAL_ERROR;
    struct auth_info *auth_info = k5_begin(user);

    if (auth_info)
    {
        status = k5_kinit(auth_info, pass);
        if (status != E_SCP_LOGIN_OK)
        {
            auth_end(auth_info);
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
/* returns non-NULL for success */
struct auth_info *
auth_uds(const char *user, enum scp_login_status *errorcode)
{
    struct auth_info *auth_info = k5_begin(user);

    if (errorcode != NULL)
    {
        *errorcode =
            (auth_info != NULL) ? E_SCP_LOGIN_OK : E_SCP_LOGIN_GENERAL_ERROR;
    }

    return auth_info;
}

/******************************************************************************/
/* returns error */
int
auth_start_session(struct auth_info *auth_info, int display_num)
{
    return 0;
}

/******************************************************************************/
int
auth_set_env(struct auth_info *auth_info)
{
    return 0;
}
