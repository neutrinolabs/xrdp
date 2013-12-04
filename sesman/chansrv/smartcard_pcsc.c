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

#define LLOG_LEVEL 1
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
#define XRDP_PCSC_STATE_GOT_BT           (1 << 5) /* begin transaction */
#define XRDP_PCSC_STATE_GOT_ET           (1 << 6) /* end transaction */
#define XRDP_PCSC_STATE_GOT_TR           (1 << 7) /* transmit */
#define XRDP_PCSC_STATE_GOT_CO           (1 << 8) /* control */
#define XRDP_PCSC_STATE_GOT_D            (1 << 9) /* disconnect */
#define XRDP_PCSC_STATE_GOT_ST           (1 << 10) /* get status */

/* TODO: put this in con */
static int g_xrdp_pcsc_state = XRDP_PCSC_STATE_NONE;

extern int g_display_num; /* in chansrv.c */

/*****************************************************************************/
struct pcsc_client
{
    struct trans *con;
};

#if 0
static struct pcsc_client *g_head = 0;
static struct pcsc_client *g_tail = 0;
#endif

static struct trans *g_lis = 0;
static struct trans *g_con = 0; /* todo, remove this */
static char g_pcsclite_ipc_dir[256] = "";
static int g_pub_file_fd = 0;

