/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Laxmikant Rashinkar 2012 LK.Rashinkar@gmail.com
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

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "drdynvc.h"

extern int g_drdynvc_chan_id; /* in chansrv.c */
int g_drdynvc_inited = 0;

static int drdynvc_send_capability_request(uint16_t version);
static int drdynvc_process_capability_response(struct stream* s, unsigned char cmd);
static int drdynvc_process_open_channel_response(struct stream *s, unsigned char cmd);
static int drdynvc_process_close_channel_response(struct stream *s, unsigned char cmd);
static int drdynvc_process_data_first(struct stream* s, unsigned char cmd);
static int drdynvc_process_data(struct stream* s, unsigned char cmd);
static int drdynvc_insert_uint_124(struct stream *s, uint32_t val);
static int drdynvc_get_chan_id(struct stream *s, char cmd, uint32_t *chan_id_p);

/**
 * bring up dynamic virtual channel
 *
 * @return 0 on success, -1 on response
 ******************************************************************************/
int
drdynvc_init(void)
{
    /* bring up X11 */
    xcommon_init();

    /* let client know what version of DVC we support */
    drdynvc_send_capability_request(2);

    return 0;
}

/**
 * let DVC Manager on client end know what version of protocol we support
 * client will respond with version that it supports
 *
 * @return 0 on success, -1 on response
 ******************************************************************************/
static int
drdynvc_send_capability_request(uint16_t version)
{
    struct stream *s;
    int bytes_in_stream;

    /* setup stream */
    make_stream(s);
    init_stream(s, MAX_PDU_SIZE);

    out_uint8(s, 0x50);         /* insert cmd       */
    out_uint8(s, 0x00);         /* insert padding   */
    out_uint16_le(s, version);  /* insert version   */

    /* channel priority unused for now              */
    out_uint16_le(s, 0x0000);  /* priority charge 0 */
    out_uint16_le(s, 0x0000);  /* priority charge 1 */
    out_uint16_le(s, 0x0000);  /* priority charge 2 */
    out_uint16_le(s, 0x0000);  /* priority charge 3 */

    /* send command to client */
    bytes_in_stream = stream_length_before_p(s);
    send_channel_data(g_drdynvc_chan_id, s->data, bytes_in_stream);
    free_stream(s);

    return 0;
}

/**
 * process capability response received from DVC manager at client end
 *
 * @param s stream containing client response
 *
 * @return 0 on success, -1 on failure
 ******************************************************************************/
static int
drdynvc_process_capability_response(struct stream *s, unsigned char cmd)
{
    int cap_version;

    /* skip padding */
    in_uint8s(s, 1);

    /* read client's version */
    in_uint16_le(s, cap_version);

    if ((cap_version != 2) && (cap_version != 3))
    {
        LOG(0, ("drdynvc_process_capability_response: incompatible DVC "
            "version %d detected", cap_version));
        return -1;
    }

    LOG(0, ("drdynvc_process_capability_response: DVC version %d selected",
        cap_version));
    g_drdynvc_inited = 1;

    return 0;
}

/**
 * create a new dynamic virtual channel
 *
 * @pram  channel_id    channel id number
 * @pram  channel_name  name of channel
 *
 * @return 0 on success, -1 on failure
 ******************************************************************************/
int
drdynvc_send_open_channel_request(int chan_pri, unsigned int chan_id,
                                  char *chan_name)
{
    struct stream *s;
    int            bytes_in_stream;
    int            cbChId;
    int            name_length;

    if ((chan_name == NULL) || (strlen(chan_name) == 0))
    {
        LOG(0, ("drdynvc_send_open_channel_request: bad channel name specified"));
        return -1;
    }

    make_stream(s);
    init_stream(s, MAX_PDU_SIZE);

    name_length = strlen(chan_name);

    /* dummy command for now */
    out_uint8(s, 0);

    /* insert channel id */
    cbChId = drdynvc_insert_uint_124(s, chan_id);

    /* insert channel name */
    out_uint8a(s, chan_name, name_length + 1);

    /* insert command */
    s->data[0] = CMD_DVC_OPEN_CHANNEL | ((chan_pri << 2) & 0x0c) | cbChId;

    /* send command */
    bytes_in_stream = stream_length_before_p(s);
    send_channel_data(g_drdynvc_chan_id, s->data, bytes_in_stream);
    free_stream(s);

    return 0;
}

