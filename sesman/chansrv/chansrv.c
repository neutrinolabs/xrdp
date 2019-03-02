/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2009-2013
 * Copyright (C) Laxmikant Rashinkar 2009-2012
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

#include "arch.h"
#include "os_calls.h"
#include "thread_calls.h"
#include "trans.h"
#include "chansrv.h"
#include "defines.h"
#include "sound.h"
#include "clipboard.h"
#include "devredir.h"
#include "list.h"
#include "file.h"
#include "log.h"
#include "rail.h"
#include "xcommon.h"
#include "chansrv_fuse.h"
#include "xrdp_sockets.h"

static struct trans *g_lis_trans = 0;
static struct trans *g_con_trans = 0;
static struct trans *g_api_lis_trans = 0;
static struct list *g_api_con_trans_list = 0; /* list of apps using api functions */
static struct chan_item g_chan_items[32];
static int g_num_chan_items = 0;
static int g_cliprdr_index = -1;
static int g_rdpsnd_index = -1;
static int g_rdpdr_index = -1;
static int g_rail_index = -1;

static tbus g_term_event = 0;
static tbus g_thread_done_event = 0;

static int g_use_unix_socket = 0;

int g_display_num = 0;
int g_cliprdr_chan_id = -1; /* cliprdr */
int g_rdpsnd_chan_id = -1;  /* rdpsnd  */
int g_rdpdr_chan_id = -1;   /* rdpdr   */
int g_rail_chan_id = -1;    /* rail    */
int g_restrict_outbound_clipboard = 0;

char *g_exec_name;
tbus g_exec_event;
tbus g_exec_mutex;
tbus g_exec_sem;
int g_exec_pid = 0;

#define ARRAYSIZE(x) (sizeof(x)/sizeof(*(x)))
/* max total channel bytes size */
#define MAX_CHANNEL_BYTES (1 * 1024 * 1024 * 1024) /* 1 GB */
#define MAX_CHANNEL_FRAG_BYTES 1600

#define CHANSRV_DRDYNVC_STATUS_CLOSED       0
#define CHANSRV_DRDYNVC_STATUS_OPEN_SENT    1
#define CHANSRV_DRDYNVC_STATUS_OPEN         2
#define CHANSRV_DRDYNVC_STATUS_CLOSE_SENT   3

struct chansrv_drdynvc
{
    int chan_id;
    int status; /* see CHANSRV_DRDYNVC_STATUS_* */
    int flags;
    int pad0;
    int (*open_response)(int chan_id, int creation_status);
    int (*close_response)(int chan_id);
    int (*data_first)(int chan_id, char *data, int bytes, int total_bytes);
    int (*data)(int chan_id, char *data, int bytes);
    struct trans *xrdp_api_trans;
};

static struct chansrv_drdynvc g_drdynvcs[256];

/* data in struct trans::callback_data */
struct xrdp_api_data
{
    int chan_flags;
    int chan_id;
};

struct timeout_obj
{
    tui32 mstime;
    void *data;
    void (*callback)(void *data);
    struct timeout_obj *next;
};

static struct timeout_obj *g_timeout_head = 0;
static struct timeout_obj *g_timeout_tail = 0;

/*****************************************************************************/
int
add_timeout(int msoffset, void (*callback)(void *data), void *data)
{
    struct timeout_obj *tobj;
    tui32 now;

    LOG(10, ("add_timeout:"));
    now = g_time3();
    tobj = g_new0(struct timeout_obj, 1);
    tobj->mstime = now + msoffset;
    tobj->callback = callback;
    tobj->data = data;
    if (g_timeout_tail == 0)
    {
        g_timeout_head = tobj;
        g_timeout_tail = tobj;
    }
    else
    {
        g_timeout_tail->next = tobj;
        g_timeout_tail = tobj;
    }
    return 0;
}

/*****************************************************************************/
static int
get_timeout(int *timeout)
{
    struct timeout_obj *tobj;
    tui32 now;
    int ltimeout;

    LOG(10, ("get_timeout:"));
    ltimeout = *timeout;
    if (ltimeout < 1)
    {
        ltimeout = 0;
    }
    tobj = g_timeout_head;
    if (tobj != 0)
    {
        now = g_time3();
        while (tobj != 0)
        {
            LOG(10, ("  now %u tobj->mstime %u", now, tobj->mstime));
            if (now < tobj->mstime)
            {
                ltimeout = tobj->mstime - now;
            }
            tobj = tobj->next;
        }
    }
    if (ltimeout > 0)
    {
        LOG(10, ("  ltimeout %d", ltimeout));
        if (*timeout < 1)
        {
            *timeout = ltimeout;
        }
        else
        {
            if (*timeout > ltimeout)
            {
                *timeout = ltimeout;
            }
        }
    }
    return 0;
}

/*****************************************************************************/
static int
check_timeout(void)
{
    struct timeout_obj *tobj;
    struct timeout_obj *last_tobj;
    struct timeout_obj *temp_tobj;
    int count;
    tui32 now;

    LOG(10, ("check_timeout:"));
    count = 0;
    tobj = g_timeout_head;
    if (tobj != 0)
    {
        last_tobj = 0;
        while (tobj != 0)
        {
            count++;
            now = g_time3();
            if (now >= tobj->mstime)
            {
                tobj->callback(tobj->data);
                if (last_tobj == 0)
                {
                    g_timeout_head = tobj->next;
                    if (g_timeout_head == 0)
                    {
                        g_timeout_tail = 0;
                    }
                }
                else
                {
                    last_tobj->next = tobj->next;
                    if (g_timeout_tail == tobj)
                    {
                        g_timeout_tail = last_tobj;
                    }
                }
                temp_tobj = tobj;
                tobj = tobj->next;
                g_free(temp_tobj);
            }
            else
            {
                last_tobj = tobj;
                tobj = tobj->next;
            }
        }
    }
    LOG(10, ("  count %d", count));
    return 0;
}

/*****************************************************************************/
int
g_is_term(void)
{
    return g_is_wait_obj_set(g_term_event);
}

/*****************************************************************************/
/* send data to a static virtual channel
   size can be > MAX_CHANNEL_FRAG_BYTES, in which case, > 1 message
   will be sent*/
