/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2006-2013
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
 *
 * channel layer
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "libxrdp.h"
#include "string_calls.h"
#include "xrdp_channel.h"

#define CMD_DVC_OPEN_CHANNEL    0x10
#define CMD_DVC_DATA_FIRST      0x20
#define CMD_DVC_DATA            0x30
#define CMD_DVC_CLOSE_CHANNEL   0x40
#define CMD_DVC_CAPABILITY      0x50

#define XRDP_DRDYNVC_CHANNEL_ID_TO_NAME(self, chan_id) \
    (xrdp_channel_get_item((self), (chan_id)) != NULL \
     ? xrdp_channel_get_item((self), (chan_id))->name \
     : "unknown")

/*****************************************************************************/
/* returns pointer or nil on error */
static struct mcs_channel_item *
xrdp_channel_get_item(struct xrdp_channel *self, int channel_id)
{
    struct mcs_channel_item *channel;

    if (self->mcs_layer->channel_list == NULL)
    {
        LOG(LOG_LEVEL_WARNING, "Channel list is NULL, returning NULL");
        return NULL ;
    }

    channel = (struct mcs_channel_item *)
              list_get_item(self->mcs_layer->channel_list, channel_id);
    return channel;
}

/*****************************************************************************/
struct xrdp_channel *
xrdp_channel_create(struct xrdp_sec *owner, struct xrdp_mcs *mcs_layer)
{
    struct xrdp_channel *self;

    self = (struct xrdp_channel *)g_malloc(sizeof(struct xrdp_channel), 1);
    self->sec_layer = owner;
    self->mcs_layer = mcs_layer;
    self->drdynvc_channel_id = -1;
    return self;
}

/*****************************************************************************/
/* returns error */
void
xrdp_channel_delete(struct xrdp_channel *self)
{
    if (self == 0)
    {
        return;
    }
    free_stream(self->s);
    g_memset(self, 0, sizeof(struct xrdp_channel));
    g_free(self);
}

/*****************************************************************************/
/* returns error */
int
xrdp_channel_init(struct xrdp_channel *self, struct stream *s)
{
    if (xrdp_sec_init(self->sec_layer, s) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_channel_init: xrdp_sec_init failed");
        return 1;
    }

    s_push_layer(s, channel_hdr, 8);
    return 0;
}

/*****************************************************************************/
/* returns error */
/* This sends data out to the secure layer. */
int
xrdp_channel_send(struct xrdp_channel *self, struct stream *s, int channel_id,
                  int total_data_len, int flags)
{
    struct mcs_channel_item *channel;

    channel = xrdp_channel_get_item(self, channel_id);

    if (channel == NULL)
    {
        LOG(LOG_LEVEL_ERROR,
            "Request to send a message to non-existent channel_id %d",
            channel_id);
        return 1;
    }

    if (channel->disabled)
    {
        LOG(LOG_LEVEL_DEBUG,
            "Request to send a message to the disabled channel %s (%d)",
            channel->name, channel_id);
        return 0; /* not an error */
    }

    s_pop_layer(s, channel_hdr);
    out_uint32_le(s, total_data_len);

    /*
     * According to 2.2.1.3.4.1 Channel Definition Structure (CHANNEL_DEF):
     * CHANNEL_OPTION_SHOW_PROTOCOL 0x00200000
     *  The value of this flag MUST be ignored by the server. The
     *  visibility of the Channel PDU Header (section 2.2.6.1.1) is
     *  determined by the CHANNEL_FLAG_SHOW_PROTOCOL
     *  (0x00000010) flag as defined in the flags field (section
     *  2.2.6.1.1).
     *
     *   That's flag makes MSTSC crash when using RAIL channel.
     */
    //    if (channel->flags & XR_CHANNEL_OPTION_SHOW_PROTOCOL)
    //    {
    //        flags |= CHANNEL_FLAG_SHOW_PROTOCOL;
    //    }

    out_uint32_le(s, flags);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Adding header [MS-RDPBCGR] CHANNEL_PDU_HEADER "
              "length %d, flags 0x%8.8x", total_data_len, flags);

    if (xrdp_sec_send(self->sec_layer, s, channel->chanid) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_channel_send: xrdp_sec_send failed");
        return 1;
    }

    return 0;
}

/*****************************************************************************/
/* returns error */
/* this will inform the callback, whatever it is that some channel data is
   ready.  the default for this is a call to xrdp_wm.c. */
