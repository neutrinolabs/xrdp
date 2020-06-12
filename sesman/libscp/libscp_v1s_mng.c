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
 * @file libscp_v1s_mng.c
 * @brief libscp version 1 server api code - session management
 * @author Simone Fedele
 *
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#ifndef LIBSCP_V1S_MNG_C
#define LIBSCP_V1S_MNG_C

#include "libscp_v1s_mng.h"

//extern struct log_config* s_log;

static enum SCP_SERVER_STATES_E
_scp_v1s_mng_check_response(struct SCP_CONNECTION *c, struct SCP_SESSION *s);

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
 *     libscp_v1s.c
 */
static
int in_string8(struct stream *s, char str[], const char *param, int line)
{
    int result;

    if (!s_check_rem(s, 1))
    {
        log_message(LOG_LEVEL_WARNING,
                    "[v1s_mng:%d] connection aborted: %s len missing",
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
                        "[v1s_mng:%d] connection aborted: %s data missing",
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
 * Initialises a V1 management session object
 *
 * At call time, the command set value has been read from the wire, and
 * the command still needs to be processed.
 *
 * @param c Connection
 * @param [out] session pre-allocated session object
 * @return SCP_SERVER_STATE_START_MANAGE for success
 */
static enum SCP_SERVER_STATES_E
scp_v1s_mng_init_session(struct SCP_CONNECTION *c, struct SCP_SESSION *session)
{
    tui32 ipaddr;
    tui16 cmd;
    tui8 sz;
    char buf[256];

    scp_session_set_version(session, 1);

    /* reading command */
    if (!s_check_rem(c->in_s, 2))
    {
        /* Caller should have checked this */
        return SCP_SERVER_STATE_SIZE_ERR;
    }
    in_uint16_be(c->in_s, cmd);

    if (cmd != 1) /* manager login */
    {
        return SCP_SERVER_STATE_SEQUENCE_ERR;
    }

    /* reading username */
    if (!in_string8(c->in_s, buf, "username", __LINE__))
    {
        return SCP_SERVER_STATE_SIZE_ERR;
    }

    if (0 != scp_session_set_username(session, buf))
    {
        return SCP_SERVER_STATE_INTERNAL_ERR;
    }

    /* reading password */
    if (!in_string8(c->in_s, buf, "passwd", __LINE__))
    {
        return SCP_SERVER_STATE_SIZE_ERR;
    }

    if (0 != scp_session_set_password(session, buf))
    {
        return SCP_SERVER_STATE_INTERNAL_ERR;
    }

    /* reading remote address
     * Check there's enough data left for at least an IPv4 address (+len) */
    if (!s_check_rem(c->in_s, 1 + 4))
    {
        log_message(LOG_LEVEL_WARNING,
                    "[v1s_mng:%d] connection aborted: IP addr len missing",
                    __LINE__);
        return SCP_SERVER_STATE_SIZE_ERR;
    }

    in_uint8(c->in_s, sz);
    if (sz == SCP_ADDRESS_TYPE_IPV4)
    {
        in_uint32_be(c->in_s, ipaddr);
        scp_session_set_addr(session, sz, &ipaddr);
    }
    else if (sz == SCP_ADDRESS_TYPE_IPV6)
    {
        if (!s_check_rem(c->in_s, 16))
        {
            log_message(LOG_LEVEL_WARNING,
                        "[v1s_mng:%d] connection aborted: IP addr missing",
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
        return SCP_SERVER_STATE_INTERNAL_ERR;
    }

    return SCP_SERVER_STATE_START_MANAGE;
}

enum SCP_SERVER_STATES_E
scp_v1s_mng_accept(struct SCP_CONNECTION *c, struct SCP_SESSION **s)
{
    enum SCP_SERVER_STATES_E result;
    struct SCP_SESSION *session;

    session = scp_session_create();
    if (NULL == session)
    {
        result = SCP_SERVER_STATE_INTERNAL_ERR;
    }
    else
    {
        scp_session_set_type(session, SCP_SESSION_TYPE_MANAGE);

        result = scp_v1s_mng_init_session(c, session);
        if (result != SCP_SERVER_STATE_START_MANAGE)
        {
            scp_session_destroy(session);
            session = NULL;
        }
    }

    (*s) = session;

    return result;
}

/* 002 */
enum SCP_SERVER_STATES_E
scp_v1s_mng_allow_connection(struct SCP_CONNECTION *c, struct SCP_SESSION *s)
{
    init_stream(c->out_s, c->out_s->size);

    out_uint32_be(c->out_s, 1);
    /* packet size: 4 + 4 + 2 + 2 */
    /* version + size + cmdset + cmd */
    out_uint32_be(c->out_s, 12);
    out_uint16_be(c->out_s, SCP_COMMAND_SET_MANAGE);
    out_uint16_be(c->out_s, SCP_CMD_MNG_LOGIN_ALLOW);

    if (0 != scp_tcp_force_send(c->in_sck, c->out_s->data, 12))
    {
        return SCP_SERVER_STATE_NETWORK_ERR;
    }

    return _scp_v1s_mng_check_response(c, s);
}

/* 003 */
enum SCP_SERVER_STATES_E
scp_v1s_mng_deny_connection(struct SCP_CONNECTION *c, const char *reason)
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
    out_uint16_be(c->out_s, SCP_COMMAND_SET_MANAGE);
    out_uint16_be(c->out_s, SCP_CMD_MNG_LOGIN_DENY);
    out_uint16_be(c->out_s, rlen);
    out_uint8p(c->out_s, reason, rlen);

    if (0 != scp_tcp_force_send(c->in_sck, c->out_s->data, rlen + 14))
    {
        return SCP_SERVER_STATE_NETWORK_ERR;
    }

    return SCP_SERVER_STATE_END;
}

/* 006 */
enum SCP_SERVER_STATES_E
scp_v1s_mng_list_sessions(struct SCP_CONNECTION *c, struct SCP_SESSION *s,
                          int sescnt, struct SCP_DISCONNECTED_SESSION *ds)
{
    tui32 version = 1;
    tui32 size = 12;
    tui16 cmd = SCP_CMD_MNG_LIST;
    int pktcnt;
    int idx;
    int sidx;
    int pidx;
    struct SCP_DISCONNECTED_SESSION *cds;

    /* calculating the number of packets to send */
    if (sescnt == 0)
    {
      pktcnt = 1;
    }
    else
    {
      pktcnt = sescnt / SCP_SERVER_MAX_LIST_SIZE;

      if ((sescnt % SCP_SERVER_MAX_LIST_SIZE) != 0)
      {
          pktcnt++;
      }
    }

    for (idx = 0; idx < pktcnt; idx++)
    {
        /* ok, we send session session list */
        init_stream(c->out_s, c->out_s->size);

        /* size: ver+size+cmdset+cmd+sescnt+continue+count */
        size = 4 + 4 + 2 + 2 + 4 + 1 + 1;

        /* header */
        s_push_layer(c->out_s, channel_hdr, 8);
        out_uint16_be(c->out_s, SCP_COMMAND_SET_MANAGE);
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
            log_message(LOG_LEVEL_WARNING, "[v1s_mng:%d] connection aborted: network error", __LINE__);
            return SCP_SERVER_STATE_NETWORK_ERR;
        }
    }

    return _scp_v1s_mng_check_response(c, s);
}

static enum SCP_SERVER_STATES_E
_scp_v1s_mng_check_response(struct SCP_CONNECTION *c, struct SCP_SESSION *s)
{
    tui32 version;
    tui32 size;
    tui16 cmd;
    //   tui8 dim;
    //   char buf[257];

    init_stream(c->in_s, c->in_s->size);

    if (0 != scp_tcp_force_recv(c->in_sck, c->in_s->data, 8))
    {
        log_message(LOG_LEVEL_WARNING, "[v1s_mng:%d] connection aborted: network error", __LINE__);
        return SCP_SERVER_STATE_NETWORK_ERR;
    }

    in_uint32_be(c->in_s, version);

    if (version != 1)
    {
        log_message(LOG_LEVEL_WARNING, "[v1s_mng:%d] connection aborted: version error", __LINE__);
        return SCP_SERVER_STATE_VERSION_ERR;
    }

    in_uint32_be(c->in_s, size);

    /* Check the message is big enough for the header, the command set, and
     * the command (but not too big) */
    if (size < (8 + 2 + 2) || size > SCP_MAX_MESSAGE_SIZE)
    {
        log_message(LOG_LEVEL_WARNING, "[v1s_mng:%d] connection aborted: size error", __LINE__);
        return SCP_SERVER_STATE_SIZE_ERR;
    }

    init_stream(c->in_s, size - 8);

    /* read the rest of the packet */
    if (0 != scp_tcp_force_recv(c->in_sck, c->in_s->data, size - 8))
    {
        log_message(LOG_LEVEL_WARNING, "[v1s_mng:%d] connection aborted: network error", __LINE__);
        return SCP_SERVER_STATE_NETWORK_ERR;
    }

    c->in_s->end = c->in_s->data + (size - 8);

    in_uint16_be(c->in_s, cmd);

    if (cmd != SCP_COMMAND_SET_MANAGE)
    {
        log_message(LOG_LEVEL_WARNING, "[v1s_mng:%d] connection aborted: sequence error", __LINE__);
        return SCP_SERVER_STATE_SEQUENCE_ERR;
    }

    in_uint16_be(c->in_s, cmd);

    if (cmd == SCP_CMD_MNG_LIST_REQ) /* request session list */
    {
        log_message(LOG_LEVEL_INFO, "[v1s_mng:%d] request session list", __LINE__);
        return SCP_SERVER_STATE_MNG_LISTREQ;
    }
    else if (cmd == SCP_CMD_MNG_ACTION) /* execute an action */
    {
        /*in_uint8(c->in_s, dim);
        buf[dim]='\0';
        in_uint8a(c->in_s, buf, dim);
        scp_session_set_errstr(s, buf);*/

        log_message(LOG_LEVEL_INFO, "[v1s_mng:%d] action request", __LINE__);
        return SCP_SERVER_STATE_MNG_ACTION;
    }

    /* else if (cmd == 20) / * password change * /
    {
      in_uint16_be(c->in_s, s->display);

      return SCP_SERVER_STATE_OK;
    }
    else if (cmd == 40) / * session list * /
    {
      return SCP_SERVER_STATE_SESSION_LIST;
    }*/

    log_message(LOG_LEVEL_WARNING, "[v1s_mng:%d] connection aborted: sequence error", __LINE__);
    return SCP_SERVER_STATE_SEQUENCE_ERR;
}

#endif