/* returns error */
int
send_channel_data(int chan_id, const char *data, int size)
{
    int sending_bytes;
    int chan_flags;
    int error;
    int total_size;
    struct stream *s;

    if ((chan_id < 0) || (chan_id > 31) ||
        (data == NULL) ||
        (size < 1) || (size > MAX_CHANNEL_BYTES))
    {
        /* bad param */
        return 1;
    }
    total_size = size;
    chan_flags = 1; /* first */
    while (size > 0)
    {
        sending_bytes = MIN(MAX_CHANNEL_FRAG_BYTES, size);
        if (sending_bytes >= size)
        {
            chan_flags |= 2; /* last */
        }
        s = trans_get_out_s(g_con_trans, 26 + sending_bytes);
        if (s == NULL)
        {
            return 2;
        }
        out_uint32_le(s, 0); /* version */
        out_uint32_le(s, 26 + sending_bytes);
        out_uint32_le(s, 8); /* msg id */
        out_uint32_le(s, 18 + sending_bytes);
        out_uint16_le(s, chan_id);
        out_uint16_le(s, chan_flags);
        out_uint16_le(s, sending_bytes);
        out_uint32_le(s, total_size);
        out_uint8a(s, data, sending_bytes);
        s_mark_end(s);
        size -= sending_bytes;
        data += sending_bytes;
        error = trans_write_copy(g_con_trans);
        if (error != 0)
        {
            return 3;
        }
        chan_flags = 0;
    }
    return 0;
}

/*****************************************************************************/
/* returns error */
int
send_rail_drawing_orders(char* data, int size)
{
    LOGM((LOG_LEVEL_DEBUG, "chansrv::send_rail_drawing_orders: size %d", size));

    struct stream* s;
    int error;

    s = trans_get_out_s(g_con_trans, 8192);
    out_uint32_le(s, 0); /* version */
    out_uint32_le(s, 8 + 8 + size); /* size */
    out_uint32_le(s, 10); /* msg id */
    out_uint32_le(s, 8 + size); /* size */
    out_uint8a(s, data, size);
    s_mark_end(s);
    error = trans_force_write(g_con_trans);
    if (error != 0)
    {
        return 1;
    }
    return 0;
}

/*****************************************************************************/
/* returns error */
static int
process_message_channel_setup(struct stream *s)
{
    int num_chans;
    int index;
    int rv;
    struct chan_item *ci;

    g_num_chan_items = 0;
    g_cliprdr_index = -1;
    g_rdpsnd_index = -1;
    g_rdpdr_index = -1;
    g_rail_index = -1;
    g_cliprdr_chan_id = -1;
    g_rdpsnd_chan_id = -1;
    g_rdpdr_chan_id = -1;
    g_rail_chan_id = -1;
    LOGM((LOG_LEVEL_DEBUG, "process_message_channel_setup:"));
    in_uint16_le(s, num_chans);
    LOGM((LOG_LEVEL_DEBUG, "process_message_channel_setup: num_chans %d",
          num_chans));

    for (index = 0; index < num_chans; index++)
    {
        ci = &(g_chan_items[g_num_chan_items]);
        g_memset(ci->name, 0, sizeof(ci->name));
        in_uint8a(s, ci->name, 8);
        in_uint16_le(s, ci->id);
        in_uint16_le(s, ci->flags);
        LOGM((LOG_LEVEL_DEBUG, "process_message_channel_setup: chan name '%s' "
              "id %d flags %8.8x", ci->name, ci->id, ci->flags));

        g_writeln("process_message_channel_setup: chan name '%s' "
                  "id %d flags %8.8x", ci->name, ci->id, ci->flags);

        if (g_strcasecmp(ci->name, "cliprdr") == 0)
        {
            g_cliprdr_index = g_num_chan_items;
            g_cliprdr_chan_id = ci->id;
        }
        else if (g_strcasecmp(ci->name, "rdpsnd") == 0)
        {
            g_rdpsnd_index = g_num_chan_items;
            g_rdpsnd_chan_id = ci->id;
        }
        else if (g_strcasecmp(ci->name, "rdpdr") == 0)
        {
            g_rdpdr_index = g_num_chan_items;
            g_rdpdr_chan_id = ci->id;
        }
        /* disabled for now */
        else if (g_strcasecmp(ci->name, "rail") == 0)
        {
            g_rail_index = g_num_chan_items;
            g_rail_chan_id = ci->id;
        }
        else
        {
            LOG(10, ("other %s", ci->name));
        }

        g_num_chan_items++;
    }

    rv = 0;

    if (g_cliprdr_index >= 0)
    {
        clipboard_init();
        xfuse_init();
    }

    if (g_rdpsnd_index >= 0)
    {
        sound_init();
    }

    if (g_rdpdr_index >= 0)
    {
        dev_redir_init();
        xfuse_init();
    }

    if (g_rail_index >= 0)
    {
        rail_init();
    }

    return rv;
}

/*****************************************************************************/
/* returns error */
static int
process_message_channel_data(struct stream *s)
{
    int chan_id;
    int chan_flags;
    int rv;
    int length;
    int total_length;
    int index;
    int found;
    struct stream *ls;
    struct trans *ltran;
    struct xrdp_api_data *api_data;

    in_uint16_le(s, chan_id);
    in_uint16_le(s, chan_flags);
    in_uint16_le(s, length);
    in_uint32_le(s, total_length);
    LOGM((LOG_LEVEL_DEBUG, "process_message_channel_data: chan_id %d "
          "chan_flags %d", chan_id, chan_flags));
    LOG(10, ("process_message_channel_data"));
    rv = 0;

    if (rv == 0)
    {
        if (chan_id == g_cliprdr_chan_id)
        {
            rv = clipboard_data_in(s, chan_id, chan_flags, length, total_length);
        }
        else if (chan_id == g_rdpsnd_chan_id)
        {
            rv = sound_data_in(s, chan_id, chan_flags, length, total_length);
        }
        else if (chan_id == g_rdpdr_chan_id)
        {
            rv = dev_redir_data_in(s, chan_id, chan_flags, length, total_length);
        }
        else if (chan_id == g_rail_chan_id)
        {
            rv = rail_data_in(s, chan_id, chan_flags, length, total_length);
        }
        else
        {
            found = 0;
            for (index = 0; index < g_api_con_trans_list->count; index++)
            {
                ltran = (struct trans *) list_get_item(g_api_con_trans_list, index);
                if (ltran != NULL)
                {
                    api_data = (struct xrdp_api_data *) (ltran->callback_data);
                    if (api_data != NULL)
                    {
                        if (api_data->chan_id == chan_id)
                        {
                            found = 1;
                            ls = ltran->out_s;
                            if (chan_flags & 1) /* first */
                            {
                                init_stream(ls, total_length);
                            }
                            out_uint8a(ls, s->p, length);
                            if (chan_flags & 2) /* last */
                            {
                                s_mark_end(ls);
                                rv = trans_write_copy(ltran);
                            }
                            break;
                        }
                    }
                }
            }
            if (found == 0)
            {
                LOG(0, ("process_message_channel_data: not found channel %d", chan_id));
            }
        }

    }
    return rv;
}

