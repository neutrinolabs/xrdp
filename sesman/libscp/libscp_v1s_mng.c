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

/* server API */
enum SCP_SERVER_STATES_E
scp_v1s_mng_accept(struct SCP_CONNECTION *c, struct SCP_SESSION **s)
{
    struct SCP_SESSION *session;
    tui32 ipaddr;
    tui16 cmd;
    tui8 sz;
    char buf[257];

    /* reading command */
    in_uint16_be(c->in_s, cmd);

    if (cmd != 1) /* manager login */
    {
        return SCP_SERVER_STATE_SEQUENCE_ERR;
    }

    session = scp_session_create();

    if (0 == session)
    {
        return SCP_SERVER_STATE_INTERNAL_ERR;
    }

    scp_session_set_version(session, 1);
    scp_session_set_type(session, SCP_SESSION_TYPE_MANAGE);

    /* reading username */
    in_uint8(c->in_s, sz);
    buf[sz] = '\0';
    in_uint8a(c->in_s, buf, sz);

    if (0 != scp_session_set_username(session, buf))
    {
        scp_session_destroy(session);
        return SCP_SERVER_STATE_INTERNAL_ERR;
    }

    /* reading password */
    in_uint8(c->in_s, sz);
    buf[sz] = '\0';
    in_uint8a(c->in_s, buf, sz);

    if (0 != scp_session_set_password(session, buf))
    {
        scp_session_destroy(session);
        return SCP_SERVER_STATE_INTERNAL_ERR;
    }

    /* reading remote address */
    in_uint8(c->in_s, sz);

    if (sz == SCP_ADDRESS_TYPE_IPV4)
    {
        in_uint32_be(c->in_s, ipaddr);
        scp_session_set_addr(session, sz, &ipaddr);
    }
    else if (sz == SCP_ADDRESS_TYPE_IPV6)
    {
        in_uint8a(c->in_s, buf, 16);
        scp_session_set_addr(session, sz, buf);
    }

    /* reading hostname */
    in_uint8(c->in_s, sz);
    buf[sz] = '\0';
    in_uint8a(c->in_s, buf, sz);

    if (0 != scp_session_set_hostname(session, buf))
    {
        scp_session_destroy(session);
        return SCP_SERVER_STATE_INTERNAL_ERR;
    }

    /* returning the struct */
    (*s) = session;

    return SCP_SERVER_STATE_START_MANAGE;
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

    init_stream(c->in_s, c->in_s->size);

    /* read the rest of the packet */
    if (0 != scp_tcp_force_recv(c->in_sck, c->in_s->data, size - 8))
    {
        log_message(LOG_LEVEL_WARNING, "[v1s_mng:%d] connection aborted: network error", __LINE__);
        return SCP_SERVER_STATE_NETWORK_ERR;
    }

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
