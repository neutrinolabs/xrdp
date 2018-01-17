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
 * @file libscp_v1c.c
 * @brief libscp version 1 client api code
 * @author Simone Fedele
 *
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "libscp_v1c.h"

#include <stdlib.h>
#include <stdio.h>

static enum SCP_CLIENT_STATES_E
_scp_v1c_check_response(struct SCP_CONNECTION *c, struct SCP_SESSION *s);

/* client API */
/* 001 */
enum SCP_CLIENT_STATES_E
scp_v1c_connect(struct SCP_CONNECTION *c, struct SCP_SESSION *s)
{
    tui8 sz;
    tui32 size;

    init_stream(c->out_s, c->out_s->size);
    init_stream(c->in_s, c->in_s->size);

    size = 19 + 17 + 4 + g_strlen(s->hostname) + g_strlen(s->username) +
           g_strlen(s->password);

    if (s->addr_type == SCP_ADDRESS_TYPE_IPV4)
    {
        size = size + 4;
    }
    else
    {
        size = size + 16;
    }

    /* sending request */

    /* header */
    out_uint32_be(c->out_s, 1); /* version */
    out_uint32_be(c->out_s, size);
    out_uint16_be(c->out_s, SCP_COMMAND_SET_DEFAULT);
    out_uint16_be(c->out_s, 1);

    /* body */
    out_uint8(c->out_s, s->type);
    out_uint16_be(c->out_s, s->height);
    out_uint16_be(c->out_s, s->width);
    out_uint8(c->out_s, s->bpp);
    out_uint8(c->out_s, s->rsr);
    out_uint8p(c->out_s, s->locale, 17);
    out_uint8(c->out_s, s->addr_type);

    if (s->addr_type == SCP_ADDRESS_TYPE_IPV4)
    {
        out_uint32_be(c->out_s, s->ipv4addr);
    }
    else if (s->addr_type == SCP_ADDRESS_TYPE_IPV6)
    {
        out_uint8p(c->out_s, s->ipv6addr, 16);
    }

    sz = g_strlen(s->hostname);
    out_uint8(c->out_s, sz);
    out_uint8p(c->out_s, s->hostname, sz);
    sz = g_strlen(s->username);
    out_uint8(c->out_s, sz);
    out_uint8p(c->out_s, s->username, sz);
    sz = g_strlen(s->password);
    out_uint8(c->out_s, sz);
    out_uint8p(c->out_s, s->password, sz);

    if (0 != scp_tcp_force_send(c->in_sck, c->out_s->data, size))
    {
        return SCP_CLIENT_STATE_NETWORK_ERR;
    }

    /* wait for response */
    return _scp_v1c_check_response(c, s);
}

/* 004 */
enum SCP_CLIENT_STATES_E
scp_v1c_resend_credentials(struct SCP_CONNECTION *c, struct SCP_SESSION *s)
{
    tui8 sz;
    tui32 size;

    init_stream(c->out_s, c->out_s->size);
    init_stream(c->in_s, c->in_s->size);

    size = 12 + 2 + g_strlen(s->username) + g_strlen(s->password);

    /* sending request */
    /* header */
    out_uint32_be(c->out_s, 1); /* version */
    out_uint32_be(c->out_s, size);
    out_uint16_be(c->out_s, SCP_COMMAND_SET_DEFAULT);
    out_uint16_be(c->out_s, 4);

    /* body */
    sz = g_strlen(s->username);
    out_uint8(c->out_s, sz);
    out_uint8p(c->out_s, s->username, sz);
    sz = g_strlen(s->password);
    out_uint8(c->out_s, sz);
    out_uint8p(c->out_s, s->password, sz);

    if (0 != scp_tcp_force_send(c->in_sck, c->out_s->data, size))
    {
        return SCP_CLIENT_STATE_NETWORK_ERR;
    }

    /* wait for response */
    return _scp_v1c_check_response(c, s);
}

/* 041 */
enum SCP_CLIENT_STATES_E
scp_v1c_get_session_list(struct SCP_CONNECTION *c, int *scount,
                         struct SCP_DISCONNECTED_SESSION **s)
{
    tui32 version = 1;
    tui32 size = 12;
    tui16 cmd = 41;
    tui32 sescnt = 0;    /* total session number */
    tui32 sestmp = 0;    /* additional total session number */
    tui8 pktcnt = 0;     /* packet session count */
    tui32 totalcnt = 0;  /* session counter */
    tui8 continued = 0;  /* continue flag */
    int firstpkt = 1;    /* "first packet" flag */
    int idx;
    struct SCP_DISCONNECTED_SESSION *ds = 0;

