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
 * @param [out] str Output buffer (must be >= (STRING16_MAX_LEN+1) chars)
 * @param prefix Logging prefix for errors
 * @return != 0 if string read OK
 */
static
int in_string16(struct stream *s, char str[], const char *prefix)
{
    int result;
    unsigned int sz;

    if ((result = s_check_rem_and_log(s, 2, prefix)) != 0)
    {
        in_uint16_be(s, sz);
        if (sz > STRING16_MAX_LEN)
        {
            LOG(LOG_LEVEL_ERROR, "%s input string too long (%u chars)",
                prefix, sz);
            result = 0;
        }
        else if ((result = s_check_rem_and_log(s, sz, prefix)) != 0)
        {
            in_uint8a(s, str, sz);
            str[sz] = '\0';
        }
    }
    return result;
}

/**
 * Writes a big-endian uint16 followed by a string into a buffer
 *
 * @param s Output stream
 * @param[in] str output string (must be <= (STRING16_MAX_LEN+1) chars)
 * @param param Parameter we're sending
 * @return != 0 if string written OK
 */
static
int out_string16(struct stream *out_s, const char *str, const char *prefix)
{
    int result;

    unsigned int sz = g_strlen(str);
    if (sz > STRING16_MAX_LEN)
    {
        LOG(LOG_LEVEL_WARNING, "%s String too long (%u chars)", prefix, sz);
        result = 0;
    }
    else if ((result = s_check_rem_out_and_log(out_s, 2 + sz, prefix)) != 0)
    {
        out_uint16_be(out_s, sz);
        out_uint8a(out_s, str, sz);
    }

    return result;
}

/***
 * Terminates a V0 request, adds the header and sends it.
 *
 * On entry, channel_hdr on the transport output stream is expected to
 * contain the location for the SCP header
 *
 * @param atrans Transport for the message
 * @return error code
 */
static enum SCP_CLIENT_STATES_E
terminate_and_send_v0_request(struct trans *atrans)
{
    enum SCP_CLIENT_STATES_E e;

    struct stream *s = atrans->out_s;
    s_mark_end(s);
    s_pop_layer(s, channel_hdr);

    /* version */
    out_uint32_be(s, 0);
    /* size */
    out_uint32_be(s, s->end - s->data);

    if (trans_force_write_s(atrans, s) == 0)
    {
        e = SCP_CLIENT_STATE_OK;
    }
    else
    {
        LOG(LOG_LEVEL_ERROR, "connection aborted: network error");
        e = SCP_CLIENT_STATE_NETWORK_ERR;
    }

    return e;
}

/* client API */
/******************************************************************************/
enum SCP_CLIENT_STATES_E
scp_v0c_create_session_request(struct trans *atrans,
                               const char *username,
                               const char *password,
                               unsigned short code,
                               unsigned short width,
                               unsigned short height,
                               unsigned short bpp,
                               const char *domain,
                               const char *shell,
                               const char *directory,
                               const char *client_ip)
{
    enum SCP_CLIENT_STATES_E e;

    struct stream *s = trans_get_out_s(atrans, 8192);
    s_push_layer(s, channel_hdr, 8);

    out_uint16_be(s, code);
    if (!out_string16(s, username, "Session username") ||
            !out_string16(s, password, "Session passwd"))
    {
        e = SCP_CLIENT_STATE_SIZE_ERR;
    }
    else
    {
        out_uint16_be(s, width);
        out_uint16_be(s, height);
        out_uint16_be(s, bpp);
        if (!out_string16(s, domain, "Session domain") ||
                !out_string16(s, shell, "Session shell") ||
                !out_string16(s, directory, "Session directory") ||
                !out_string16(s, client_ip, "Session client IP"))
        {
            e = SCP_CLIENT_STATE_SIZE_ERR;
        }
        else
        {
            e = terminate_and_send_v0_request(atrans);
        }
    }

    return e;
}

enum SCP_CLIENT_STATES_E
scp_v0c_gateway_request(struct trans *atrans,
                        const char *username,
                        const char *password)
{
    enum SCP_CLIENT_STATES_E e;

    struct stream *s = trans_get_out_s(atrans, 500);
    s_push_layer(s, channel_hdr, 8);

    out_uint16_be(s, SCP_GW_AUTHENTICATION);
    if (!out_string16(s, username, "Gateway username") ||
            !out_string16(s, password, "Gateway passwd"))
    {
        e = SCP_CLIENT_STATE_SIZE_ERR;
    }
    else
    {
        e = terminate_and_send_v0_request(atrans);
    }

    return e;
}

/**************************************************************************//**
 * Is a reply available from the other end?
 *
 * Returns true if it is, or if an error has occurred which needs handling.
 *
 * @param trans Transport to be polled
 * @return True if scp_v0c_get_reply() should be called
 */
