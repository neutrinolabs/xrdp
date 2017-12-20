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
 * @file libscp_v0.c
 * @brief libscp version 0 code
 * @author Simone Fedele
 *
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "libscp_v0.h"

#include "os_calls.h"

extern struct log_config *s_log;

/* client API */
/******************************************************************************/
enum SCP_CLIENT_STATES_E
scp_v0c_connect(struct SCP_CONNECTION *c, struct SCP_SESSION *s)
{
    tui32 version;
    tui32 size;
    tui16 sz;

    init_stream(c->in_s, c->in_s->size);
    init_stream(c->out_s, c->in_s->size);

    LOG_DBG("[v0:%d] starting connection", __LINE__);
    g_tcp_set_non_blocking(c->in_sck);
    g_tcp_set_no_delay(c->in_sck);
    s_push_layer(c->out_s, channel_hdr, 8);

    /* code */
    if (s->type == SCP_SESSION_TYPE_XVNC)
    {
        out_uint16_be(c->out_s, 0);
    }
    else if (s->type == SCP_SESSION_TYPE_XRDP)
    {
        out_uint16_be(c->out_s, 10);
    }
    else if (s->type == SCP_SESSION_TYPE_XORG)
    {
        out_uint16_be(c->out_s, 20);
    }
    else
    {
        log_message(LOG_LEVEL_WARNING, "[v0:%d] connection aborted: network error", __LINE__);
        return SCP_CLIENT_STATE_INTERNAL_ERR;
    }

    sz = g_strlen(s->username);
    out_uint16_be(c->out_s, sz);
    out_uint8a(c->out_s, s->username, sz);

    sz = g_strlen(s->password);
    out_uint16_be(c->out_s, sz);
    out_uint8a(c->out_s, s->password, sz);
    out_uint16_be(c->out_s, s->width);
    out_uint16_be(c->out_s, s->height);
    out_uint16_be(c->out_s, s->bpp);

    s_mark_end(c->out_s);
    s_pop_layer(c->out_s, channel_hdr);

    /* version */
    out_uint32_be(c->out_s, 0);
    /* size */
    out_uint32_be(c->out_s, c->out_s->end - c->out_s->data);

    if (0 != scp_tcp_force_send(c->in_sck, c->out_s->data, c->out_s->end - c->out_s->data))
    {
        log_message(LOG_LEVEL_WARNING, "[v0:%d] connection aborted: network error", __LINE__);
        return SCP_CLIENT_STATE_NETWORK_ERR;
    }

    if (0 != scp_tcp_force_recv(c->in_sck, c->in_s->data, 8))
    {
        log_message(LOG_LEVEL_WARNING, "[v0:%d] connection aborted: network error", __LINE__);
        return SCP_CLIENT_STATE_NETWORK_ERR;
    }

    in_uint32_be(c->in_s, version);

    if (0 != version)
    {
        log_message(LOG_LEVEL_WARNING, "[v0:%d] connection aborted: version error", __LINE__);
        return SCP_CLIENT_STATE_VERSION_ERR;
    }

    in_uint32_be(c->in_s, size);

    if (size < 14)
    {
        log_message(LOG_LEVEL_WARNING, "[v0:%d] connection aborted: packet size error", __LINE__);
        return SCP_CLIENT_STATE_SIZE_ERR;
    }

    /* getting payload */
    init_stream(c->in_s, c->in_s->size);

    if (0 != scp_tcp_force_recv(c->in_sck, c->in_s->data, size - 8))
    {
        log_message(LOG_LEVEL_WARNING, "[v0:%d] connection aborted: network error", __LINE__);
        return SCP_CLIENT_STATE_NETWORK_ERR;
    }

    /* check code */
    in_uint16_be(c->in_s, sz);

    if (3 != sz)
    {
        log_message(LOG_LEVEL_WARNING, "[v0:%d] connection aborted: sequence error", __LINE__);
        return SCP_CLIENT_STATE_SEQUENCE_ERR;
    }

    /* message payload */
    in_uint16_be(c->in_s, sz);

    if (1 != sz)
    {
        log_message(LOG_LEVEL_WARNING, "[v0:%d] connection aborted: connection denied", __LINE__);
        return SCP_CLIENT_STATE_CONNECTION_DENIED;
    }

    in_uint16_be(c->in_s, sz);
    s->display = sz;

    LOG_DBG("[v0:%d] connection terminated", __LINE__);
    return SCP_CLIENT_STATE_END;
}