    init_stream(c->out_s, c->out_s->size);

    /* we request session list */
    out_uint32_be(c->out_s, version);                 /* version */
    out_uint32_be(c->out_s, size);                    /* size    */
    out_uint16_be(c->out_s, SCP_COMMAND_SET_DEFAULT); /* cmdset  */
    out_uint16_be(c->out_s, cmd);                     /* cmd     */

    if (0 != scp_tcp_force_send(c->in_sck, c->out_s->data, size))
    {
        return SCP_CLIENT_STATE_NETWORK_ERR;
    }

    do
    {
        /* then we wait for server response */
        init_stream(c->in_s, c->in_s->size);

        if (0 != scp_tcp_force_recv(c->in_sck, c->in_s->data, 8))
        {
            g_free(ds);
            return SCP_CLIENT_STATE_NETWORK_ERR;
        }

        in_uint32_be(c->in_s, version);

        if (version != 1)
        {
            g_free(ds);
            return SCP_CLIENT_STATE_VERSION_ERR;
        }

        in_uint32_be(c->in_s, size);

        if (size < 12)
        {
            g_free(ds);
            return SCP_CLIENT_STATE_SIZE_ERR;
        }

        init_stream(c->in_s, c->in_s->size);

        if (0 != scp_tcp_force_recv(c->in_sck, c->in_s->data, size - 8))
        {
            g_free(ds);
            return SCP_CLIENT_STATE_NETWORK_ERR;
        }

        in_uint16_be(c->in_s, cmd);

        if (cmd != SCP_COMMAND_SET_DEFAULT)
        {
            g_free(ds);
            return SCP_CLIENT_STATE_SEQUENCE_ERR;
        }

        in_uint16_be(c->in_s, cmd);

        if (cmd != 42)
        {
            g_free(ds);
            return SCP_CLIENT_STATE_SEQUENCE_ERR;
        }

        if (firstpkt)
        {
            firstpkt = 0;
            in_uint32_be(c->in_s, sescnt);
            sestmp = sescnt;

            ds = g_new(struct SCP_DISCONNECTED_SESSION, sescnt);

            if (ds == 0)
            {
                return SCP_CLIENT_STATE_INTERNAL_ERR;
            }
        }
        else
        {
            in_uint32_be(c->in_s, sestmp);
        }

        in_uint8(c->in_s, continued);
        in_uint8(c->in_s, pktcnt);

        for (idx = 0; idx < pktcnt; idx++)
        {
            in_uint32_be(c->in_s, (ds[totalcnt]).SID); /* session id */
            in_uint8(c->in_s, (ds[totalcnt]).type);
            in_uint16_be(c->in_s, (ds[totalcnt]).height);
            in_uint16_be(c->in_s, (ds[totalcnt]).width);
            in_uint8(c->in_s, (ds[totalcnt]).bpp);
            in_uint8(c->in_s, (ds[totalcnt]).idle_days);
            in_uint8(c->in_s, (ds[totalcnt]).idle_hours);
            in_uint8(c->in_s, (ds[totalcnt]).idle_minutes);

            in_uint16_be(c->in_s, (ds[totalcnt]).conn_year);
            in_uint8(c->in_s, (ds[totalcnt]).conn_month);
            in_uint8(c->in_s, (ds[totalcnt]).conn_day);
            in_uint8(c->in_s, (ds[totalcnt]).conn_hour);
            in_uint8(c->in_s, (ds[totalcnt]).conn_minute);
            in_uint8(c->in_s, (ds[totalcnt]).addr_type);

            if ((ds[totalcnt]).addr_type == SCP_ADDRESS_TYPE_IPV4)
            {
                in_uint32_be(c->in_s, (ds[totalcnt]).ipv4addr);
            }
            else if ((ds[totalcnt]).addr_type == SCP_ADDRESS_TYPE_IPV6)
            {
                in_uint8a(c->in_s, (ds[totalcnt]).ipv6addr, 16);
            }

            totalcnt++;
        }
    }
    while (continued);

    printf("fine\n");
    /* return data... */
    (*scount) = sescnt;
    (*s) = ds;

    return SCP_CLIENT_STATE_LIST_OK;
}

