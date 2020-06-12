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

/** Maximum length of a string (two bytes + len), excluding the terminator
 *
 * Practially this is limited by [MS-RDPBCGR] TS_INFO_PACKET
 * */
#define STRING16_MAX_LEN 512

/**
 * Reads a big-endian uint16 followed by a string into a buffer
 *
 * Buffer is null-terminated on success
 *
 * @param s Input stream
 * @param [out] Output buffer (must be >= (STRING16_MAX_LEN+1) chars)
 * @param param Parameter we're reading
 * @param line Line number reference
 * @return != 0 if string read OK
 */
static
int in_string16(struct stream *s, char str[], const char *param, int line)
{
    int result;

    if (!s_check_rem(s, 2))
    {
        log_message(LOG_LEVEL_WARNING,
                    "[v0:%d] connection aborted: %s len missing",
                    line, param);
        result = 0;
    }
    else
    {
        unsigned int sz;

        in_uint16_be(s, sz);
        if (sz > STRING16_MAX_LEN)
        {
            log_message(LOG_LEVEL_WARNING,
                        "[v0:%d] connection aborted: %s too long (%u chars)",
                        line, param, sz);
            result = 0;
        }
        else
        {
            result = s_check_rem(s, sz);
            if (!result)
            {
                log_message(LOG_LEVEL_WARNING,
                            "[v0:%d] connection aborted: %s data missing",
                            line, param);
            }
            else
            {
                in_uint8a(s, str, sz);
                str[sz] = '\0';
            }
        }
    }
    return result;
}
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
    if (sz > STRING16_MAX_LEN)
    {
        log_message(LOG_LEVEL_WARNING,
                    "[v0:%d] connection aborted: username too long",
                    __LINE__);
        return SCP_CLIENT_STATE_SIZE_ERR;
    }
    out_uint16_be(c->out_s, sz);
    out_uint8a(c->out_s, s->username, sz);

    sz = g_strlen(s->password);
    if (sz > STRING16_MAX_LEN)
    {
        log_message(LOG_LEVEL_WARNING,
                    "[v0:%d] connection aborted: password too long",
                    __LINE__);
        return SCP_CLIENT_STATE_SIZE_ERR;
    }
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

    if (size < (8 + 2 + 2 + 2) || size > SCP_MAX_MESSAGE_SIZE)
    {
        log_message(LOG_LEVEL_WARNING,
                    "[v0:%d] connection aborted: msg size = %u",
                    __LINE__, (unsigned int)size);
        return SCP_CLIENT_STATE_SIZE_ERR;
    }

    /* getting payload */
    init_stream(c->in_s, size - 8);

    if (0 != scp_tcp_force_recv(c->in_sck, c->in_s->data, size - 8))
    {
        log_message(LOG_LEVEL_WARNING, "[v0:%d] connection aborted: network error", __LINE__);
        return SCP_CLIENT_STATE_NETWORK_ERR;
    }

    c->in_s->end = c->in_s->data + (size - 8);

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

/**
 * Initialises a V0 session object
 *
 * At the time of the call, the version has been read from the connection
 *
 * @param c Connection
 * @param [out] session pre-allocated session object
 * @return SCP_SERVER_STATE_OK for success
 */
