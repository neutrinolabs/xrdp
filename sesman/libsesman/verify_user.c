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
 * @file verify_user.c
 * @brief Authenticate user using standard unix passwd/shadow system
 * @author Jay Sorg, Simone Fedele
 *
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "arch.h"
#include "sesman_auth.h"
#include "log.h"
#include "os_calls.h"
#include "string_calls.h"

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <crypt.h>
#include <shadow.h>
#include <pwd.h>

#ifndef SECS_PER_DAY
#define SECS_PER_DAY (24L*3600L)
#endif

static int
auth_crypt_pwd(const char *pwd, const char *pln, char *crp);

static int
auth_account_disabled(struct spwd *stp);

/*
 * Need a complete type for struct auth_info, even though we're
 * not really using it if this module (UNIX authentication) is selected */
struct auth_info
{
    char dummy;
};

/******************************************************************************/
/* returns non-NULL for success */
struct auth_info *
auth_userpass(const char *user, const char *pass,
              const char *client_ip, enum scp_login_status *errorcode)
{
    const char *encr = NULL;
    struct passwd *spw;

    /* Need a non-NULL pointer to return to indicate success */
    static struct auth_info success = {0};

    /* Most likely codepath return from here is 'not authenticated' */
    enum scp_login_status status = E_SCP_LOGIN_NOT_AUTHENTICATED;

    /* Find the encrypted password */
    if ((spw = getpwnam(user)) != NULL)
    {
        if (g_strncmp(spw->pw_passwd, "x", 3) == 0)
        {
            struct spwd *stp;

            /* the system is using shadow */
            if ((stp = getspnam(user)) == NULL)
            {
                LOG(LOG_LEVEL_ERROR, "Can't get shadow entry for account %s",
                    user);
                status = E_SCP_LOGIN_GENERAL_ERROR;
            }
            else
            {
                if (1 == auth_account_disabled(stp))
                {
                    LOG(LOG_LEVEL_INFO, "account %s is disabled", user);
                    status = E_SCP_LOGIN_NOT_AUTHORIZED;
                }
                else
                {
                    encr = stp->sp_pwdp;
                }
            }
        }
        else
        {
            /* old system with only passwd */
            encr = spw->pw_passwd;
        }
    }

    if (encr != NULL)
    {
        const char *epass;
        if ((epass = crypt(pass, encr)) != NULL &&
                g_strcmp(encr, epass) == 0)
        {
            status = E_SCP_LOGIN_OK;
        }
    }

    if (errorcode != NULL)
    {
        *errorcode = status;
    }

    return (status == E_SCP_LOGIN_OK) ? &success : NULL;
}

/******************************************************************************/

