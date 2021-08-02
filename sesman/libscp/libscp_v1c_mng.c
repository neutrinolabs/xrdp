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
 * @file libscp_v1c_mng.c
 * @brief libscp version 1 client api code - session management
 * @author Simone Fedele
 *
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "libscp_v1c_mng.h"
#include "string_calls.h"

#include <stdlib.h>
#include <stdio.h>

//extern struct log_config* s_log;

static enum SCP_CLIENT_STATES_E
_scp_v1c_mng_check_response(struct trans *t, struct SCP_SESSION *s);

/* client API */
/* 001 */
enum SCP_CLIENT_STATES_E
scp_v1c_mng_connect(struct trans *t, struct SCP_SESSION *s)
{
    struct stream *out_s = t->out_s;

    tui8 sz;
    int size;

    size = (12 + 4 + g_strlen(s->hostname) + g_strlen(s->username) +
            g_strlen(s->password));

    if (s->addr_type == SCP_ADDRESS_TYPE_IPV4)
    {
        size = size + 4;
    }
    else
    {
        size = size + 16;
    }

    init_stream(out_s, size);

    /* sending request */

    /* header */
    out_uint32_be(out_s, 1); /* version */
    out_uint32_be(out_s, size);
    out_uint16_be(out_s, SCP_COMMAND_SET_MANAGE);
    out_uint16_be(out_s, SCP_CMD_MNG_LOGIN);

    /* data */
    sz = g_strlen(s->username);
    out_uint8(out_s, sz);
    out_uint8p(out_s, s->username, sz);
    sz = g_strlen(s->password);
    out_uint8(out_s, sz);
    out_uint8p(out_s, s->password, sz);

    /* address */
    out_uint8(out_s, s->addr_type);

    if (s->addr_type == SCP_ADDRESS_TYPE_IPV4)
    {
        out_uint32_be(out_s, s->ipv4addr);
    }
    else
    {
        out_uint8p(out_s, s->ipv6addr, 16);
    }

    /* hostname */
    sz = g_strlen(s->hostname);
    out_uint8(out_s, sz);
    out_uint8p(out_s, s->hostname, sz);
    s_mark_end(out_s);

    if (0 != trans_force_write(t))
    {
        LOG(LOG_LEVEL_WARNING, "[v1c_mng:%d] connection aborted: network error", __LINE__);
        return SCP_CLIENT_STATE_NETWORK_ERR;
    }

    /* wait for response */
    return _scp_v1c_mng_check_response(t, s);
}

/* 004 */
enum SCP_CLIENT_STATES_E
scp_v1c_mng_get_session_list(struct trans *t, int *scount,
                             struct SCP_DISCONNECTED_SESSION **s)
{
    struct stream *in_s = t->in_s;
    struct stream *out_s = t->out_s;
    tui32 version = 1;
    int size = 12;
    tui16 cmd = SCP_CMD_MNG_LIST_REQ;       /* request session list */
    tui32 sescnt = 0;    /* total session number */
    tui32 sestmp = 0;    /* additional total session number */
    tui8 pktcnt = 0;     /* packet session count */
    tui32 totalcnt = 0;  /* session counter */
    tui8 continued = 0;  /* continue flag */
    int firstpkt = 1;    /* "first packet" flag */
    int idx;
    struct SCP_DISCONNECTED_SESSION *ds = 0;
    //   tui8 addr[16];

    init_stream(out_s, 64);

    /* we request session list */
    out_uint32_be(out_s, version);                 /* version */
    out_uint32_be(out_s, size);                    /* size    */
    out_uint16_be(out_s, SCP_COMMAND_SET_MANAGE); /* cmdset  */
    out_uint16_be(out_s, cmd);                     /* cmd     */
    s_mark_end(out_s);

    if (0 != trans_force_write(t))
    {
        LOG(LOG_LEVEL_WARNING, "[v1c_mng:%d] connection aborted: network error", __LINE__);
        return SCP_CLIENT_STATE_NETWORK_ERR;
    }