static int
xrdp_channel_call_callback(struct xrdp_channel *self, struct stream *s,
                           int channel_id,
                           int total_data_len, int flags)
{
    struct xrdp_session *session;
    int rv;
    int size;

    rv = 0;
    session = self->sec_layer->rdp_layer->session;

    if (session != 0)
    {
        if (session->callback != 0)
        {
            size = (int)(s->end - s->p);
            /* in xrdp_wm.c */
            rv = session->callback(session->id, 0x5555,
                                   MAKELONG(channel_id, flags),
                                   size, (tbus)(s->p), total_data_len);
        }
        else
        {
            LOG_DEVEL(LOG_LEVEL_WARNING, "session->callback is NULL");
        }
    }
    else
    {
        LOG_DEVEL(LOG_LEVEL_WARNING, "session is NULL");
    }

    return rv;
}

/*****************************************************************************/
/**
 * Write a variable length unsigned int (1, 2, or 4 bytes) to the stream.
 *
 * The number of bytes written is the minimum number of bytes needed to
 * represent the value.
 *
 * @param s the stream to write to
 * @param val the value to write
 * @return the DYNVC cbId length code for the number of bytes written (see [MS-RDPEDYC] 2.2.2.1)
 */
static int
drdynvc_insert_uint_124(struct stream *s, uint32_t val)
{
    int ret_val;

    if (val <= 0xff)
    {
        out_uint8(s, val);
        ret_val = 0;
    }
    else if (val <= 0xffff)
    {
        out_uint16_le(s, val);
        ret_val = 1;
    }
    else
    {
        out_uint32_le(s, val);
        ret_val = 2;
    }

    return ret_val;
}

/*****************************************************************************/
/**
 * Read a variable length unsigned int (1, 2, or 4 bytes) from the stream.
 *
 * The number of bytes read is determined by the cbId bit field flag in the
 * cmd argument (see [MS-RDPEDYC] 2.2.2.1).
 *
 * @param s [in] the stream to read from
 * @param cmd [in] the cmd byte which contains the cbId bit field flag
 * @param chan_id_p [out] a pointer to the value read from the stream
 * @return error code
 */
static int
drdynvc_get_chan_id(struct stream *s, char cmd, uint32_t *chan_id_p)
{
    int cbChId;
    int chan_id;

    cbChId = cmd & 0x03;
    if (cbChId == 0)
    {
        if (!s_check_rem_and_log(s, 1, "Parsing [MS-RDPEDYC] channel id"))
        {
            return 1;
        }
        in_uint8(s, chan_id);
    }
    else if (cbChId == 1)
    {
        if (!s_check_rem_and_log(s, 2, "Parsing [MS-RDPEDYC] channel id"))
        {
            return 1;
        }
        in_uint16_le(s, chan_id);
    }
    else
    {
        if (!s_check_rem_and_log(s, 4, "Parsing [MS-RDPEDYC] channel id"))
        {
            return 1;
        }
        in_uint32_le(s, chan_id);
    }
    *chan_id_p = chan_id;
    return 0;
}

/*****************************************************************************/
/*
 * Process a [MS-RDPEDYC] DYNVC_CAPS_RSP message.
 */
static int
drdynvc_process_capability_response(struct xrdp_channel *self,
                                    int cmd, struct stream *s)
{
    struct xrdp_session *session;
    int cap_version;
    int rv;

    if (!s_check_rem_and_log(s, 3, "Parsing [MS-RDPEDYC] DYNVC_CAPS_RSP"))
    {
        return 1;
    }
    in_uint8s(s, 1);              /* skip padding */
    in_uint16_le(s, cap_version); /* Version */
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPEDYC] DYNVC_CAPS_RSP "
              "version %d", cap_version);

    if ((cap_version != 2) && (cap_version != 3))
    {
        LOG(LOG_LEVEL_ERROR,
            "Dynamic Virtual Channel version %d is not supported",
            cap_version);
        return 1;
    }
    self->drdynvc_state = 1;
    session = self->sec_layer->rdp_layer->session;
    rv = session->callback(session->id, 0x5558, 0, 0, 0, 0);
    return rv;
}

/*****************************************************************************/
/*
 * Process a [MS-RDPEDYC] DYNVC_CREATE_RSP message.
 */