/*****************************************************************************/
/* returns error */
/* open response from client */
static int
process_message_drdynvc_open_response(struct stream *s)
{
    struct chansrv_drdynvc *drdynvc;
    int chan_id;
    int creation_status;

    LOG(10, ("process_message_drdynvc_open_response:"));
    if (!s_check_rem(s, 8))
    {
        return 1;
    }
    in_uint32_le(s, chan_id);
    in_uint32_le(s, creation_status);
    if ((chan_id < 0) || (chan_id > 255))
    {
        return 1;
    }
    drdynvc = g_drdynvcs + chan_id;
    if (drdynvc->status != CHANSRV_DRDYNVC_STATUS_OPEN_SENT)
    {
        g_writeln("process_message_drdynvc_open_response: status not right");
        return 1;
    }
    if (creation_status == 0)
    {
        drdynvc->status = CHANSRV_DRDYNVC_STATUS_OPEN;
    }
    else
    {
        drdynvc->status = CHANSRV_DRDYNVC_STATUS_CLOSED;
    }
    if (drdynvc->open_response != NULL)
    {
        if (drdynvc->open_response(chan_id, creation_status) != 0)
        {
            return 1;
        }
    }
    return 0;
}

/*****************************************************************************/
/* returns error */
/* close response from client */
static int
process_message_drdynvc_close_response(struct stream *s)
{
    struct chansrv_drdynvc *drdynvc;
    int chan_id;

    LOG(10, ("process_message_drdynvc_close_response:"));
    if (!s_check_rem(s, 4))
    {
        return 1;
    }
    in_uint32_le(s, chan_id);
    if ((chan_id < 0) || (chan_id > 255))
    {
        return 1;
    }
    drdynvc = g_drdynvcs + chan_id;
    if (drdynvc->status != CHANSRV_DRDYNVC_STATUS_CLOSE_SENT)
    {
        g_writeln("process_message_drdynvc_close_response: status not right");
        return 0;
    }
    drdynvc->status = CHANSRV_DRDYNVC_STATUS_CLOSED;
    if (drdynvc->close_response != NULL)
    {
        if (drdynvc->close_response(chan_id) != 0)
        {
            return 1;
        }
    }
    return 0;
}

/*****************************************************************************/
/* returns error */
/* data from client */
static int
process_message_drdynvc_data_first(struct stream *s)
{
    struct chansrv_drdynvc *drdynvc;
    int chan_id;
    int bytes;
    int total_bytes;
    char *data;

    LOG(10, ("process_message_drdynvc_data_first:"));
    if (!s_check_rem(s, 12))
    {
        return 1;
    }
    in_uint32_le(s, chan_id);
    in_uint32_le(s, bytes);
    in_uint32_le(s, total_bytes);
    if (!s_check_rem(s, bytes))
    {
        return 1;
    }
    in_uint8p(s, data, bytes);
    if ((chan_id < 0) || (chan_id > 255))
    {
        return 1;
    }
    drdynvc = g_drdynvcs + chan_id;
    if (drdynvc->data_first != NULL)
    {
        if (drdynvc->data_first(chan_id, data, bytes, total_bytes) != 0)
        {
            return 1;
        }
    }
    return 0;
}

/*****************************************************************************/
/* returns error */
/* data from client */
static int
process_message_drdynvc_data(struct stream *s)
{
    struct chansrv_drdynvc *drdynvc;
    int chan_id;
    int bytes;
    char *data;

    LOG(10, ("process_message_drdynvc_data:"));
    if (!s_check_rem(s, 8))
    {
        return 1;
    }
    in_uint32_le(s, chan_id);
    in_uint32_le(s, bytes);
    if (!s_check_rem(s, bytes))
    {
        return 1;
    }
    in_uint8p(s, data, bytes);
    drdynvc = g_drdynvcs + chan_id;
    if (drdynvc->data != NULL)
    {
        if (drdynvc->data(chan_id, data, bytes) != 0)
        {
            return 1;
        }
    }
    return 0;
}

/*****************************************************************************/
/* open call from chansrv */
int
chansrv_drdynvc_open(const char *name, int flags,
                     struct chansrv_drdynvc_procs *procs, int *chan_id)
{
    struct stream *s;
    int name_bytes;
    int lchan_id;
    int error;

    lchan_id = 1;
    while (g_drdynvcs[lchan_id].status != CHANSRV_DRDYNVC_STATUS_CLOSED)
    {
        lchan_id++;
        if (lchan_id > 255)
        {
            return 1;
        }
    }
    s = trans_get_out_s(g_con_trans, 8192);
    if (s == NULL)
    {
        return 1;
    }
    name_bytes = g_strlen(name);
    out_uint32_le(s, 0); /* version */
    out_uint32_le(s, 8 + 8 + 4 + name_bytes + 4 + 4);
    out_uint32_le(s, 12); /* msg id */
    out_uint32_le(s, 8 + 4 + name_bytes + 4 + 4);
    out_uint32_le(s, name_bytes);
    out_uint8a(s, name, name_bytes);
    out_uint32_le(s, flags);
    out_uint32_le(s, lchan_id);
    s_mark_end(s);
    error = trans_write_copy(g_con_trans);
    if (error == 0)
    {
        if (chan_id != NULL)
        {
            *chan_id = lchan_id;
            g_drdynvcs[lchan_id].open_response = procs->open_response;
            g_drdynvcs[lchan_id].close_response = procs->close_response;
            g_drdynvcs[lchan_id].data_first = procs->data_first;
            g_drdynvcs[lchan_id].data = procs->data;
            g_drdynvcs[lchan_id].status = CHANSRV_DRDYNVC_STATUS_OPEN_SENT;

        }
    }
    return error;
}

/*****************************************************************************/
/* close call from chansrv */
int
chansrv_drdynvc_close(int chan_id)
{
    struct stream *s;
    int error;

    s = trans_get_out_s(g_con_trans, 8192);
    if (s == NULL)
    {
        return 1;
    }
    out_uint32_le(s, 0); /* version */
    out_uint32_le(s, 20);
    out_uint32_le(s, 14); /* msg id */
    out_uint32_le(s, 12);
    out_uint32_le(s, chan_id);
    s_mark_end(s);
    error = trans_write_copy(g_con_trans);
    g_drdynvcs[chan_id].status = CHANSRV_DRDYNVC_STATUS_CLOSE_SENT;
    return error;
}