    do
    {
        /* then we wait for server response */
        init_stream(in_s, 8);

        if (0 != trans_force_read(t, 8))
        {
            LOG(LOG_LEVEL_WARNING, "[v1c_mng:%d] connection aborted: network error", __LINE__);
            g_free(ds);
            return SCP_CLIENT_STATE_NETWORK_ERR;
        }

        in_uint32_be(in_s, version);

        if (version != 1)
        {
            LOG(LOG_LEVEL_WARNING, "[v1c_mng:%d] connection aborted: version error", __LINE__);
            g_free(ds);
            return SCP_CLIENT_STATE_VERSION_ERR;
        }

        in_uint32_be(in_s, size);

        if (size < 12)
        {
            LOG(LOG_LEVEL_WARNING, "[v1c_mng:%d] connection aborted: size error", __LINE__);
            g_free(ds);
            return SCP_CLIENT_STATE_SIZE_ERR;
        }

        init_stream(in_s, size);

        if (0 != trans_force_read(t, size - 8))
        {
            LOG(LOG_LEVEL_WARNING, "[v1c_mng:%d] connection aborted: network error", __LINE__);
            g_free(ds);
            return SCP_CLIENT_STATE_NETWORK_ERR;
        }

        in_uint16_be(in_s, cmd);

        if (cmd != SCP_COMMAND_SET_MANAGE)
        {
            LOG(LOG_LEVEL_WARNING, "[v1c_mng:%d] connection aborted: sequence error", __LINE__);
            g_free(ds);
            return SCP_CLIENT_STATE_SEQUENCE_ERR;
        }

        in_uint16_be(in_s, cmd);

        if (cmd != SCP_CMD_MNG_LIST) /* session list */
        {
            LOG(LOG_LEVEL_WARNING, "[v1c_mng:%d] connection aborted: sequence error", __LINE__);
            g_free(ds);
            return SCP_CLIENT_STATE_SEQUENCE_ERR;
        }

        if (firstpkt)
        {
            firstpkt = 0;
            in_uint32_be(in_s, sescnt);
            sestmp = sescnt;

            if (0 == sescnt)
            {
                /* return data... */
                (*scount) = sescnt;
                (*s) = NULL;

                LOG_DEVEL(LOG_LEVEL_DEBUG, "[v1c_mng] end list - no session on TS");
                return SCP_CLIENT_STATE_LIST_OK;
            }

            ds = g_new(struct SCP_DISCONNECTED_SESSION, sescnt);

            if (ds == 0)
            {
                LOG(LOG_LEVEL_WARNING, "[v1c_mng:%d] connection aborted: internal error", __LINE__);
                return SCP_CLIENT_STATE_INTERNAL_ERR;
            }
        }
        else
        {
            in_uint32_be(in_s, sestmp);
        }

        in_uint8(in_s, continued);
        in_uint8(in_s, pktcnt);

        for (idx = 0; idx < pktcnt; idx++)
        {
            in_uint32_be(in_s, (ds[totalcnt]).SID); /* session id */
            in_uint8(in_s, (ds[totalcnt]).type);
            in_uint16_be(in_s, (ds[totalcnt]).height);
            in_uint16_be(in_s, (ds[totalcnt]).width);
            in_uint8(in_s, (ds[totalcnt]).bpp);
            in_uint8(in_s, (ds[totalcnt]).idle_days);
            in_uint8(in_s, (ds[totalcnt]).idle_hours);
            in_uint8(in_s, (ds[totalcnt]).idle_minutes);

            in_uint16_be(in_s, (ds[totalcnt]).conn_year);
            in_uint8(in_s, (ds[totalcnt]).conn_month);
            in_uint8(in_s, (ds[totalcnt]).conn_day);
            in_uint8(in_s, (ds[totalcnt]).conn_hour);
            in_uint8(in_s, (ds[totalcnt]).conn_minute);
            in_uint8(in_s, (ds[totalcnt]).addr_type);

            if ((ds[totalcnt]).addr_type == SCP_ADDRESS_TYPE_IPV4)
            {
                in_uint32_be(in_s, (ds[totalcnt]).ipv4addr);
            }

            if ((ds[totalcnt]).addr_type == SCP_ADDRESS_TYPE_IPV6)
            {
                in_uint8a(in_s, (ds[totalcnt]).ipv6addr, 16);
            }

            totalcnt++;
        }
    }
    while (continued);

    /* return data... */
    (*scount) = sescnt;
    (*s) = ds;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "[v1c_mng] end list");
    return SCP_CLIENT_STATE_LIST_OK;
}

