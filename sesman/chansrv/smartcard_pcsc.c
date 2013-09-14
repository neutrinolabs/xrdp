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

/* using pcsc-lite-1.7.4 */

struct establish_struct
{
    tui32 dwScope;
    tui32 hContext;
    tui32 rv;
};

struct release_struct
{
    tui32 hContext;
    tui32 rv;
};

/** Major version of the current message protocol */
#define PROTOCOL_VERSION_MAJOR 4
/** Minor version of the current message protocol */
#define PROTOCOL_VERSION_MINOR 2

struct version_struct
{
    tsi32 major; /**< IPC major \ref PROTOCOL_VERSION_MAJOR */
    tsi32 minor; /**< IPC minor \ref PROTOCOL_VERSION_MINOR */
    tui32 rv;
};

#define SCARD_S_SUCCESS          0x00000000 /**< No error was encountered. */
#define SCARD_F_INTERNAL_ERROR   0x80100001 /**< An internal consistency
                                             *   check failed. */
#define SCARD_E_TIMEOUT          0x8010000A /**< The user-specified timeout
                                             *   value has expired. */

#define MAX_READERNAME 100
#define MAX_ATR_SIZE 33

struct pubReaderStatesList
{
    char readerName[MAX_READERNAME]; /**< reader name */
    tui32 eventCounter;              /**< number of card events */
    tui32 readerState;               /**< SCARD_* bit field */
    tsi32 readerSharing;             /**< PCSCLITE_SHARING_* sharing status */
    tui8 cardAtr[MAX_ATR_SIZE];      /**< ATR */
    tui32 cardAtrLength;             /**< ATR length */
    tui32 cardProtocol;              /**< SCARD_PROTOCOL_* value */
};

#define PCSCLITE_MAX_READERS_CONTEXTS 16

static int g_num_readers = 0;
/* pcsc list */
static struct pubReaderStatesList
                    g_pcsc_reader_states[PCSCLITE_MAX_READERS_CONTEXTS];
/* rdp list */
static READER_STATE g_xrdp_reader_states[PCSCLITE_MAX_READERS_CONTEXTS];

struct wait_reader_state_change
{
    tui32 timeOut; /**< timeout in ms */
    tui32 rv;
};

#define XRDP_PCSC_STATE_NONE              0
#define XRDP_PCSC_STATE_GOT_RSC          (1 << 0) /* read state change */
#define XRDP_PCSC_STATE_GOT_EC           (1 << 1) /* establish context */
#define XRDP_PCSC_STATE_GOT_LR           (1 << 2) /* list readers */
#define XRDP_PCSC_STATE_GOT_RC           (1 << 3) /* release context */

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
static char g_pub_file_name[256] = "";

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
    struct establish_struct in_es;

    LLOGLN(0, ("scard_process_establish_context:"));
    if (g_xrdp_pcsc_state & XRDP_PCSC_STATE_GOT_EC)
    {
        LLOGLN(0, ("scard_process_establish_context: opps"));
        return 1;
    }
    g_xrdp_pcsc_state |= XRDP_PCSC_STATE_GOT_EC;
    in_uint8a(in_s, &in_es, sizeof(in_es));
    LLOGLN(0, ("scard_process_establish_context: dwScope 0x%8.8x",
           in_es.dwScope));
    scard_send_irp_establish_context(con, in_es.dwScope);
    return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
scard_function_establish_context_return(struct trans *con, int context)
{
    struct establish_struct out_es;
    struct stream *out_s;

    LLOGLN(0, ("scard_function_establish_context_return: context %d",
           context));
    if ((g_xrdp_pcsc_state & XRDP_PCSC_STATE_GOT_EC) == 0)
    {
        LLOGLN(0, ("scard_function_establish_context_return: opps"));
        return 1;
    }
    g_xrdp_pcsc_state &= ~XRDP_PCSC_STATE_GOT_EC;
    out_es.dwScope = 0;
    out_es.hContext = context;
    out_es.rv = SCARD_S_SUCCESS;
    out_s = trans_get_out_s(con, 8192);
    out_uint8a(out_s, &out_es, sizeof(out_es));
    s_mark_end(out_s);
    return trans_force_write(con);
}