static int
drdynvc_process_open_channel_response(struct stream *s, unsigned char cmd)
{
    struct xrdp_api_data *adp;

    uint32_t chan_id;
    int      creation_status;

    drdynvc_get_chan_id(s, cmd, &chan_id);
    in_uint32_le(s, creation_status);

    /* LK_TODO now do something using useful! */

    if (creation_status < 0)
    {
        // TODO
    }
    else
    {
        /* get struct xrdp_api_data containing this channel id */
        if ((adp = struct_from_dvc_chan_id(chan_id)) == NULL)
        {
            LOG(0, ("drdynvc_process_open_channel_response: error : "
                    "could not find xrdp_api_data containing chan_id %d", chan_id));

            return -1;
        }

        adp->is_connected = 1;
    }

    return 0;
}

int
drdynvc_send_close_channel_request(unsigned int chan_id)
{
    struct stream *s;
    int            bytes_in_stream;
    int            cbChId;

    make_stream(s);
    init_stream(s, MAX_PDU_SIZE);

    /* insert dummy cmd for now */
    out_uint8(s, 0);

    /* insert channel id */
    cbChId = drdynvc_insert_uint_124(s, chan_id);

    /* insert command */
    s->data[0] = CMD_DVC_CLOSE_CHANNEL | cbChId;

    /* send command */
    bytes_in_stream = stream_length_before_p(s);
    send_channel_data(g_drdynvc_chan_id, s->data, bytes_in_stream);

    free_stream(s);
    return 0;
}

static int
drdynvc_process_close_channel_response(struct stream *s, unsigned char cmd)
{
    uint32_t chan_id;

    drdynvc_get_chan_id(s, cmd, &chan_id);

    /* LK_TODO now do something using useful! */

    return 0;
}

/*
 * send data to client
 *
 * @param  chan_id    the virtual channel to write to
 * @param  data       data to write
 * @param  data_size  number of bytes to write
 *
 * @return 0 on success, -1 on failure
 ******************************************************************************/
int drdynvc_write_data(uint32_t chan_id, char *data, int data_size)
{
    struct stream *s;
    char          *saved_ptr;
    int            cbChId;
    int            Len;
    int            bytes_in_stream;
    int            frag_size;

    if (data == NULL)
    {
        LOG(0, ("drdynvc_write_data: data is NULL\n"));
        return -1;
    }

    if (data_size <= 0)
    {
        return 0;
    }

    make_stream(s);
    init_stream(s, MAX_PDU_SIZE);

    /* this is a dummy write */
    out_uint8(s, 0);

    /* insert channel id */
    cbChId = drdynvc_insert_uint_124(s, chan_id);

    /* will data fit into one pkt? */
    bytes_in_stream = stream_length_before_p(s);

    if ((bytes_in_stream + data_size) <= MAX_PDU_SIZE)
    {
        /* yes it will - insert data */
        out_uint8p(s, data, data_size);

        /* insert command */
        s->data[0] = CMD_DVC_DATA | cbChId;

        /* write data to client */
        bytes_in_stream = stream_length_before_p(s);
        send_channel_data(g_drdynvc_chan_id, s->data, bytes_in_stream);
        free_stream(s);
        return 0;
    }

    /* no it won't - fragment it */

    saved_ptr = s->p;

    /* let client know how much data to expect */
    Len = drdynvc_insert_uint_124(s, data_size);

    /* insert data into first fragment */
    frag_size = MAX_PDU_SIZE - stream_length_before_p(s);
    out_uint8p(s, data, frag_size);

    /* insert command */
    s->data[0] = CMD_DVC_DATA_FIRST | Len << 2 | cbChId;

    /* write first fragment to client */
    bytes_in_stream = stream_length_before_p(s);
    send_channel_data(g_drdynvc_chan_id, s->data, bytes_in_stream);
    data_size -= frag_size;
    data += frag_size;
    s->data[0] = CMD_DVC_DATA | cbChId;
    s->p = saved_ptr;

    /* now send rest of the data using CMD_DVC_DATA */
    while (data_size > 0)
    {
        frag_size = MAX_PDU_SIZE - stream_length_before_p(s);

        if (frag_size > data_size)
        {
            frag_size = data_size;
        }

        out_uint8p(s, data, frag_size);
        bytes_in_stream = stream_length_before_p(s);
        send_channel_data(g_drdynvc_chan_id, s->data, bytes_in_stream);
        data_size -= frag_size;
        data += frag_size;
        s->p = saved_ptr;
    }

    free_stream(s);
    return 0;
}