static int
drdynvc_process_open_channel_response(struct xrdp_channel *self,
                                      int cmd, struct stream *s)
{
    struct xrdp_session *session;
    int creation_status;
    uint32_t chan_id;
    struct xrdp_drdynvc *drdynvc;

    if (drdynvc_get_chan_id(s, cmd, &chan_id) != 0) /* ChannelId */
    {
        LOG(LOG_LEVEL_ERROR,
            "Parsing [MS-RDPEDYC] DYNVC_CREATE_RSP failed");
        return 1;
    }
    if (!s_check_rem_and_log(s, 4, "Parsing [MS-RDPEDYC] DYNVC_CREATE_RSP"))
    {
        return 1;
    }
    in_uint32_le(s, creation_status); /* CreationStatus */
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPEDYC] DYNVC_CREATE_RSP "
              "ChannelId %d, CreationStatus %d", chan_id, creation_status);
    if (chan_id > 255)
    {
        LOG(LOG_LEVEL_ERROR, "Received [MS-RDPEDYC] DYNVC_CREATE_RSP for an "
            "invalid channel id. Max allowed 255, received %d", chan_id);
        return 1;
    }

    session = self->sec_layer->rdp_layer->session;
    drdynvc = self->drdynvcs + chan_id;
    if (creation_status == 0)
    {
        drdynvc->status = XRDP_DRDYNVC_STATUS_OPEN;
    }
    else
    {
        drdynvc->status = XRDP_DRDYNVC_STATUS_CLOSED;
    }
    LOG_DEVEL(LOG_LEVEL_DEBUG,
              "Dynamic Virtual Channel %s (%d) updated: status = %s",
              XRDP_DRDYNVC_CHANNEL_ID_TO_NAME(self, chan_id),
              chan_id,
              XRDP_DRDYNVC_STATUS_TO_STR(drdynvc->status));
    if (drdynvc->open_response != NULL)
    {
        return drdynvc->open_response(session->id, chan_id, creation_status);
    }
    LOG_DEVEL(LOG_LEVEL_WARNING, "Dynamic Virtual Channel %s (%d): "
              "callback 'open_response' is NULL",
              XRDP_DRDYNVC_CHANNEL_ID_TO_NAME(self, chan_id),
              chan_id);
    return 0;
}

/*****************************************************************************/
/*
 * Process a [MS-RDPEDYC] DYNVC_CLOSE message.
 */
static int
drdynvc_process_close_channel_response(struct xrdp_channel *self,
                                       int cmd, struct stream *s)
{
    struct xrdp_session *session;
    uint32_t chan_id;
    struct xrdp_drdynvc *drdynvc;

    if (drdynvc_get_chan_id(s, cmd, &chan_id) != 0) /* ChannelId */
    {
        LOG(LOG_LEVEL_ERROR,
            "drdynvc_process_close_channel_response: drdynvc_get_chan_id failed");
        return 1;
    }
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPEDYC] DYNVC_CLOSE "
              "ChannelId %d", chan_id);
    session = self->sec_layer->rdp_layer->session;
    if (chan_id > 255)
    {
        LOG(LOG_LEVEL_ERROR, "Received message for an invalid "
            "channel id. channel id %d", chan_id);
        return 1;
    }

    drdynvc = self->drdynvcs + chan_id;
    drdynvc->status = XRDP_DRDYNVC_STATUS_CLOSED;
    LOG_DEVEL(LOG_LEVEL_DEBUG,
              "Dynamic Virtual Channel %s (%d) updated: status = %s",
              XRDP_DRDYNVC_CHANNEL_ID_TO_NAME(self, chan_id),
              chan_id,
              XRDP_DRDYNVC_STATUS_TO_STR(drdynvc->status));

    if (drdynvc->close_response != NULL)
    {
        return drdynvc->close_response(session->id, chan_id);
    }
    LOG_DEVEL(LOG_LEVEL_WARNING, "Dynamic Virtual Channel %s (%d): "
              "callback 'close_response' is NULL",
              XRDP_DRDYNVC_CHANNEL_ID_TO_NAME(self, chan_id),
              chan_id);
    return 0;
}

/*****************************************************************************/
/*
 * Process a [MS-RDPEDYC] DYNVC_DATA_FIRST message.
 */
static int
drdynvc_process_data_first(struct xrdp_channel *self,
                           int cmd, struct stream *s)
{
    struct xrdp_session *session;
    uint32_t chan_id;
    int len;
    int bytes;
    int total_bytes;
    struct xrdp_drdynvc *drdynvc;

    if (drdynvc_get_chan_id(s, cmd, &chan_id) != 0) /* ChannelId */
    {
        LOG(LOG_LEVEL_ERROR,
            "Parsing [MS-RDPEDYC] DYNVC_DATA_FIRST failed");
        return 1;
    }
    len = (cmd >> 2) & 0x03;
    if (len == 0)
    {
        if (!s_check_rem_and_log(s, 1, "Parsing [MS-RDPEDYC] DYNVC_DATA_FIRST"))
        {
            return 1;
        }
        in_uint8(s, total_bytes); /* Length */
    }
    else if (len == 1)
    {
        if (!s_check_rem_and_log(s, 2, "Parsing [MS-RDPEDYC] DYNVC_DATA_FIRST"))
        {
            return 1;
        }
        in_uint16_le(s, total_bytes); /* Length */
    }
    else
    {
        if (!s_check_rem_and_log(s, 4, "Parsing [MS-RDPEDYC] DYNVC_DATA_FIRST"))
        {
            return 1;
        }
        in_uint32_le(s, total_bytes); /* Length */
    }
    bytes = (int) (s->end - s->p);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPEDYC] DYNVC_DATA_FIRST "
              "ChannelId %d, Length %d, Data (omitted from the log)",
              chan_id, total_bytes);

    session = self->sec_layer->rdp_layer->session;
    if (chan_id > 255)
    {
        LOG(LOG_LEVEL_ERROR, "Received [MS-RDPEDYC] DYNVC_DATA_FIRST for an "
            "invalid channel id. Max allowed 255, received %d", chan_id);
        return 1;
    }
    drdynvc = self->drdynvcs + chan_id;
    if (drdynvc->data_first != NULL)
    {
        return drdynvc->data_first(session->id, chan_id, s->p,
                                   bytes, total_bytes);
    }
    LOG_DEVEL(LOG_LEVEL_WARNING, "Dynamic Virtual Channel %s (%d): "
              "callback 'data_first' is NULL",
              XRDP_DRDYNVC_CHANNEL_ID_TO_NAME(self, chan_id),
              chan_id);
    return 0;
}