/*****************************************************************************/
/* returns error */
int APP_CC
scard_process_release_context(struct stream *in_s)
{
    struct release_struct in_rs;
    struct release_struct out_rs;
    struct stream *out_s;

    LLOGLN(0, ("scard_process_release_context:"));
    in_uint8a(in_s, &in_rs, sizeof(in_rs));
    LLOGLN(0, ("scard_process_release_context: hContext %d", in_rs.hContext));

    /* TODO: use XRDP_PCSC_STATE_GOT_RC */

    out_rs.hContext = in_rs.hContext;
    out_rs.rv = SCARD_S_SUCCESS;
    out_s = trans_get_out_s(g_con, 8192);
    out_uint8a(out_s, &out_rs, sizeof(out_rs));
    s_mark_end(out_s);
    return trans_force_write(g_con);
}

/*****************************************************************************/
/* returns error */
int APP_CC
scard_process_version(struct trans *con, struct stream *in_s)
{
    struct stream *out_s;
    struct version_struct in_version;
    struct version_struct out_version;

    LLOGLN(0, ("scard_process_version:"));
    in_uint8a(in_s, &in_version, sizeof(in_version));
    /* todo: check if version is compatible */
    LLOGLN(0, ("scard_process_version: version major %d minor %d",
           in_version.major, in_version.minor));
    out_s = trans_get_out_s(con, 8192);
    out_version.major = PROTOCOL_VERSION_MAJOR;
    out_version.minor = PROTOCOL_VERSION_MINOR;
    out_version.rv = SCARD_S_SUCCESS;
    out_uint8a(out_s, &out_version, sizeof(out_version));
    s_mark_end(out_s);
    return trans_force_write(con);
}