/* 043 * /
enum SCP_CLIENT_STATES_E
scp_v1c_select_session(struct SCP_CONNECTION* c, struct SCP_SESSION* s,
                       SCP_SID sid)
{
  tui32 version = 1;
  tui32 size = 16;
  tui16 cmd = 43;

  init_stream(c->out_s, c->out_s->size);

  / * sending our selection * /
  out_uint32_be(c->out_s, version);                 / * version * /
  out_uint32_be(c->out_s, size);                    / * size    * /
  out_uint16_be(c->out_s, SCP_COMMAND_SET_DEFAULT); / * cmdset  * /
  out_uint16_be(c->out_s, cmd);                     / * cmd     * /

  out_uint32_be(c->out_s, sid);

  if (0 != scp_tcp_force_send(c->in_sck, c->out_s->data, size))
  {
    return SCP_CLIENT_STATE_NETWORK_ERR;
  }

  / * waiting for response.... * /
  init_stream(c->in_s, c->in_s->size);
  if (0 != scp_tcp_force_recv(c->in_sck, c->in_s->data, 8))
  {
    return SCP_CLIENT_STATE_NETWORK_ERR;
  }

  in_uint32_be(c->in_s, version);
  if (version != 1)
  {
    return SCP_CLIENT_STATE_VERSION_ERR;
  }

  in_uint32_be(c->in_s, size);
  if (size < 12)
  {
    return SCP_CLIENT_STATE_SIZE_ERR;
  }

  init_stream(c->in_s, c->in_s->size);
  / * read the rest of the packet * /
  if (0 != scp_tcp_force_recv(c->in_sck, c->in_s->data, size - 8))
  {
    return SCP_CLIENT_STATE_NETWORK_ERR;
  }

  in_uint16_be(c->in_s, cmd);
  if (cmd != SCP_COMMAND_SET_DEFAULT)
  {
    return SCP_CLIENT_STATE_SEQUENCE_ERR;
  }

  in_uint16_be(c->in_s, cmd);
  if (cmd != 46)
  {
    return SCP_CLIENT_STATE_SEQUENCE_ERR;
  }

  / * session display * /
  in_uint16_be(c->in_s, (s->display));
  / *we don't need to return any data other than the display * /
  / *because we already sent that                            * /

  return SCP_CLIENT_STATE_OK;
}*/

/* 044 * /
enum SCP_CLIENT_STATES_E
scp_v1c_select_session_cancel(struct SCP_CONNECTION* c)
{
  tui32 version = 1;
  tui32 size = 12;
  tui16 cmd = 44;

  init_stream(c->out_s, c->out_s->size);

  / * sending our selection * /
  out_uint32_be(c->out_s, version);                 / * version * /
  out_uint32_be(c->out_s, size);                    / * size    * /
  out_uint16_be(c->out_s, SCP_COMMAND_SET_DEFAULT); / * cmdset  * /
  out_uint16_be(c->out_s, cmd);                     / * cmd     * /

  if (0 != scp_tcp_force_send(c->in_sck, c->out_s->data, size))
  {
    return SCP_CLIENT_STATE_NETWORK_ERR;
  }

  return SCP_CLIENT_STATE_END;
}*/

static enum SCP_CLIENT_STATES_E
_scp_v1c_mng_check_response(struct trans *t, struct SCP_SESSION *s)
{
    struct stream *in_s = t->in_s;
    tui32 version;
    int size;
    tui16 cmd;
    tui8 dim;
    char buf[257];

    init_stream(in_s, 8);

    if (0 != trans_force_read(t, 8))
    {
        LOG(LOG_LEVEL_WARNING, "[v1c_mng:%d] connection aborted: network error", __LINE__);
        return SCP_CLIENT_STATE_NETWORK_ERR;
    }

    in_uint32_be(in_s, version);

    if (version != 1)
    {
        LOG(LOG_LEVEL_WARNING, "[v1c_mng:%d] connection aborted: version error", __LINE__);
        return SCP_CLIENT_STATE_VERSION_ERR;
    }

    in_uint32_be(in_s, size);

    init_stream(in_s, size - 8);

    /* read the rest of the packet */
    if (0 != trans_force_read(t, size - 8))
    {
        LOG(LOG_LEVEL_WARNING, "[v1c_mng:%d] connection aborted: network error", __LINE__);
        return SCP_CLIENT_STATE_NETWORK_ERR;
    }

    in_uint16_be(in_s, cmd);

    if (cmd != SCP_COMMAND_SET_MANAGE)
    {
        LOG(LOG_LEVEL_WARNING, "[v1c_mng:%d] connection aborted: sequence error", __LINE__);
        return SCP_CLIENT_STATE_SEQUENCE_ERR;
    }

    in_uint16_be(in_s, cmd);

    if (cmd == SCP_CMD_MNG_LOGIN_ALLOW) /* connection ok */
    {
        LOG(LOG_LEVEL_INFO, "[v1c_mng:%d] connection ok", __LINE__);
        return SCP_CLIENT_STATE_OK;
    }
    else if (cmd == SCP_CMD_MNG_LOGIN_DENY) /* connection denied */
    {
        in_uint8(in_s, dim);
        buf[dim] = '\0';
        in_uint8a(in_s, buf, dim);
        scp_session_set_errstr(s, buf);

        LOG(LOG_LEVEL_INFO, "[v1c_mng:%d] connection denied: %s", __LINE__, s->errstr);
        return SCP_CLIENT_STATE_CONNECTION_DENIED;
    }

    LOG(LOG_LEVEL_WARNING, "[v1c-mng:%d] connection aborted: sequence error", __LINE__);
    return SCP_CLIENT_STATE_SEQUENCE_ERR;
}
