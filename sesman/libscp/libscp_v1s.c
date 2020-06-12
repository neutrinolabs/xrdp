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
 * @file libscp_v1s.c
 * @brief libscp version 1 server api code
 * @author Simone Fedele
 *
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#ifndef LIBSCP_V1S_C
#define LIBSCP_V1S_C

#include "libscp_v1s.h"

//extern struct log_config* s_log;

/**
 * Reads a uint8 followed by a string into a buffer
 *
 * Buffer is null-terminated on success
 *
 * @param s Input stream
 * @param [out] Output buffer (must be >= 256 chars)
 * @param param Parameter we're reading
 * @param line Line number reference
 * @return != 0 if string read OK
 *
 * @todo
 *     This needs to be merged with the func of the same name in
 *     libscp_v1s_mng.c
 */
static
int in_string8(struct stream *s, char str[], const char *param, int line)
{
    int result;

    if (!s_check_rem(s, 1))
    {
        log_message(LOG_LEVEL_WARNING,
                    "[v1s:%d] connection aborted: %s len missing",
                    line, param);
        result = 0;
    }
    else
    {
        unsigned int sz;

        in_uint8(s, sz);
        result = s_check_rem(s, sz);
        if (!result)
        {
            log_message(LOG_LEVEL_WARNING,
                        "[v1s:%d] connection aborted: %s data missing",
                        line, param);
        }
        else
        {
            in_uint8a(s, str, sz);
            str[sz] = '\0';
        }
    }
    return result;
}
/* server API */

/**
 * Initialises a V1 session object
 *
 * This is called after the V1 header, command set and command have been read
 *
 * @param c Connection
 * @param [out] session pre-allocated session object
 * @return SCP_SERVER_STATE_OK for success
 */