static enum SCP_SERVER_STATES_E
scp_v0s_init_session(struct SCP_CONNECTION *c, struct SCP_SESSION *session)
{
    tui32 size;
    tui16 height;
    tui16 width;
    tui16 bpp;
    tui32 code = 0;
    char buf[STRING16_MAX_LEN + 1];

    scp_session_set_version(session, 0);

    /* Check for a header and a code value in the length */
    in_uint32_be(c->in_s, size);
    if (size < (8 + 2) || size > SCP_MAX_MESSAGE_SIZE)
    {
        log_message(LOG_LEVEL_WARNING,
                    "[v0:%d] connection aborted: msg size = %u",
                    __LINE__, (unsigned int)size);
        return SCP_SERVER_STATE_SIZE_ERR;
    }

    init_stream(c->in_s, size - 8);

    if (0 != scp_tcp_force_recv(c->in_sck, c->in_s->data, size - 8))
    {
        log_message(LOG_LEVEL_WARNING, "[v0:%d] connection aborted: network error", __LINE__);
        return SCP_SERVER_STATE_NETWORK_ERR;
    }

    c->in_s->end = c->in_s->data + (size - 8);

    in_uint16_be(c->in_s, code);

    if (code == 0 || code == 10 || code == 20)
    {
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
        if (!in_string16(c->in_s, buf, "username", __LINE__))
        {
            return SCP_SERVER_STATE_SIZE_ERR;
        }
        if (0 != scp_session_set_username(session, buf))
        {
            log_message(LOG_LEVEL_WARNING, "[v0:%d] connection aborted: error setting username", __LINE__);
            return SCP_SERVER_STATE_INTERNAL_ERR;
        }

        /* reading password */
        if (!in_string16(c->in_s, buf, "passwd", __LINE__))
        {
            return SCP_SERVER_STATE_SIZE_ERR;
        }
        if (0 != scp_session_set_password(session, buf))
        {
            log_message(LOG_LEVEL_WARNING, "[v0:%d] connection aborted: error setting password", __LINE__);
            return SCP_SERVER_STATE_INTERNAL_ERR;
        }

        /* width  + height + bpp */
        if (!s_check_rem(c->in_s, 2 + 2 + 2))
        {
            log_message(LOG_LEVEL_WARNING,
                        "[v0:%d] connection aborted: width+height+bpp missing",
                        __LINE__);
            return SCP_SERVER_STATE_SIZE_ERR;
        }
        in_uint16_be(c->in_s, width);
        scp_session_set_width(session, width);
        in_uint16_be(c->in_s, height);
        scp_session_set_height(session, height);
        in_uint16_be(c->in_s, bpp);
        if (0 != scp_session_set_bpp(session, (tui8)bpp))
        {
            log_message(LOG_LEVEL_WARNING,
                        "[v0:%d] connection aborted: unsupported bpp: %d",
                        __LINE__, (tui8)bpp);
            return SCP_SERVER_STATE_INTERNAL_ERR;
        }

        if (s_check_rem(c->in_s, 2))
        {
            /* reading domain */
            if (!in_string16(c->in_s, buf, "domain", __LINE__))
            {
                return SCP_SERVER_STATE_SIZE_ERR;
            }
            if (buf[0] != '\0')
            {
                scp_session_set_domain(session, buf);
            }
        }

        if (s_check_rem(c->in_s, 2))
        {
            /* reading program */
            if (!in_string16(c->in_s, buf, "program", __LINE__))
            {
                return SCP_SERVER_STATE_SIZE_ERR;
            }

            if (buf[0] != '\0')
            {
                scp_session_set_program(session, buf);
            }
        }

        if (s_check_rem(c->in_s, 2))
        {
            /* reading directory */
            if (!in_string16(c->in_s, buf, "directory", __LINE__))
            {
                return SCP_SERVER_STATE_SIZE_ERR;
            }

            if (buf[0] != '\0')
            {
                scp_session_set_directory(session, buf);
            }
        }

        if (s_check_rem(c->in_s, 2))
        {
            /* reading client IP address */
            if (!in_string16(c->in_s, buf, "client IP", __LINE__))
            {
                return SCP_SERVER_STATE_SIZE_ERR;
            }
            if (buf[0] != '\0')
            {
                scp_session_set_client_ip(session, buf);
            }
        }
    }
    else if (code == SCP_GW_AUTHENTICATION)
    {
        scp_session_set_type(session, SCP_GW_AUTHENTICATION);
        /* reading username */
        if (!in_string16(c->in_s, buf, "username", __LINE__))
        {
            return SCP_SERVER_STATE_SIZE_ERR;
        }

        /* g_writeln("Received user name: %s",buf); */
        if (0 != scp_session_set_username(session, buf))
        {
            /* until syslog merge log_message(s_log, LOG_LEVEL_WARNING, "[v0:%d] connection aborted: error setting        username", __LINE__);*/
            return SCP_SERVER_STATE_INTERNAL_ERR;
        }

        /* reading password */
        if (!in_string16(c->in_s, buf, "passwd", __LINE__))
        {
            return SCP_SERVER_STATE_SIZE_ERR;
        }

        /* g_writeln("Received password: %s",buf); */
        if (0 != scp_session_set_password(session, buf))
        {
            /* until syslog merge log_message(s_log, LOG_LEVEL_WARNING, "[v0:%d] connection aborted: error setting password", __LINE__); */
            return SCP_SERVER_STATE_INTERNAL_ERR;
        }
    }
    else
    {
        log_message(LOG_LEVEL_WARNING, "[v0:%d] connection aborted: sequence error", __LINE__);
        return SCP_SERVER_STATE_SEQUENCE_ERR;
    }

    return SCP_SERVER_STATE_OK;
}


/* server API */
/******************************************************************************/
enum SCP_SERVER_STATES_E
scp_v0s_accept(struct SCP_CONNECTION *c, struct SCP_SESSION **s, int skipVchk)
{
    enum SCP_SERVER_STATES_E result = SCP_SERVER_STATE_OK;
    struct SCP_SESSION *session = NULL;
    tui32 version = 0;

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
                result = SCP_SERVER_STATE_VERSION_ERR;
            }
        }
        else
        {
            log_message(LOG_LEVEL_WARNING, "[v0:%d] connection aborted: network error", __LINE__);
            result = SCP_SERVER_STATE_NETWORK_ERR;
        }
    }

    if (result == SCP_SERVER_STATE_OK)
    {
        session = scp_session_create();
        if (NULL == session)
        {
            log_message(LOG_LEVEL_WARNING,
                        "[v0:%d] connection aborted: no memory",
                        __LINE__);
            result = SCP_SERVER_STATE_INTERNAL_ERR;
        }
        else
        {
            result = scp_v0s_init_session(c, session);
            if (result != SCP_SERVER_STATE_OK)
            {
                scp_session_destroy(session);
                session = NULL;
            }
        }
    }

    (*s) = session;

    return result;
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