/*****************************************************************************/
int
chansrv_drdynvc_data_first(int chan_id, const char *data, int data_bytes,
                           int total_data_bytes)
{
    struct stream *s;
    int error;

    //g_writeln("chansrv_drdynvc_data_first: data_bytes %d total_data_bytes %d",
    //          data_bytes, total_data_bytes);
    s = trans_get_out_s(g_con_trans, 8192);
    if (s == NULL)
    {
        return 1;
    }
    out_uint32_le(s, 0); /* version */
    out_uint32_le(s, 28 + data_bytes);
    out_uint32_le(s, 16); /* msg id */
    out_uint32_le(s, 20 + data_bytes);
    out_uint32_le(s, chan_id);
    out_uint32_le(s, data_bytes);
    out_uint32_le(s, total_data_bytes);
    out_uint8a(s, data, data_bytes);
    s_mark_end(s);
    error = trans_write_copy(g_con_trans);
    return error;
}

/*****************************************************************************/
int
chansrv_drdynvc_data(int chan_id, const char *data, int data_bytes)
{
    struct stream *s;
    int error;

    //g_writeln("chansrv_drdynvc_data: data_bytes %d", data_bytes);
    s = trans_get_out_s(g_con_trans, 8192);
    if (s == NULL)
    {
        return 1;
    }
    out_uint32_le(s, 0); /* version */
    out_uint32_le(s, 24 + data_bytes);
    out_uint32_le(s, 18); /* msg id */
    out_uint32_le(s, 16 + data_bytes);
    out_uint32_le(s, chan_id);
    out_uint32_le(s, data_bytes);
    out_uint8a(s, data, data_bytes);
    s_mark_end(s);
    error = trans_write_copy(g_con_trans);
    return error;
}

/*****************************************************************************/
int
chansrv_drdynvc_send_data(int chan_id, const char *data, int data_bytes)
{
    int this_send_bytes;

    //g_writeln("chansrv_drdynvc_send_data: data_bytes %d", data_bytes);
    if (data_bytes > 1590)
    {
        if (chansrv_drdynvc_data_first(chan_id, data, 1590, data_bytes) != 0)
        {
            return 1;
        }
        data_bytes -= 1590;
        data += 1590;
    }
    while (data_bytes > 0)
    {
        this_send_bytes = MIN(1590, data_bytes);
        if (chansrv_drdynvc_data(chan_id, data, this_send_bytes) != 0)
        {
            return 1;
        }
        data_bytes -= this_send_bytes;
        data += this_send_bytes;
    }
    return 0;
}

/*****************************************************************************/
/* returns error */
static int
process_message(void)
{
    struct stream *s = (struct stream *)NULL;
    int size = 0;
    int id = 0;
    int rv = 0;
    char *next_msg = (char *)NULL;

    if (g_con_trans == 0)
    {
        return 1;
    }

    s = trans_get_in_s(g_con_trans);

    if (s == 0)
    {
        return 1;
    }

    rv = 0;

    while (s_check_rem(s, 8))
    {
        next_msg = s->p;
        in_uint32_le(s, id);
        in_uint32_le(s, size);
        next_msg += size;

        switch (id)
        {
            case 3: /* channel setup */
                rv = process_message_channel_setup(s);
                break;
            case 5: /* channel data */
                rv = process_message_channel_data(s);
                break;
            case 13: /* drdynvc open response */
                rv = process_message_drdynvc_open_response(s);
                break;
            case 15: /* drdynvc close response */
                rv = process_message_drdynvc_close_response(s);
                break;
            case 17: /* drdynvc data first */
                rv = process_message_drdynvc_data_first(s);
                break;
            case 19: /* drdynvc data */
                rv = process_message_drdynvc_data(s);
                break;
            default:
                LOGM((LOG_LEVEL_ERROR, "process_message: unknown msg %d", id));
                break;
        }

        if (rv != 0)
        {
            g_writeln("process_message: error rv %d id %d", rv, id);
            rv = 0;
        }

        s->p = next_msg;
    }

    return rv;
}

/*****************************************************************************/
/* returns error */
int
my_trans_data_in(struct trans *trans)
{
    struct stream *s = (struct stream *)NULL;
    int size = 0;
    int error = 0;

    if (trans == 0)
    {
        return 0;
    }

    if (trans != g_con_trans)
    {
        return 1;
    }

    LOGM((LOG_LEVEL_DEBUG, "my_trans_data_in:"));
    s = trans_get_in_s(trans);
    in_uint8s(s, 4); /* id */
    in_uint32_le(s, size);
    error = trans_force_read(trans, size - 8);

    if (error == 0)
    {
        /* here, the entire message block is read in, process it */
        error = process_message();
        if (error == 0)
        {
        }
        else
        {
            g_writeln("my_trans_data_in: process_message failed");
        }
    }
    else
    {
        g_writeln("my_trans_data_in: trans_force_read failed");
    }

    return error;
}

/*****************************************************************************/
struct trans *
get_api_trans_from_chan_id(int chan_id)
{
    return g_drdynvcs[chan_id].xrdp_api_trans;
}

/*****************************************************************************/
static int
my_api_open_response(int chan_id, int creation_status)
{
    struct trans *trans;
    struct stream *s;

    //g_writeln("my_api_open_response: chan_id %d creation_status %d",
    //          chan_id, creation_status);
    trans = get_api_trans_from_chan_id(chan_id);
    if (trans == NULL)
    {
        return 1;
    }
    s = trans_get_out_s(trans, 8192);
    if (s == NULL)
    {
        return 1;
    }
    out_uint32_le(s, creation_status);
    s_mark_end(s);
    if (trans_write_copy(trans) != 0)
    {
        return 1;
    }
    return 0;
}

/*****************************************************************************/
static int
my_api_close_response(int chan_id)
{
    //g_writeln("my_api_close_response:");
    return 0;
}

/*****************************************************************************/
static int
my_api_data_first(int chan_id, char *data, int bytes, int total_bytes)
{
    struct trans *trans;
    struct stream *s;

    //g_writeln("my_api_data_first: bytes %d total_bytes %d", bytes, total_bytes);
    trans = get_api_trans_from_chan_id(chan_id);
    if (trans == NULL)
    {
        return 1;
    }
    s = trans_get_out_s(trans, bytes);
    if (s == NULL)
    {
        return 1;
    }
    out_uint8a(s, data, bytes);
    s_mark_end(s);
    if (trans_write_copy(trans) != 0)
    {
        return 1;
    }
    return 0;
}