static enum SCP_SERVER_STATES_E
scp_v1s_init_session(struct SCP_CONNECTION *c, struct SCP_SESSION *session)
{
    tui8 type;
    tui16 height;
    tui16 width;
    tui8 bpp;
    tui8 sz;
    char buf[256];

    scp_session_set_version(session, 1);

    /* Check there's data for the session type, the height, the width, the
     * bpp, the resource sharing indicator and the locale */
    if (!s_check_rem(c->in_s, 1 + 2 + 2 + 1 + 1 + 17))
    {
        log_message(LOG_LEVEL_WARNING,
                    "[v1s:%d] connection aborted: short packet",
                    __LINE__);
        return SCP_SERVER_STATE_SIZE_ERR;
    }

    in_uint8(c->in_s, type);

    if ((type != SCP_SESSION_TYPE_XVNC) && (type != SCP_SESSION_TYPE_XRDP))
    {
        log_message(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: unknown session type", __LINE__);
        return SCP_SERVER_STATE_SESSION_TYPE_ERR;
    }

    scp_session_set_type(session, type);

    in_uint16_be(c->in_s, height);
    scp_session_set_height(session, height);
    in_uint16_be(c->in_s, width);
    scp_session_set_width(session, width);
    in_uint8(c->in_s, bpp);
    if (0 != scp_session_set_bpp(session, bpp))
    {
        log_message(LOG_LEVEL_WARNING,
                    "[v1s:%d] connection aborted: unsupported bpp: %d",
                    __LINE__, bpp);
        return SCP_SERVER_STATE_INTERNAL_ERR;
    }
    in_uint8(c->in_s, sz);
    scp_session_set_rsr(session, sz);
    in_uint8a(c->in_s, buf, 17);
    buf[17] = '\0';
    scp_session_set_locale(session, buf);

    /* Check there's enough data left for at least an IPv4 address (+len) */
    if (!s_check_rem(c->in_s, 1 + 4))
    {
        log_message(LOG_LEVEL_WARNING,
                    "[v1s:%d] connection aborted: IP addr len missing",
                    __LINE__);
        return SCP_SERVER_STATE_SIZE_ERR;
    }

    in_uint8(c->in_s, sz);

    if (sz == SCP_ADDRESS_TYPE_IPV4)
    {
        tui32 ipv4;
        in_uint32_be(c->in_s, ipv4);
        scp_session_set_addr(session, sz, &ipv4);
    }
    else if (sz == SCP_ADDRESS_TYPE_IPV6)
    {
        if (!s_check_rem(c->in_s, 16))
        {
            log_message(LOG_LEVEL_WARNING,
                        "[v1s:%d] connection aborted: IP addr missing",
                        __LINE__);
            return SCP_SERVER_STATE_SIZE_ERR;
        }
        in_uint8a(c->in_s, buf, 16);
        scp_session_set_addr(session, sz, buf);
    }

    /* reading hostname */
    if (!in_string8(c->in_s, buf, "hostname", __LINE__))
    {
        return SCP_SERVER_STATE_SIZE_ERR;
    }

    if (0 != scp_session_set_hostname(session, buf))
    {
        log_message(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: internal error", __LINE__);
        return SCP_SERVER_STATE_INTERNAL_ERR;
    }

    /* reading username */
    if (!in_string8(c->in_s, buf, "username", __LINE__))
    {
        return SCP_SERVER_STATE_SIZE_ERR;
    }

    if (0 != scp_session_set_username(session, buf))
    {
        log_message(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: internal error", __LINE__);
        return SCP_SERVER_STATE_INTERNAL_ERR;
    }

    /* reading password */
    if (!in_string8(c->in_s, buf, "passwd", __LINE__))
    {
        return SCP_SERVER_STATE_SIZE_ERR;
    }

    if (0 != scp_session_set_password(session, buf))
    {
        log_message(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: internal error", __LINE__);
        return SCP_SERVER_STATE_INTERNAL_ERR;
    }

    return SCP_SERVER_STATE_OK;
}

/* server API */
enum SCP_SERVER_STATES_E scp_v1s_accept(struct SCP_CONNECTION *c, struct SCP_SESSION **s, int skipVchk)
{
    enum SCP_SERVER_STATES_E result;
    struct SCP_SESSION *session;
    tui32 version;
    tui32 size;
    tui16 cmdset;
    tui16 cmd;

    (*s) = NULL;

    if (!skipVchk)
    {

        if (0 == scp_tcp_force_recv(c->in_sck, c->in_s->data, 8))
        {
            in_uint32_be(c->in_s, version);

            if (version != 1)
            {
                log_message(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: version error", __LINE__);
                return SCP_SERVER_STATE_VERSION_ERR;
            }
        }
        else
        {
            log_message(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: network error", __LINE__);
            return SCP_SERVER_STATE_NETWORK_ERR;
        }
    }

    in_uint32_be(c->in_s, size);

    /* Check the message is big enough for the header, the command set, and
     * the command (but not too big) */
    if (size < (8 + 2 + 2) || size > SCP_MAX_MESSAGE_SIZE)
    {
        log_message(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: size error", __LINE__);
        return SCP_SERVER_STATE_SIZE_ERR;
    }

    init_stream(c->in_s, size - 8);

    if (0 != scp_tcp_force_recv(c->in_sck, c->in_s->data, (size - 8)))
    {
        log_message(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: network error", __LINE__);
        return SCP_SERVER_STATE_NETWORK_ERR;
    }

    c->in_s->end = c->in_s->data + (size - 8);

    /* reading command set */
    in_uint16_be(c->in_s, cmdset);

    /* if we are starting a management session */
    if (cmdset == SCP_COMMAND_SET_MANAGE)
    {
        log_message(LOG_LEVEL_DEBUG, "[v1s:%d] requested management connection", __LINE__);
        /* should return SCP_SERVER_STATE_START_MANAGE */
        return scp_v1s_mng_accept(c, s);
    }

    /* if we started with resource sharing... */
    if (cmdset == SCP_COMMAND_SET_RSR)
    {
        log_message(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: sequence error", __LINE__);
        return SCP_SERVER_STATE_SEQUENCE_ERR;
    }

    /* reading command */
    in_uint16_be(c->in_s, cmd);

    if (cmd != 1)
    {
        log_message(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: sequence error", __LINE__);
        return SCP_SERVER_STATE_SEQUENCE_ERR;
    }

    session = scp_session_create();

    if (NULL == session)
    {
        log_message(LOG_LEVEL_WARNING,
                    "[v1s:%d] connection aborted: internal error "
                    "(malloc returned NULL)", __LINE__);
        result = SCP_SERVER_STATE_INTERNAL_ERR;
    }
    else
    {
        result = scp_v1s_init_session(c, session);
        if (result != SCP_SERVER_STATE_OK)
        {
            scp_session_destroy(session);
            session = NULL;
        }
    }

    /* returning the struct */
    (*s) = session;

    return result;
}

enum SCP_SERVER_STATES_E
scp_v1s_deny_connection(struct SCP_CONNECTION *c, const char *reason)
{
    int rlen;

    init_stream(c->out_s, c->out_s->size);

    /* forcing message not to exceed 64k */
    rlen = g_strlen(reason);

    if (rlen > 65535)
    {
        rlen = 65535;
    }

    out_uint32_be(c->out_s, 1);
    /* packet size: 4 + 4 + 2 + 2 + 2 + strlen(reason)*/
    /* version + size + cmdset + cmd + msglen + msg */
    out_uint32_be(c->out_s, rlen + 14);
    out_uint16_be(c->out_s, SCP_COMMAND_SET_DEFAULT);
    out_uint16_be(c->out_s, 2);
    out_uint16_be(c->out_s, rlen);
    out_uint8p(c->out_s, reason, rlen);

    if (0 != scp_tcp_force_send(c->in_sck, c->out_s->data, rlen + 14))
    {
        log_message(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: network error", __LINE__);
        return SCP_SERVER_STATE_NETWORK_ERR;
    }

    return SCP_SERVER_STATE_END;
}

enum SCP_SERVER_STATES_E
scp_v1s_request_password(struct SCP_CONNECTION *c, struct SCP_SESSION *s,
                         const char *reason)
{
    tui32 version;
    tui32 size;
    tui16 cmdset;
    tui16 cmd;
    int rlen;
    char buf[256];

    init_stream(c->in_s, c->in_s->size);
    init_stream(c->out_s, c->out_s->size);

    /* forcing message not to exceed 64k */
    rlen = g_strlen(reason);

    if (rlen > 65535)
    {
        rlen = 65535;
    }

    /* send password request */
    version = 1;
    cmd = 3;

    out_uint32_be(c->out_s, version);                 /* version */
    out_uint32_be(c->out_s, 14 + rlen);               /* size    */
    out_uint16_be(c->out_s, SCP_COMMAND_SET_DEFAULT); /* cmdset  */
    out_uint16_be(c->out_s, cmd);                     /* cmd     */

    out_uint16_be(c->out_s, rlen);
    out_uint8p(c->out_s, reason, rlen);

    if (0 != scp_tcp_force_send(c->in_sck, c->out_s->data, 14 + rlen))
    {
        log_message(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: network error", __LINE__);
        return SCP_SERVER_STATE_NETWORK_ERR;
    }

    /* receive password & username */
    if (0 != scp_tcp_force_recv(c->in_sck, c->in_s->data, 8))
    {
        log_message(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: network error", __LINE__);
        return SCP_SERVER_STATE_NETWORK_ERR;
    }

    in_uint32_be(c->in_s, version);

    if (version != 1)
    {
        log_message(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: version error", __LINE__);
        return SCP_SERVER_STATE_VERSION_ERR;
    }

    in_uint32_be(c->in_s, size);

    /* Check the message is big enough for the header, the command set, and
     * the command (but not too big) */
    if (size < (8 + 2 + 2) || size > SCP_MAX_MESSAGE_SIZE)
    {
        log_message(LOG_LEVEL_WARNING,
                    "[v1s:%d] connection aborted: size error",
                    __LINE__);
        return SCP_SERVER_STATE_SIZE_ERR;
    }

    init_stream(c->in_s, size - 8);

    if (0 != scp_tcp_force_recv(c->in_sck, c->in_s->data, (size - 8)))
    {
        log_message(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: network error", __LINE__);
        return SCP_SERVER_STATE_NETWORK_ERR;
    }

    c->in_s->end = c->in_s->data + (size - 8);

    in_uint16_be(c->in_s, cmdset);

    if (cmdset != SCP_COMMAND_SET_DEFAULT)
    {
        log_message(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: sequence error", __LINE__);
        return SCP_SERVER_STATE_SEQUENCE_ERR;
    }

    in_uint16_be(c->in_s, cmd);

    if (cmd != 4)
    {
        log_message(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: sequence error", __LINE__);
        return SCP_SERVER_STATE_SEQUENCE_ERR;
    }

    /* reading username */
    if (!in_string8(c->in_s, buf, "username", __LINE__))
    {
        return SCP_SERVER_STATE_SIZE_ERR;
    }

    if (0 != scp_session_set_username(s, buf))
    {
        log_message(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: internal error", __LINE__);
        return SCP_SERVER_STATE_INTERNAL_ERR;
    }

    /* reading password */
    if (!in_string8(c->in_s, buf, "passwd", __LINE__))
    {
        return SCP_SERVER_STATE_SIZE_ERR;
    }

    if (0 != scp_session_set_password(s, buf))
    {
        log_message(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: internal error", __LINE__);
        return SCP_SERVER_STATE_INTERNAL_ERR;
    }

    return SCP_SERVER_STATE_OK;
}

/* 020 */
enum SCP_SERVER_STATES_E
scp_v1s_request_pwd_change(struct SCP_CONNECTION *c, char *reason, char *npw)
{
    return SCP_SERVER_STATE_INTERNAL_ERR;
}

/* 023 */
enum SCP_SERVER_STATES_E
scp_v1s_pwd_change_error(struct SCP_CONNECTION *c, char *error, int retry, char *npw)
{
    return SCP_SERVER_STATE_INTERNAL_ERR;
}

/* 030 */
enum SCP_SERVER_STATES_E
scp_v1s_connect_new_session(struct SCP_CONNECTION *c, SCP_DISPLAY d)
{
    /* send password request */
    tui32 version = 1;
    tui32 size = 14;
    tui16 cmd = 30;

    init_stream(c->out_s, c->out_s->size);

    out_uint32_be(c->out_s, version);                 /* version */
    out_uint32_be(c->out_s, size);                    /* size    */
    out_uint16_be(c->out_s, SCP_COMMAND_SET_DEFAULT); /* cmdset  */
    out_uint16_be(c->out_s, cmd);                     /* cmd     */

    out_uint16_be(c->out_s, d);                       /* display */

    if (0 != scp_tcp_force_send(c->in_sck, c->out_s->data, 14))
    {
        log_message(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: network error", __LINE__);
        return SCP_SERVER_STATE_NETWORK_ERR;
    }

    return SCP_SERVER_STATE_OK;
}

/* 032 */
enum SCP_SERVER_STATES_E
scp_v1s_connection_error(struct SCP_CONNECTION *c, const char *error)
{
    tui16 len;

    len = g_strlen(error);
    init_stream(c->out_s, c->out_s->size);

    out_uint32_be(c->out_s, 1);
    /* packet size: 4 + 4 + 2 + 2 + len */
    /* version + size + cmdset + cmd */
    out_uint32_be(c->out_s, (12 + len));
    out_uint16_be(c->out_s, SCP_COMMAND_SET_DEFAULT);
    out_uint16_be(c->out_s, SCP_CMD_CONN_ERROR);

    if (0 != scp_tcp_force_send(c->in_sck, c->out_s->data, (12 + len)))
    {
        return SCP_SERVER_STATE_NETWORK_ERR;
    }

    return SCP_SERVER_STATE_END;
}

/* 040 */
enum SCP_SERVER_STATES_E
scp_v1s_list_sessions(struct SCP_CONNECTION *c, int sescnt, struct SCP_DISCONNECTED_SESSION *ds, SCP_SID *sid)
{
    tui32 version = 1;
    tui32 size = 12;
    tui16 cmd = 40;
    int pktcnt;
    int idx;
    int sidx;
    int pidx;
    struct SCP_DISCONNECTED_SESSION *cds;

    /* first we send a notice that we have some disconnected sessions */
    init_stream(c->out_s, c->out_s->size);

    out_uint32_be(c->out_s, version);                 /* version */
    out_uint32_be(c->out_s, size);                    /* size    */
    out_uint16_be(c->out_s, SCP_COMMAND_SET_DEFAULT); /* cmdset  */
    out_uint16_be(c->out_s, cmd);                     /* cmd     */

    if (0 != scp_tcp_force_send(c->in_sck, c->out_s->data, size))
    {
        log_message(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: network error", __LINE__);
        return SCP_SERVER_STATE_NETWORK_ERR;
    }

    /* then we wait for client ack */

    /*
     * Maybe this message could say if the session should be resized on
     * server side or client side.
     */
    init_stream(c->in_s, c->in_s->size);

    if (0 != scp_tcp_force_recv(c->in_sck, c->in_s->data, 8))
    {
        log_message(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: network error", __LINE__);
        return SCP_SERVER_STATE_NETWORK_ERR;
    }

    in_uint32_be(c->in_s, version);

    if (version != 1)
    {
        log_message(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: version error", __LINE__);
        return SCP_SERVER_STATE_VERSION_ERR;
    }

    in_uint32_be(c->in_s, size);

    /* Check the message is big enough for the header, the command set, and
     * the command (but not too big) */
    if (size < (8 + 2 + 2) || size > SCP_MAX_MESSAGE_SIZE)
    {
        log_message(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: size error", __LINE__);
        return SCP_SERVER_STATE_SIZE_ERR;
    }

    init_stream(c->in_s, size - 8);

    if (0 != scp_tcp_force_recv(c->in_sck, c->in_s->data, (size - 8)))
    {
        log_message(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: network error", __LINE__);
        return SCP_SERVER_STATE_NETWORK_ERR;
    }

    c->in_s->end = c->in_s->data + (size - 8);

    in_uint16_be(c->in_s, cmd);

    if (cmd != SCP_COMMAND_SET_DEFAULT)
    {
        log_message(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: sequence error", __LINE__);
        return SCP_SERVER_STATE_SEQUENCE_ERR;
    }

    in_uint16_be(c->in_s, cmd);

    if (cmd != 41)
    {
        log_message(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: sequence error", __LINE__);
        return SCP_SERVER_STATE_SEQUENCE_ERR;
    }

    /* calculating the number of packets to send */
    pktcnt = sescnt / SCP_SERVER_MAX_LIST_SIZE;

    if ((sescnt % SCP_SERVER_MAX_LIST_SIZE) != 0)
    {
        pktcnt++;
    }

    for (idx = 0; idx < pktcnt; idx++)
    {
        /* ok, we send session session list */
        init_stream(c->out_s, c->out_s->size);

        /* size: ver+size+cmdset+cmd+sescnt+continue+count */
        size = 4 + 4 + 2 + 2 + 4 + 1 + 1;

        /* header */
        cmd = 42;
        s_push_layer(c->out_s, channel_hdr, 8);
        out_uint16_be(c->out_s, SCP_COMMAND_SET_DEFAULT);
        out_uint16_be(c->out_s, cmd);

        /* session count */
        out_uint32_be(c->out_s, sescnt);

        /* setting the continue flag */
        if ((idx + 1)*SCP_SERVER_MAX_LIST_SIZE >= sescnt)
        {
            out_uint8(c->out_s, 0);
            /* setting session count for this packet */
            pidx = sescnt - (idx * SCP_SERVER_MAX_LIST_SIZE);
            out_uint8(c->out_s, pidx);
        }
        else
        {
            out_uint8(c->out_s, 1);
            /* setting session count for this packet */
            pidx = SCP_SERVER_MAX_LIST_SIZE;
            out_uint8(c->out_s, pidx);
        }

        /* adding session descriptors */
        for (sidx = 0; sidx < pidx; sidx++)
        {
            /* shortcut to the current session to send */
            cds = ds + ((idx) * SCP_SERVER_MAX_LIST_SIZE) + sidx;

            /* session data */
            out_uint32_be(c->out_s, cds->SID); /* session id */
            out_uint8(c->out_s, cds->type);
            out_uint16_be(c->out_s, cds->height);
            out_uint16_be(c->out_s, cds->width);
            out_uint8(c->out_s, cds->bpp);
            out_uint8(c->out_s, cds->idle_days);
            out_uint8(c->out_s, cds->idle_hours);
            out_uint8(c->out_s, cds->idle_minutes);
            size += 13;

            out_uint16_be(c->out_s, cds->conn_year);
            out_uint8(c->out_s, cds->conn_month);
            out_uint8(c->out_s, cds->conn_day);
            out_uint8(c->out_s, cds->conn_hour);
            out_uint8(c->out_s, cds->conn_minute);
            out_uint8(c->out_s, cds->addr_type);
            size += 7;

            if (cds->addr_type == SCP_ADDRESS_TYPE_IPV4)
            {
                in_uint32_be(c->out_s, cds->ipv4addr);
                size += 4;
            }
            else if (cds->addr_type == SCP_ADDRESS_TYPE_IPV6)
            {
                in_uint8a(c->out_s, cds->ipv6addr, 16);
                size += 16;
            }
        }

        s_pop_layer(c->out_s, channel_hdr);
        out_uint32_be(c->out_s, version);
        out_uint32_be(c->out_s, size);

        if (0 != scp_tcp_force_send(c->in_sck, c->out_s->data, size))
        {
            log_message(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: network error", __LINE__);
            return SCP_SERVER_STATE_NETWORK_ERR;
        }
    }

    /* we get the response */
    init_stream(c->in_s, c->in_s->size);

    if (0 != scp_tcp_force_recv(c->in_sck, c->in_s->data, (8)))
    {
        log_message(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: network error", __LINE__);
        return SCP_SERVER_STATE_NETWORK_ERR;
    }

    in_uint32_be(c->in_s, version);

    if (version != 1)
    {
        log_message(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: version error", __LINE__);
        return SCP_SERVER_STATE_VERSION_ERR;
    }

    in_uint32_be(c->in_s, size);

    /* Check the message is big enough for the header, the command set, and
     * the command (but not too big) */
    if (size < (8 + 2 + 2) || size > SCP_MAX_MESSAGE_SIZE)
    {
        log_message(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: size error", __LINE__);
        return SCP_SERVER_STATE_SIZE_ERR;
    }

    /* rest of the packet */
    init_stream(c->in_s, size - 8);

    if (0 != scp_tcp_force_recv(c->in_sck, c->in_s->data, (size - 8)))
    {
        log_message(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: network error", __LINE__);
        return SCP_SERVER_STATE_NETWORK_ERR;
    }

    c->in_s->end = c->in_s->data + (size - 8);

    in_uint16_be(c->in_s, cmd);

    if (cmd != SCP_COMMAND_SET_DEFAULT)
    {
        log_message(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: sequence error", __LINE__);
        return SCP_SERVER_STATE_SEQUENCE_ERR;
    }

    in_uint16_be(c->in_s, cmd);

    if (cmd == 43)
    {
        if (!s_check_rem(c->in_s, 4))
        {
            log_message(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: missing session", __LINE__);
            return SCP_SERVER_STATE_SIZE_ERR;
        }
        /* select session */
        in_uint32_be(c->in_s, (*sid));

        /* checking sid value */
        for (idx = 0; idx < sescnt; idx++)
        {
            /* the sid is valid */
            if (ds[idx].SID == (*sid))
            {
                /* ok, session selected */
                return SCP_SERVER_STATE_OK;
            }
        }

        /* if we got here, the requested sid wasn't one from the list we sent */
        /* we should kill the connection                                      */
        log_message(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: internal error (no such session in list)", __LINE__);
        return SCP_SERVER_STATE_INTERNAL_ERR;
    }
    else if (cmd == 44)
    {
        /* cancel connection */
        return SCP_SERVER_STATE_SELECTION_CANCEL;
    }
    //  else if (cmd == 45)
    //  {
    //    /* force new connection */
    //    return SCP_SERVER_STATE_FORCE_NEW;
    //  }
    else
    {
        /* wrong response */
        log_message(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: sequence error", __LINE__);
        return SCP_SERVER_STATE_SEQUENCE_ERR;
    }

    return SCP_SERVER_STATE_OK;
}

/* 046 was: 031 struct SCP_DISCONNECTED_SESSION* ds, */
enum SCP_SERVER_STATES_E
scp_v1s_reconnect_session(struct SCP_CONNECTION *c, SCP_DISPLAY d)
{
    tui32 version = 1;
    tui32 size = 14;
    tui16 cmd = 46;

    /* ok, we send session data and display */
    init_stream(c->out_s, c->out_s->size);

    /* header */
    out_uint32_be(c->out_s, version);
    out_uint32_be(c->out_s, size);
    out_uint16_be(c->out_s, SCP_COMMAND_SET_DEFAULT);
    out_uint16_be(c->out_s, cmd);

    /* session data */
    out_uint16_be(c->out_s, d); /* session display */
    /*out_uint8(c->out_s, ds->type);
    out_uint16_be(c->out_s, ds->height);
    out_uint16_be(c->out_s, ds->width);
    out_uint8(c->out_s, ds->bpp);
    out_uint8(c->out_s, ds->idle_days);
    out_uint8(c->out_s, ds->idle_hours);
    out_uint8(c->out_s, ds->idle_minutes);*/
    /* these last three are not really needed... */

    if (0 != scp_tcp_force_send(c->in_sck, c->out_s->data, size))
    {
        log_message(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: network error", __LINE__);
        return SCP_SERVER_STATE_NETWORK_ERR;
    }

    return SCP_SERVER_STATE_OK;
}

#endif
