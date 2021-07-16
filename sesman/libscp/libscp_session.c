/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2012
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
 * @file libscp_session.c
 * @brief SCP_SESSION handling code
 * @author Simone Fedele
 *
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "libscp_session.h"
#include "string_calls.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

//extern struct log_config* s_log;

/*******************************************************************/
struct SCP_SESSION *
scp_session_create(void)
{
    struct SCP_SESSION *s;

    s = (struct SCP_SESSION *)g_malloc(sizeof(struct SCP_SESSION), 1);

    if (0 == s)
    {
        LOG(LOG_LEVEL_ERROR, "[session:%d] session create: malloc error", __LINE__);
        return 0;
    }

    return s;
}

/*******************************************************************/
int
scp_session_set_type(struct SCP_SESSION *s, tui8 type)
{
    switch (type)
    {
        case SCP_SESSION_TYPE_XVNC:
            s->type = SCP_SESSION_TYPE_XVNC;
            break;

        case SCP_SESSION_TYPE_XRDP:
            s->type = SCP_SESSION_TYPE_XRDP;
            break;

        case SCP_SESSION_TYPE_XORG:
            s->type = SCP_SESSION_TYPE_XORG;
            break;

        case SCP_GW_AUTHENTICATION:
            s->type = SCP_GW_AUTHENTICATION;
            break;

        case SCP_SESSION_TYPE_MANAGE:
            s->type = SCP_SESSION_TYPE_MANAGE;
            break;

        default:
            LOG(LOG_LEVEL_WARNING, "[session:%d] set_type: unknown type", __LINE__);
            return 1;
    }

    return 0;
}

/*******************************************************************/
int
scp_session_set_version(struct SCP_SESSION *s, tui32 version)
{
    switch (version)
    {
        case 0:
            s->version = 0;
            break;
        case 1:
            s->version = 1;
            break;
        default:
            LOG(LOG_LEVEL_WARNING, "[session:%d] set_version: unknown version", __LINE__);
            return 1;
    }

    return 0;
}

/*******************************************************************/
int
scp_session_set_height(struct SCP_SESSION *s, tui16 h)
{
    s->height = h;
    return 0;
}

/*******************************************************************/
int
scp_session_set_width(struct SCP_SESSION *s, tui16 w)
{
    s->width = w;
    return 0;
}

/*******************************************************************/
int
scp_session_set_bpp(struct SCP_SESSION *s, tui8 bpp)
{
    switch (bpp)
    {
        case 8:
        case 15:
        case 16:
        case 24:
        case 32:
            s->bpp = bpp;
            break;
        default:
            return 1;
    }
    return 0;
}

/*******************************************************************/
int
scp_session_set_rsr(struct SCP_SESSION *s, tui8 rsr)
{
    if (s->rsr)
    {
        s->rsr = 1;
    }
    else
    {
        s->rsr = 0;
    }

    return 0;
}

/*******************************************************************/
int
scp_session_set_locale(struct SCP_SESSION *s, const char *str)
{
    if (0 == str)
    {
        LOG(LOG_LEVEL_WARNING, "[session:%d] set_locale: null locale", __LINE__);
        s->locale[0] = '\0';
        return 1;
    }

    g_strncpy(s->locale, str, 17);
    s->locale[17] = '\0';
    return 0;
}

/*******************************************************************/
int
scp_session_set_username(struct SCP_SESSION *s, const char *str)
{
    if (0 == str)
    {
        LOG(LOG_LEVEL_WARNING, "[session:%d] set_username: null username", __LINE__);
        return 1;
    }

    if (0 != s->username)
    {
        g_free(s->username);
    }

    s->username = g_strdup(str);

    if (0 == s->username)
    {
        LOG(LOG_LEVEL_WARNING, "[session:%d] set_username: strdup error", __LINE__);
        return 1;
    }

    return 0;
}

/*******************************************************************/
int
scp_session_set_password(struct SCP_SESSION *s, const char *str)
{
    if (0 == str)
    {
        LOG(LOG_LEVEL_WARNING, "[session:%d] set_password: null password", __LINE__);
        return 1;
    }

    if (0 != s->password)
    {
        g_free(s->password);
    }

    s->password = g_strdup(str);

    if (0 == s->password)
    {
        LOG(LOG_LEVEL_WARNING, "[session:%d] set_password: strdup error", __LINE__);
        return 1;
    }

    return 0;
}

/*******************************************************************/
int
scp_session_set_domain(struct SCP_SESSION *s, const char *str)
{
    if (0 == str)
    {
        LOG(LOG_LEVEL_WARNING, "[session:%d] set_domain: null domain", __LINE__);
        return 1;
    }

    if (0 != s->domain)
    {
        g_free(s->domain);
    }

    s->domain = g_strdup(str);

    if (0 == s->domain)
    {
        LOG(LOG_LEVEL_WARNING, "[session:%d] set_domain: strdup error", __LINE__);
        return 1;
    }

    return 0;
}

/*******************************************************************/
int
scp_session_set_program(struct SCP_SESSION *s, const char *str)
{
    if (0 == str)
    {
        LOG(LOG_LEVEL_WARNING, "[session:%d] set_program: null program", __LINE__);
        return 1;
    }

    if (0 != s->program)
    {
        g_free(s->program);
    }

    s->program = g_strdup(str);

    if (0 == s->program)
    {
        LOG(LOG_LEVEL_WARNING, "[session:%d] set_program: strdup error", __LINE__);
        return 1;
    }

    return 0;
}