/* server API */
/******************************************************************************/
enum SCP_SERVER_STATES_E
scp_v0s_accept(struct SCP_CONNECTION *c, struct SCP_SESSION **s, int skipVchk)
{
    tui32 version = 0;
    tui32 size;
    struct SCP_SESSION *session = 0;
    tui16 sz;
    tui32 code = 0;
    char *buf = 0;

    if (!skipVchk)
    {
        LOG_DBG("[v0:%d] starting connection", __LINE__);

        if (0 == scp_tcp_force_recv(c->in_sck, c->in_s->data, 8))
        {
            c->in_s->end = c->in_s->data + 8;
            in_uint32_be(c->in_s, version);

            if (version != 0)
            {
                log_message(LOG_LEVEL_WARNING, "[v0:%d] connection aborted: version error", __LINE__);
                return SCP_SERVER_STATE_VERSION_ERR;
            }
        }
        else
        {
            log_message(LOG_LEVEL_WARNING, "[v0:%d] connection aborted: network error", __LINE__);
            return SCP_SERVER_STATE_NETWORK_ERR;
        }
    }

    in_uint32_be(c->in_s, size);

    init_stream(c->in_s, 8196);

    if (0 != scp_tcp_force_recv(c->in_sck, c->in_s->data, size - 8))
    {
        log_message(LOG_LEVEL_WARNING, "[v0:%d] connection aborted: network error", __LINE__);
        return SCP_SERVER_STATE_NETWORK_ERR;
    }

    c->in_s->end = c->in_s->data + (size - 8);

    in_uint16_be(c->in_s, code);

    if (code == 0 || code == 10 || code == 20)
    {
        session = scp_session_create();

        if (0 == session)
        {
            log_message(LOG_LEVEL_WARNING, "[v0:%d] connection aborted: network error", __LINE__);
            return SCP_SERVER_STATE_INTERNAL_ERR;
        }

        scp_session_set_version(session, version);

        if (code == 0)
        {
            scp_session_set_type(session, SCP_SESSION_TYPE_XVNC);
        }
        else if (code == 10)
        {
            scp_session_set_type(session, SCP_SESSION_TYPE_XRDP);
        }
        else if (code == 20)
        {
            scp_session_set_type(session, SCP_SESSION_TYPE_XORG);
        }

        /* reading username */
        in_uint16_be(c->in_s, sz);
        buf = g_new0(char, sz + 1);
        in_uint8a(c->in_s, buf, sz);
        buf[sz] = '\0';
        if (0 != scp_session_set_username(session, buf))
        {
            scp_session_destroy(session);
            log_message(LOG_LEVEL_WARNING, "[v0:%d] connection aborted: error setting username", __LINE__);
            g_free(buf);
            return SCP_SERVER_STATE_INTERNAL_ERR;
        }
        g_free(buf);

        /* reading password */
        in_uint16_be(c->in_s, sz);
        buf = g_new0(char, sz + 1);
        in_uint8a(c->in_s, buf, sz);
        buf[sz] = '\0';
        if (0 != scp_session_set_password(session, buf))
        {
            scp_session_destroy(session);
            log_message(LOG_LEVEL_WARNING, "[v0:%d] connection aborted: error setting password", __LINE__);
            g_free(buf);
            return SCP_SERVER_STATE_INTERNAL_ERR;
        }
        g_free(buf);

        /* width */
        in_uint16_be(c->in_s, sz);
        scp_session_set_width(session, sz);
        /* height */
        in_uint16_be(c->in_s, sz);
        scp_session_set_height(session, sz);
        /* bpp */
        in_uint16_be(c->in_s, sz);
        if (0 != scp_session_set_bpp(session, (tui8)sz))
        {
            scp_session_destroy(session);
            log_message(LOG_LEVEL_WARNING,
                        "[v0:%d] connection aborted: unsupported bpp: %d",
                        __LINE__, (tui8)sz);
            return SCP_SERVER_STATE_INTERNAL_ERR;
        }

        if (s_check_rem(c->in_s, 2))
        {
            /* reading domain */
            in_uint16_be(c->in_s, sz);

            if (sz > 0)
            {
                buf = g_new0(char, sz + 1);
                in_uint8a(c->in_s, buf, sz);
                buf[sz] = '\0';
                scp_session_set_domain(session, buf);
                g_free(buf);
            }
        }

        if (s_check_rem(c->in_s, 2))
        {
            /* reading program */
            in_uint16_be(c->in_s, sz);

            if (sz > 0)
            {
                buf = g_new0(char, sz + 1);
                in_uint8a(c->in_s, buf, sz);
                buf[sz] = '\0';
                scp_session_set_program(session, buf);
                g_free(buf);
            }
        }

        if (s_check_rem(c->in_s, 2))
        {
            /* reading directory */
            in_uint16_be(c->in_s, sz);

            if (sz > 0)
            {
                buf = g_new0(char, sz + 1);
                in_uint8a(c->in_s, buf, sz);
                buf[sz] = '\0';
                scp_session_set_directory(session, buf);
                g_free(buf);
            }
        }

        if (s_check_rem(c->in_s, 2))
        {
            /* reading client IP address */
            in_uint16_be(c->in_s, sz);

            if (sz > 0)
            {
                buf = g_new0(char, sz + 1);
                in_uint8a(c->in_s, buf, sz);
                buf[sz] = '\0';
                scp_session_set_client_ip(session, buf);
                g_free(buf);
            }
        }
    }
    else if (code == SCP_GW_AUTHENTICATION)
    {
        /* g_writeln("Command is SCP_GW_AUTHENTICATION"); */
        session = scp_session_create();

        if (0 == session)
        {
            /* until syslog merge log_message(s_log, LOG_LEVEL_WARNING, "[v0:%d] connection aborted: network error",      __LINE__);*/
            return SCP_SERVER_STATE_INTERNAL_ERR;
        }

        scp_session_set_version(session, version);
        scp_session_set_type(session, SCP_GW_AUTHENTICATION);
        /* reading username */
        in_uint16_be(c->in_s, sz);
        buf = g_new0(char, sz + 1);
        in_uint8a(c->in_s, buf, sz);
        buf[sz] = '\0';

        /* g_writeln("Received user name: %s",buf); */
        if (0 != scp_session_set_username(session, buf))
        {
            scp_session_destroy(session);
            /* until syslog merge log_message(s_log, LOG_LEVEL_WARNING, "[v0:%d] connection aborted: error setting        username", __LINE__);*/
            g_free(buf);
            return SCP_SERVER_STATE_INTERNAL_ERR;
        }
        g_free(buf);

        /* reading password */
        in_uint16_be(c->in_s, sz);
        buf = g_new0(char, sz + 1);
        in_uint8a(c->in_s, buf, sz);
        buf[sz] = '\0';

        /* g_writeln("Received password: %s",buf); */
        if (0 != scp_session_set_password(session, buf))
        {
            scp_session_destroy(session);
            /* until syslog merge log_message(s_log, LOG_LEVEL_WARNING, "[v0:%d] connection aborted: error setting password", __LINE__); */
            g_free(buf);
            return SCP_SERVER_STATE_INTERNAL_ERR;
        }
        g_free(buf);
    }
    else
    {
        log_message(LOG_LEVEL_WARNING, "[v0:%d] connection aborted: sequence error", __LINE__);
        return SCP_SERVER_STATE_SEQUENCE_ERR;
    }

    (*s) = session;
    return SCP_SERVER_STATE_OK;
}