/*****************************************************************************/
static int
my_api_data(int chan_id, char *data, int bytes)
{
    struct trans *trans;
    struct stream *s;

    //g_writeln("my_api_data: bytes %d", bytes);
    trans = get_api_trans_from_chan_id(chan_id);
    if (trans == NULL)
    {
        return 1;
    }
    s = trans_get_out_s(trans, bytes);
    if (s == NULL)
    {
        return 1;
    }
    out_uint8a(s, data, bytes);
    s_mark_end(s);
    if (trans_write_copy(trans) != 0)
    {
        return 1;
    }
    return 0;
}

/*
 * called when WTSVirtualChannelWrite() is invoked in xrdpapi.c
 *
 ******************************************************************************/
int
my_api_trans_data_in(struct trans *trans)
{
    struct stream *s;
    struct stream *out_s;
    struct xrdp_api_data *ad;
    int index;
    int rv;
    int bytes;
    int ver;
    int channel_name_bytes;
    struct chansrv_drdynvc_procs procs;
    char *chan_name;

    //g_writeln("my_api_trans_data_in: extra_flags %d", trans->extra_flags);
    rv = 0;
    ad = (struct xrdp_api_data *) (trans->callback_data);
    s = trans_get_in_s(trans);
    if (trans->extra_flags == 0)
    {
        in_uint32_le(s, bytes);
        in_uint32_le(s, ver);
        //g_writeln("my_api_trans_data_in: bytes %d ver %d", bytes, ver);
        if (ver != 0)
        {
            return 1;
        }
        trans->header_size = bytes;
        trans->extra_flags = 1;
    }
    else if (trans->extra_flags == 1)
    {
        rv = 1;
        in_uint32_le(s, channel_name_bytes);
        //g_writeln("my_api_trans_data_in: channel_name_bytes %d", channel_name_bytes);
        chan_name = g_new0(char, channel_name_bytes + 1);
        if (chan_name == NULL)
        {
            return 1;
        }
        in_uint8a(s, chan_name, channel_name_bytes);
        in_uint32_le(s, ad->chan_flags);
        //g_writeln("my_api_trans_data_in: chan_name %s chan_flags 0x%8.8x", chan_name, ad->chan_flags);
        if (ad->chan_flags == 0)
        {
            /* SVC */
            for (index = 0; index < g_num_chan_items; index++)
            {
                if (g_strcasecmp(g_chan_items[index].name, chan_name) == 0)
                {
                    ad->chan_id = g_chan_items[index].id;
                    rv = 0;
                    break;
                }
            }
            if (rv == 0)
            {
                /* open ok */
                out_s = trans_get_out_s(trans, 8192);
                if (out_s == NULL)
                {
                    return 1;
                }
                out_uint32_le(out_s, 0);
                s_mark_end(out_s);
                if (trans_write_copy(trans) != 0)
                {
                    return 1;
                }
            }
            else
            {
                /* open failed */
                out_s = trans_get_out_s(trans, 8192);
                if (out_s == NULL)
                {
                    return 1;
                }
                out_uint32_le(out_s, 1);
                s_mark_end(out_s);
                if (trans_write_copy(trans) != 0)
                {
                    return 1;
                }
            }
        }
        else
        {
            /* DVS */
            g_memset(&procs, 0, sizeof(procs));
            procs.open_response = my_api_open_response;
            procs.close_response = my_api_close_response;
            procs.data_first = my_api_data_first;
            procs.data = my_api_data;
            rv = chansrv_drdynvc_open(chan_name, ad->chan_flags,
                                      &procs, &(ad->chan_id));
            //g_writeln("my_api_trans_data_in: chansrv_drdynvc_open rv %d "
            //          "chan_id %d", rv, ad->chan_id);
            g_drdynvcs[ad->chan_id].xrdp_api_trans = trans;
        }
        g_free(chan_name);
        init_stream(s, 0);
        trans->extra_flags = 2;
        trans->header_size = 0;
    }
    else
    {
        bytes = g_sck_recv(trans->sck, s->data, s->size, 0);
        if (bytes < 1)
        {
            //g_writeln("my_api_trans_data_in: disconnect");
            return 1;
        }
        if (ad->chan_flags == 0)
        {
            /* SVC */
            rv = send_channel_data(ad->chan_id, s->data, bytes);
        }
        else
        {
            /* DVS */
            //g_writeln("my_api_trans_data_in: s->size %d bytes %d", s->size, bytes);
            rv = chansrv_drdynvc_send_data(ad->chan_id, s->data, bytes);
        }
        init_stream(s, 0);
    }
    return rv;
}

/*****************************************************************************/
int
my_trans_conn_in(struct trans *trans, struct trans *new_trans)
{
    if (trans == 0)
    {
        return 1;
    }

    if (trans != g_lis_trans)
    {
        return 1;
    }

    if (g_con_trans != 0) /* if already set, error */
    {
        return 1;
    }

    if (new_trans == 0)
    {
        return 1;
    }

    LOGM((LOG_LEVEL_DEBUG, "my_trans_conn_in:"));
    g_con_trans = new_trans;
    g_con_trans->trans_data_in = my_trans_data_in;
    g_con_trans->header_size = 8;
    /* stop listening */
    trans_delete(g_lis_trans);
    g_lis_trans = 0;
    return 0;
}

/*
 * called when WTSVirtualChannelOpenEx is invoked in xrdpapi.c
 *
 ******************************************************************************/
int
my_api_trans_conn_in(struct trans *trans, struct trans *new_trans)
{
    struct xrdp_api_data *ad;

    //g_writeln("my_api_trans_conn_in:");
    if ((trans == NULL) || (trans != g_api_lis_trans) || (new_trans == NULL))
    {
        g_writeln("my_api_trans_conn_in: error");
        return 1;
    }
    new_trans->trans_data_in = my_api_trans_data_in;
    new_trans->header_size = 8;
    new_trans->no_stream_init_on_data_in = 1;
    ad = g_new0(struct xrdp_api_data, 1);
    if (ad == NULL)
    {
        g_writeln("my_api_trans_conn_in: error");
        return 1;
    }
    new_trans->callback_data = ad;
    list_add_item(g_api_con_trans_list, (intptr_t) new_trans);
    return 0;
}