/*****************************************************************************/
/*
 * Process a [MS-RDPEDYC] DYNVC_DATA message.
 */
static int
drdynvc_process_data(struct xrdp_channel *self,
                     int cmd, struct stream *s)
{
    struct xrdp_session *session;
    uint32_t chan_id;
    int bytes;
    struct xrdp_drdynvc *drdynvc;

    if (drdynvc_get_chan_id(s, cmd, &chan_id) != 0) /* ChannelId */
    {
        LOG(LOG_LEVEL_ERROR, "drdynvc_process_data: drdynvc_get_chan_id failed");
        return 1;
    }
    bytes = (int) (s->end - s->p);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPEDYC] DYNVC_DATA "
              "ChannelId %d, (re-assembled) Length %d, Data (omitted from the log)",
              chan_id, bytes);
    session = self->sec_layer->rdp_layer->session;
    if (chan_id > 255)
    {
        LOG(LOG_LEVEL_ERROR, "Received message for an invalid "
            "channel id. channel id %d", chan_id);
        return 1;
    }
    drdynvc = self->drdynvcs + chan_id;
    if (drdynvc->data != NULL)
    {
        return drdynvc->data(session->id, chan_id, s->p, bytes);
    }
    LOG_DEVEL(LOG_LEVEL_WARNING, "Dynamic Virtual Channel %s (%d): "
              "callback 'data' is NULL",
              XRDP_DRDYNVC_CHANNEL_ID_TO_NAME(self, chan_id),
              chan_id);
    return 0;
}

/*****************************************************************************/
/**
 * Process a [MS-RDPBCGR] 2.2.6.1 Virtual Channel PDU and re-assemble the
 * data chunks as needed.
 */
static int
xrdp_channel_process_drdynvc(struct xrdp_channel *self,
                             struct mcs_channel_item *channel,
                             struct stream *s)
{
    int total_length;
    int length;
    int flags;
    int cmd;
    int rv;
    struct stream *ls;