int
scp_v0c_reply_available(struct trans *trans)
{
    int result = 1;
    if (trans != NULL && trans->status == TRANS_STATUS_UP)
    {
        /* Have we read enough data from the stream? */
        if ((unsigned int)(trans->in_s->end - trans->in_s->data) < trans->header_size)
        {
            result = 0;
        }
        else if (trans->extra_flags == 0)
        {
            int version;
            int size;

            /* We've read the header only */
            in_uint32_be(trans->in_s, version);
            in_uint32_be(trans->in_s, size);

            if (version != 0)
            {
                LOG(LOG_LEVEL_ERROR, "Unexpected version number %d from SCP",
                    version);
                trans->status = TRANS_STATUS_DOWN;
            }
            else if (size <= 8 || size > trans->in_s->size)
            {
                LOG(LOG_LEVEL_ERROR,
                    "Invalid V0 message length %d from SCP",
                    size);
                trans->status = TRANS_STATUS_DOWN;
            }
            else
            {
                /* Read the rest of the message */
                trans->header_size = size;
                trans->extra_flags = 1;
                result = 0;
            }
        }
    }


    return result;
}
/**************************************************************************//**
 * Get a reply from the V0 transport
 *
 * Only call this once scp_v0c_reply_available() has returned true
 *
 * After a successful call, the transport is ready to be used for the
 * next incoming message
 *
 * @param trans Transport containing the reply
 * @param[out] reply, provided result is SCP_CLIENT_STATE_OK
 * @return SCP client state
 */
enum SCP_CLIENT_STATES_E
scp_v0c_get_reply(struct trans *trans, struct scp_v0_reply_type *reply)
{
    enum SCP_CLIENT_STATES_E e;

    if (trans == NULL || trans->status != TRANS_STATUS_UP)
    {
        e = SCP_CLIENT_STATE_NETWORK_ERR;
    }
    else if (!s_check_rem_and_log(trans->in_s, 6, "SCPV0 reply"))
    {
        trans->status = TRANS_STATUS_DOWN;
        e = SCP_CLIENT_STATE_NETWORK_ERR;
    }
    else
    {
        int word1;
        int word2;
        int word3;
        in_uint16_be(trans->in_s, word1);
        in_uint16_be(trans->in_s, word2);
        in_uint16_be(trans->in_s, word3);

        if (word1 == SCP_GW_AUTHENTICATION)
        {
            reply->is_gw_auth_response = 1;
            reply->auth_result = word2;
            reply->display = 0;
            guid_clear(&reply->guid);
        }
        else
        {
            reply->is_gw_auth_response = 0;
            reply->auth_result = word2;
            reply->display = word3;
            if (s_check_rem(trans->in_s, GUID_SIZE))
            {
                in_uint8a(trans->in_s, reply->guid.g, GUID_SIZE);
            }
            else
            {
                guid_clear(&reply->guid);
            }
        }

        e = SCP_CLIENT_STATE_OK;

        /* Reset the input stream for the next message */
        trans->header_size = 8;
        trans->extra_flags = 0;
        init_stream(trans->in_s, 0);
    }

    return e;
}

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
        if (!in_string16(in_s, buf, "Session username"))
        {
            return SCP_SERVER_STATE_SIZE_ERR;
        }
        if (0 != scp_session_set_username(session, buf))
        {
            LOG(LOG_LEVEL_WARNING, "connection aborted: error setting username");
            return SCP_SERVER_STATE_INTERNAL_ERR;
        }

        /* reading password */
        if (!in_string16(in_s, buf, "Session passwd"))
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
        if (session_type == SCP_SESSION_TYPE_XORG)
        {
            /* Client value is ignored */
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
            if (!in_string16(in_s, buf, "Session domain"))
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
            if (!in_string16(in_s, buf, "Session program"))
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
            if (!in_string16(in_s, buf, "Session directory"))
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
            if (!in_string16(in_s, buf, "connection description"))
            {
                return SCP_SERVER_STATE_SIZE_ERR;
            }
            if (buf[0] != '\0')
            {
                scp_session_set_connection_description(session, buf);
            }
        }
    }
    else if (code == SCP_GW_AUTHENTICATION)
    {
        scp_session_set_type(session, SCP_GW_AUTHENTICATION);
        /* reading username */
        if (!in_string16(in_s, buf, "Session username"))
        {
            return SCP_SERVER_STATE_SIZE_ERR;
        }

        if (0 != scp_session_set_username(session, buf))
        {
            LOG(LOG_LEVEL_WARNING, "connection aborted: error setting username");
            return SCP_SERVER_STATE_INTERNAL_ERR;
        }

        /* reading password */
        if (!in_string16(in_s, buf, "Session passwd"))
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
scp_v0s_allow_connection(struct trans *atrans, SCP_DISPLAY d,
                         const struct guid *guid)
{
    int msg_size;
    struct stream *out_s;

    out_s = trans_get_out_s(atrans, 0);
    msg_size = guid_is_set(guid) ? 14 + GUID_SIZE : 14;
    out_uint32_be(out_s, 0);  /* version */
    out_uint32_be(out_s, msg_size); /* size */
    out_uint16_be(out_s, 3);  /* cmd */
    out_uint16_be(out_s, 1);  /* data */
    out_uint16_be(out_s, d);  /* data */
    if (msg_size > 14)
    {
        out_uint8a(out_s, guid->g, GUID_SIZE);
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