/*****************************************************************************/
static int
setup_listen(void)
{
    char port[256];
    int error = 0;

    if (g_lis_trans != 0)
    {
        trans_delete(g_lis_trans);
    }

    if (g_use_unix_socket)
    {
        g_lis_trans = trans_create(TRANS_MODE_UNIX, 8192, 8192);
        g_lis_trans->is_term = g_is_term;
        g_snprintf(port, 255, XRDP_CHANSRV_STR, g_display_num);
    }
    else
    {
        g_lis_trans = trans_create(TRANS_MODE_TCP, 8192, 8192);
        g_lis_trans->is_term = g_is_term;
        g_snprintf(port, 255, "%d", 7200 + g_display_num);
    }

    g_lis_trans->trans_conn_in = my_trans_conn_in;
    error = trans_listen(g_lis_trans, port);

    if (error != 0)
    {
        LOGM((LOG_LEVEL_ERROR, "setup_listen: trans_listen failed for port %s",
              port));
        return 1;
    }

    return 0;
}

/*****************************************************************************/
static int
setup_api_listen(void)
{
    char port[256];
    int error = 0;

    g_api_lis_trans = trans_create(TRANS_MODE_UNIX, 8192 * 4, 8192 * 4);
    g_api_lis_trans->is_term = g_is_term;
    g_snprintf(port, 255, CHANSRV_API_STR, g_display_num);
    g_api_lis_trans->trans_conn_in = my_api_trans_conn_in;
    error = trans_listen(g_api_lis_trans, port);

    if (error != 0)
    {
        LOGM((LOG_LEVEL_ERROR, "setup_api_listen: trans_listen failed for port %s",
              port));
        return 1;
    }

    return 0;
}

/*****************************************************************************/
static int
api_con_trans_list_get_wait_objs_rw(intptr_t *robjs, int *rcount,
                                    intptr_t *wobjs, int *wcount,
                                    int *timeout)
{
    int api_con_index;
    struct trans *ltran;

    for (api_con_index = g_api_con_trans_list->count - 1;
         api_con_index >= 0;
         api_con_index--)
    {
        ltran = (struct trans *)
            list_get_item(g_api_con_trans_list, api_con_index);
        if (ltran != NULL)
        {
            trans_get_wait_objs_rw(ltran, robjs, rcount, wobjs, wcount,
                                   timeout);
        }
    }
    return 0;
}

/*****************************************************************************/
static int
api_con_trans_list_check_wait_objs(void)
{
    int api_con_index;
    int drdynvc_index;
    struct trans *ltran;
    struct xrdp_api_data *ad;

    for (api_con_index = g_api_con_trans_list->count - 1;
         api_con_index >= 0;
         api_con_index--)
    {
        ltran = (struct trans *)
            list_get_item(g_api_con_trans_list, api_con_index);
        if (ltran != NULL)
        {
            if (trans_check_wait_objs(ltran) != 0)
            {
                /* disconnect */
                list_remove_item(g_api_con_trans_list, api_con_index);
                ad = (struct xrdp_api_data *) (ltran->callback_data);
                if (ad->chan_flags != 0)
                {
                    chansrv_drdynvc_close(ad->chan_id);
                }
                for (drdynvc_index = 0;
                     drdynvc_index < (int) ARRAYSIZE(g_drdynvcs);
                     drdynvc_index++)
                {
                    if (g_drdynvcs[drdynvc_index].xrdp_api_trans == ltran)
                    {
                        g_drdynvcs[drdynvc_index].xrdp_api_trans = NULL;
                    }
                }
                g_free(ad);
                trans_delete(ltran);
            }
        }
    }
    return 0;
}

/*****************************************************************************/
static int
api_con_trans_list_remove_all(void)
{
    int api_con_index;
    struct trans *ltran;

    for (api_con_index = g_api_con_trans_list->count - 1;
         api_con_index >= 0;
         api_con_index--)
    {
        ltran = (struct trans *)
            list_get_item(g_api_con_trans_list, api_con_index);
        if (ltran != NULL)
        {
            list_remove_item(g_api_con_trans_list, api_con_index);
            g_free(ltran->callback_data);
            trans_delete(ltran);
        }
    }
    return 0;
}

/*****************************************************************************/
THREAD_RV THREAD_CC
channel_thread_loop(void *in_val)
{
    tbus objs[32];
    tbus wobjs[32];
    int num_objs;
    int num_wobjs;
    int timeout;
    int error;
    THREAD_RV rv;

    LOGM((LOG_LEVEL_INFO, "channel_thread_loop: thread start"));
    rv = 0;
    g_api_con_trans_list = list_create();
    setup_api_listen();
    error = setup_listen();

    if (error == 0)
    {
        timeout = -1;
        num_objs = 0;
        num_wobjs = 0;
        objs[num_objs] = g_term_event;
        num_objs++;
        trans_get_wait_objs(g_lis_trans, objs, &num_objs);
        trans_get_wait_objs(g_api_lis_trans, objs, &num_objs);

        //g_writeln("timeout %d", timeout);
        while (g_obj_wait(objs, num_objs, wobjs, num_wobjs, timeout) == 0)
        {
            check_timeout();
            if (g_is_wait_obj_set(g_term_event))
            {
                LOGM((LOG_LEVEL_INFO, "channel_thread_loop: g_term_event set"));
                clipboard_deinit();
                sound_deinit();
                dev_redir_deinit();
                rail_deinit();
                break;
            }

            if (g_lis_trans != 0)
            {
                if (trans_check_wait_objs(g_lis_trans) != 0)
                {
                    LOGM((LOG_LEVEL_INFO, "channel_thread_loop: "
                          "trans_check_wait_objs error"));
                }
            }

            if (g_con_trans != 0)
            {
                if (trans_check_wait_objs(g_con_trans) != 0)
                {
                    LOGM((LOG_LEVEL_INFO, "channel_thread_loop: "
                          "trans_check_wait_objs error resetting"));
                    clipboard_deinit();
                    sound_deinit();
                    dev_redir_deinit();
                    rail_deinit();
                    /* delete g_con_trans */
                    trans_delete(g_con_trans);
                    g_con_trans = 0;
                    /* create new listener */
                    error = setup_listen();

                    if (error != 0)
                    {
                        break;
                    }
                }
            }

            if (g_api_lis_trans != 0)
            {
                if (trans_check_wait_objs(g_api_lis_trans) != 0)
                {
                    LOG(0, ("channel_thread_loop: trans_check_wait_objs failed"));
                }
            }
            /* check the wait_objs in g_api_con_trans_list */
            api_con_trans_list_check_wait_objs();
            xcommon_check_wait_objs();
            sound_check_wait_objs();
            dev_redir_check_wait_objs();
            xfuse_check_wait_objs();
            timeout = -1;
            num_objs = 0;
            num_wobjs = 0;
            objs[num_objs] = g_term_event;
            num_objs++;
            trans_get_wait_objs_rw(g_lis_trans, objs, &num_objs,
                                   wobjs, &num_wobjs, &timeout);
            trans_get_wait_objs_rw(g_con_trans, objs, &num_objs,
                                   wobjs, &num_wobjs, &timeout);
            trans_get_wait_objs_rw(g_api_lis_trans, objs, &num_objs,
                                   wobjs, &num_wobjs, &timeout);
            /* get the wait_objs from in g_api_con_trans_list */
            api_con_trans_list_get_wait_objs_rw(objs, &num_objs,
                                                wobjs, &num_wobjs,
                                                &timeout);
            xcommon_get_wait_objs(objs, &num_objs, &timeout);
            sound_get_wait_objs(objs, &num_objs, &timeout);
            dev_redir_get_wait_objs(objs, &num_objs, &timeout);
            xfuse_get_wait_objs(objs, &num_objs, &timeout);
            get_timeout(&timeout);
        } /* end while (g_obj_wait(objs, num_objs, 0, 0, timeout) == 0) */
    }

    trans_delete(g_lis_trans);
    g_lis_trans = 0;
    trans_delete(g_con_trans);
    g_con_trans = 0;
    trans_delete(g_api_lis_trans);
    g_api_lis_trans = 0;
    api_con_trans_list_remove_all();
    list_delete(g_api_con_trans_list);
    LOGM((LOG_LEVEL_INFO, "channel_thread_loop: thread stop"));
    g_set_wait_obj(g_thread_done_event);
    return rv;
}

