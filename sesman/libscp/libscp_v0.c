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
#include "string_calls.h"

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
 * @return != 0 if string read OK
 */
static
int in_string16(struct stream *s, char str[], const char *param)
{
    int result;

    if (!s_check_rem(s, 2))
    {
        LOG(LOG_LEVEL_WARNING, "connection aborted: %s len missing", param);
        result = 0;
    }
    else
    {
        unsigned int sz;

        in_uint16_be(s, sz);
        if (sz > STRING16_MAX_LEN)
        {
            LOG(LOG_LEVEL_WARNING,
                "connection aborted: %s too long (%u chars)",  param, sz);
            result = 0;
        }
        else
        {
            result = s_check_rem(s, sz);
            if (!result)
            {
                LOG(LOG_LEVEL_WARNING, "connection aborted: %s data missing", param);
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
#if 0
/******************************************************************************/
static enum SCP_CLIENT_STATES_E
scp_v0c_connect(struct SCP_CONNECTION *c, struct SCP_SESSION *s)
{
    tui32 version;
    int size;
    tui16 sz;

    init_stream(c->in_s, c->in_s->size);
    init_stream(c->out_s, c->in_s->size);

    LOG_DEVEL(LOG_LEVEL_DEBUG, "starting connection");
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
        LOG(LOG_LEVEL_WARNING, "connection aborted: network error");
        return SCP_CLIENT_STATE_INTERNAL_ERR;
    }

    sz = g_strlen(s->username);
    if (sz > STRING16_MAX_LEN)
    {
        LOG(LOG_LEVEL_WARNING, "connection aborted: username too long");
        return SCP_CLIENT_STATE_SIZE_ERR;
    }
    out_uint16_be(c->out_s, sz);
    out_uint8a(c->out_s, s->username, sz);

    sz = g_strlen(s->password);
    if (sz > STRING16_MAX_LEN)
    {
        LOG(LOG_LEVEL_WARNING, "connection aborted: password too long");
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
        LOG(LOG_LEVEL_WARNING, "connection aborted: network error");
        return SCP_CLIENT_STATE_NETWORK_ERR;
    }

    if (0 != scp_tcp_force_recv(c->in_sck, c->in_s->data, 8))
    {
        LOG(LOG_LEVEL_WARNING, "connection aborted: network error");
        return SCP_CLIENT_STATE_NETWORK_ERR;
    }

    in_uint32_be(c->in_s, version);

    if (0 != version)
    {
        LOG(LOG_LEVEL_WARNING, "connection aborted: version error");
        return SCP_CLIENT_STATE_VERSION_ERR;
    }

    in_uint32_be(c->in_s, size);

    if (size < (8 + 2 + 2 + 2) || size > SCP_MAX_MESSAGE_SIZE)
    {
        LOG(LOG_LEVEL_WARNING, "connection aborted: msg size = %d", size);
        return SCP_CLIENT_STATE_SIZE_ERR;
    }

    /* getting payload */
    init_stream(c->in_s, size - 8);

    if (0 != scp_tcp_force_recv(c->in_sck, c->in_s->data, size - 8))
    {
        LOG(LOG_LEVEL_WARNING, "connection aborted: network error");
        return SCP_CLIENT_STATE_NETWORK_ERR;
    }

    c->in_s->end = c->in_s->data + (size - 8);

    /* check code */
    in_uint16_be(c->in_s, sz);

    if (3 != sz)
    {
        LOG(LOG_LEVEL_WARNING, "connection aborted: sequence error");
        return SCP_CLIENT_STATE_SEQUENCE_ERR;
    }

    /* message payload */
    in_uint16_be(c->in_s, sz);

    if (1 != sz)
    {
        LOG(LOG_LEVEL_WARNING, "connection aborted: connection denied");
        return SCP_CLIENT_STATE_CONNECTION_DENIED;
    }

    in_uint16_be(c->in_s, sz);
    s->display = sz;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "connection terminated");
    return SCP_CLIENT_STATE_END;
}
#endif

/* server API */

/**
 * Initialises a V0 session object
 *
 * At the time of the call, the version has been read from the connection
 *
 * @param c Connection
 * @param [out] session pre-allocated session object
 * @return SCP_SERVER_STATE_OK for success
 */
enum SCP_SERVER_STATES_E
scp_v0s_accept(struct trans *atrans, struct SCP_SESSION *session)
{
    tui16 height;
    tui16 width;
    tui16 bpp;
    tui32 code = 0;
    char buf[STRING16_MAX_LEN + 1];
    struct stream *in_s = atrans->in_s;
    int session_type = -1;
    scp_session_set_version(session, 0);

    if (!s_check_rem(in_s, 6))
    {
        return SCP_SERVER_STATE_SIZE_ERR;
    }
    in_uint8s(in_s, 4); /* size */
    in_uint16_be(in_s, code);
    if ((code == 0) || (code == 10) || (code == 20))
    {
        if (code == 0)
        {
            session_type = SCP_SESSION_TYPE_XVNC;
        }
        else if (code == 10)
        {
            session_type = SCP_SESSION_TYPE_XRDP;
        }
        else
        {
            session_type = SCP_SESSION_TYPE_XORG;
        }
        scp_session_set_type(session, session_type);

        /* reading username */
        if (!in_string16(in_s, buf, "username"))
        {
            return SCP_SERVER_STATE_SIZE_ERR;
        }
        if (0 != scp_session_set_username(session, buf))
        {
            LOG(LOG_LEVEL_WARNING, "connection aborted: error setting username");
            return SCP_SERVER_STATE_INTERNAL_ERR;
        }

        /* reading password */
        if (!in_string16(in_s, buf, "passwd"))
        {
            return SCP_SERVER_STATE_SIZE_ERR;
        }
        if (0 != scp_session_set_password(session, buf))
        {
            LOG(LOG_LEVEL_WARNING, "connection aborted: error setting password");
            return SCP_SERVER_STATE_INTERNAL_ERR;
        }

        /* width  + height + bpp */
        if (!s_check_rem(in_s, 2 + 2 + 2))
        {
            LOG(LOG_LEVEL_WARNING, "connection aborted: width+height+bpp missing");
            return SCP_SERVER_STATE_SIZE_ERR;
        }
        in_uint16_be(in_s, width);
        scp_session_set_width(session, width);
        in_uint16_be(in_s, height);
        scp_session_set_height(session, height);
        in_uint16_be(in_s, bpp);
        if (session_type == SCP_SESSION_TYPE_XORG && bpp != 24)
        {
            LOG(LOG_LEVEL_WARNING,
                "Setting bpp to 24 from %d for Xorg session", bpp);
            bpp = 24;
        }
        if (0 != scp_session_set_bpp(session, (tui8)bpp))
        {
            LOG(LOG_LEVEL_WARNING,
                "connection aborted: unsupported bpp: %d", (tui8)bpp);
            return SCP_SERVER_STATE_INTERNAL_ERR;
        }

        if (s_check_rem(in_s, 2))
        {
            /* reading domain */
            if (!in_string16(in_s, buf, "domain"))
            {
                return SCP_SERVER_STATE_SIZE_ERR;
            }

            if (buf[0] != '\0')
            {
                scp_session_set_domain(session, buf);
            }
        }

        if (s_check_rem(in_s, 2))
        {
            /* reading program */
            if (!in_string16(in_s, buf, "program"))
            {
                return SCP_SERVER_STATE_SIZE_ERR;
            }

            if (buf[0] != '\0')
            {
                scp_session_set_program(session, buf);
            }
        }

        if (s_check_rem(in_s, 2))
        {
            /* reading directory */
            if (!in_string16(in_s, buf, "directory"))
            {
                return SCP_SERVER_STATE_SIZE_ERR;
            }

            if (buf[0] != '\0')
            {
                scp_session_set_directory(session, buf);
            }
        }

        if (s_check_rem(in_s, 2))
        {
            /* reading client IP address */
            if (!in_string16(in_s, buf, "client IP"))
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
        if (!in_string16(in_s, buf, "username"))
        {
            return SCP_SERVER_STATE_SIZE_ERR;
        }

        if (0 != scp_session_set_username(session, buf))
        {
            LOG(LOG_LEVEL_WARNING, "connection aborted: error setting username");
            return SCP_SERVER_STATE_INTERNAL_ERR;
        }

        /* reading password */
        if (!in_string16(in_s, buf, "passwd"))
        {
            return SCP_SERVER_STATE_SIZE_ERR;
        }

        if (0 != scp_session_set_password(session, buf))
        {
            LOG(LOG_LEVEL_WARNING, "connection aborted: error setting password");
            return SCP_SERVER_STATE_INTERNAL_ERR;
        }
    }
    else
    {
        LOG(LOG_LEVEL_WARNING, "connection aborted: sequence error");
        return SCP_SERVER_STATE_SEQUENCE_ERR;
    }

    return SCP_SERVER_STATE_OK;
}

/******************************************************************************/
enum SCP_SERVER_STATES_E
scp_v0s_allow_connection(struct trans *atrans, SCP_DISPLAY d, const tui8 *guid)
{
    int msg_size;
    struct stream *out_s;

    out_s = trans_get_out_s(atrans, 0);
    msg_size = guid == 0 ? 14 : 14 + 16;
    out_uint32_be(out_s, 0);  /* version */
    out_uint32_be(out_s, msg_size); /* size */
    out_uint16_be(out_s, 3);  /* cmd */
    out_uint16_be(out_s, 1);  /* data */
    out_uint16_be(out_s, d);  /* data */
    if (msg_size > 14)
    {
        out_uint8a(out_s, guid, 16);
    }
    s_mark_end(out_s);
    if (0 != trans_write_copy(atrans))
    {
        LOG(LOG_LEVEL_WARNING, "connection aborted: network error");
        return SCP_SERVER_STATE_NETWORK_ERR;
    }

    LOG_DEVEL(LOG_LEVEL_DEBUG, "connection terminated (allowed)");
    return SCP_SERVER_STATE_OK;
}

/******************************************************************************/
enum SCP_SERVER_STATES_E
scp_v0s_deny_connection(struct trans *atrans)
{
    struct stream *out_s;

    out_s = trans_get_out_s(atrans, 0);
    out_uint32_be(out_s, 0);  /* version */
    out_uint32_be(out_s, 14); /* size */
    out_uint16_be(out_s, 3);  /* cmd */
    out_uint16_be(out_s, 0);  /* data = 0 - means NOT ok*/
    out_uint16_be(out_s, 0);  /* reserved for display number*/
    s_mark_end(out_s);
    if (0 != trans_write_copy(atrans))
    {
        LOG(LOG_LEVEL_WARNING, "connection aborted: network error");
        return SCP_SERVER_STATE_NETWORK_ERR;
    }

    LOG_DEVEL(LOG_LEVEL_DEBUG, "connection terminated (denied)");
    return SCP_SERVER_STATE_OK;
}

/******************************************************************************/
enum SCP_SERVER_STATES_E
scp_v0s_replyauthentication(struct trans *atrans, unsigned short int value)
{
    struct stream *out_s;

    out_s = trans_get_out_s(atrans, 0);
    out_uint32_be(out_s, 0);  /* version */
    out_uint32_be(out_s, 14); /* size */
    /* cmd SCP_GW_AUTHENTICATION means authentication reply */
    out_uint16_be(out_s, SCP_GW_AUTHENTICATION);
    out_uint16_be(out_s, value);  /* reply code  */
    out_uint16_be(out_s, 0);  /* dummy data */
    s_mark_end(out_s);
    if (0 != trans_write_copy(atrans))
    {
        LOG(LOG_LEVEL_WARNING, "connection aborted: network error");
        return SCP_SERVER_STATE_NETWORK_ERR;
    }

    LOG_DEVEL(LOG_LEVEL_DEBUG, "connection terminated (scp_v0s_deny_authentication)");
    return SCP_SERVER_STATE_OK;
}