/*******************************************************************/
int
scp_session_set_directory(struct SCP_SESSION *s, const char *str)
{
    if (0 == str)
    {
        LOG(LOG_LEVEL_WARNING, "[session:%d] set_directory: null directory", __LINE__);
        return 1;
    }

    if (0 != s->directory)
    {
        g_free(s->directory);
    }

    s->directory = g_strdup(str);

    if (0 == s->directory)
    {
        LOG(LOG_LEVEL_WARNING, "[session:%d] set_directory: strdup error", __LINE__);
        return 1;
    }

    return 0;
}

/*******************************************************************/
int
scp_session_set_client_ip(struct SCP_SESSION *s, const char *str)
{
    if (0 == str)
    {
        LOG(LOG_LEVEL_WARNING, "[session:%d] set_client_ip: null ip", __LINE__);
        return 1;
    }

    if (0 != s->client_ip)
    {
        g_free(s->client_ip);
    }

    s->client_ip = g_strdup(str);

    if (0 == s->client_ip)
    {
        LOG(LOG_LEVEL_WARNING, "[session:%d] set_client_ip: strdup error", __LINE__);
        return 1;
    }

    return 0;
}

/*******************************************************************/
int
scp_session_set_hostname(struct SCP_SESSION *s, const char *str)
{
    if (0 == str)
    {
        LOG(LOG_LEVEL_WARNING, "[session:%d] set_hostname: null hostname", __LINE__);
        return 1;
    }

    if (0 != s->hostname)
    {
        g_free(s->hostname);
    }

    s->hostname = g_strdup(str);

    if (0 == s->hostname)
    {
        LOG(LOG_LEVEL_WARNING, "[session:%d] set_hostname: strdup error", __LINE__);
        return 1;
    }

    return 0;
}

/*******************************************************************/
int
scp_session_set_errstr(struct SCP_SESSION *s, const char *str)
{
    if (0 == str)
    {
        LOG(LOG_LEVEL_WARNING, "[session:%d] set_errstr: null string", __LINE__);
        return 1;
    }

    if (0 != s->errstr)
    {
        g_free(s->errstr);
    }

    s->errstr = g_strdup(str);

    if (0 == s->errstr)
    {
        LOG(LOG_LEVEL_WARNING, "[session:%d] set_errstr: strdup error", __LINE__);
        return 1;
    }

    return 0;
}

/*******************************************************************/
int
scp_session_set_display(struct SCP_SESSION *s, SCP_DISPLAY display)
{
    s->display = display;
    return 0;
}

/*******************************************************************/
int
scp_session_set_addr(struct SCP_SESSION *s, int type, const void *addr)
{
    switch (type)
    {
        case SCP_ADDRESS_TYPE_IPV4:
            g_memcpy(&(s->ipv4addr), addr, 4);
            break;
#ifdef IN6ADDR_ANY_INIT
        case SCP_ADDRESS_TYPE_IPV6:
            g_memcpy(s->ipv6addr, addr, 16);
            break;
#endif
        default:
            return 1;
    }

    return 0;
}

/*******************************************************************/
int
scp_session_set_guid(struct SCP_SESSION *s, const tui8 *guid)
{
    if (0 == guid)
    {
        LOG(LOG_LEVEL_WARNING, "[session:%d] set_guid: null guid", __LINE__);
        return 1;
    }

    g_memcpy(s->guid, guid, 16);

    return 0;
}

/*******************************************************************/
void
scp_session_destroy(struct SCP_SESSION *s)
{
    if (s != NULL)
    {
        g_free(s->username);
        g_free(s->password);
        g_free(s->hostname);
        g_free(s->domain);
        g_free(s->program);
        g_free(s->directory);
        g_free(s->client_ip);
        g_free(s->errstr);
        g_free(s);
    }
}

/*******************************************************************/
struct SCP_SESSION *
scp_session_clone(const struct SCP_SESSION *s)
{
    struct SCP_SESSION *result = NULL;

    if (s != NULL && (result = g_new(struct SCP_SESSION, 1)) != NULL)
    {
        /* Duplicate all the scalar variables */
        g_memcpy(result, s, sizeof(*s));

        /* Now duplicate all the strings */
        result->username = g_strdup(s->username);
        result->password = g_strdup(s->password);
        result->hostname = g_strdup(s->hostname);
        result->errstr = g_strdup(s->errstr);
        result->domain = g_strdup(s->domain);
        result->program = g_strdup(s->program);
        result->directory = g_strdup(s->directory);
        result->client_ip = g_strdup(s->client_ip);

        /* Did all the string copies succeed? */
        if ((s->username != NULL && result->username == NULL) ||
                (s->password != NULL && result->password == NULL) ||
                (s->hostname != NULL && result->hostname == NULL) ||
                (s->errstr != NULL && result->errstr == NULL) ||
                (s->domain != NULL && result->domain == NULL) ||
                (s->program != NULL && result->program == NULL) ||
                (s->directory != NULL && result->directory == NULL) ||
                (s->client_ip != NULL && result->client_ip == NULL))
        {
            scp_session_destroy(result);
            result = NULL;
        }
    }

    return result;
}