/*****************************************************************************/
void
term_signal_handler(int sig)
{
    LOGM((LOG_LEVEL_INFO, "term_signal_handler: got signal %d", sig));
    g_set_wait_obj(g_term_event);
}

/*****************************************************************************/
void
nil_signal_handler(int sig)
{
    LOGM((LOG_LEVEL_INFO, "nil_signal_handler: got signal %d", sig));
}

/*****************************************************************************/
void
child_signal_handler(int sig)
{
    int pid;

    LOG(0, ("child_signal_handler:"));
    do
    {
        pid = g_waitchild();
        LOG(0, ("child_signal_handler: child pid %d", pid));
        if ((pid == g_exec_pid) && (pid > 0))
        {
            LOG(0, ("child_signal_handler: found pid %d", pid));
            //shutdownx();
        }
    }
    while (pid >= 0);
}

/*****************************************************************************/
void
segfault_signal_handler(int sig)
{
    LOG(0, ("segfault_signal_handler: entered......."));
    xfuse_deinit();
    exit(0);
}

/*****************************************************************************/
static void
x_server_fatal_handler(void)
{
    LOGM((LOG_LEVEL_INFO, "xserver_fatal_handler: entered......."));
    /* At this point the X server has gone away. Dont make any X calls. */
    xfuse_deinit();
    exit(0);
}

/*****************************************************************************/
static int
get_display_num_from_display(char *display_text)
{
    int index;
    int mode;
    int host_index;
    int disp_index;
    int scre_index;
    char host[256];
    char disp[256];
    char scre[256];

    g_memset(host, 0, 256);
    g_memset(disp, 0, 256);
    g_memset(scre, 0, 256);

    index = 0;
    host_index = 0;
    disp_index = 0;
    scre_index = 0;
    mode = 0;

    while (display_text[index] != 0)
    {
        if (display_text[index] == ':')
        {
            mode = 1;
        }
        else if (display_text[index] == '.')
        {
            mode = 2;
        }
        else if (mode == 0)
        {
            host[host_index] = display_text[index];
            host_index++;
        }
        else if (mode == 1)
        {
            disp[disp_index] = display_text[index];
            disp_index++;
        }
        else if (mode == 2)
        {
            scre[scre_index] = display_text[index];
            scre_index++;
        }

        index++;
    }

    host[host_index] = 0;
    disp[disp_index] = 0;
    scre[scre_index] = 0;
    g_display_num = g_atoi(disp);
    return 0;
}

/*****************************************************************************/
int
main_cleanup(void)
{
    g_delete_wait_obj(g_term_event);
    g_delete_wait_obj(g_thread_done_event);
    g_delete_wait_obj(g_exec_event);
    tc_mutex_delete(g_exec_mutex);
    g_deinit(); /* os_calls */
    return 0;
}

/*****************************************************************************/
static int
read_ini(void)
{
    char filename[256];
    struct list *names;
    struct list *values;
    char *name;
    char *value;
    int index;

    g_memset(filename, 0, (sizeof(char) * 256));
    names = list_create();
    names->auto_free = 1;
    values = list_create();
    values->auto_free = 1;
    g_use_unix_socket = 0;
    g_snprintf(filename, 255, "%s/sesman.ini", XRDP_CFG_PATH);

    if (file_by_name_read_section(filename, "Globals", names, values) == 0)
    {
        for (index = 0; index < names->count; index++)
        {
            name = (char *)list_get_item(names, index);
            value = (char *)list_get_item(values, index);

            if (g_strcasecmp(name, "ListenAddress") == 0)
            {
                if (g_strcasecmp(value, "127.0.0.1") == 0)
                {
                    g_use_unix_socket = 1;
                }
            }
        }
    }

    list_delete(names);
    list_delete(values);
    return 0;
}

/*****************************************************************************/
static int
get_log_path(char *path, int bytes)
{
    char* log_path;
    int rv;

    rv = 1;
    log_path = g_getenv("CHANSRV_LOG_PATH");
    if (log_path == 0)
    {
        log_path = g_getenv("XDG_DATA_HOME");
        if (log_path != 0)
        {
            g_snprintf(path, bytes, "%s%s", log_path, "/xrdp");
            if (g_directory_exist(path) || (g_mkdir(path) == 0))
            {
                rv = 0;
            }
        }
    }
    else
    {
        g_snprintf(path, bytes, "%s", log_path);
        if (g_directory_exist(path) || (g_mkdir(path) == 0))
        {
            rv = 0;
        }
    }
    if (rv != 0)
    {
        log_path = g_getenv("HOME");
        if (log_path != 0)
        {
            g_snprintf(path, bytes, "%s%s", log_path, "/.local");
            if (g_directory_exist(path) || (g_mkdir(path) == 0))
            {
                g_snprintf(path, bytes, "%s%s", log_path, "/.local/share");
                if (g_directory_exist(path) || (g_mkdir(path) == 0))
                {
                    g_snprintf(path, bytes, "%s%s", log_path, "/.local/share/xrdp");
                    if (g_directory_exist(path) || (g_mkdir(path) == 0))
                    {
                        rv = 0;
                    }
                }
            }
        }
    }
    return rv;
}

