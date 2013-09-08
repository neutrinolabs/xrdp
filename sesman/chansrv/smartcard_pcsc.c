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

#if PCSC_STANDIN

#define LLOG_LEVEL 11
#define LLOGLN(_level, _args) \
  do \
  { \
    if (_level < LLOG_LEVEL) \
    { \
      g_write("chansrv:smartcard [%10.10u]: ", g_time3()); \
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

static struct pubReaderStatesList
                   g_reader_states[PCSCLITE_MAX_READERS_CONTEXTS];

struct wait_reader_state_change
{
    tui32 timeOut; /**< timeout in ms */
    tui32 rv;
};

#define XRDP_PCSC_STATE_NONE               0
#define XRDP_PCSC_STATE_GOT_RSC            1

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
scard_proess_release_context(struct stream *in_s)
{
    struct release_struct in_rs;
    struct release_struct out_rs;
    struct stream *out_s;

    LLOGLN(0, ("scard_proess_release_context:"));
    in_uint8a(in_s, &in_rs, sizeof(in_rs));
    LLOGLN(0, ("scard_proess_release_context: hContext %d", in_rs.hContext));
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
scard_process_get_readers_state(struct stream *in_s)
{
    LLOGLN(0, ("scard_process_get_readers_state:"));
    scard_send_irp_list_readers(g_con);
    return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
scard_function_get_readers_state_return(struct trans *con,
                                        struct stream *in_s,
                                        int len)
{
    struct stream *out_s;

    g_hexdump(in_s->p, len);
    out_s = trans_get_out_s(con, 8192);
    out_uint8a(out_s, g_reader_states, sizeof(g_reader_states));
    s_mark_end(out_s);
    return trans_force_write(con);
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

    LLOGLN(0, ("scard_process_read_state_change:"));
    in_uint8a(in_s, &in_rsc, sizeof(in_rsc));
    g_xrdp_pcsc_state |= XRDP_PCSC_STATE_GOT_RSC;
    LLOGLN(0, ("scard_process_read_state_change: timeout %d rv %d",
           in_rsc.timeOut, in_rsc.rv));
    add_timeout(in_rsc.timeOut, scard_read_state_chage_timeout, con);
    return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
scard_process_stop_read_state_change(struct stream *in_s)
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
        out_s = trans_get_out_s(g_con, 8192);
        out_rsc.timeOut = in_rsc.timeOut;
        out_rsc.rv = SCARD_S_SUCCESS;
        out_uint8a(out_s, &out_rsc, sizeof(out_rsc));
        g_xrdp_pcsc_state &= ~XRDP_PCSC_STATE_GOT_RSC;
        s_mark_end(out_s);
        return trans_force_write(g_con);
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
            rv = scard_proess_release_context(in_s);
            break;
        case 0x11: /* CMD_VERSION */
            LLOGLN(0, ("scard_process_msg: CMD_VERSION"));
            rv = scard_process_version(con, in_s);
            break;
        case 0x12: /* CMD_GET_READERS_STATE */
            LLOGLN(0, ("scard_process_msg: CMD_GET_READERS_STATE"));
            rv = scard_process_get_readers_state(in_s);
            break;
        case 0x13: /* CMD_WAIT_READER_STATE_CHANGE */
            LLOGLN(0, ("scard_process_msg: CMD_WAIT_READER_STATE_CHANGE"));
            rv = scard_process_read_state_change(con, in_s);
            break;
        case 0x14: /* CMD_STOP_WAITING_READER_STATE_CHANGE */
            LLOGLN(0, ("scard_process_msg: CMD_STOP_WAITING_READER_STATE_CHANGE"));
            rv = scard_process_stop_read_state_change(in_s);
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
    g_memset(g_reader_states, 0, sizeof(g_reader_states));
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