    if (!s_check_rem_and_log(s, 8, "Parsing [MS-RDPBCGR] CHANNEL_PDU_HEADER"))
    {
        return 1;
    }
    in_uint32_le(s, total_length); /* length */
    in_uint32_le(s, flags);        /* flags */
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received header [MS-RDPBCGR] CHANNEL_PDU_HEADER "
              "length %d, flags 0x%8.8x", total_length, flags);
    ls = NULL;
    switch (flags & 3)
    {
        case 0: /* not first chunk and not last chunk */
            length = (int) (s->end - s->p);
            LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPBCGR] data chunk (middle) "
                      "length %d", length);
            if (length > s_rem_out(self->s))
            {
                LOG(LOG_LEVEL_ERROR, "[MS-RDPBCGR] Data chunk length is bigger than "
                    "the remaining chunk buffer size. length %d, remaining %d",
                    length, s_rem_out(self->s));
                return 1;
            }
            out_uint8a(self->s, s->p, length); /* append data to chunk buffer */
            in_uint8s(s, length);              /* virtualChannelData */
            return 0;
        case 1: /* CHANNEL_FLAG_FIRST */
            free_stream(self->s);
            make_stream(self->s);
            init_stream(self->s, total_length);
            length = (int) (s->end - s->p);
            LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPBCGR] data chunk (first) "
                      "length %d", length);
            if (length > s_rem_out(self->s))
            {
                LOG(LOG_LEVEL_ERROR, "[MS-RDPBCGR] Data chunk length is bigger than "
                    "the remaining chunk buffer size. length %d, remaining %d",
                    length, s_rem_out(self->s));
                return 1;
            }
            out_uint8a(self->s, s->p, length); /* append data to chunk buffer */
            in_uint8s(s, length);              /* virtualChannelData */
            return 0;
        case 2: /* CHANNEL_FLAG_LAST */
            length = (int) (s->end - s->p);
            LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPBCGR] data chunk (last) "
                      "length %d", length);
            if (length > s_rem_out(self->s))
            {
                LOG(LOG_LEVEL_ERROR, "[MS-RDPBCGR] Data chunk length is bigger than "
                    "the remaining chunk buffer size. length %d, remaining %d",
                    length, s_rem_out(self->s));
                return 1;
            }
            out_uint8a(self->s, s->p, length); /* append data to chunk buffer */
            in_uint8s(s, length);              /* virtualChannelData */
            ls = self->s;
            break;
        case 3: /* CHANNEL_FLAG_FIRST and CHANNEL_FLAG_LAST */
            LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPBCGR] data chunk (first and last) "
                      "length %d", total_length);
            ls = s;
            break;
        default:
            LOG(LOG_LEVEL_ERROR, "Received [MS-RDPBCGR] data chunk with "
                "unknown flag 0x%8.8x", (int) (flags & 3));
            return 1;
    }
    if (ls == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "BUG: ls must not be NULL");
        return 1;
    }
    in_uint8(ls, cmd); /* cbId (low 2 bits), Sp (2 bits), Cmd (hi 4 bits) */
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received header [MS-RDPEDYC] "
              "cbId %d, Sp %d, Cmd 0x%2.2x",
              (cmd & 0x03), (cmd & 0x0c) >> 2, (cmd & 0xf0) >> 4);
    rv = 1;
    switch (cmd & 0xf0)
    {
        case CMD_DVC_CAPABILITY:
            rv = drdynvc_process_capability_response(self, cmd, s);
            break;
        case CMD_DVC_OPEN_CHANNEL:
            rv = drdynvc_process_open_channel_response(self, cmd, s);
            break;
        case CMD_DVC_CLOSE_CHANNEL:
            rv = drdynvc_process_close_channel_response(self, cmd, s);
            break;
        case CMD_DVC_DATA_FIRST:
            rv = drdynvc_process_data_first(self, cmd, s);
            break;
        case CMD_DVC_DATA:
            rv = drdynvc_process_data(self, cmd, s);
            break;
        default:
            LOG(LOG_LEVEL_ERROR, "Received header [MS-RDPEDYC] with "
                "unknown command 0x%2.2x", cmd);
            break;
    }
    return rv;
}

/*****************************************************************************/
/* Process a static ([MS-RDPBCGR] 2.2.6) or dynamic (MS-RDPEDYC 2.2.3)
 * virtual channel message.
 * returns error */
/* This is called from the secure layer to process an incoming non global
   channel packet.
   'chanid' passed in here is the mcs channel id so it MCS_GLOBAL_CHANNEL
   plus something. */
int
xrdp_channel_process(struct xrdp_channel *self, struct stream *s,
                     int chanid)
{
    int length;
    int flags;
    int rv;
    int channel_id;
    struct mcs_channel_item *channel;

    /* this assumes that the channels are in order of chanid(mcs channel id)
       but they should be, see xrdp_sec_process_mcs_data_channels
       the first channel should be MCS_GLOBAL_CHANNEL + 1, second
       one should be MCS_GLOBAL_CHANNEL + 2, and so on */
    channel_id = (chanid - MCS_GLOBAL_CHANNEL) - 1;
    channel = xrdp_channel_get_item(self, channel_id);
    if (channel == NULL)
    {
        LOG(LOG_LEVEL_ERROR,
            "Received a message for an unknown channel id. channel id %d",
            chanid);
        return 1;
    }
    if (channel->disabled)
    {
        LOG(LOG_LEVEL_WARNING,
            "Received a message for the disabled channel %s (%d)",
            channel->name, chanid);
        return 0; /* not an error */
    }
    if (channel_id == self->drdynvc_channel_id)
    {
        return xrdp_channel_process_drdynvc(self, channel, s);
    }
    rv = 0;
    in_uint32_le(s, length); /* length */
    in_uint32_le(s, flags);  /* flags */
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received header [MS-RDPBCGR] CHANNEL_PDU_HEADER "
              "length %d, flags 0x%8.8x", length, flags);
    rv = xrdp_channel_call_callback(self, s, channel_id, length, flags);
    return rv;
}

