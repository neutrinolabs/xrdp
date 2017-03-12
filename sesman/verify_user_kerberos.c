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
#include "os_calls.h"

#include <krb5.h>

typedef enum { INIT_PW, INIT_KT, RENEW, VALIDATE } action_type;

struct k_opts
{
    /* in seconds */
    krb5_deltat starttime;
    krb5_deltat lifetime;
    krb5_deltat rlife;

    int forwardable;
    int proxiable;
    int addresses;

    int not_forwardable;
    int not_proxiable;
    int no_addresses;

    int verbose;

    char *principal_name;
    char *service_name;
    char *keytab_name;
    char *k5_cache_name;
    char *k4_cache_name;

    action_type action;
};

struct k5_data
{
    krb5_context ctx;
    krb5_ccache cc;
    krb5_principal me;
    char *name;
};

struct user_info
{
    const char *name;
    const char *pass;
};

/******************************************************************************/
/* returns boolean */
static int
k5_begin(struct k_opts *opts, struct k5_data *k5, struct user_info *u_info)
{
    krb5_error_code code = 0;

    code = krb5_init_context(&k5->ctx);

    if (code != 0)
    {
        g_printf("krb5_init_context failed in k5_begin\n");
        return 0;
    }

    if (opts->k5_cache_name)
    {
        code = krb5_cc_resolve(k5->ctx, opts->k5_cache_name, &k5->cc);

        if (code != 0)
        {
            g_printf("krb5_cc_resolve failed in k5_begin\n");
            return 0;
        }
    }
    else
    {
        code = krb5_cc_default(k5->ctx, &k5->cc);

        if (code != 0)
        {
            g_printf("krb5_cc_default failed in k5_begin\n");
            return 0;
        }
    }

    if (opts->principal_name)
    {
        /* Use specified name */
        code = krb5_parse_name(k5->ctx, opts->principal_name, &k5->me);

        if (code != 0)
        {
            g_printf("krb5_parse_name failed in k5_begin\n");
            return 0;
        }
    }
    else
    {
        /* No principal name specified */
        if (opts->action == INIT_KT)
        {
            /* Use the default host/service name */
            code = krb5_sname_to_principal(k5->ctx, NULL, NULL,
                                           KRB5_NT_SRV_HST, &k5->me);

            if (code != 0)
            {
                g_printf("krb5_sname_to_principal failed in k5_begin\n");
                return 0;
            }
        }
        else
        {
            /* Get default principal from cache if one exists */
            code = krb5_cc_get_principal(k5->ctx, k5->cc, &k5->me);

            if (code != 0)
            {
                code = krb5_parse_name(k5->ctx, u_info->name, &k5->me);

                if (code != 0)
                {
                    g_printf("krb5_parse_name failed in k5_begin\n");
                    return 0;
                }
            }
        }
    }

    code = krb5_unparse_name(k5->ctx, k5->me, &k5->name);

    if (code != 0)
    {
        g_printf("krb5_unparse_name failed in k5_begin\n");
        return 0;
    }

    opts->principal_name = k5->name;
    return 1;
}

/******************************************************************************/
static void
k5_end(struct k5_data *k5)
{
    if (k5->name)
    {
        krb5_free_unparsed_name(k5->ctx, k5->name);
    }

    if (k5->me)
    {
        krb5_free_principal(k5->ctx, k5->me);
    }

    if (k5->cc)
    {
        krb5_cc_close(k5->ctx, k5->cc);
    }

    if (k5->ctx)
    {
        krb5_free_context(k5->ctx);
    }

    g_memset(k5, 0, sizeof(struct k5_data));
}

/******************************************************************************/
static krb5_error_code KRB5_CALLCONV
kinit_prompter(krb5_context ctx, void *data, const char *name,
               const char *banner, int num_prompts, krb5_prompt prompts[])
{
    int i;
    krb5_prompt_type *types;
    krb5_error_code rc;
    struct user_info *u_info;

    u_info = (struct user_info *)data;
    rc = 0;
    types = krb5_get_prompt_types(ctx);

    for (i = 0; i < num_prompts; i++)
    {
        if (types[i] == KRB5_PROMPT_TYPE_PASSWORD ||
                types[i] == KRB5_PROMPT_TYPE_NEW_PASSWORD_AGAIN)
        {
            g_strncpy(prompts[i].reply->data, u_info->pass, 255);
        }
    }

    return rc;
}

