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
#include "string_calls.h"

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
        LOG(LOG_LEVEL_WARNING,
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
            LOG(LOG_LEVEL_WARNING,
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
scp_v1s_init_session(struct trans *t, struct SCP_SESSION *session)
{
    tui8 type;
    tui16 height;
    tui16 width;
    tui8 bpp;
    tui8 sz;
    char buf[256];
    struct stream *in_s = t->in_s;

    scp_session_set_version(session, 1);

    /* Check there's data for the session type, the height, the width, the
     * bpp, the resource sharing indicator and the locale */
    if (!s_check_rem(in_s, 1 + 2 + 2 + 1 + 1 + 17))
    {
        LOG(LOG_LEVEL_WARNING,
            "[v1s:%d] connection aborted: short packet",
            __LINE__);
        return SCP_SERVER_STATE_SIZE_ERR;
    }

    in_uint8(in_s, type);

    if ((type != SCP_SESSION_TYPE_XVNC) && (type != SCP_SESSION_TYPE_XRDP))
    {
        LOG(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: unknown session type", __LINE__);
        return SCP_SERVER_STATE_SESSION_TYPE_ERR;
    }

    scp_session_set_type(session, type);

    in_uint16_be(in_s, height);
    scp_session_set_height(session, height);
    in_uint16_be(in_s, width);
    scp_session_set_width(session, width);
    in_uint8(in_s, bpp);
    if (0 != scp_session_set_bpp(session, bpp))
    {
        LOG(LOG_LEVEL_WARNING,
            "[v1s:%d] connection aborted: unsupported bpp: %d",
            __LINE__, bpp);
        return SCP_SERVER_STATE_INTERNAL_ERR;
    }
    in_uint8(in_s, sz);
    scp_session_set_rsr(session, sz);
    in_uint8a(in_s, buf, 17);
    buf[17] = '\0';
    scp_session_set_locale(session, buf);

    /* Check there's enough data left for at least an IPv4 address (+len) */
    if (!s_check_rem(in_s, 1 + 4))
    {
        LOG(LOG_LEVEL_WARNING,
            "[v1s:%d] connection aborted: IP addr len missing",
            __LINE__);
        return SCP_SERVER_STATE_SIZE_ERR;
    }

    in_uint8(in_s, sz);

    if (sz == SCP_ADDRESS_TYPE_IPV4)
    {
        tui32 ipv4;
        in_uint32_be(in_s, ipv4);
        scp_session_set_addr(session, sz, &ipv4);
    }
    else if (sz == SCP_ADDRESS_TYPE_IPV6)
    {
        if (!s_check_rem(in_s, 16))
        {
            LOG(LOG_LEVEL_WARNING,
                "[v1s:%d] connection aborted: IP addr missing",
                __LINE__);
            return SCP_SERVER_STATE_SIZE_ERR;
        }
        in_uint8a(in_s, buf, 16);
        scp_session_set_addr(session, sz, buf);
    }

    /* reading hostname */
    if (!in_string8(in_s, buf, "hostname", __LINE__))
    {
        return SCP_SERVER_STATE_SIZE_ERR;
    }

    if (0 != scp_session_set_hostname(session, buf))
    {
        LOG(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: internal error", __LINE__);
        return SCP_SERVER_STATE_INTERNAL_ERR;
    }

    /* reading username */
    if (!in_string8(in_s, buf, "username", __LINE__))
    {
        return SCP_SERVER_STATE_SIZE_ERR;
    }

    if (0 != scp_session_set_username(session, buf))
    {
        LOG(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: internal error", __LINE__);
        return SCP_SERVER_STATE_INTERNAL_ERR;
    }

    /* reading password */
    if (!in_string8(in_s, buf, "passwd", __LINE__))
    {
        return SCP_SERVER_STATE_SIZE_ERR;
    }

    if (0 != scp_session_set_password(session, buf))
    {
        LOG(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: internal error", __LINE__);
        return SCP_SERVER_STATE_INTERNAL_ERR;
    }

    return SCP_SERVER_STATE_OK;
}

/* server API */
enum SCP_SERVER_STATES_E
scp_v1s_accept(struct trans *t, struct SCP_SESSION *s)
{
    tui32 size;
    tui16 cmdset;
    tui16 cmd;
    struct stream *in_s = t->in_s;
    enum SCP_SERVER_STATES_E result;

    in_uint32_be(in_s, size);

    /* Check the message is big enough for the header, the command set, and
     * the command (but not too big) */
    if (size < (8 + 2 + 2) || size > SCP_MAX_MESSAGE_SIZE)
    {
        LOG(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: size error", __LINE__);
        return SCP_SERVER_STATE_SIZE_ERR;
    }

    /* reading command set */
    in_uint16_be(in_s, cmdset);

    /* if we are starting a management session */
    if (cmdset == SCP_COMMAND_SET_MANAGE)
    {
        LOG(LOG_LEVEL_DEBUG, "[v1s:%d] requested management connection", __LINE__);
        /* should return SCP_SERVER_STATE_START_MANAGE */
        return scp_v1s_mng_accept(t, s);
    }

    /* if we started with resource sharing... */
    if (cmdset == SCP_COMMAND_SET_RSR)
    {
        LOG(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: sequence error", __LINE__);
        return SCP_SERVER_STATE_SEQUENCE_ERR;
    }

    /* reading command */
    in_uint16_be(in_s, cmd);

    switch (cmd)
    {
        case SCP_CMD_LOGIN:
            s->current_cmd = cmd;
            result = scp_v1s_init_session(t, s);
            break;

        case SCP_CMD_RESEND_CREDS:
            result = scp_v1s_accept_password_reply(t, s);
            s->current_cmd = SCP_CMD_LOGIN; /* Caller re-parses credentials */
            break;

        default:
            LOG(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: sequence "
                "error. Unrecognised cmd %d", __LINE__, cmd);
            result = SCP_SERVER_STATE_SEQUENCE_ERR;
    }

    return result;
}

enum SCP_SERVER_STATES_E
scp_v1s_deny_connection(struct trans *t, const char *reason)
{
    struct stream *out_s;
    int rlen;

    /* forcing message not to exceed 64k */
    rlen = g_strlen(reason);
    if (rlen > 65535)
    {
        rlen = 65535;
    }
    out_s = trans_get_out_s(t, rlen + 14);
    out_uint32_be(out_s, 1); /* version */
    /* packet size: 4 + 4 + 2 + 2 + 2 + strlen(reason) */
    /* version + size + cmdset + cmd + msglen + msg */
    out_uint32_be(out_s, rlen + 14);
    out_uint16_be(out_s, SCP_COMMAND_SET_DEFAULT);
    out_uint16_be(out_s, SCP_REPLY_LOGIN_DENIED);
    out_uint16_be(out_s, rlen);
    out_uint8p(out_s, reason, rlen);
    s_mark_end(out_s);
    if (0 != trans_force_write(t))
    {
        LOG(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: network error", __LINE__);
        return SCP_SERVER_STATE_NETWORK_ERR;
    }
    return SCP_SERVER_STATE_END;
}

enum SCP_SERVER_STATES_E
scp_v1s_request_password(struct trans *t, struct SCP_SESSION *s,
                         const char *reason)
{
    struct stream *out_s;
    int rlen;

    /* forcing message not to exceed 64k */
    rlen = g_strlen(reason);
    if (rlen > 65535)
    {
        rlen = 65535;
    }
    out_s = trans_get_out_s(t, rlen + 14);
    out_uint32_be(out_s, 1); /* version */
    /* packet size: 4 + 4 + 2 + 2 + 2 + strlen(reason) */
    /* version + size + cmdset + cmd + msglen + msg */
    out_uint32_be(out_s, rlen + 14);
    out_uint16_be(out_s, SCP_COMMAND_SET_DEFAULT);
    out_uint16_be(out_s, SCP_REPLY_REREQUEST_CREDS);
    out_uint16_be(out_s, rlen);
    out_uint8p(out_s, reason, rlen);
    s_mark_end(out_s);
    if (0 != trans_force_write(t))
    {
        LOG(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: network error", __LINE__);
        return SCP_SERVER_STATE_NETWORK_ERR;
    }

    return SCP_SERVER_STATE_OK;
}

enum SCP_SERVER_STATES_E
scp_v1s_accept_password_reply(struct trans *t, struct SCP_SESSION *s)
{
    struct stream *in_s;
    char buf[257];

    in_s = t->in_s;
    buf[256] = '\0';

    /* reading username */
    if (!in_string8(in_s, buf, "username", __LINE__))
    {
        return SCP_SERVER_STATE_SIZE_ERR;
    }
    if (0 != scp_session_set_username(s, buf))
    {
        LOG(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: internal error",
            __LINE__);
        return SCP_SERVER_STATE_INTERNAL_ERR;
    }

    /* reading password */
    if (!in_string8(in_s, buf, "passwd", __LINE__))
    {
        return SCP_SERVER_STATE_SIZE_ERR;
    }

    if (0 != scp_session_set_password(s, buf))
    {
        LOG(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: internal error",
            __LINE__);
        return SCP_SERVER_STATE_INTERNAL_ERR;
    }

    return SCP_SERVER_STATE_OK;
}

enum SCP_SERVER_STATES_E
scp_v1s_request_pwd_change(struct trans *t, char *reason, char *npw)
{
    return SCP_SERVER_STATE_INTERNAL_ERR;
}

enum SCP_SERVER_STATES_E
scp_v1s_pwd_change_error(struct trans *t, char *error, int retry, char *npw)
{
    return SCP_SERVER_STATE_INTERNAL_ERR;
}

enum SCP_SERVER_STATES_E
scp_v1s_connect_new_session(struct trans *t, SCP_DISPLAY d)
{
    struct stream *out_s;

    out_s = trans_get_out_s(t, 14);
    out_uint32_be(out_s, 1); /* version */
    /* packet size: 4 + 4 + 2 + 2 + 2 */
    /* version + size + cmdset + cmd + msglen + msg */
    out_uint32_be(out_s, 14);
    out_uint16_be(out_s, SCP_COMMAND_SET_DEFAULT);
    out_uint16_be(out_s, SCP_REPLY_NEW_SESSION);
    out_uint16_be(out_s, d);
    s_mark_end(out_s);
    if (0 != trans_force_write(t))
    {
        LOG(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: network error", __LINE__);
        return SCP_SERVER_STATE_NETWORK_ERR;
    }
    return SCP_SERVER_STATE_OK;
}

enum SCP_SERVER_STATES_E
scp_v1s_connection_error(struct trans *t, const char *error)
{
    struct stream *out_s;
    tui16 len;

    len = g_strlen(error);
    if (len > 8192 - 12)
    {
        len = 8192 - 12;
    }
    out_s = trans_get_out_s(t, 12 + len);
    out_uint32_be(out_s, 1); /* version */
    /* packet size: 4 + 4 + 2 + 2 + len */
    /* version + size + cmdset + cmd */
    out_uint32_be(out_s, 12 + len);
    out_uint16_be(out_s, SCP_COMMAND_SET_DEFAULT);
    out_uint16_be(out_s, SCP_REPLY_CMD_CONN_ERROR);
    out_uint8a(out_s, error, len);
    s_mark_end(out_s);
    if (0 != trans_force_write(t))
    {
        return SCP_SERVER_STATE_NETWORK_ERR;
    }
    return SCP_SERVER_STATE_END;
}

#if 0
enum SCP_SERVER_STATES_E
scp_v1s_list_sessions(struct SCP_CONNECTION *c, int sescnt, struct SCP_DISCONNECTED_SESSION *ds, SCP_SID *sid)
{
    tui32 version = 1;
    int size = 12;
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
        LOG(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: network error", __LINE__);
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
        LOG(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: network error", __LINE__);
        return SCP_SERVER_STATE_NETWORK_ERR;
    }

    in_uint32_be(c->in_s, version);

    if (version != 1)
    {
        LOG(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: version error", __LINE__);
        return SCP_SERVER_STATE_VERSION_ERR;
    }

    in_uint32_be(c->in_s, size);

    /* Check the message is big enough for the header, the command set, and
     * the command (but not too big) */
    if (size < (8 + 2 + 2) || size > SCP_MAX_MESSAGE_SIZE)
    {
        LOG(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: size error", __LINE__);
        return SCP_SERVER_STATE_SIZE_ERR;
    }

    init_stream(c->in_s, size - 8);

    if (0 != scp_tcp_force_recv(c->in_sck, c->in_s->data, (size - 8)))
    {
        LOG(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: network error", __LINE__);
        return SCP_SERVER_STATE_NETWORK_ERR;
    }

    c->in_s->end = c->in_s->data + (size - 8);

    in_uint16_be(c->in_s, cmd);

    if (cmd != SCP_COMMAND_SET_DEFAULT)
    {
        LOG(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: sequence error", __LINE__);
        return SCP_SERVER_STATE_SEQUENCE_ERR;
    }

    in_uint16_be(c->in_s, cmd);

    if (cmd != 41)
    {
        LOG(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: sequence error", __LINE__);
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
            LOG(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: network error", __LINE__);
            return SCP_SERVER_STATE_NETWORK_ERR;
        }
    }

    /* we get the response */
    init_stream(c->in_s, c->in_s->size);

    if (0 != scp_tcp_force_recv(c->in_sck, c->in_s->data, (8)))
    {
        LOG(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: network error", __LINE__);
        return SCP_SERVER_STATE_NETWORK_ERR;
    }

    in_uint32_be(c->in_s, version);

    if (version != 1)
    {
        LOG(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: version error", __LINE__);
        return SCP_SERVER_STATE_VERSION_ERR;
    }

    in_uint32_be(c->in_s, size);

    /* Check the message is big enough for the header, the command set, and
     * the command (but not too big) */
    if (size < (8 + 2 + 2) || size > SCP_MAX_MESSAGE_SIZE)
    {
        LOG(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: size error", __LINE__);
        return SCP_SERVER_STATE_SIZE_ERR;
    }

    /* rest of the packet */
    init_stream(c->in_s, size - 8);

    if (0 != scp_tcp_force_recv(c->in_sck, c->in_s->data, (size - 8)))
    {
        LOG(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: network error", __LINE__);
        return SCP_SERVER_STATE_NETWORK_ERR;
    }

    c->in_s->end = c->in_s->data + (size - 8);

    in_uint16_be(c->in_s, cmd);

    if (cmd != SCP_COMMAND_SET_DEFAULT)
    {
        LOG(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: sequence error", __LINE__);
        return SCP_SERVER_STATE_SEQUENCE_ERR;
    }

    in_uint16_be(c->in_s, cmd);

    if (cmd == 43)
    {
        if (!s_check_rem(c->in_s, 4))
        {
            LOG(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: missing session", __LINE__);
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
        LOG(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: internal error (no such session in list)", __LINE__);
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
        LOG(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: sequence error", __LINE__);
        return SCP_SERVER_STATE_SEQUENCE_ERR;
    }

    return SCP_SERVER_STATE_OK;
}
#endif

enum SCP_SERVER_STATES_E
scp_v1s_list_sessions40(struct trans *t)
{
    struct stream *out_s;

    out_s = trans_get_out_s(t, 12);
    /* send a notice that we have some disconnected sessions */
    out_uint32_be(out_s, 1);                         /* version */
    out_uint32_be(out_s, 12);                        /* size    */
    out_uint16_be(out_s, SCP_COMMAND_SET_DEFAULT);   /* cmdset  */
    out_uint16_be(out_s, SCP_REPLY_USER_SESSIONS_EXIST);/* cmd     */
    s_mark_end(out_s);
    if (0 != trans_force_write(t))
    {
        LOG(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: network error", __LINE__);
        return SCP_SERVER_STATE_NETWORK_ERR;
    }
    return SCP_SERVER_STATE_OK;
}

enum SCP_SERVER_STATES_E
scp_v1s_list_sessions42(struct trans *t, int sescnt, struct SCP_DISCONNECTED_SESSION *ds)
{
    int pktcnt;
    int idx;
    int sidx;
    int pidx;
    int size;
    struct stream *out_s;
    struct SCP_DISCONNECTED_SESSION *cds;

    /* calculating the number of packets to send */
    pktcnt = sescnt / SCP_SERVER_MAX_LIST_SIZE;
    if ((sescnt % SCP_SERVER_MAX_LIST_SIZE) != 0)
    {
        pktcnt++;
    }

    for (idx = 0; idx < pktcnt; idx++)
    {
        out_s = trans_get_out_s(t, 8192);

        /* size: ver+size+cmdset+cmd+sescnt+continue+count */
        size = 4 + 4 + 2 + 2 + 4 + 1 + 1;

        /* header */
        s_push_layer(out_s, channel_hdr, 8);
        out_uint16_be(out_s, SCP_COMMAND_SET_DEFAULT);
        out_uint16_be(out_s, SCP_REPLY_SESSIONS_INFO);

        /* session count */
        out_uint32_be(out_s, sescnt);

        /* setting the continue flag */
        if ((idx + 1) * SCP_SERVER_MAX_LIST_SIZE >= sescnt)
        {
            out_uint8(out_s, 0);
            /* setting session count for this packet */
            pidx = sescnt - (idx * SCP_SERVER_MAX_LIST_SIZE);
            out_uint8(out_s, pidx);
        }
        else
        {
            out_uint8(out_s, 1);
            /* setting session count for this packet */
            pidx = SCP_SERVER_MAX_LIST_SIZE;
            out_uint8(out_s, pidx);
        }

        /* adding session descriptors */
        for (sidx = 0; sidx < pidx; sidx++)
        {
            /* shortcut to the current session to send */
            cds = ds + (idx * SCP_SERVER_MAX_LIST_SIZE) + sidx;

            /* session data */
            out_uint32_be(out_s, cds->SID); /* session id */
            out_uint8(out_s, cds->type);
            out_uint16_be(out_s, cds->height);
            out_uint16_be(out_s, cds->width);
            out_uint8(out_s, cds->bpp);
            out_uint8(out_s, cds->idle_days);
            out_uint8(out_s, cds->idle_hours);
            out_uint8(out_s, cds->idle_minutes);
            size += 13;

            out_uint16_be(out_s, cds->conn_year);
            out_uint8(out_s, cds->conn_month);
            out_uint8(out_s, cds->conn_day);
            out_uint8(out_s, cds->conn_hour);
            out_uint8(out_s, cds->conn_minute);
            out_uint8(out_s, cds->addr_type);
            size += 7;

            if (cds->addr_type == SCP_ADDRESS_TYPE_IPV4)
            {
                in_uint32_be(out_s, cds->ipv4addr);
                size += 4;
            }
            else if (cds->addr_type == SCP_ADDRESS_TYPE_IPV6)
            {
                in_uint8a(out_s, cds->ipv6addr, 16);
                size += 16;
            }
        }
        s_mark_end(out_s);
        s_pop_layer(out_s, channel_hdr);
        out_uint32_be(out_s, 1); /* version */
        out_uint32_be(out_s, size);

        if (0 != trans_force_write(t))
        {
            LOG(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: network error", __LINE__);
            return SCP_SERVER_STATE_NETWORK_ERR;
        }
    }
    return SCP_SERVER_STATE_OK;
}

// TODO
enum SCP_SERVER_STATES_E
scp_v1s_accept_list_sessions_reply(int cmd, struct trans *t)
{
    struct SCP_SESSION *s;
    struct stream *in_s;

    s = (struct SCP_SESSION *) (t->callback_data);
    if (s == NULL)
    {
        LOG(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: internal error", __LINE__);
        return SCP_SERVER_STATE_INTERNAL_ERR;
    }
    s->current_cmd = cmd;
    in_s = t->in_s;
    switch (cmd)
    {
        case SCP_CMD_GET_SESSION_LIST:
            break;
        case SCP_CMD_SELECT_SESSION:
            in_uint32_be(in_s, s->return_sid);
            break;
        case SCP_CMD_SELECT_SESSION_CANCEL:
            break;
        case SCP_CMD_FORCE_NEW_CONN:
            break;
        default:
            break;
    }
    return SCP_SERVER_STATE_OK;
}

enum SCP_SERVER_STATES_E
scp_v1s_reconnect_session(struct trans *t, SCP_DISPLAY d)
{
    struct stream *out_s;

    /* ok, we send session data and display */
    out_s = trans_get_out_s(t, 14);
    /* header */
    out_uint32_be(out_s, 1); /* version */
    out_uint32_be(out_s, 14); /* size */
    out_uint16_be(out_s, SCP_COMMAND_SET_DEFAULT);
    out_uint16_be(out_s, SCP_REPLY_SESSION_RECONNECTED); /* cmd */
    /* session data */
    out_uint16_be(out_s, d); /* session display */
    s_mark_end(out_s);

    /*out_uint8(c->out_s, ds->type);
    out_uint16_be(c->out_s, ds->height);
    out_uint16_be(c->out_s, ds->width);
    out_uint8(c->out_s, ds->bpp);
    out_uint8(c->out_s, ds->idle_days);
    out_uint8(c->out_s, ds->idle_hours);
    out_uint8(c->out_s, ds->idle_minutes);*/
    /* these last three are not really needed... */

    if (0 != trans_force_write(t))
    {
        LOG(LOG_LEVEL_WARNING, "[v1s:%d] connection aborted: network error", __LINE__);
        return SCP_SERVER_STATE_NETWORK_ERR;
    }

    return SCP_SERVER_STATE_OK;
}

#endif
