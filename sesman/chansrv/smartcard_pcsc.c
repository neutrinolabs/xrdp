/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2013 jay.sorg@gmail.com
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
 */

/*
 * smartcard redirection support, PCSC daemon standin
 * this will act like pcsc daemon
 * pcsc lib and daemon write struct on unix domain socket for communication
 */

#define JAY_TODO_CONTEXT    0
#define JAY_TODO_WIDE       1

#define PCSC_STANDIN 1

#include "os_calls.h"
#include "smartcard.h"
#include "log.h"
#include "irp.h"
#include "devredir.h"
#include "trans.h"
#include "chansrv.h"

#if PCSC_STANDIN

#define LLOG_LEVEL 11
#define LLOGLN(_level, _args) \
  do \
  { \
    if (_level < LLOG_LEVEL) \
    { \
      g_write("chansrv:smartcard_pcsc [%10.10u]: ", g_time3()); \
      g_writeln _args ; \
    } \
  } \
  while (0)

#define XRDP_PCSC_STATE_NONE              0
#define XRDP_PCSC_STATE_GOT_EC           (1 << 0) /* establish context */
#define XRDP_PCSC_STATE_GOT_LR           (1 << 1) /* list readers */
#define XRDP_PCSC_STATE_GOT_RC           (1 << 2) /* release context */
#define XRDP_PCSC_STATE_GOT_GSC          (1 << 3) /* get status change */
#define XRDP_PCSC_STATE_GOT_C            (1 << 4) /* connect */

/* TODO: put this in con */
static int g_xrdp_pcsc_state = XRDP_PCSC_STATE_NONE;

extern int g_display_num; /* in chansrv.c */

/*****************************************************************************/
struct pcsc_client
{
    struct trans *con;
};

static struct pcsc_client *g_head = 0;
static struct pcsc_client *g_tail = 0;

static struct trans *g_lis = 0;
static struct trans *g_con = 0; /* todo, remove this */
static char g_pcsclite_ipc_dir[256] = "";
static int g_pub_file_fd = 0;

/*****************************************************************************/
int APP_CC
scard_pcsc_get_wait_objs(tbus *objs, int *count, int *timeout)
{
    LLOGLN(0, ("scard_pcsc_get_wait_objs"));
    if (g_lis != 0)
    {
        trans_get_wait_objs(g_lis, objs, count);
    }
    if (g_con != 0)
    {
        trans_get_wait_objs(g_con, objs, count);
    }
    return 0;
}