struct auth_info *
auth_uds(const char *user, enum scp_login_status *errorcode)
{
    struct passwd *spw;

    /* Need a non-NULL pointer to return to indicate success */
    static struct auth_info success = {0};

    enum scp_login_status status = E_SCP_LOGIN_OK;

    /* Try to check for a disabled account */
    if ((spw = getpwnam(user)) != NULL)
    {
        if (g_strncmp(spw->pw_passwd, "x", 3) == 0)
        {
            struct spwd *stp;

            /* the system is using shadow */
            if ((stp = getspnam(user)) == NULL)
            {
                LOG(LOG_LEVEL_ERROR, "Can't get shadow entry for account %s",
                    user);
                status = E_SCP_LOGIN_GENERAL_ERROR;
            }
            else
            {
                if (1 == auth_account_disabled(stp))
                {
                    LOG(LOG_LEVEL_INFO, "account %s is disabled", user);
                    status = E_SCP_LOGIN_NOT_AUTHORIZED;
                }
            }
        }
    }

    if (errorcode != NULL)
    {
        *errorcode = status;
    }

    return (status == E_SCP_LOGIN_OK) ? &success : NULL;
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
auth_end(struct auth_info *auth_info)
{
    return 0;
}

/******************************************************************************/
int
auth_set_env(struct auth_info *auth_info)
{
    return 0;
}

/******************************************************************************/
int
auth_check_pwd_chg(const char *user)
{
    struct passwd *spw;
    struct spwd *stp;
    int now;
    long today;

    spw = getpwnam(user);

    if (spw == 0)
    {
        return AUTH_PWD_CHG_ERROR;
    }

    if (g_strncmp(spw->pw_passwd, "x", 3) != 0)
    {
        /* old system with only passwd */
        return AUTH_PWD_CHG_OK;
    }

    /* the system is using shadow */
    stp = getspnam(user);

    if (stp == 0)
    {
        return AUTH_PWD_CHG_ERROR;
    }

    /* check if we need a pwd change */
    now = g_time1();
    today = now / SECS_PER_DAY;

    if (stp->sp_expire == -1)
    {
        return AUTH_PWD_CHG_OK;
    }

    if (today >= (stp->sp_lstchg + stp->sp_max - stp->sp_warn))
    {
        return AUTH_PWD_CHG_CHANGE;
    }

    if (today >= (stp->sp_lstchg + stp->sp_max))
    {
        return AUTH_PWD_CHG_CHANGE_MANDATORY;
    }

    if (today < ((stp->sp_lstchg) + (stp->sp_min)))
    {
        /* cannot change pwd for now */
        return AUTH_PWD_CHG_NOT_NOW;
    }

    return AUTH_PWD_CHG_OK;
}

int
auth_change_pwd(const char *user, const char *newpwd)
{
    struct passwd *spw;
    struct spwd *stp;
    char hash[35] = "";
    long today;

    FILE *fd;

    if (0 != lckpwdf())
    {
        return 1;
    }

    /* open passwd */
    spw = getpwnam(user);

    if (spw == 0)
    {
        return 1;
    }

    if (g_strncmp(spw->pw_passwd, "x", 3) != 0)
    {
        /* old system with only passwd */
        if (auth_crypt_pwd(spw->pw_passwd, newpwd, hash) != 0)
        {
            ulckpwdf();
            return 1;
        }

        spw->pw_passwd = g_strdup(hash);
        fd = fopen("/etc/passwd", "rw");
        putpwent(spw, fd);
    }
    else
    {
        /* the system is using shadow */
        stp = getspnam(user);

        if (stp == 0)
        {
            return 1;
        }

        /* old system with only passwd */
        if (auth_crypt_pwd(stp->sp_pwdp, newpwd, hash) != 0)
        {
            ulckpwdf();
            return 1;
        }

        stp->sp_pwdp = g_strdup(hash);
        today = g_time1() / SECS_PER_DAY;
        stp->sp_lstchg = today;
        stp->sp_expire = today + stp->sp_max + stp->sp_inact;
        fd = fopen("/etc/shadow", "rw");
        putspent(stp, fd);
    }

    ulckpwdf();
    return 0;
}

/**
 *
 * @brief Password encryption
 * @param pwd Old password
 * @param pln Plaintext new password
 * @param crp Crypted new password
 *
 */

static int
auth_crypt_pwd(const char *pwd, const char *pln, char *crp)
{
    char salt[13] = "$1$";
    int saltcnt = 0;
    char *encr;

    if (g_strncmp(pwd, "$1$", 3) == 0)
    {
        /* gnu style crypt(); */
        saltcnt = 3;

        while ((pwd[saltcnt] != '$') && (saltcnt < 11))
        {
            salt[saltcnt] = pwd[saltcnt];
            saltcnt++;
        }

        salt[saltcnt] = '$';
        salt[saltcnt + 1] = '\0';
    }
    else
    {
        /* classic two char salt */
        salt[0] = pwd[0];
        salt[1] = pwd[1];
        salt[2] = '\0';
    }

    encr = crypt(pln, salt);
    g_strncpy(crp, encr, 34);

    return 0;
}

/**
 *
 * @return 1 if the account is disabled, 0 otherwise
 *
 */
static int
auth_account_disabled(struct spwd *stp)
{
    int today;

    if (0 == stp)
    {
        /* if an invalid struct was passed we assume a disabled account */
        return 1;
    }

    today = g_time1() / SECS_PER_DAY;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "last   %ld", stp->sp_lstchg);
    LOG_DEVEL(LOG_LEVEL_DEBUG, "min    %ld", stp->sp_min);
    LOG_DEVEL(LOG_LEVEL_DEBUG, "max    %ld", stp->sp_max);
    LOG_DEVEL(LOG_LEVEL_DEBUG, "inact  %ld", stp->sp_inact);
    LOG_DEVEL(LOG_LEVEL_DEBUG, "warn   %ld", stp->sp_warn);
    LOG_DEVEL(LOG_LEVEL_DEBUG, "expire %ld", stp->sp_expire);
    LOG_DEVEL(LOG_LEVEL_DEBUG, "today  %d", today);

    if ((stp->sp_expire != -1) && (today >= stp->sp_expire))
    {
        return 1;
    }

    if ((stp->sp_max >= 0) &&
            (stp->sp_inact >= 0) &&
            (stp->sp_lstchg > 0) &&
            (today >= (stp->sp_lstchg + stp->sp_max + stp->sp_inact)))
    {
        return 1;
    }

    return 0;
}