static int
drdynvc_process_data_first(struct stream *s, unsigned char cmd)
{
    struct xrdp_api_data *adp;
    struct stream        *ls;

    uint32_t chan_id;
    int      bytes_in_stream;
    int      Len;

    drdynvc_get_chan_id(s, cmd, &chan_id);

    Len = (cmd >> 2) & 0x03;

    /* skip data_len */
    if (Len == 0)
    {
        in_uint8s(s, 1);
    }
    else if (Len == 1)
    {
        in_uint8s(s, 2);
    }
    else
    {
        in_uint8s(s, 4);
    }

    bytes_in_stream = stream_length_after_p(s);

    /* get struct xrdp_api_data containing this channel id */
    if ((adp = struct_from_dvc_chan_id(chan_id)) == NULL)
    {
        LOG(0, ("drdynvc_process_data_first: error : "
                "could not find xrdp_api_data containing chan_id %d", chan_id));

        return -1;
    }

    ls = trans_get_out_s(adp->transp, MAX_PDU_SIZE);
    out_uint8p(ls, s->p, bytes_in_stream);
    s_mark_end(ls);
    trans_force_write(adp->transp);

    return 0;
}

static int
drdynvc_process_data(struct stream *s, unsigned char cmd)
{
    struct xrdp_api_data *adp;
    struct stream        *ls;

    uint32_t chan_id;
    int      bytes_in_stream;

    drdynvc_get_chan_id(s, cmd, &chan_id);
    bytes_in_stream = stream_length_after_p(s);

    /* get struct xrdp_api_data containing this channel id */
    if ((adp = struct_from_dvc_chan_id(chan_id)) == NULL)
    {
        LOG(0, ("drdynvc_process_data: error : "
                "could not find xrdp_api_data containing chan_id %d", chan_id));

        return -1;
    }

    ls = trans_get_out_s(adp->transp, MAX_PDU_SIZE);
    out_uint8p(ls, s->p, bytes_in_stream);
    s_mark_end(ls);
    trans_force_write(adp->transp);

    return 0;
}

/**
 * process incoming data on a dynamic virtual channel
 *
 * @pram  s             stream containing the incoming data
 * @pram  chan_id       LK_TODO
 * @pram  chan_flags    LK_TODO
 * @pram  length        LK_TODO
 * @pram  total_length  LK_TODO
 *
 * @return 0 on success, -1 on failure
 ******************************************************************************/
int
drdynvc_data_in(struct stream *s, int chan_id, int chan_flags, int length,
                int total_length)
{
    unsigned char cmd;

    in_uint8(s, cmd); /* read command */

    switch (cmd & 0xf0)
    {
        case CMD_DVC_CAPABILITY:
            drdynvc_process_capability_response(s, cmd);
            break;

        case CMD_DVC_OPEN_CHANNEL:
            drdynvc_process_open_channel_response(s, cmd);
            break;

        case CMD_DVC_CLOSE_CHANNEL:
            drdynvc_process_close_channel_response(s, cmd);
            break;

        case CMD_DVC_DATA_FIRST:
            drdynvc_process_data_first(s, cmd);
            break;

        case CMD_DVC_DATA:
            drdynvc_process_data(s, cmd);
            break;

        default:
            LOG(0, ("drdynvc_data_in: got unknown command 0x%x", cmd));
            break;
    }

    return 0;
}

/*
 * insert a byte, short or 32bit value into specified stream
 *
 * @param  s    stream used for insertion
 * @param  val  value to insert
 *
 * @return 0 for byte insertions
 * @return 1 for short insertion
 * @return 2 for uint32_t insertions
 ******************************************************************************/
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

/*
 * extract channel id from stream
 *
 * @param   s        stream containing channel id
 * @param   cmd      first byte in stream
 * @param   chan_id  return channel id  here
 ******************************************************************************/
static int
drdynvc_get_chan_id(struct stream *s, char cmd, uint32_t *chan_id_p)
{
    int cbChId;
    int chan_id;

    cbChId = cmd & 0x03;

    if (cbChId == 0)
    {
        in_uint8(s, chan_id);
    }
    else if (cbChId == 1)
    {
        in_uint16_le(s, chan_id);
    }
    else
    {
        in_uint32_le(s, chan_id);
    }

    *chan_id_p = chan_id;

    return 0;
}