/*****************************************************************************/
/* Send a [MS-RDPEDYC] DYNVC_CAPS_VERSION2 message */
static int
xrdp_channel_drdynvc_send_capability_request(struct xrdp_channel *self)
{
    struct stream *s;
    int flags;
    int total_data_len;
    int channel_id;
    char *phold;

    /* setup stream */
    make_stream(s);
    init_stream(s, 8192);
    if (xrdp_channel_init(self, s) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "xrdp_channel_drdynvc_send_capability_request: xrdp_channel_init failed");
        free_stream(s);
        return 1;
    }
    phold = s->p;
    out_uint8(s, 0x50);         /* insert cbId (2 bits), Sp (2 bits), cmd (4 bits) */
    out_uint8(s, 0x00);         /* insert padding   */
    out_uint16_le(s, 2);        /* insert version   */
    /* channel priority unused for now              */
    out_uint16_le(s, 0x0000);  /* priority charge 0 */
    out_uint16_le(s, 0x0000);  /* priority charge 1 */
    out_uint16_le(s, 0x0000);  /* priority charge 2 */
    out_uint16_le(s, 0x0000);  /* priority charge 3 */
    s_mark_end(s);
    /* send command to client */
    total_data_len = (int) (s->end - phold);
    flags = XR_CHANNEL_FLAG_FIRST | XR_CHANNEL_FLAG_LAST;
    channel_id = self->drdynvc_channel_id;
    LOG_DEVEL(LOG_LEVEL_TRACE, "Sending [MS-RDPEDYC] DYNVC_CAPS_VERSION2 "
              "cbId 0, Sp 0, Cmd 0x05, Version 2, PriorityCharge0 0, "
              "PriorityCharge1 0, PriorityCharge2 0, PriorityCharge3 0");
    if (xrdp_channel_send(self, s, channel_id, total_data_len, flags) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "xrdp_channel_drdynvc_send_capability_request: xrdp_channel_send failed");
        free_stream(s);
        return 1;
    }
    free_stream(s);
    return 0;
}

/*****************************************************************************/
int
xrdp_channel_drdynvc_start(struct xrdp_channel *self)
{
    int index;
    int count;
    struct mcs_channel_item *ci;
    struct mcs_channel_item *dci;

    LOG_DEVEL(LOG_LEVEL_INFO, "xrdp_channel_drdynvc_start: drdynvc_channel_id %d", self->drdynvc_channel_id);
    if (self->drdynvc_channel_id != -1)
    {
        LOG_DEVEL(LOG_LEVEL_INFO, "xrdp_channel_drdynvc_start: already started");
        return 0;
    }
    dci = NULL;
    count = self->mcs_layer->channel_list->count;
    for (index = 0; index < count; index++)
    {
        ci = (struct mcs_channel_item *)
             list_get_item(self->mcs_layer->channel_list, index);
        if (ci != NULL)
        {
            if (g_strcasecmp(ci->name, "drdynvc") == 0)
            {
                dci = ci;
            }
        }
    }
    if (dci != NULL)
    {
        self->drdynvc_channel_id = (dci->chanid - MCS_GLOBAL_CHANNEL) - 1;
        LOG_DEVEL(LOG_LEVEL_DEBUG,
                  "Initializing Dynamic Virtual Channel with channel id %d",
                  self->drdynvc_channel_id);
        xrdp_channel_drdynvc_send_capability_request(self);
    }
    else
    {
        LOG(LOG_LEVEL_WARNING,
            "Dynamic Virtual Channel named 'drdynvc' not found, "
            "channel not initialized");
    }
    return 0;
}

/*****************************************************************************/
/*
 * Send a [MS-RDPEDYC] DYNVC_CREATE_REQ message to request the creation of a channel.
 */
int
xrdp_channel_drdynvc_open(struct xrdp_channel *self, const char *name,
                          int flags, struct xrdp_drdynvc_procs *procs,
                          int *chan_id)
{
    struct stream *s;
    int ChId;
    int cbChId;
    int chan_pri;
    int static_channel_id;
    int name_length;
    int total_data_len;
    int static_flags;
    char *cmd_ptr;

    make_stream(s);
    init_stream(s, 8192);
    if (xrdp_channel_init(self, s) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "xrdp_channel_drdynvc_open: xrdp_channel_init failed");
        free_stream(s);
        return 1;
    }
    cmd_ptr = s->p;
    out_uint8(s, 0); /* set later */
    ChId = 1;
    while (self->drdynvcs[ChId].status != XRDP_DRDYNVC_STATUS_CLOSED)
    {
        ChId++;
        if (ChId > 255)
        {
            LOG(LOG_LEVEL_ERROR,
                "Attempting to create a new channel when the maximum "
                "number of channels have already been created. "
                "XRDP only supports 255 open channels.");
            free_stream(s);
            return 1;
        }
    }
    cbChId = drdynvc_insert_uint_124(s, ChId); /* ChannelId */
    name_length = g_strlen(name);
    out_uint8a(s, name, name_length + 1); /* ChannelName */
    chan_pri = 0;
    /* cbId (low 2 bits), Pri (2 bits), Cmd (hi 4 bits) */
    cmd_ptr[0] = CMD_DVC_OPEN_CHANNEL | ((chan_pri << 2) & 0x0c) | cbChId;
    static_channel_id = self->drdynvc_channel_id;
    static_flags = XR_CHANNEL_FLAG_FIRST | XR_CHANNEL_FLAG_LAST;
    s_mark_end(s);
    total_data_len = (int) (s->end - cmd_ptr);

    LOG_DEVEL(LOG_LEVEL_TRACE, "Sending [MS-RDPEDYC] DYNVC_CREATE_REQ "
              "cbId %d, Pri %d, Cmd 0x%2.2x, ChannelId %d, ChannelName [%s]",
              cbChId, chan_pri, CMD_DVC_OPEN_CHANNEL, ChId, name);
    if (xrdp_channel_send(self, s, static_channel_id, total_data_len,
                          static_flags) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "Sending [MS-RDPEDYC] DYNVC_CREATE_REQ failed");
        free_stream(s);
        return 1;
    }
    free_stream(s);
    *chan_id = ChId;
    self->drdynvcs[ChId].open_response = procs->open_response;
    self->drdynvcs[ChId].close_response = procs->close_response;
    self->drdynvcs[ChId].data_first = procs->data_first;
    self->drdynvcs[ChId].data = procs->data;
    self->drdynvcs[ChId].status = XRDP_DRDYNVC_STATUS_OPEN_SENT;
    return 0;
}