/*****************************************************************************/
/* returns error */
int APP_CC
scard_process_get_readers_state(struct trans *con, struct stream *in_s)
{
    //struct stream *out_s;

    LLOGLN(0, ("scard_process_get_readers_state:"));

    //out_s = trans_get_out_s(con, 8192);
    //out_uint8a(out_s, g_pcsc_reader_states, sizeof(g_pcsc_reader_states));
    //s_mark_end(out_s);
    //return trans_force_write(con);

    if (g_xrdp_pcsc_state & XRDP_PCSC_STATE_GOT_LR)
    {
        LLOGLN(0, ("scard_process_get_readers_state: opps"));
        return 1;
    }
    g_xrdp_pcsc_state |= XRDP_PCSC_STATE_GOT_LR;

    scard_send_irp_list_readers(con);

    return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
scard_function_list_readers_return(struct trans *con,
                                   struct stream *in_s,
                                   int len)
{
    struct stream *out_s;
    int            chr;
    int            readers;
    int            rn_index;
    char           reader_name[100];

    g_hexdump(in_s->p, len);

    if ((g_xrdp_pcsc_state & XRDP_PCSC_STATE_GOT_LR) == 0)
    {
        LLOGLN(0, ("scard_function_list_readers_return: opps"));
        return 1;
    }
    g_xrdp_pcsc_state &= ~XRDP_PCSC_STATE_GOT_LR;

    in_uint8s(in_s, 28);
    len -= 28;
    in_uint32_le(in_s, len);
    g_writeln("len %d", len);
    rn_index = 0;
    readers = 1;
    while (len > 0)
    {
        in_uint16_le(in_s, chr);
        len -= 2;
        if (chr == 0)
        {
            if (reader_name[0] != 0)
            {
                g_writeln("1 %s", reader_name);
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
            g_writeln("2 %s", reader_name);
            readers++;
        }
    }
#if 0
    g_strcpy(g_reader_states[0].readerName, "ACS AET65 00 00");
    g_reader_states[0].readerState = 0x14;
    g_reader_states[0].cardProtocol = 3;

    g_reader_states[0].cardAtrLength = 10;
    g_reader_states[0].cardAtr[0] = 0x3B;
    g_reader_states[0].cardAtr[1] = 0x95;
    g_reader_states[0].cardAtr[2] = 0x95;
    g_reader_states[0].cardAtr[3] = 0x40;
    g_reader_states[0].cardAtr[4] = 0xFF;
    g_reader_states[0].cardAtr[5] = 0xD0;
    g_reader_states[0].cardAtr[6] = 0x00;
    g_reader_states[0].cardAtr[7] = 0x54;
    g_reader_states[0].cardAtr[8] = 0x01;
    g_reader_states[0].cardAtr[9] = 0x32;

    //g_reader_states[0].eventCounter++;

    out_s = trans_get_out_s(con, 8192);
    out_uint8a(out_s, g_reader_states, sizeof(g_reader_states));
    s_mark_end(out_s);
    return trans_force_write(con);
#endif
    return 0;
}

/*****************************************************************************/
/* callback from chansrv when timeout occurs */
void DEFAULT_CC
scard_read_state_chage_timeout(void* data)
{
    struct trans *con;
    struct stream *out_s;
    struct wait_reader_state_change out_rsc;

    LLOGLN(0, ("scard_read_state_chage_timeout:"));
    con = (struct trans *) data;
    if (g_xrdp_pcsc_state & XRDP_PCSC_STATE_GOT_RSC)
    {
        out_s = trans_get_out_s(con, 8192);
        out_rsc.timeOut = 0;
        out_rsc.rv = SCARD_S_SUCCESS;
        out_uint8a(out_s, &out_rsc, sizeof(out_rsc));
        g_xrdp_pcsc_state &= ~XRDP_PCSC_STATE_GOT_RSC;
        s_mark_end(out_s);
        trans_force_write(con);
    }
    else
    {
         LLOGLN(0, ("scard_read_state_chage_timeout: already stopped"));
    }
}

/*****************************************************************************/
/* returns error */
int APP_CC
scard_process_read_state_change(struct trans *con, struct stream *in_s)
{
    struct wait_reader_state_change in_rsc;
    struct stream *out_s;
    int index;

    LLOGLN(0, ("scard_process_read_state_change:"));
    in_uint8a(in_s, &in_rsc, sizeof(in_rsc));
    if (g_xrdp_pcsc_state & XRDP_PCSC_STATE_GOT_RSC)
    {
        LLOGLN(0, ("scard_process_read_state_change: opps"));
        return 0;
    }

#if 0
    for (index = 0; index < 16; index++)
    {
        //g_memcpy(rd[index].reader_name, g_pcsc_reader_states[index].readerName, 99);
        g_strncpy(rd[index].reader_name, "Gemalto PC Twin Reader 00 00", 99);
        rd[index].current_state = g_pcsc_reader_states[index].readerState;
        rd[index].event_state = g_pcsc_reader_states[index].eventCounter;
        rd[index].atr_len = g_pcsc_reader_states[index].cardAtrLength;
        g_memcpy(rd[index].atr, g_pcsc_reader_states[index].cardAtr, 33);
    }
#endif

    g_xrdp_pcsc_state |= XRDP_PCSC_STATE_GOT_RSC;
    scard_send_irp_get_status_change(con, 1, in_rsc.timeOut, g_num_readers,
                                     g_xrdp_reader_states);

    LLOGLN(0, ("scard_process_read_state_change: timeout %d rv %d",
           in_rsc.timeOut, in_rsc.rv));

    add_timeout(in_rsc.timeOut, scard_read_state_chage_timeout, con);

    return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
scard_process_stop_read_state_change(struct trans *con, struct stream *in_s)
{
    struct wait_reader_state_change in_rsc;
    struct wait_reader_state_change out_rsc;
    struct stream *out_s;

    LLOGLN(0, ("scard_process_stop_read_state_change:"));
    in_uint8a(in_s, &in_rsc, sizeof(in_rsc));
    LLOGLN(0, ("scard_process_stop_read_state_change: timeout %d rv %d",
           in_rsc.timeOut, in_rsc.rv));
    if (g_xrdp_pcsc_state & XRDP_PCSC_STATE_GOT_RSC)
    {
        out_s = trans_get_out_s(con, 8192);
        out_rsc.timeOut = in_rsc.timeOut;
        out_rsc.rv = SCARD_S_SUCCESS;
        out_uint8a(out_s, &out_rsc, sizeof(out_rsc));
        g_xrdp_pcsc_state &= ~XRDP_PCSC_STATE_GOT_RSC;
        s_mark_end(out_s);
        return trans_force_write(con);
    }
    else
    {
         LLOGLN(0, ("scard_process_stop_read_state_change: already stopped"));
    }
    return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
scard_function_get_status_change_return(struct trans *con,
                                        struct stream *in_s,
                                        int len)
{
    struct stream *out_s;
    int num_readers;
    int index;
    int current_state;
    int event_state;
    int atr_len;
    char atr[36];

    LLOGLN(0, ("scard_function_get_status_change_return:"));

    //g_hexdump(in_s->p, len);

    in_uint8s(in_s, 28);
    in_uint32_le(in_s, num_readers);
    LLOGLN(0, ("  num_reader %d", num_readers));

    g_num_readers = num_readers;

    for (index = 0; index < num_readers; index++)
    {
        in_uint32_le(in_s, current_state);
        in_uint32_le(in_s, event_state);
        in_uint32_le(in_s, atr_len);
        in_uint8a(in_s, atr, 36);
        LLOGLN(0, ("  current_state 0x%8.8x event_state 0x%8.8x "
               "atr_len 0x%8.8x", current_state, event_state, atr_len));
        g_xrdp_reader_states[index].current_state = current_state;
        g_xrdp_reader_states[index].event_state = event_state;
        g_xrdp_reader_states[index].atr_len = atr_len;
        g_memcpy(g_xrdp_reader_states[index].atr, atr, 36);
        
    }
    //out_s = trans_get_out_s(con, 8192);
    //out_uint8a(out_s, g_reader_states, sizeof(g_reader_states));
    //s_mark_end(out_s);
    //return trans_force_write(con);

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
            rv = scard_process_release_context(in_s);
            break;

        case 0x03: /* SCARD_LIST_READERS */
            LLOGLN(0, ("scard_process_msg: SCARD_LIST_READERS"));
            break;

        case 0x04: /* SCARD_CONNECT */
            LLOGLN(0, ("scard_process_msg: SCARD_CONNECT"));
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

        case 0x11: /* CMD_VERSION */
            LLOGLN(0, ("scard_process_msg: CMD_VERSION"));
            rv = scard_process_version(con, in_s);
            break;
        case 0x12: /* CMD_GET_READERS_STATE */
            LLOGLN(0, ("scard_process_msg: CMD_GET_READERS_STATE"));
            rv = scard_process_get_readers_state(con, in_s);
            break;
        case 0x13: /* CMD_WAIT_READER_STATE_CHANGE */
            LLOGLN(0, ("scard_process_msg: CMD_WAIT_READER_STATE_CHANGE"));
            rv = scard_process_read_state_change(con, in_s);
            break;
        case 0x14: /* CMD_STOP_WAITING_READER_STATE_CHANGE */
            LLOGLN(0, ("scard_process_msg: CMD_STOP_WAITING_READER_STATE_CHANGE"));
            rv = scard_process_stop_read_state_change(con, in_s);
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
    g_memset(g_pcsc_reader_states, 0, sizeof(g_pcsc_reader_states));
    g_memset(g_xrdp_reader_states, 0, sizeof(g_xrdp_reader_states));
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
        g_snprintf(g_pub_file_name, 255, "%s/pcscd.pub", g_pcsclite_ipc_dir);
        g_pub_file_fd = g_file_open(g_pub_file_name);
        index = 0;
        while (index < 65537)
        {
            g_file_write(g_pub_file_fd, "", 1);
            index++;
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
        g_file_delete(g_pub_file_name);
        g_pub_file_name[0] = 0;

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