/******************************************************************************/
enum SCP_SERVER_STATES_E
scp_v0s_allow_connection(struct SCP_CONNECTION *c, SCP_DISPLAY d, const tui8 *guid)
{
    int msg_size;

    msg_size = guid == 0 ? 14 : 14 + 16;
    out_uint32_be(c->out_s, 0);  /* version */
    out_uint32_be(c->out_s, msg_size); /* size */
    out_uint16_be(c->out_s, 3);  /* cmd */
    out_uint16_be(c->out_s, 1);  /* data */
    out_uint16_be(c->out_s, d);  /* data */
    if (msg_size > 14)
    {
        out_uint8a(c->out_s, guid, 16);
    }
    s_mark_end(c->out_s);

    if (0 != scp_tcp_force_send(c->in_sck, c->out_s->data, c->out_s->end - c->out_s->data))
    {
        log_message(LOG_LEVEL_WARNING, "[v0:%d] connection aborted: network error", __LINE__);
        return SCP_SERVER_STATE_NETWORK_ERR;
    }

    LOG_DBG("[v0:%d] connection terminated (allowed)", __LINE__);
    return SCP_SERVER_STATE_OK;
}

/******************************************************************************/
enum SCP_SERVER_STATES_E
scp_v0s_deny_connection(struct SCP_CONNECTION *c)
{
    out_uint32_be(c->out_s, 0);  /* version */
    out_uint32_be(c->out_s, 14); /* size */
    out_uint16_be(c->out_s, 3);  /* cmd */
    out_uint16_be(c->out_s, 0);  /* data = 0 - means NOT ok*/
    out_uint16_be(c->out_s, 0);  /* reserved for display number*/
    s_mark_end(c->out_s);

    if (0 != scp_tcp_force_send(c->in_sck, c->out_s->data, c->out_s->end - c->out_s->data))
    {
        log_message(LOG_LEVEL_WARNING, "[v0:%d] connection aborted: network error", __LINE__);
        return SCP_SERVER_STATE_NETWORK_ERR;
    }

    LOG_DBG("[v0:%d] connection terminated (denied)", __LINE__);
    return SCP_SERVER_STATE_OK;
}

/******************************************************************************/
enum SCP_SERVER_STATES_E
scp_v0s_replyauthentication(struct SCP_CONNECTION *c, unsigned short int value)
{
    out_uint32_be(c->out_s, 0);  /* version */
    out_uint32_be(c->out_s, 14); /* size */
    /* cmd SCP_GW_AUTHENTICATION means authentication reply */
    out_uint16_be(c->out_s, SCP_GW_AUTHENTICATION);
    out_uint16_be(c->out_s, value);  /* reply code  */
    out_uint16_be(c->out_s, 0);  /* dummy data */
    s_mark_end(c->out_s);

    /* g_writeln("Total number of bytes that will be sent %d",c->out_s->end - c->out_s->data);*/
    if (0 != scp_tcp_force_send(c->in_sck, c->out_s->data, c->out_s->end - c->out_s->data))
    {
        /* until syslog merge log_message(s_log, LOG_LEVEL_WARNING, "[v0:%d] connection aborted: network error", __LINE__); */
        return SCP_SERVER_STATE_NETWORK_ERR;
    }

    /* until syslog merge LOG_DBG(s_log, "[v0:%d] connection terminated (scp_v0s_deny_authentication)", __LINE__);*/
    return SCP_SERVER_STATE_OK;
}