/*****************************************************************************/
/*
 * Send a [MS-RDPEDYC] DYNVC_CLOSE message to request the closing of a channel.
 */
int
xrdp_channel_drdynvc_close(struct xrdp_channel *self, int chan_id)
{
    struct stream *s;
    int ChId;
    int cbChId;
    int static_channel_id;
    int total_data_len;
    int static_flags;
    char *cmd_ptr;

    if ((chan_id < 0) || (chan_id > 255))
    {
        LOG(LOG_LEVEL_ERROR, "Attempting to close an invalid channel id. "
            "channel id %d", chan_id);
        return 1;
    }
    if ((self->drdynvcs[chan_id].status != XRDP_DRDYNVC_STATUS_OPEN) &&
            (self->drdynvcs[chan_id].status != XRDP_DRDYNVC_STATUS_OPEN_SENT))
    {
        /* not open */
        LOG(LOG_LEVEL_ERROR, "Attempting to close a channel that is not open. "
            "channel id %d, channel status %s",
            chan_id,
            XRDP_DRDYNVC_STATUS_TO_STR(self->drdynvcs[chan_id].status));
        return 1;
    }
    make_stream(s);
    init_stream(s, 8192);
    if (xrdp_channel_init(self, s) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "xrdp_channel_drdynvc_close: xrdp_channel_init failed");
        free_stream(s);
        return 1;
    }
    cmd_ptr = s->p;
    out_uint8(s, 0); /* set later */
    ChId = chan_id;
    cbChId = drdynvc_insert_uint_124(s, ChId); /* ChannelId */
    /* cbId (low 2 bits), Sp (2 bits), Cmd (hi 4 bits) */
    cmd_ptr[0] = CMD_DVC_CLOSE_CHANNEL | cbChId;
    static_channel_id = self->drdynvc_channel_id;
    static_flags = XR_CHANNEL_FLAG_FIRST | XR_CHANNEL_FLAG_LAST;
    s_mark_end(s);
    total_data_len = (int) (s->end - cmd_ptr);

    LOG_DEVEL(LOG_LEVEL_TRACE, "Sending [MS-RDPEDYC] DYNVC_CLOSE "
              "cbId %d, Sp 0, Cmd 0x%2.2x, ChannelId %d",
              cbChId, CMD_DVC_CLOSE_CHANNEL, ChId);
    if (xrdp_channel_send(self, s, static_channel_id, total_data_len,
                          static_flags) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "xrdp_channel_drdynvc_open: xrdp_channel_send failed");
        free_stream(s);
        return 1;
    }
    free_stream(s);
    self->drdynvcs[ChId].status = XRDP_DRDYNVC_STATUS_CLOSE_SENT;
    return 0;
}

/*****************************************************************************/
/*
 * Send a [MS-RDPEDYC] DYNVC_DATA_FIRST message.
 */
