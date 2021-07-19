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
#include "string_calls.h"

#include <stdlib.h>
#include <stdio.h>

static enum SCP_CLIENT_STATES_E
_scp_v1c_check_response(struct trans *t, struct SCP_SESSION *s);

/* client API */
enum SCP_CLIENT_STATES_E
scp_v1c_connect(struct trans *t, struct SCP_SESSION *s)
{
    tui8 sz;
    int size;
    struct stream *out_s = t->out_s;

    size = (19 + 17 + 4 + g_strlen(s->hostname) + g_strlen(s->username) +
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
    out_uint16_be(out_s, SCP_COMMAND_SET_DEFAULT);
    out_uint16_be(out_s, SCP_CMD_LOGIN);

    /* body */
    out_uint8(out_s, s->type);
    out_uint16_be(out_s, s->height);
    out_uint16_be(out_s, s->width);
    out_uint8(out_s, s->bpp);
    out_uint8(out_s, s->rsr);
    out_uint8p(out_s, s->locale, 17);
    out_uint8(out_s, s->addr_type);

    if (s->addr_type == SCP_ADDRESS_TYPE_IPV4)
    {
        out_uint32_be(out_s, s->ipv4addr);
    }
    else if (s->addr_type == SCP_ADDRESS_TYPE_IPV6)
    {
        out_uint8p(out_s, s->ipv6addr, 16);
    }

    sz = g_strlen(s->hostname);
    out_uint8(out_s, sz);
    out_uint8p(out_s, s->hostname, sz);
    sz = g_strlen(s->username);
    out_uint8(out_s, sz);
    out_uint8p(out_s, s->username, sz);
    sz = g_strlen(s->password);
    out_uint8(out_s, sz);
    out_uint8p(out_s, s->password, sz);
    s_mark_end(out_s);

    if (0 != trans_force_write(t))
    {
        return SCP_CLIENT_STATE_NETWORK_ERR;
    }

    /* wait for response */
    return _scp_v1c_check_response(t, s);
}

enum SCP_CLIENT_STATES_E
scp_v1c_resend_credentials(struct trans *t, struct SCP_SESSION *s)
{
    struct stream *out_s = t->out_s;
    tui8 sz;
    int size;

    size = 12 + 2 + g_strlen(s->username) + g_strlen(s->password);

    init_stream(out_s, size);

    /* sending request */
    /* header */
    out_uint32_be(out_s, 1); /* version */
    out_uint32_be(out_s, size);
    out_uint16_be(out_s, SCP_COMMAND_SET_DEFAULT);
    out_uint16_be(out_s, SCP_CMD_RESEND_CREDS);

    /* body */
    sz = g_strlen(s->username);
    out_uint8(out_s, sz);
    out_uint8p(out_s, s->username, sz);
    sz = g_strlen(s->password);
    out_uint8(out_s, sz);
    out_uint8p(out_s, s->password, sz);
    s_mark_end(out_s);

    if (0 != trans_force_write(t))
    {
        return SCP_CLIENT_STATE_NETWORK_ERR;
    }

    /* wait for response */
    return _scp_v1c_check_response(t, s);
}

enum SCP_CLIENT_STATES_E
scp_v1c_get_session_list(struct trans *t, int *scount,
                         struct SCP_DISCONNECTED_SESSION **s)
{
    struct stream *in_s = t->in_s;
    struct stream *out_s = t->out_s;
    tui32 version = 1;
    int size = 12;
    tui16 cmd = SCP_CMD_GET_SESSION_LIST;
    tui32 sescnt = 0;    /* total session number */
    tui32 sestmp = 0;    /* additional total session number */
    tui8 pktcnt = 0;     /* packet session count */
    tui32 totalcnt = 0;  /* session counter */
    tui8 continued = 0;  /* continue flag */
    int firstpkt = 1;    /* "first packet" flag */
    int idx;
    struct SCP_DISCONNECTED_SESSION *ds = 0;

    init_stream(out_s, 64);

    /* we request session list */
    out_uint32_be(out_s, version);                 /* version */
    out_uint32_be(out_s, size);                    /* size    */
    out_uint16_be(out_s, SCP_COMMAND_SET_DEFAULT); /* cmdset  */
    out_uint16_be(out_s, cmd);                     /* cmd     */
    s_mark_end(out_s);

    if (0 != trans_force_write(t))
    {
        return SCP_CLIENT_STATE_NETWORK_ERR;
    }

    do
    {
        /* then we wait for server response */
        init_stream(in_s, 8);

        if (0 != trans_force_read(t, 8))
        {
            g_free(ds);
            return SCP_CLIENT_STATE_NETWORK_ERR;
        }

        in_uint32_be(t->in_s, version);

        if (version != 1)
        {
            g_free(ds);
            return SCP_CLIENT_STATE_VERSION_ERR;
        }

        in_uint32_be(t->in_s, size);

        if (size < 12)
        {
            g_free(ds);
            return SCP_CLIENT_STATE_SIZE_ERR;
        }

        init_stream(t->in_s, size - 8);

        if (0 != trans_force_read(t, size - 8))
        {
            g_free(ds);
            return SCP_CLIENT_STATE_NETWORK_ERR;
        }

        in_uint16_be(in_s, cmd);

        if (cmd != SCP_COMMAND_SET_DEFAULT)
        {
            g_free(ds);
            return SCP_CLIENT_STATE_SEQUENCE_ERR;
        }

        in_uint16_be(in_s, cmd);

        if (cmd != SCP_REPLY_SESSIONS_INFO)
        {
            g_free(ds);
            return SCP_CLIENT_STATE_SEQUENCE_ERR;
        }

        if (firstpkt)
        {
            firstpkt = 0;
            in_uint32_be(in_s, sescnt);
            sestmp = sescnt;

            ds = g_new(struct SCP_DISCONNECTED_SESSION, sescnt);

            if (ds == 0)
            {
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
            else if ((ds[totalcnt]).addr_type == SCP_ADDRESS_TYPE_IPV6)
            {
                in_uint8a(in_s, (ds[totalcnt]).ipv6addr, 16);
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

enum SCP_CLIENT_STATES_E
scp_v1c_select_session(struct trans *t, struct SCP_SESSION *s,
                       SCP_SID sid)
{
    struct stream *in_s = t->in_s;
    struct stream *out_s = t->out_s;
    tui32 version = 1;
    int size = 16;
    tui16 cmd = SCP_CMD_SELECT_SESSION;

    init_stream(out_s, 64);

    /* sending our selection */
    out_uint32_be(out_s, version);                 /* version */
    out_uint32_be(out_s, size);                    /* size    */
    out_uint16_be(out_s, SCP_COMMAND_SET_DEFAULT); /* cmdset  */
    out_uint16_be(out_s, cmd);                     /* cmd     */

    out_uint32_be(out_s, sid);
    s_mark_end(out_s);

    if (0 != trans_force_write(t))
    {
        return SCP_CLIENT_STATE_NETWORK_ERR;
    }

    /* waiting for response.... */
    init_stream(in_s, 8);

    if (0 != trans_force_read(t, 8))
    {
        return SCP_CLIENT_STATE_NETWORK_ERR;
    }

    in_uint32_be(in_s, version);

    if (version != 1)
    {
        return SCP_CLIENT_STATE_VERSION_ERR;
    }

    in_uint32_be(in_s, size);

    if (size < 12)
    {
        return SCP_CLIENT_STATE_SIZE_ERR;
    }

    init_stream(in_s, size - 8);

    /* read the rest of the packet */
    if (0 != trans_force_read(t, size - 8))
    {
        return SCP_CLIENT_STATE_NETWORK_ERR;
    }

    in_uint16_be(in_s, cmd);

    if (cmd != SCP_COMMAND_SET_DEFAULT)
    {
        return SCP_CLIENT_STATE_SEQUENCE_ERR;
    }

    in_uint16_be(in_s, cmd);

    if (cmd != SCP_REPLY_SESSION_RECONNECTED)
    {
        return SCP_CLIENT_STATE_SEQUENCE_ERR;
    }

    /* session display */
    in_uint16_be(in_s, (s->display));
    /*we don't need to return any data other than the display */
    /*because we already sent that                            */

    return SCP_CLIENT_STATE_OK;
}

enum SCP_CLIENT_STATES_E
scp_v1c_select_session_cancel(struct trans *t)
{
    struct stream *out_s = t->out_s;
    tui32 version = 1;
    tui32 size = 12;
    tui16 cmd = SCP_CMD_SELECT_SESSION_CANCEL;

    init_stream(out_s, 64);

    /* sending our selection */
    out_uint32_be(out_s, version);                 /* version */
    out_uint32_be(out_s, size);                    /* size    */
    out_uint16_be(out_s, SCP_COMMAND_SET_DEFAULT); /* cmdset  */
    out_uint16_be(out_s, cmd);                     /* cmd     */
    s_mark_end(out_s);

    if (0 != trans_force_write(t))
    {
        return SCP_CLIENT_STATE_NETWORK_ERR;
    }

    return SCP_CLIENT_STATE_END;
}

static enum SCP_CLIENT_STATES_E
_scp_v1c_check_response(struct trans *t, struct SCP_SESSION *s)
{
    struct stream *in_s = t->in_s;
    tui32 version;
    int size;
    tui16 cmd;
    tui16 dim;

    init_stream(in_s, 64);

    if (0 != trans_force_read(t, 8))
    {
        return SCP_CLIENT_STATE_NETWORK_ERR;
    }

    in_uint32_be(in_s, version);

    if (version != 1)
    {
        return SCP_CLIENT_STATE_VERSION_ERR;
    }

    in_uint32_be(in_s, size);

    init_stream(in_s, size - 8);

    /* read the rest of the packet */
    if (0 != trans_force_read(t, size - 8))
    {
        return SCP_CLIENT_STATE_NETWORK_ERR;
    }

    in_uint16_be(in_s, cmd);

    if (cmd != SCP_COMMAND_SET_DEFAULT)
    {
        return SCP_CLIENT_STATE_SEQUENCE_ERR;
    }

    in_uint16_be(in_s, cmd);

    if (cmd == SCP_REPLY_LOGIN_DENIED)
    {
        in_uint16_be(in_s, dim);

        if (s->errstr != 0)
        {
            g_free(s->errstr);
        }

        s->errstr = g_new(char, dim + 1);

        if (s->errstr == 0)
        {
            return SCP_CLIENT_STATE_INTERNAL_ERR;
        }

        in_uint8a(in_s, s->errstr, dim);
        (s->errstr)[dim] = '\0';

        return SCP_CLIENT_STATE_CONNECTION_DENIED;
    }
    else if (cmd == SCP_REPLY_REREQUEST_CREDS)
    {
        in_uint16_be(in_s, dim);

        if (s->errstr != 0)
        {
            g_free(s->errstr);
        }

        s->errstr = g_new(char, dim + 1);

        if (s->errstr == 0)
        {
            return SCP_CLIENT_STATE_INTERNAL_ERR;
        }

        in_uint8a(in_s, s->errstr, dim);
        (s->errstr)[dim] = '\0';

        return SCP_CLIENT_STATE_RESEND_CREDENTIALS;
    }
    else if (cmd == SCP_REPLY_CHANGE_PASSWD)
    {
        in_uint16_be(in_s, dim);

        if (s->errstr != 0)
        {
            g_free(s->errstr);
        }

        s->errstr = g_new(char, dim + 1);

        if (s->errstr == 0)
        {
            return SCP_CLIENT_STATE_INTERNAL_ERR;
        }

        in_uint8a(in_s, s->errstr, dim);
        (s->errstr)[dim] = '\0';

        return SCP_CLIENT_STATE_PWD_CHANGE_REQ;
    }
    else if (cmd == SCP_REPLY_NEW_SESSION)
    {
        in_uint16_be(in_s, s->display);

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
    else if (cmd == SCP_REPLY_USER_SESSIONS_EXIST) /* session list */
    {
        return SCP_CLIENT_STATE_SESSION_LIST;
    }

    return SCP_CLIENT_STATE_SEQUENCE_ERR;
}