/* 043 */
enum SCP_CLIENT_STATES_E
scp_v1c_select_session(struct SCP_CONNECTION *c, struct SCP_SESSION *s,
                       SCP_SID sid)
{
    tui32 version = 1;
    tui32 size = 16;
    tui16 cmd = 43;

    init_stream(c->out_s, c->out_s->size);

    /* sending our selection */
    out_uint32_be(c->out_s, version);                 /* version */
    out_uint32_be(c->out_s, size);                    /* size    */
    out_uint16_be(c->out_s, SCP_COMMAND_SET_DEFAULT); /* cmdset  */
    out_uint16_be(c->out_s, cmd);                     /* cmd     */

    out_uint32_be(c->out_s, sid);

    if (0 != scp_tcp_force_send(c->in_sck, c->out_s->data, size))
    {
        return SCP_CLIENT_STATE_NETWORK_ERR;
    }

    /* waiting for response.... */
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

    /* read the rest of the packet */
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

    /* session display */
    in_uint16_be(c->in_s, (s->display));
    /*we don't need to return any data other than the display */
    /*because we already sent that                            */

    return SCP_CLIENT_STATE_OK;
}

/* 044 */
enum SCP_CLIENT_STATES_E
scp_v1c_select_session_cancel(struct SCP_CONNECTION *c)
{
    tui32 version = 1;
    tui32 size = 12;
    tui16 cmd = 44;

    init_stream(c->out_s, c->out_s->size);

    /* sending our selection */
    out_uint32_be(c->out_s, version);                 /* version */
    out_uint32_be(c->out_s, size);                    /* size    */
    out_uint16_be(c->out_s, SCP_COMMAND_SET_DEFAULT); /* cmdset  */
    out_uint16_be(c->out_s, cmd);                     /* cmd     */

    if (0 != scp_tcp_force_send(c->in_sck, c->out_s->data, size))
    {
        return SCP_CLIENT_STATE_NETWORK_ERR;
    }

    return SCP_CLIENT_STATE_END;
}

static enum SCP_CLIENT_STATES_E
_scp_v1c_check_response(struct SCP_CONNECTION *c, struct SCP_SESSION *s)
{
    tui32 version;
    tui32 size;
    tui16 cmd;
    tui16 dim;

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

    init_stream(c->in_s, c->in_s->size);

    /* read the rest of the packet */
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

    if (cmd == 2) /* connection denied */
    {
        in_uint16_be(c->in_s, dim);

        if (s->errstr != 0)
        {
            g_free(s->errstr);
        }

        s->errstr = g_new(char, dim + 1);

        if (s->errstr == 0)
        {
            return SCP_CLIENT_STATE_INTERNAL_ERR;
        }

        in_uint8a(c->in_s, s->errstr, dim);
        (s->errstr)[dim] = '\0';

        return SCP_CLIENT_STATE_CONNECTION_DENIED;
    }
    else if (cmd == 3) /* resend usr/pwd */
    {
        in_uint16_be(c->in_s, dim);

        if (s->errstr != 0)
        {
            g_free(s->errstr);
        }

        s->errstr = g_new(char, dim + 1);

        if (s->errstr == 0)
        {
            return SCP_CLIENT_STATE_INTERNAL_ERR;
        }

        in_uint8a(c->in_s, s->errstr, dim);
        (s->errstr)[dim] = '\0';

        return SCP_CLIENT_STATE_RESEND_CREDENTIALS;
    }
    else if (cmd == 20) /* password change */
    {
        in_uint16_be(c->in_s, dim);

        if (s->errstr != 0)
        {
            g_free(s->errstr);
        }

        s->errstr = g_new(char, dim + 1);

        if (s->errstr == 0)
        {
            return SCP_CLIENT_STATE_INTERNAL_ERR;
        }

        in_uint8a(c->in_s, s->errstr, dim);
        (s->errstr)[dim] = '\0';

        return SCP_CLIENT_STATE_PWD_CHANGE_REQ;
    }
    else if (cmd == 30) /* display */
    {
        in_uint16_be(c->in_s, s->display);

        return SCP_CLIENT_STATE_OK;
    }
    //else if (cmd == 31) /* there's a disconnected session */
    //{
    //  return SCP_CLIENT_STATE_RECONNECT_SINGLE;
    //}
    //else if (cmd == 33) /* display of a disconnected session */
    //{
    //  return SCP_CLIENT_STATE_RECONNECT;
    //}
    else if (cmd == 40) /* session list */
    {
        return SCP_CLIENT_STATE_SESSION_LIST;
    }

    return SCP_CLIENT_STATE_SEQUENCE_ERR;
}