/*****************************************************************************/
int APP_CC
scard_pcsc_get_wait_objs(tbus *objs, int *count, int *timeout)
{
    LLOGLN(10, ("scard_pcsc_get_wait_objs"));
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
    LLOGLN(10, ("scard_pcsc_check_wait_objs"));
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

    LLOGLN(10, ("scard_process_establish_context:"));
    if (g_xrdp_pcsc_state & XRDP_PCSC_STATE_GOT_EC)
    {
        LLOGLN(0, ("scard_process_establish_context: opps"));
        return 1;
    }
    g_xrdp_pcsc_state |= XRDP_PCSC_STATE_GOT_EC;
    in_uint32_le(in_s, dwScope);
    LLOGLN(10, ("scard_process_establish_context: dwScope 0x%8.8x", dwScope));
    scard_send_establish_context(con, dwScope);
    return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
scard_function_establish_context_return(struct trans *con,
                                        struct stream *in_s,
                                        int len, int status)
{
    int bytes;
    tui32 context;
    tui32 context_len;
    struct stream *out_s;

    LLOGLN(10, ("scard_function_establish_context_return:"));
    LLOGLN(10, ("  status 0x%8.8x", status));
    //g_hexdump(in_s->p, len);
    if ((g_xrdp_pcsc_state & XRDP_PCSC_STATE_GOT_EC) == 0)
    {
        LLOGLN(0, ("scard_function_establish_context_return: opps"));
        return 1;
    }
    g_xrdp_pcsc_state &= ~XRDP_PCSC_STATE_GOT_EC;
    context = 0;
    if (status == 0)
    {
        in_uint8s(in_s, 28);
        in_uint32_le(in_s, context_len);
        if (context_len != 4)
        {
            LLOGLN(0, ("scard_function_establish_context_return: opps"));
            return 1;
        }
        in_uint32_le(in_s, context);
        LLOGLN(10, ("scard_function_establish_context_return: context 0x%8.8x",
               context));
    }
    out_s = trans_get_out_s(con, 8192);
    s_push_layer(out_s, iso_hdr, 8);
    out_uint32_le(out_s, context);
    out_uint32_le(out_s, status); /* SCARD_S_SUCCESS status */
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

    LLOGLN(10, ("scard_process_release_context:"));
    if (g_xrdp_pcsc_state & XRDP_PCSC_STATE_GOT_RC)
    {
        LLOGLN(0, ("scard_process_establish_context: opps"));
        return 1;
    }
    g_xrdp_pcsc_state |= XRDP_PCSC_STATE_GOT_RC;
    in_uint32_le(in_s, hContext);
    LLOGLN(10, ("scard_process_release_context: hContext 0x%8.8x", hContext));
    scard_send_release_context(con, hContext);
    return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
scard_function_release_context_return(struct trans *con,
                                      struct stream *in_s,
                                      int len, int status)
{
    int bytes;
    struct stream *out_s;

    LLOGLN(10, ("scard_function_release_context_return:"));
    LLOGLN(10, ("  status 0x%8.8x", status));
    if ((g_xrdp_pcsc_state & XRDP_PCSC_STATE_GOT_RC) == 0)
    {
        LLOGLN(0, ("scard_function_release_context_return: opps"));
        return 1;
    }
    g_xrdp_pcsc_state &= ~XRDP_PCSC_STATE_GOT_RC;
    out_s = trans_get_out_s(con, 8192);
    s_push_layer(out_s, iso_hdr, 8);
    out_uint32_le(out_s, status); /* SCARD_S_SUCCESS status */
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

    LLOGLN(10, ("scard_process_list_readers:"));
    if (g_xrdp_pcsc_state & XRDP_PCSC_STATE_GOT_LR)
    {
        LLOGLN(0, ("scard_process_list_readers: opps"));
        return 1;
    }
    g_xrdp_pcsc_state |= XRDP_PCSC_STATE_GOT_LR;
    in_uint32_le(in_s, hContext);
    LLOGLN(10, ("scard_process_list_readers: dwScope 0x%8.8x", hContext));
    scard_send_list_readers(con, hContext, 1);
    return 0;
}

/*****************************************************************************/
int APP_CC
scard_function_list_readers_return(struct trans *con,
                                   struct stream *in_s,
                                   int len, int status)
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
    LLOGLN(10, ("  status 0x%8.8x", status));
    //g_hexdump(in_s->p, len);
    if ((g_xrdp_pcsc_state & XRDP_PCSC_STATE_GOT_LR) == 0)
    {
        LLOGLN(0, ("scard_function_list_readers_return: opps"));
        return 1;
    }
    g_xrdp_pcsc_state &= ~XRDP_PCSC_STATE_GOT_LR;

    g_memset(reader_name, 0, sizeof(reader_name));
    g_memset(lreader_name, 0, sizeof(lreader_name));
    rn_index = 0;
    readers = 0;
    if (status == 0)
    {
        in_uint8s(in_s, 28);
        in_uint32_le(in_s, len);
        while (len > 0)
        {
            in_uint16_le(in_s, chr);
            len -= 2;
            if (chr == 0)
            {
                if (reader_name[0] != 0)
                {
                    g_wcstombs(lreader_name[readers], reader_name, 99);
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
                g_memset(reader_name, 0, sizeof(reader_name));
                readers++;
            }
        }
    }

    out_s = trans_get_out_s(con, 8192);
    s_push_layer(out_s, iso_hdr, 8);
    out_uint32_le(out_s, readers);
    for (index = 0; index < readers; index++)
    {
        out_uint8a(out_s, lreader_name[index], 100);
    }
    out_uint32_le(out_s, status); /* SCARD_S_SUCCESS status */
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
    READER_STATE rs;

    LLOGLN(10, ("scard_process_connect:"));
    if (g_xrdp_pcsc_state & XRDP_PCSC_STATE_GOT_C)
    {
        LLOGLN(0, ("scard_process_connect: opps"));
        return 1;
    }
    g_memset(&rs, 0, sizeof(rs));
    g_xrdp_pcsc_state |= XRDP_PCSC_STATE_GOT_C;
    in_uint32_le(in_s, hContext);
    in_uint8a(in_s, rs.reader_name, 100);
    in_uint32_le(in_s, rs.dwShareMode);
    in_uint32_le(in_s, rs.dwPreferredProtocols);
    LLOGLN(10, ("scard_process_connect: rs.reader_name %s dwShareMode 0x%8.8x "
           "dwPreferredProtocols 0x%8.8x", rs.reader_name, rs.dwShareMode,
           rs.dwPreferredProtocols));
    scard_send_connect(con, hContext, 1, &rs);
    return 0;
}

/*****************************************************************************/
int APP_CC
scard_function_connect_return(struct trans *con,
                              struct stream *in_s,
                              int len, int status)
{
    int dwActiveProtocol;
    int hCard;
    int bytes;
    struct stream *out_s;

    LLOGLN(10, ("scard_function_connect_return:"));
    LLOGLN(10, ("  status 0x%8.8x", status));
    //g_hexdump(in_s->p, len);
    if ((g_xrdp_pcsc_state & XRDP_PCSC_STATE_GOT_C) == 0)
    {
        LLOGLN(0, ("scard_function_connect_return: opps"));
        return 1;
    }
    g_xrdp_pcsc_state &= ~XRDP_PCSC_STATE_GOT_C;
    dwActiveProtocol = 0;
    hCard = 0;
    if (status == 0)
    {
        in_uint8s(in_s, 36);
        in_uint32_le(in_s, dwActiveProtocol);
        in_uint8s(in_s, 4);
        in_uint32_le(in_s, hCard);
        LLOGLN(10, ("  hCard %d dwActiveProtocol %d", hCard, dwActiveProtocol));
    }
    out_s = trans_get_out_s(con, 8192);
    s_push_layer(out_s, iso_hdr, 8);
    out_uint32_le(out_s, hCard);
    out_uint32_le(out_s, dwActiveProtocol);
    out_uint32_le(out_s, status); /* SCARD_S_SUCCESS status */
    s_mark_end(out_s);
    bytes = (int) (out_s->end - out_s->data);
    s_pop_layer(out_s, iso_hdr);
    out_uint32_le(out_s, bytes - 8);
    out_uint32_le(out_s, 0x04); /* SCARD_CONNECT 0x04 */
    return trans_force_write(con);
}

/*****************************************************************************/
/* returns error */
int APP_CC
scard_process_disconnect(struct trans *con, struct stream *in_s)
{
    int hContext;
    int hCard;
    int dwDisposition;

    LLOGLN(10, ("scard_process_disconnect:"));
    if (g_xrdp_pcsc_state & XRDP_PCSC_STATE_GOT_D)
    {
        LLOGLN(0, ("scard_process_disconnect: opps"));
        return 1;
    }
    g_xrdp_pcsc_state |= XRDP_PCSC_STATE_GOT_D;
    in_uint32_le(in_s, hCard);
    in_uint32_le(in_s, dwDisposition);

    hContext = 1;

    scard_send_disconnect(con, hContext, hCard, dwDisposition);

    return 0;
}

/*****************************************************************************/
int APP_CC
scard_function_disconnect_return(struct trans *con,
                                 struct stream *in_s,
                                 int len, int status)
{
    int dwActiveProtocol;
    int hCard;
    int bytes;
    struct stream *out_s;

    LLOGLN(10, ("scard_function_disconnect_return:"));
    LLOGLN(10, ("  status 0x%8.8x", status));
    //g_hexdump(in_s->p, len);
    if ((g_xrdp_pcsc_state & XRDP_PCSC_STATE_GOT_D) == 0)
    {
        LLOGLN(0, ("scard_function_connect_return: opps"));
        return 1;
    }
    g_xrdp_pcsc_state &= ~XRDP_PCSC_STATE_GOT_D;
    dwActiveProtocol = 0;
    hCard = 0;
    if (status == 0)
    {
        in_uint8s(in_s, 36);
        in_uint32_le(in_s, dwActiveProtocol);
        in_uint8s(in_s, 4);
        in_uint32_le(in_s, hCard);
        LLOGLN(10, ("scard_function_connect_return: hCard %d dwActiveProtocol %d", hCard, dwActiveProtocol));
    }
    out_s = trans_get_out_s(con, 8192);
    s_push_layer(out_s, iso_hdr, 8);
    out_uint32_le(out_s, status); /* SCARD_S_SUCCESS status */
    s_mark_end(out_s);
    bytes = (int) (out_s->end - out_s->data);
    s_pop_layer(out_s, iso_hdr);
    out_uint32_le(out_s, bytes - 8);
    out_uint32_le(out_s, 0x06); /* SCARD_DISCONNECT 0x06 */
    return trans_force_write(con);
}

/*****************************************************************************/
/* returns error */
int APP_CC
scard_process_begin_transaction(struct trans *con, struct stream *in_s)
{
    int hCard;

    LLOGLN(10, ("scard_process_begin_transaction:"));
    if (g_xrdp_pcsc_state & XRDP_PCSC_STATE_GOT_BT)
    {
        LLOGLN(0, ("scard_process_begin_transaction: opps"));
        return 1;
    }
    g_xrdp_pcsc_state |= XRDP_PCSC_STATE_GOT_BT;
    in_uint32_le(in_s, hCard);
    LLOGLN(10, ("scard_process_begin_transaction: hCard 0x%8.8x", hCard));
    scard_send_begin_transaction(con, hCard);
    return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
scard_function_begin_transaction_return(struct trans *con,
                                        struct stream *in_s,
                                        int len, int status)
{
    struct stream *out_s;
    int bytes;

    LLOGLN(10, ("scard_function_begin_transaction_return:"));
    LLOGLN(10, ("  status 0x%8.8x", status));
    //g_hexdump(in_s->p, len);
    if ((g_xrdp_pcsc_state & XRDP_PCSC_STATE_GOT_BT) == 0)
    {
        LLOGLN(0, ("scard_function_begin_transaction_return: opps"));
        return 1;
    }
    g_xrdp_pcsc_state &= ~XRDP_PCSC_STATE_GOT_BT;
    out_s = trans_get_out_s(con, 8192);
    s_push_layer(out_s, iso_hdr, 8);
    out_uint32_le(out_s, status); /* SCARD_S_SUCCESS status */
    s_mark_end(out_s);
    bytes = (int) (out_s->end - out_s->data);
    s_pop_layer(out_s, iso_hdr);
    out_uint32_le(out_s, bytes - 8);
    out_uint32_le(out_s, 0x07); /* SCARD_BEGIN_TRANSACTION 0x07 */
    return trans_force_write(con);
}

/*****************************************************************************/
/* returns error */
int APP_CC
scard_process_end_transaction(struct trans *con, struct stream *in_s)
{
    int hCard;
    int dwDisposition;

    LLOGLN(10, ("scard_process_end_transaction:"));
    if (g_xrdp_pcsc_state & XRDP_PCSC_STATE_GOT_ET)
    {
        LLOGLN(0, ("scard_process_end_transaction: opps"));
        return 1;
    }
    g_xrdp_pcsc_state |= XRDP_PCSC_STATE_GOT_ET;
    in_uint32_le(in_s, hCard);
    in_uint32_le(in_s, dwDisposition);
    LLOGLN(10, ("scard_process_end_transaction: hCard 0x%8.8x", hCard));
    scard_send_end_transaction(con, hCard, dwDisposition);
    return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
scard_function_end_transaction_return(struct trans *con,
                                      struct stream *in_s,
                                      int len, int status)
{
    struct stream *out_s;
    int bytes;

    LLOGLN(10, ("scard_function_end_transaction_return:"));
    LLOGLN(10, ("  status 0x%8.8x", status));
    //g_hexdump(in_s->p, len);
    if ((g_xrdp_pcsc_state & XRDP_PCSC_STATE_GOT_ET) == 0)
    {
        LLOGLN(0, ("scard_function_end_transaction_return: opps"));
        return 1;
    }
    g_xrdp_pcsc_state &= ~XRDP_PCSC_STATE_GOT_ET;

    out_s = trans_get_out_s(con, 8192);
    s_push_layer(out_s, iso_hdr, 8);
    out_uint32_le(out_s, status); /* SCARD_S_SUCCESS status */
    s_mark_end(out_s);
    bytes = (int) (out_s->end - out_s->data);
    s_pop_layer(out_s, iso_hdr);
    out_uint32_le(out_s, bytes - 8);
    out_uint32_le(out_s, 0x08); /* SCARD_END_TRANSACTION 0x08 */
    return trans_force_write(con);
}

/*****************************************************************************/
/* returns error */
int APP_CC
scard_function_cancel_return(struct trans *con,
                             struct stream *in_s,
                             int len, int status)
{
    return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
scard_function_get_attrib_return(struct trans *con,
                                 struct stream *in_s,
                                 int len, int status)
{
    return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
scard_process_transmit(struct trans *con, struct stream *in_s)
{
    int hCard;
    int recv_bytes;
    int send_bytes;
    char *send_data;
    struct xrdp_scard_io_request send_ior;
    struct xrdp_scard_io_request recv_ior;

    LLOGLN(10, ("scard_process_transmit:"));
    if (g_xrdp_pcsc_state & XRDP_PCSC_STATE_GOT_TR)
    {
        LLOGLN(0, ("scard_process_transmit: opps"));
        return 1;
    }
    g_xrdp_pcsc_state |= XRDP_PCSC_STATE_GOT_TR;
    LLOGLN(10, ("scard_process_transmit:"));
    in_uint32_le(in_s, hCard);
    in_uint32_le(in_s, send_ior.dwProtocol);
    in_uint32_le(in_s, send_ior.cbPciLength);
    in_uint32_le(in_s, send_ior.extra_bytes);
    in_uint8p(in_s, send_ior.extra_data, send_ior.extra_bytes);
    in_uint32_le(in_s, send_bytes);
    in_uint8p(in_s, send_data, send_bytes);
    in_uint32_le(in_s, recv_ior.dwProtocol);
    in_uint32_le(in_s, recv_ior.cbPciLength);
    in_uint32_le(in_s, recv_ior.extra_bytes);
    in_uint8p(in_s, recv_ior.extra_data, recv_ior.extra_bytes);
    in_uint32_le(in_s, recv_bytes);
    LLOGLN(10, ("scard_process_transmit: send dwProtocol %d cbPciLength %d "
           "recv dwProtocol %d cbPciLength %d send_bytes %d ",
           send_ior.dwProtocol, send_ior.cbPciLength, recv_ior.dwProtocol,
           recv_ior.cbPciLength, send_bytes));
    //g_hexdump(in_s->p, send_bytes);
    LLOGLN(10, ("scard_process_transmit: recv_bytes %d", recv_bytes));
    scard_send_transmit(con, hCard, send_data, send_bytes, recv_bytes,
                        &send_ior, &recv_ior);
    return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
scard_function_transmit_return(struct trans *con,
                               struct stream *in_s,
                               int len, int status)
{
    struct stream *out_s;
    int bytes;
    int val;
    int cbRecvLength;
    struct xrdp_scard_io_request recv_ior;
    char *recvBuf;

    LLOGLN(10, ("scard_function_transmit_return:"));
    LLOGLN(10, ("  status 0x%8.8x", status));
    //g_hexdump(in_s->p, len);
    if ((g_xrdp_pcsc_state & XRDP_PCSC_STATE_GOT_TR) == 0)
    {
        LLOGLN(0, ("scard_function_transmit_return: opps"));
        return 1;
    }
    g_xrdp_pcsc_state &= ~XRDP_PCSC_STATE_GOT_TR;

    g_memset(&recv_ior, 0, sizeof(recv_ior));
    cbRecvLength = 0;

    if (status == 0)
    {
        in_uint8s(in_s, 20);
        in_uint32_le(in_s, val);
        g_memset(&recv_ior, 0, sizeof(recv_ior));
        if (val != 0)
        {
            /* pioRecvPci */
            LLOGLN(0, ("scard_function_transmit_return: pioRecvPci not zero!"));
        }
        in_uint8s(in_s, 4);
        in_uint32_le(in_s, val);
        cbRecvLength = 0;
        recvBuf = 0;
        if (val != 0)
        {
            in_uint32_le(in_s, cbRecvLength);
            in_uint8p(in_s, recvBuf, cbRecvLength);
        }
    }
    LLOGLN(10, ("scard_function_transmit_return: cbRecvLength %d", cbRecvLength));
    out_s = trans_get_out_s(con, 8192);
    s_push_layer(out_s, iso_hdr, 8);
    out_uint32_le(out_s, recv_ior.dwProtocol);
    out_uint32_le(out_s, recv_ior.cbPciLength);
    out_uint32_le(out_s, recv_ior.extra_bytes);
    out_uint8a(out_s, recv_ior.extra_data, recv_ior.extra_bytes);
    out_uint32_le(out_s, cbRecvLength);
    out_uint8a(out_s, recvBuf, cbRecvLength);
    out_uint32_le(out_s, status); /* SCARD_S_SUCCESS status */
    s_mark_end(out_s);
    bytes = (int) (out_s->end - out_s->data);
    s_pop_layer(out_s, iso_hdr);
    out_uint32_le(out_s, bytes - 8);
    out_uint32_le(out_s, 0x09); /* SCARD_TRANSMIT 0x09 */
    return trans_force_write(con);
}

/*****************************************************************************/
/* returns error */
int APP_CC
scard_process_control(struct trans *con, struct stream *in_s)
{
    tui32 context;
    int hCard;
    int send_bytes;
    int recv_bytes;
    int control_code;
    char *send_data;

    LLOGLN(10, ("scard_process_control:"));
    if (g_xrdp_pcsc_state & XRDP_PCSC_STATE_GOT_CO)
    {
        LLOGLN(0, ("scard_process_control: opps"));
        return 1;
    }
    g_xrdp_pcsc_state |= XRDP_PCSC_STATE_GOT_CO;
    LLOGLN(10, ("scard_process_control:"));

    in_uint32_le(in_s, hCard);
    in_uint32_le(in_s, control_code);
    in_uint32_le(in_s, send_bytes);
    in_uint8p(in_s, send_data, send_bytes);
    in_uint32_le(in_s, recv_bytes);

    context = 1;

    scard_send_control(con, context, hCard, send_data, send_bytes, recv_bytes,
                       control_code);

    return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
scard_function_control_return(struct trans *con,
                              struct stream *in_s,
                              int len, int status)
{
    struct stream *out_s;
    int bytes;
    int cbRecvLength;
    char *recvBuf;

    LLOGLN(10, ("scard_function_control_return:"));
    LLOGLN(10, ("  status 0x%8.8x", status));
    //g_hexdump(in_s->p, len);
    if ((g_xrdp_pcsc_state & XRDP_PCSC_STATE_GOT_CO) == 0)
    {
        LLOGLN(0, ("scard_function_control_return: opps"));
        return 1;
    }
    g_xrdp_pcsc_state &= ~XRDP_PCSC_STATE_GOT_CO;
    cbRecvLength = 0;
    recvBuf = 0;
    if (status == 0)
    {
        in_uint8s(in_s, 28);
        in_uint32_le(in_s, cbRecvLength);
        in_uint8p(in_s, recvBuf, cbRecvLength);
    }
    LLOGLN(10, ("scard_function_control_return: cbRecvLength %d", cbRecvLength));
    out_s = trans_get_out_s(con, 8192);
    s_push_layer(out_s, iso_hdr, 8);
    out_uint32_le(out_s, cbRecvLength);
    out_uint8a(out_s, recvBuf, cbRecvLength);
    out_uint32_le(out_s, status); /* SCARD_S_SUCCESS status */
    s_mark_end(out_s);
    bytes = (int) (out_s->end - out_s->data);
    s_pop_layer(out_s, iso_hdr);
    out_uint32_le(out_s, bytes - 8);
    out_uint32_le(out_s, 0x0A); /* SCARD_CONTROL 0x0A */
    return trans_force_write(con);
}

/*****************************************************************************/
/* returns error */
int APP_CC
scard_process_status(struct trans *con, struct stream *in_s)
{
    int hCard;
    int cchReaderLen;
    int cbAtrLen;

    LLOGLN(10, ("scard_process_status:"));
    if (g_xrdp_pcsc_state & XRDP_PCSC_STATE_GOT_ST)
    {
        LLOGLN(0, ("scard_process_status: opps"));
        return 1;
    }
    g_xrdp_pcsc_state |= XRDP_PCSC_STATE_GOT_ST;

    in_uint32_le(in_s, hCard);
    in_uint32_le(in_s, cchReaderLen);
    in_uint32_le(in_s, cbAtrLen);

    scard_send_status(con, 1, hCard, cchReaderLen, cbAtrLen);

    return 0;
}

#define MS_SCARD_UNKNOWN    0
#define MS_SCARD_ABSENT     1
#define MS_SCARD_PRESENT    2
#define MS_SCARD_SWALLOWED  3
#define MS_SCARD_POWERED    4
#define MS_SCARD_NEGOTIABLE 5
#define MS_SCARD_SPECIFIC   6

#define PC_SCARD_UNKNOWN    0x0001 /**< Unknown state */
#define PC_SCARD_ABSENT     0x0002 /**< Card is absent */
#define PC_SCARD_PRESENT    0x0004 /**< Card is present */
#define PC_SCARD_SWALLOWED  0x0008 /**< Card not powered */
#define PC_SCARD_POWERED    0x0010 /**< Card is powered */
#define PC_SCARD_NEGOTIABLE 0x0020 /**< Ready for PTS */
#define PC_SCARD_SPECIFIC   0x0040 /**< PTS has been set */

static int g_ms2pc[] = { PC_SCARD_UNKNOWN, PC_SCARD_ABSENT,
                         PC_SCARD_PRESENT, PC_SCARD_SWALLOWED,
                         PC_SCARD_POWERED, PC_SCARD_NEGOTIABLE,
                         PC_SCARD_SPECIFIC };

/*****************************************************************************/
/* returns error */
int APP_CC
scard_function_status_return(struct trans *con,
                             struct stream *in_s,
                             int len, int status)
{
    struct stream *out_s;
    int index;
    int bytes;
    int dwReaderLen;
    int dwState;
    int dwProtocol;
    int dwAtrLen;
    char attr[32];
    twchar reader_name[100];
    char lreader_name[100];

    LLOGLN(10, ("scard_function_status_return:"));
    LLOGLN(10, ("  status 0x%8.8x", status));
    //g_hexdump(in_s->p, len);
    if ((g_xrdp_pcsc_state & XRDP_PCSC_STATE_GOT_ST) == 0)
    {
        LLOGLN(0, ("scard_function_status_return: opps"));
        return 1;
    }
    g_xrdp_pcsc_state &= ~XRDP_PCSC_STATE_GOT_ST;
    dwReaderLen = 0;
    dwState = 0;
    dwProtocol = 0;
    dwAtrLen = 0;
    lreader_name[0] = 0;
    if (status == 0)
    {
        in_uint8s(in_s, 20);
        in_uint32_le(in_s, dwReaderLen);
        in_uint8s(in_s, 4);
        in_uint32_le(in_s, dwState);
        dwState = g_ms2pc[dwState % 6];
        in_uint32_le(in_s, dwProtocol);
        in_uint8a(in_s, attr, 32);
        in_uint32_le(in_s, dwAtrLen);
        in_uint32_le(in_s, dwReaderLen);
        dwReaderLen /= 2;
        g_memset(reader_name, 0, sizeof(reader_name));
        g_memset(lreader_name, 0, sizeof(lreader_name));
        for (index = 0; index < dwReaderLen; index++)
        {
            in_uint16_le(in_s, reader_name[index]);
        }
        g_wcstombs(lreader_name, reader_name, 99);
    }
    LLOGLN(10, ("scard_function_status_return: dwAtrLen %d dwReaderLen %d "
           "dwProtocol %d dwState %d name %s",
           dwAtrLen, dwReaderLen, dwProtocol, dwState, lreader_name));
    out_s = trans_get_out_s(con, 8192);
    s_push_layer(out_s, iso_hdr, 8);
    dwReaderLen = g_strlen(lreader_name);
    out_uint32_le(out_s, dwReaderLen);
    out_uint8a(out_s, lreader_name, dwReaderLen);
    out_uint32_le(out_s, dwState);
    out_uint32_le(out_s, dwProtocol);
    out_uint32_le(out_s, dwAtrLen);
    out_uint8a(out_s, attr, dwAtrLen);
    out_uint32_le(out_s, status); /* SCARD_S_SUCCESS status */
    s_mark_end(out_s);
    bytes = (int) (out_s->end - out_s->data);
    s_pop_layer(out_s, iso_hdr);
    out_uint32_le(out_s, bytes - 8);
    out_uint32_le(out_s, 0x0B); /* SCARD_STATUS 0x0B */
    return trans_force_write(con);
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

    LLOGLN(10, ("scard_process_get_status_change:"));
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

    LLOGLN(10, ("scard_process_get_status_change: hContext 0x%8.8x dwTimeout "
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
                                        int len, int status)
{
    int bytes;
    int index;
    int cReaders;
    tui32 current_state;
    tui32 event_state;
    tui32 atr_len; /* number of bytes in atr[] */
    tui8 atr[36];
    struct stream *out_s;

    LLOGLN(10, ("scard_function_get_status_change_return:"));
    LLOGLN(10, ("  status 0x%8.8x", status));
    //g_hexdump(in_s->p, len);
    if ((g_xrdp_pcsc_state & XRDP_PCSC_STATE_GOT_GSC) == 0)
    {
        LLOGLN(0, ("scard_function_establish_context_return: opps"));
        return 1;
    }
    g_xrdp_pcsc_state &= ~XRDP_PCSC_STATE_GOT_GSC;

    out_s = trans_get_out_s(con, 8192);
    s_push_layer(out_s, iso_hdr, 8);
    if (status != 0)
    {
        out_uint32_le(out_s, 0); /* cReaders */
        out_uint32_le(out_s, status); /* SCARD_S_SUCCESS status */
    }
    else
    {
        in_uint8s(in_s, 28);
        in_uint32_le(in_s, cReaders);
        LLOGLN(10, ("  cReaders %d", cReaders));
        out_uint32_le(out_s, cReaders);
        if (cReaders > 0)
        {
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
        }
        out_uint32_le(out_s, status); /* SCARD_S_SUCCESS status */
    }

    s_mark_end(out_s);
    bytes = (int) (out_s->end - out_s->data);
    s_pop_layer(out_s, iso_hdr);
    out_uint32_le(out_s, bytes - 8);
    out_uint32_le(out_s, 0x0C); /* SCARD_ESTABLISH_CONTEXT 0x0C */
    return trans_force_write(con);
}

/*****************************************************************************/
/* returns error */
int APP_CC
scard_function_is_context_valid_return(struct trans *con,
                                       struct stream *in_s,
                                       int len, int status)
{
    return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC scard_function_reconnect_return(struct trans *con,
                                           struct stream *in_s,
                                           int len, int status)
{
    return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
scard_process_msg(struct trans *con, struct stream *in_s, int command)
{
    int rv;

    LLOGLN(10, ("scard_process_msg: command 0x%4.4x", command));
    rv = 0;
    switch (command)
    {
        case 0x01: /* SCARD_ESTABLISH_CONTEXT */
            LLOGLN(10, ("scard_process_msg: SCARD_ESTABLISH_CONTEXT"));
            rv = scard_process_establish_context(con, in_s);
            break;
        case 0x02: /* SCARD_RELEASE_CONTEXT */
            LLOGLN(10, ("scard_process_msg: SCARD_RELEASE_CONTEXT"));
            rv = scard_process_release_context(con, in_s);
            break;

        case 0x03: /* SCARD_LIST_READERS */
            LLOGLN(10, ("scard_process_msg: SCARD_LIST_READERS"));
            rv = scard_process_list_readers(con, in_s);
            break;

        case 0x04: /* SCARD_CONNECT */
            LLOGLN(10, ("scard_process_msg: SCARD_CONNECT"));
            rv = scard_process_connect(con, in_s);
            break;

        case 0x05: /* SCARD_RECONNECT */
            LLOGLN(0, ("scard_process_msg: SCARD_RECONNECT"));
            break;

        case 0x06: /* SCARD_DISCONNECT */
            LLOGLN(10, ("scard_process_msg: SCARD_DISCONNECT"));
            rv = scard_process_disconnect(con, in_s);
            break;

        case 0x07: /* SCARD_BEGIN_TRANSACTION */
            LLOGLN(10, ("scard_process_msg: SCARD_BEGIN_TRANSACTION"));
            rv = scard_process_begin_transaction(con, in_s);
            break;

        case 0x08: /* SCARD_END_TRANSACTION */
            LLOGLN(10, ("scard_process_msg: SCARD_END_TRANSACTION"));
            rv = scard_process_end_transaction(con, in_s);
            break;

        case 0x09: /* SCARD_TRANSMIT */
            LLOGLN(10, ("scard_process_msg: SCARD_TRANSMIT"));
            rv = scard_process_transmit(con, in_s);
            break;

        case 0x0A: /* SCARD_CONTROL */
            LLOGLN(10, ("scard_process_msg: SCARD_CONTROL"));
            rv = scard_process_control(con, in_s);
            break;

        case 0x0B: /* SCARD_STATUS */
            LLOGLN(10, ("scard_process_msg: SCARD_STATUS"));
            rv = scard_process_status(con, in_s);
            break;

        case 0x0C: /* SCARD_GET_STATUS_CHANGE */
            LLOGLN(10, ("scard_process_msg: SCARD_GET_STATUS_CHANGE"));
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
    int size;
    int command;
    int error;

    LLOGLN(10, ("my_pcsc_trans_data_in:"));
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
    LLOGLN(10, ("my_pcsc_trans_data_in: size %d command %d", size, command));
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
    LLOGLN(10, ("my_pcsc_trans_conn_in:"));

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
    g_con->header_size = 8;
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