/*****************************************************************************/
int APP_CC
scard_pcsc_check_wait_objs(void)
{
    LLOGLN(0, ("scard_pcsc_check_wait_objs"));
    if (g_lis != 0)
    {
        trans_check_wait_objs(g_lis);
    }
    if (g_con != 0)
    {
        trans_check_wait_objs(g_con);
    }
    return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
scard_process_establish_context(struct trans *con, struct stream *in_s)
{
    int dwScope;

    LLOGLN(0, ("scard_process_establish_context:"));
    if (g_xrdp_pcsc_state & XRDP_PCSC_STATE_GOT_EC)
    {
        LLOGLN(0, ("scard_process_establish_context: opps"));
        return 1;
    }
    g_xrdp_pcsc_state |= XRDP_PCSC_STATE_GOT_EC;
    in_uint32_le(in_s, dwScope);
    LLOGLN(0, ("scard_process_establish_context: dwScope 0x%8.8x", dwScope));
    scard_send_establish_context(con, dwScope);
    return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
scard_function_establish_context_return(struct trans *con,
                                      struct stream *in_s,
                                      int len)
{
    int bytes;
    tui32 context;
    tui32 context_len;
    struct stream *out_s;

    LLOGLN(0, ("scard_function_establish_context_return:"));
    if ((g_xrdp_pcsc_state & XRDP_PCSC_STATE_GOT_EC) == 0)
    {
        LLOGLN(0, ("scard_function_establish_context_return: opps"));
        return 1;
    }
    g_xrdp_pcsc_state &= ~XRDP_PCSC_STATE_GOT_EC;
    in_uint8s(in_s, 28);
    in_uint32_le(in_s, context_len);
    if (context_len != 4)
    {
        LLOGLN(0, ("scard_function_establish_context_return: opps"));
        return 1;
    }
    in_uint32_le(in_s, context);
    LLOGLN(0, ("scard_function_establish_context_return: context 0x%8.8x", context));
    out_s = trans_get_out_s(con, 8192);
    s_push_layer(out_s, iso_hdr, 8);
    out_uint32_le(out_s, context);
    out_uint32_le(out_s, 0); /* SCARD_S_SUCCESS status */
    s_mark_end(out_s);
    bytes = (int) (out_s->end - out_s->data);
    s_pop_layer(out_s, iso_hdr);
    out_uint32_le(out_s, bytes - 8);
    out_uint32_le(out_s, 0x01); /* SCARD_ESTABLISH_CONTEXT 0x01 */
    return trans_force_write(con);
}

/*****************************************************************************/
/* returns error */
int APP_CC
scard_process_release_context(struct trans *con, struct stream *in_s)
{
    int hContext;

    LLOGLN(0, ("scard_process_release_context:"));
    if (g_xrdp_pcsc_state & XRDP_PCSC_STATE_GOT_RC)
    {
        LLOGLN(0, ("scard_process_establish_context: opps"));
        return 1;
    }
    g_xrdp_pcsc_state |= XRDP_PCSC_STATE_GOT_RC;
    in_uint32_le(in_s, hContext);
    LLOGLN(0, ("scard_process_release_context: hContext 0x%8.8x", hContext));
    scard_send_release_context(con, hContext);
    return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
scard_function_release_context_return(struct trans *con,
                                      struct stream *in_s,
                                      int len)
{
    int bytes;
    struct stream *out_s;
    tui32 context;

    LLOGLN(0, ("scard_function_release_context_return:"));
    if ((g_xrdp_pcsc_state & XRDP_PCSC_STATE_GOT_RC) == 0)
    {
        LLOGLN(0, ("scard_function_release_context_return: opps"));
        return 1;
    }
    g_xrdp_pcsc_state &= ~XRDP_PCSC_STATE_GOT_RC;
    out_s = trans_get_out_s(con, 8192);
    s_push_layer(out_s, iso_hdr, 8);
    out_uint32_le(out_s, 0); /* SCARD_S_SUCCESS status */
    s_mark_end(out_s);
    bytes = (int) (out_s->end - out_s->data);
    s_pop_layer(out_s, iso_hdr);
    out_uint32_le(out_s, bytes - 8);
    out_uint32_le(out_s, 0x02); /* SCARD_RELEASE_CONTEXT 0x02 */
    return trans_force_write(con);
}

/*****************************************************************************/
/* returns error */
int APP_CC
scard_process_list_readers(struct trans *con, struct stream *in_s)
{
    int hContext;

    LLOGLN(0, ("scard_process_list_readers:"));
    if (g_xrdp_pcsc_state & XRDP_PCSC_STATE_GOT_LR)
    {
        LLOGLN(0, ("scard_process_list_readers: opps"));
        return 1;
    }
    g_xrdp_pcsc_state |= XRDP_PCSC_STATE_GOT_LR;
    in_uint32_le(in_s, hContext);
    LLOGLN(0, ("scard_process_list_readers: dwScope 0x%8.8x", hContext));
    scard_send_list_readers(con, hContext, 1);
    return 0;
}

/*****************************************************************************/
int APP_CC
scard_function_list_readers_return(struct trans *con,
                                   struct stream *in_s,
                                   int len)
{
    struct stream *out_s;
    int            chr;
    int            readers;
    int            rn_index;
    int            index;
    int            bytes;
    twchar         reader_name[100];
    char           lreader_name[16][100];

    LLOGLN(10, ("scard_function_list_readers_return:"));
    g_hexdump(in_s->p, len);
    if ((g_xrdp_pcsc_state & XRDP_PCSC_STATE_GOT_LR) == 0)
    {
        LLOGLN(10, ("scard_function_list_readers_return: opps"));
        return 1;
    }
    g_xrdp_pcsc_state &= ~XRDP_PCSC_STATE_GOT_LR;

    g_memset(reader_name, 0, sizeof(reader_name));
    g_memset(lreader_name, 0, sizeof(lreader_name));
    in_uint8s(in_s, 28);
    len -= 28;
    in_uint32_le(in_s, len);
    g_writeln("len %d", len);
    rn_index = 0;
    readers = 0;
    while (len > 0)
    {
        in_uint16_le(in_s, chr);
        len -= 2;
        if (chr == 0)
        {
            if (reader_name[0] != 0)
            {
                g_wcstombs(lreader_name[readers], reader_name, 99);
                g_writeln("1 %s", lreader_name[readers]);
                g_memset(reader_name, 0, sizeof(reader_name));
                readers++;
            }
            reader_name[0] = 0;
            rn_index = 0;
        }
        else
        {
            reader_name[rn_index] = chr;
            rn_index++;
        }
    }
    if (rn_index > 0)
    {
        if (reader_name[0] != 0)
        {
            g_wcstombs(lreader_name[readers], reader_name, 99);
            g_writeln("2 %s", lreader_name[readers]);
            g_memset(reader_name, 0, sizeof(reader_name));
            readers++;
        }
    }

    out_s = trans_get_out_s(con, 8192);
    s_push_layer(out_s, iso_hdr, 8);
    out_uint32_le(out_s, readers);
    for (index = 0; index < readers; index++)
    {
        g_writeln("3 - %s", lreader_name[index]);
        out_uint8a(out_s, lreader_name[index], 100);
    }
    out_uint32_le(out_s, 0); /* SCARD_S_SUCCESS status */
    s_mark_end(out_s);
    bytes = (int) (out_s->end - out_s->data);
    s_pop_layer(out_s, iso_hdr);
    out_uint32_le(out_s, bytes - 8);
    out_uint32_le(out_s, 0x03); /* SCARD_LIST_READERS 0x03 */
    return trans_force_write(con);
}

/*****************************************************************************/
/* returns error */
int APP_CC
scard_process_connect(struct trans *con, struct stream *in_s)
{
    int hContext;
    char szReader[100];
    READER_STATE rs;

    LLOGLN(0, ("scard_process_connect:"));
    if (g_xrdp_pcsc_state & XRDP_PCSC_STATE_GOT_C)
    {
        LLOGLN(0, ("scard_process_connect: opps"));
        return 1;
    }
    g_memset(&rs, 0, sizeof(rs));
    g_xrdp_pcsc_state |= XRDP_PCSC_STATE_GOT_C;
    in_uint32_le(in_s, hContext);
    in_uint8a(in_s, szReader, 100);
    in_uint32_le(in_s, rs.dwShareMode);
    in_uint32_le(in_s, rs.dwPreferredProtocols);
    LLOGLN(0, ("scard_process_connect: dwShareMode 0x%8.8x "
           "dwPreferredProtocols 0x%8.8x", rs.dwShareMode,
           rs.dwPreferredProtocols));
    scard_send_connect(con, hContext, 1, &rs);
    return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
scard_process_get_status_change(struct trans *con, struct stream *in_s)
{
    int index;
    int hContext;
    int dwTimeout;
    int cReaders;
    READER_STATE *rsa;

    LLOGLN(0, ("scard_process_get_status_change:"));
    if (g_xrdp_pcsc_state & XRDP_PCSC_STATE_GOT_GSC)
    {
        LLOGLN(0, ("scard_process_get_status_change: opps"));
        return 1;
    }
    in_uint32_le(in_s, hContext);
    in_uint32_le(in_s, dwTimeout);
    in_uint32_le(in_s, cReaders);
    if ((cReaders < 0) || (cReaders > 16))
    {
        LLOGLN(0, ("scard_process_get_status_change: bad cReaders %d", cReaders));
        return 1;
    }
    rsa = (READER_STATE *) g_malloc(sizeof(READER_STATE) * cReaders, 1);

    for (index = 0; index < cReaders; index++)
    {
        in_uint8a(in_s, rsa[index].reader_name, 100);
        LLOGLN(10, ("scard_process_get_status_change: reader_name %s",
               rsa[index].reader_name));
        in_uint32_le(in_s, rsa[index].current_state);
        LLOGLN(10, ("scard_process_get_status_change: current_state %d",
               rsa[index].current_state));
        in_uint32_le(in_s, rsa[index].event_state);
        LLOGLN(10, ("scard_process_get_status_change: event_state %d",
               rsa[index].event_state));
        in_uint32_le(in_s, rsa[index].atr_len);
        LLOGLN(10, ("scard_process_get_status_change: atr_len %d",
               rsa[index].atr_len));
        in_uint8a(in_s, rsa[index].atr, 36);
    }

    LLOGLN(0, ("scard_process_get_status_change: hContext 0x%8.8x dwTimeout "
           "%d cReaders %d", hContext, dwTimeout, cReaders));

    g_xrdp_pcsc_state |= XRDP_PCSC_STATE_GOT_GSC;

    scard_send_get_status_change(con, hContext, 1, dwTimeout, cReaders, rsa);

    g_free(rsa);

    return 0;
}

/*****************************************************************************/
int APP_CC
scard_function_get_status_change_return(struct trans *con,
                                        struct stream *in_s,
                                        int len)
{
    int bytes;
    int index;
    int cReaders;
    tui32 current_state;
    tui32 event_state;
    tui32 atr_len; /* number of bytes in atr[] */
    tui8 atr[36];
    struct stream *out_s;

    LLOGLN(0, ("scard_function_get_status_change_return:"));
    g_hexdump(in_s->p, len);
    if ((g_xrdp_pcsc_state & XRDP_PCSC_STATE_GOT_GSC) == 0)
    {
        LLOGLN(0, ("scard_function_establish_context_return: opps"));
        return 1;
    }
    g_xrdp_pcsc_state &= ~XRDP_PCSC_STATE_GOT_GSC;
    in_uint8s(in_s, 28);
    in_uint32_le(in_s, cReaders);
    if (cReaders > 0)
    {
        out_s = trans_get_out_s(con, 8192);
        s_push_layer(out_s, iso_hdr, 8);
        out_uint32_le(out_s, cReaders);
        for (index = 0; index < cReaders; index++)
        {
            in_uint32_le(in_s, current_state);
            out_uint32_le(out_s, current_state);
            in_uint32_le(in_s, event_state);
            out_uint32_le(out_s, event_state);
            in_uint32_le(in_s, atr_len);
            out_uint32_le(out_s, atr_len);
            in_uint8a(in_s, atr, 36);
            out_uint8a(out_s, atr, 36);
        }
        out_uint32_le(out_s, 0); /* SCARD_S_SUCCESS status */
        s_mark_end(out_s);
        bytes = (int) (out_s->end - out_s->data);
        s_pop_layer(out_s, iso_hdr);
        out_uint32_le(out_s, bytes - 8);
        out_uint32_le(out_s, 0x0C); /* SCARD_ESTABLISH_CONTEXT 0x0C */
        return trans_force_write(con);
    }
    return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
scard_process_msg(struct trans *con, struct stream *in_s, int command)
{
    int rv;

    LLOGLN(0, ("scard_process_msg: command 0x%4.4x", command));
    rv = 0;
    switch (command)
    {
        case 0x01: /* SCARD_ESTABLISH_CONTEXT */
            LLOGLN(0, ("scard_process_msg: SCARD_ESTABLISH_CONTEXT"));
            rv = scard_process_establish_context(con, in_s);
            break;
        case 0x02: /* SCARD_RELEASE_CONTEXT */
            LLOGLN(0, ("scard_process_msg: SCARD_RELEASE_CONTEXT"));
            rv = scard_process_release_context(con, in_s);
            break;

        case 0x03: /* SCARD_LIST_READERS */
            LLOGLN(0, ("scard_process_msg: SCARD_LIST_READERS"));
            rv = scard_process_list_readers(con, in_s);
            break;

        case 0x04: /* SCARD_CONNECT */
            LLOGLN(0, ("scard_process_msg: SCARD_CONNECT"));
            rv = scard_process_connect(con, in_s);
            break;

        case 0x05: /* SCARD_RECONNECT */
            LLOGLN(0, ("scard_process_msg: SCARD_RECONNECT"));
            break;

        case 0x06: /* SCARD_DISCONNECT */
            LLOGLN(0, ("scard_process_msg: SCARD_DISCONNECT"));
            break;

        case 0x07: /* SCARD_BEGIN_TRANSACTION */
            LLOGLN(0, ("scard_process_msg: SCARD_BEGIN_TRANSACTION"));
            break;

        case 0x08: /* SCARD_END_TRANSACTION */
            LLOGLN(0, ("scard_process_msg: SCARD_END_TRANSACTION"));
            break;

        case 0x09: /* SCARD_TRANSMIT */
            LLOGLN(0, ("scard_process_msg: SCARD_TRANSMIT"));
            break;

        case 0x0A: /* SCARD_CONTROL */
            LLOGLN(0, ("scard_process_msg: SCARD_CONTROL"));
            break;

        case 0x0B: /* SCARD_STATUS */
            LLOGLN(0, ("scard_process_msg: SCARD_STATUS"));
            break;

        case 0x0C: /* SCARD_GET_STATUS_CHANGE */
            LLOGLN(0, ("scard_process_msg: SCARD_GET_STATUS_CHANGE"));
            rv = scard_process_get_status_change(con, in_s);
            break;

        case 0x0D: /* SCARD_CANCEL */
            LLOGLN(0, ("scard_process_msg: SCARD_CANCEL"));
            break;

        case 0x0E: /* SCARD_CANCEL_TRANSACTION */
            LLOGLN(0, ("scard_process_msg: SCARD_CANCEL_TRANSACTION"));
            break;

        case 0x0F: /* SCARD_GET_ATTRIB */
            LLOGLN(0, ("scard_process_msg: SCARD_GET_ATTRIB"));
            break;

        case 0x10: /* SCARD_SET_ATTRIB */
            LLOGLN(0, ("scard_process_msg: SCARD_SET_ATTRIB"));
            break;

        default:
            LLOGLN(0, ("scard_process_msg: unknown mtype 0x%4.4x", command));
            rv = 1;
            break;
    }
    return rv;
}

/*****************************************************************************/
/* returns error */
int DEFAULT_CC
my_pcsc_trans_data_in(struct trans *trans)
{
    struct stream *s;
    int id;
    int size;
    int command;
    int error;

    LLOGLN(0, ("my_pcsc_trans_data_in:"));
    if (trans == 0)
    {
        return 0;
    }
    if (trans != g_con)
    {
        return 1;
    }
    s = trans_get_in_s(trans);
    in_uint32_le(s, size);
    in_uint32_le(s, command);
    LLOGLN(0, ("my_pcsc_trans_data_in: size %d command %d", size, command));
    error = trans_force_read(trans, size);
    if (error == 0)
    {
        error = scard_process_msg(g_con, s, command);
    }
    return error;
}

/*****************************************************************************/
/* got a new connection from libpcsclite */
int DEFAULT_CC
my_pcsc_trans_conn_in(struct trans *trans, struct trans *new_trans)
{
    LLOGLN(0, ("my_pcsc_trans_conn_in:"));

    if (trans == 0)
    {
        return 1;
    }

    if (trans != g_lis)
    {
        return 1;
    }

    if (new_trans == 0)
    {
        return 1;
    }

    g_con = new_trans;
    g_con->trans_data_in = my_pcsc_trans_data_in;
#if 1
    g_con->header_size = 8;
#else
    g_con->header_size = RXSHAREDSEGMENT_BYTES;
    LLOGLN(0, ("my_pcsc_trans_conn_in: sizeof sharedSegmentMsg is %d",
           sizeof(sharedSegmentMsg)));
#endif
    return 0;
}

/*****************************************************************************/
int APP_CC
scard_pcsc_init(void)
{
    char port[256];
    char *home;
    int disp;
    int error;
    int index;

    LLOGLN(0, ("scard_pcsc_init:"));
    if (g_lis == 0)
    {
        g_lis = trans_create(2, 8192, 8192);

        //g_snprintf(g_pcsclite_ipc_dir, 255, "/tmp/.xrdp/pcsc%d", g_display_num);
        home = g_getenv("HOME");
        disp = g_display_num;
        g_snprintf(g_pcsclite_ipc_dir, 255, "%s/.pcsc%d", home, disp);

        if (g_directory_exist(g_pcsclite_ipc_dir))
        {
            if (g_remove_dir(g_pcsclite_ipc_dir) != 0)
            {
                LLOGLN(0, ("scard_pcsc_init: g_remove_dir failed"));
            }
        }
        if (!g_create_dir(g_pcsclite_ipc_dir))
        {
            LLOGLN(0, ("scard_pcsc_init: g_create_dir failed"));
        }
        g_chmod_hex(g_pcsclite_ipc_dir, 0x1777);
        g_snprintf(port, 255, "%s/pcscd.comm", g_pcsclite_ipc_dir);
        g_lis->trans_conn_in = my_pcsc_trans_conn_in;
        error = trans_listen(g_lis, port);
        if (error != 0)
        {
            LLOGLN(0, ("scard_pcsc_init: trans_listen failed for port %s",
                   port));
            return 1;
        }
    }
    return 0;
}

/*****************************************************************************/
int APP_CC
scard_pcsc_deinit(void)
{
    LLOGLN(0, ("scard_pcsc_deinit:"));
    if (g_lis != 0)
    {
        trans_delete(g_lis);
        g_lis = 0;

        g_file_close(g_pub_file_fd);
        g_pub_file_fd = 0;

        if (g_remove_dir(g_pcsclite_ipc_dir) != 0)
        {
            LLOGLN(0, ("scard_pcsc_deinit: g_remove_dir failed"));
        }
        g_pcsclite_ipc_dir[0] = 0;
    }
    return 0;
}

#else

int APP_CC
scard_pcsc_get_wait_objs(tbus *objs, int *count, int *timeout)
{
    return 0;
}
int APP_CC
scard_pcsc_check_wait_objs(void)
{
    return 0;
}
int APP_CC
scard_pcsc_init(void)
{
    return 0;
}
int APP_CC
scard_pcsc_deinit(void)
{
    return 0;
}

#endif