int
xrdp_channel_drdynvc_data_first(struct xrdp_channel *self, int chan_id,
                                const char *data, int data_bytes,
                                int total_data_bytes)
{
    struct stream *s;
    int ChId;
    int cbChId;
    int cbTotalDataSize;
    int static_channel_id;
    int total_data_len;
    int static_flags;
    char *cmd_ptr;

    if ((chan_id < 0) || (chan_id > 255))
    {
        LOG(LOG_LEVEL_ERROR, "Attempting to send data to an invalid "
            "channel id. channel id %d", chan_id);
        return 1;
    }
    if (self->drdynvcs[chan_id].status != XRDP_DRDYNVC_STATUS_OPEN)
    {
        LOG(LOG_LEVEL_ERROR, "Attempting to send data to a channel that "
            "is not open. channel id %d, channel status %s",
            chan_id,
            XRDP_DRDYNVC_STATUS_TO_STR(self->drdynvcs[chan_id].status));
        return 1;
    }
    if (data_bytes > 1590)
    {
        LOG(LOG_LEVEL_ERROR, "Payload for channel id %d is is too big. "
            "data_bytes %d", chan_id, data_bytes);
        return 1;
    }
    make_stream(s);
    init_stream(s, 8192);
    if (xrdp_channel_init(self, s) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "xrdp_channel_drdynvc_data_first: xrdp_channel_init failed");
        free_stream(s);
        return 1;
    }
    cmd_ptr = s->p;
    out_uint8(s, 0); /* set later */
    ChId = chan_id;
    cbChId = drdynvc_insert_uint_124(s, ChId); /* ChannelId */
    cbTotalDataSize = drdynvc_insert_uint_124(s, total_data_bytes); /* Length */
    out_uint8p(s, data, data_bytes);           /* Data */
    /* cbId (low 2 bits), Len (2 bits), Cmd (hi 4 bits) */
    cmd_ptr[0] = CMD_DVC_DATA_FIRST | (cbTotalDataSize << 2) | cbChId;
    static_channel_id = self->drdynvc_channel_id;
    static_flags = XR_CHANNEL_FLAG_FIRST | XR_CHANNEL_FLAG_LAST;
    s_mark_end(s);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Sending [MS-RDPEDYC] DYNVC_DATA_FIRST "
              "cbId %d, Len %d, Cmd 0x%2.2x, ChannelId %d, Length %d",
              cbChId, cbTotalDataSize, CMD_DVC_DATA_FIRST, ChId, total_data_bytes);
    total_data_len = (int) (s->end - cmd_ptr);
    if (xrdp_channel_send(self, s, static_channel_id, total_data_len,
                          static_flags) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "xrdp_channel_drdynvc_data_first: xrdp_channel_send failed");
        free_stream(s);
        return 1;
    }
    free_stream(s);
    return 0;
}

/*****************************************************************************/
/*
 * Send a [MS-RDPEDYC] DYNVC_DATA message.
 */
int
xrdp_channel_drdynvc_data(struct xrdp_channel *self, int chan_id,
                          const char *data, int data_bytes)
{
    struct stream *s;
    int ChId;
    int cbChId;
    int static_channel_id;
    int total_data_len;
    int static_flags;
    char *cmd_ptr;

    if ((chan_id < 0) || (chan_id > 255))
    {
        LOG(LOG_LEVEL_ERROR, "Attempting to send data to an invalid "
            "channel id. channel id %d", chan_id);
        return 1;
    }
    if (self->drdynvcs[chan_id].status != XRDP_DRDYNVC_STATUS_OPEN)
    {
        LOG(LOG_LEVEL_ERROR, "Attempting to send data to a channel that "
            "is not open. channel id %d, channel status %s",
            chan_id,
            XRDP_DRDYNVC_STATUS_TO_STR(self->drdynvcs[chan_id].status));
        return 1;
    }
    if (data_bytes > 1590)
    {
        LOG(LOG_LEVEL_ERROR, "Payload for channel id %d is is too big. "
            "data_bytes %d", chan_id, data_bytes);
        return 1;
    }
    make_stream(s);
    init_stream(s, 8192);
    if (xrdp_channel_init(self, s) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "xrdp_channel_drdynvc_data: xrdp_channel_init failed");
        free_stream(s);
        return 1;
    }
    cmd_ptr = s->p;
    out_uint8(s, 0); /* set later */
    ChId = chan_id;
    cbChId = drdynvc_insert_uint_124(s, ChId); /* ChannelId */
    out_uint8p(s, data, data_bytes);           /* Data */
    /* cbId (low 2 bits), Sp (2 bits), Cmd (hi 4 bits) */
    cmd_ptr[0] = CMD_DVC_DATA | cbChId;
    static_channel_id = self->drdynvc_channel_id;
    static_flags = XR_CHANNEL_FLAG_FIRST | XR_CHANNEL_FLAG_LAST;
    s_mark_end(s);
    total_data_len = (int) (s->end - cmd_ptr);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Sending [MS-RDPEDYC] DYNVC_DATA "
              "cbId %d, Sp 0, Cmd 0x%2.2x, ChannelId %d",
              cbChId, CMD_DVC_DATA_FIRST, ChId);
    if (xrdp_channel_send(self, s, static_channel_id, total_data_len,
                          static_flags) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "xrdp_channel_drdynvc_data: xrdp_channel_send failed");
        free_stream(s);
        return 1;
    }
    free_stream(s);
    return 0;
}