/******************************************************************************/
/* returns boolean */
static int
k5_kinit(struct k_opts *opts, struct k5_data *k5, struct user_info *u_info)
{
    const char *doing;
    int notix = 1;
    krb5_keytab keytab = 0;
    krb5_creds my_creds;
    krb5_error_code code = 0;
    krb5_get_init_creds_opt options;
    krb5_address **addresses;

    krb5_get_init_creds_opt_init(&options);
    g_memset(&my_creds, 0, sizeof(my_creds));

    /*
      From this point on, we can goto cleanup because my_creds is
      initialized.
    */
    if (opts->lifetime)
    {
        krb5_get_init_creds_opt_set_tkt_life(&options, opts->lifetime);
    }

    if (opts->rlife)
    {
        krb5_get_init_creds_opt_set_renew_life(&options, opts->rlife);
    }

    if (opts->forwardable)
    {
        krb5_get_init_creds_opt_set_forwardable(&options, 1);
    }

    if (opts->not_forwardable)
    {
        krb5_get_init_creds_opt_set_forwardable(&options, 0);
    }

    if (opts->proxiable)
    {
        krb5_get_init_creds_opt_set_proxiable(&options, 1);
    }

    if (opts->not_proxiable)
    {
        krb5_get_init_creds_opt_set_proxiable(&options, 0);
    }

    if (opts->addresses)
    {
        addresses = NULL;
        code = krb5_os_localaddr(k5->ctx, &addresses);

        if (code != 0)
        {
            g_printf("krb5_os_localaddr failed in k5_kinit\n");
            goto cleanup;
        }

        krb5_get_init_creds_opt_set_address_list(&options, addresses);
    }

    if (opts->no_addresses)
    {
        krb5_get_init_creds_opt_set_address_list(&options, NULL);
    }

    if ((opts->action == INIT_KT) && opts->keytab_name)
    {
        code = krb5_kt_resolve(k5->ctx, opts->keytab_name, &keytab);

        if (code != 0)
        {
            g_printf("krb5_kt_resolve failed in k5_kinit\n");
            goto cleanup;
        }
    }

    switch (opts->action)
    {
        case INIT_PW:
            code = krb5_get_init_creds_password(k5->ctx, &my_creds, k5->me,
                                                0, kinit_prompter, u_info,
                                                opts->starttime,
                                                opts->service_name,
                                                &options);
            break;
        case INIT_KT:
            code = krb5_get_init_creds_keytab(k5->ctx, &my_creds, k5->me,
                                              keytab,
                                              opts->starttime,
                                              opts->service_name,
                                              &options);
            break;
        case VALIDATE:
            code = krb5_get_validated_creds(k5->ctx, &my_creds, k5->me, k5->cc,
                                            opts->service_name);
            break;
        case RENEW:
            code = krb5_get_renewed_creds(k5->ctx, &my_creds, k5->me, k5->cc,
                                          opts->service_name);
            break;
    }

    if (code != 0)
    {
        doing = 0;

        switch (opts->action)
        {
            case INIT_PW:
            case INIT_KT:
                doing = "getting initial credentials";
                break;
            case VALIDATE:
                doing = "validating credentials";
                break;
            case RENEW:
                doing = "renewing credentials";
                break;
        }

        if (code == KRB5KRB_AP_ERR_BAD_INTEGRITY)
        {
            g_printf("sesman: Password incorrect while %s in k5_kinit\n", doing);
        }
        else
        {
            g_printf("sesman: error while %s in k5_kinit\n", doing);
        }

        goto cleanup;
    }

    if (!opts->lifetime)
    {
        /* We need to figure out what lifetime to use for Kerberos 4. */
        opts->lifetime = my_creds.times.endtime - my_creds.times.authtime;
    }

    code = krb5_cc_initialize(k5->ctx, k5->cc, k5->me);

    if (code != 0)
    {
        g_printf("krb5_cc_initialize failed in k5_kinit\n");
        goto cleanup;
    }

    code = krb5_cc_store_cred(k5->ctx, k5->cc, &my_creds);

    if (code != 0)
    {
        g_printf("krb5_cc_store_cred failed in k5_kinit\n");
        goto cleanup;
    }

    notix = 0;

cleanup:

    if (my_creds.client == k5->me)
    {
        my_creds.client = 0;
    }

    krb5_free_cred_contents(k5->ctx, &my_creds);

    if (keytab)
    {
        krb5_kt_close(k5->ctx, keytab);
    }

    return notix ? 0 : 1;
}

/******************************************************************************/
/* returns boolean */
int
auth_userpass(const char *user, const char *pass, int *errorcode)
{
    struct k_opts opts;
    struct k5_data k5;
    struct user_info u_info;
    int got_k5;
    int authed_k5;

    g_memset(&opts, 0, sizeof(opts));
    opts.action = INIT_PW;
    g_memset(&k5, 0, sizeof(k5));
    g_memset(&u_info, 0, sizeof(u_info));
    u_info.name = user;
    u_info.pass = pass;
    authed_k5 = 0;
    got_k5 = k5_begin(&opts, &k5, &u_info);

    if (got_k5)
    {
        authed_k5 = k5_kinit(&opts, &k5, &u_info);
        k5_end(&k5);
    }

    return authed_k5;
}

/******************************************************************************/
/* returns error */
int
auth_start_session(long in_val, int in_display)
{
    return 0;
}

/******************************************************************************/
/* returns error */
int
auth_stop_session(long in_val)
{
    return 0;
}

/******************************************************************************/
int
auth_end(long in_val)
{
    return 0;
}

/******************************************************************************/
int
auth_set_env(long in_val)
{
    return 0;
}