/*****************************************************************************/
static enum logLevels
get_log_level(const char* level_str, enum logLevels default_level)
{
    static const char* levels[] = {
        "LOG_LEVEL_ALWAYS",
        "LOG_LEVEL_ERROR",
        "LOG_LEVEL_WARNING",
        "LOG_LEVEL_INFO",
        "LOG_LEVEL_DEBUG",
        "LOG_LEVEL_TRACE"
    };
    unsigned int i;

    if (level_str == NULL || level_str[0] == 0)
    {
        return default_level;
    }
    for (i = 0; i < ARRAYSIZE(levels); ++i)
    {
        if (g_strcasecmp(levels[i], level_str) == 0)
        {
            return (enum logLevels) i;
        }
    }
    return default_level;
}

/*****************************************************************************/
static int
run_exec(void)
{
    int pid;

    LOG(10, ("run_exec:"));
    pid = g_fork();

    if (pid == 0)
    {
        trans_delete(g_con_trans);
        g_close_wait_obj(g_term_event);
        g_close_wait_obj(g_thread_done_event);
        g_close_wait_obj(g_exec_event);
        tc_mutex_delete(g_exec_mutex);
        tc_sem_delete(g_exec_sem);
        g_execlp3(g_exec_name, g_exec_name, 0);
        g_exit(0);
    }

    g_exec_pid = pid;
    tc_sem_inc(g_exec_sem);

    return 0;
}

/*****************************************************************************/
int
main(int argc, char **argv)
{
    tbus waiters[4];
    int pid = 0;
    char text[256];
    char log_path[256];
    char *display_text;
    char log_file[256];
    enum logReturns error;
    struct log_config logconfig;
    enum logLevels log_level;
    char *restrict_outbound_clipboard_env;
    g_init("xrdp-chansrv"); /* os_calls */

    log_path[255] = 0;
    if (get_log_path(log_path, 255) != 0)
    {
        g_writeln("error reading CHANSRV_LOG_PATH and HOME environment variable");
        g_deinit();
        return 1;
    }

    restrict_outbound_clipboard_env = g_getenv("CHANSRV_RESTRICT_OUTBOUND_CLIPBOARD");
    if (restrict_outbound_clipboard_env != 0) {
        if (g_strcmp(restrict_outbound_clipboard_env, "1") == 0) {
            g_restrict_outbound_clipboard = 1;
        }
    }

    read_ini();
    pid = g_getpid();
    display_text = g_getenv("DISPLAY");

    if (display_text)
        get_display_num_from_display(display_text);

    log_level = get_log_level(g_getenv("CHANSRV_LOG_LEVEL"), LOG_LEVEL_INFO);

    /* starting logging subsystem */
    g_memset(&logconfig, 0, sizeof(struct log_config));
    logconfig.program_name = "xrdp-chansrv";
    g_snprintf(log_file, 255, "%s/xrdp-chansrv.%d.log", log_path, g_display_num);
    g_writeln("chansrv::main: using log file [%s]", log_file);

    if (g_file_exist(log_file))
    {
        g_file_delete(log_file);
    }

    g_memset(g_drdynvcs, 0, sizeof(g_drdynvcs));

    logconfig.log_file = log_file;
    logconfig.fd = -1;
    logconfig.log_level = log_level;
    logconfig.enable_syslog = 0;
    logconfig.syslog_level = LOG_LEVEL_ALWAYS;
    error = log_start_from_param(&logconfig);

    if (error != LOG_STARTUP_OK)
    {
        switch (error)
        {
            case LOG_ERROR_MALLOC:
                g_writeln("error on malloc. cannot start logging. quitting.");
                break;
            case LOG_ERROR_FILE_OPEN:
                g_writeln("error opening log file [%s]. quitting.",
                          getLogFile(text, 255));
                break;
            default:
                g_writeln("log_start error");
                break;
        }

        g_deinit();
        return 1;
    }

    LOGM((LOG_LEVEL_ALWAYS, "main: app started pid %d(0x%8.8x)", pid, pid));
    /*  set up signal handler  */
    g_signal_terminate(term_signal_handler); /* SIGTERM */
    g_signal_user_interrupt(term_signal_handler); /* SIGINT */
    g_signal_pipe(nil_signal_handler); /* SIGPIPE */
    g_signal_child_stop(child_signal_handler); /* SIGCHLD */
    g_signal_segfault(segfault_signal_handler);

    /* Cater for the X server exiting unexpectedly */
    xcommon_set_x_server_fatal_handler(x_server_fatal_handler);

    LOGM((LOG_LEVEL_INFO, "main: DISPLAY env var set to %s", display_text));

    if (g_display_num == 0)
    {
        LOGM((LOG_LEVEL_ERROR, "main: error, display is zero"));
        g_deinit();
        return 1;
    }

    LOGM((LOG_LEVEL_INFO, "main: using DISPLAY %d", g_display_num));
    g_snprintf(text, 255, "xrdp_chansrv_%8.8x_main_term", pid);
    g_term_event = g_create_wait_obj(text);
    g_snprintf(text, 255, "xrdp_chansrv_%8.8x_thread_done", pid);
    g_thread_done_event = g_create_wait_obj(text);
    g_snprintf(text, 255, "xrdp_chansrv_%8.8x_exec", pid);
    g_exec_event = g_create_wait_obj(text);
    g_exec_mutex = tc_mutex_create();
    g_exec_sem = tc_sem_create(0);
    tc_thread_create(channel_thread_loop, 0);

    while (g_term_event > 0 && !g_is_wait_obj_set(g_term_event))
    {
        waiters[0] = g_term_event;
        waiters[1] = g_exec_event;

        if (g_obj_wait(waiters, 2, 0, 0, 0) != 0)
        {
            LOGM((LOG_LEVEL_ERROR, "main: error, g_obj_wait failed"));
            break;
        }

        if (g_is_wait_obj_set(g_term_event))
        {
            break;
        }

        if (g_is_wait_obj_set(g_exec_event))
        {
            g_reset_wait_obj(g_exec_event);
            run_exec();
        }
    }

    while (g_thread_done_event > 0 && !g_is_wait_obj_set(g_thread_done_event))
    {
        /* wait for thread to exit */
        if (g_obj_wait(&g_thread_done_event, 1, 0, 0, 0) != 0)
        {
            LOGM((LOG_LEVEL_ERROR, "main: error, g_obj_wait failed"));
            break;
        }
    }

    /* cleanup */
    main_cleanup();
    LOGM((LOG_LEVEL_INFO, "main: app exiting pid %d(0x%8.8x)", pid, pid));
    g_deinit();
    return 0;
}

