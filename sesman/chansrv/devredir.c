/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * xrdp device redirection - only drive redirection is currently supported
 *
 * Copyright (C) Laxmikant Rashinkar 2013 LK.Rashinkar@gmail.com
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

/*
 * TODO
 *      o there is one difference in the protocol initialization
 *        sequence upon reconnection: if a user is already logged on,
 *        send a SERVER_USER_LOGGED_ON msg as per section 3.3.5.1.5
 *
 *      o how to announce / delete a drive after a connection has been
 *        established?
 *
 *      o need to support multiple drives;
 *      o for multiple drives, we cannot have global device_id's and file_id's
 *        and all functions must be reenterant
 *      o replace printf's with log_xxx
 *      o mark local funcs with static
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include "arch.h"
#include "parse.h"
#include "os_calls.h"
#include "string_calls.h"
#include "log.h"
#include "chansrv.h"
#include "chansrv_fuse.h"
#include "devredir.h"
#include "smartcard.h"
#include "ms-rdpefs.h"
#include "ms-smb2.h"
#include "ms-fscc.h"
#include "ms-erref.h"


/* client minor versions */
#define RDP_CLIENT_50                   0x0002
#define RDP_CLIENT_51                   0x0005
#define RDP_CLIENT_52                   0x000a
#define RDP_CLIENT_60_61                0x000c

/* Windows time starts on Jan 1, 1601 */
/* Linux   time starts on Jan 1, 1970 */
#define EPOCH_DIFF 11644473600LL
#define WINDOWS_TO_LINUX_TIME(_t) (((_t) / 10000000) - EPOCH_DIFF);
#define LINUX_TO_WINDOWS_TIME(_t) (((_t) + EPOCH_DIFF) * 10000000)

/*
 * CompletionID types, used in IRPs to indicate I/O operation
 */

enum COMPLETION_TYPE
{
    CID_CREATE_DIR_REQ = 1,
    CID_DIRECTORY_CONTROL,
    CID_CREATE_REQ,
    CID_OPEN_REQ,
    CID_READ,
    CID_WRITE,
    CID_CLOSE,
    CID_FILE_CLOSE,
    CID_RMDIR_OR_FILE,
    CID_RMDIR_OR_FILE_RESP,
    CID_RENAME_FILE,
    CID_RENAME_FILE_RESP,
    CID_LOOKUP,
    CID_SETATTR
};


/* globals */
extern int g_rdpdr_chan_id; /* in chansrv.c */
int g_is_printer_redir_supported = 0;
int g_is_port_redir_supported = 0;
int g_is_drive_redir_supported = 0;
int g_is_smartcard_redir_supported = 0;
int g_drive_redir_version = 1;
char g_full_name_for_filesystem[1024];
tui32 g_completion_id = 1;

tui32 g_clientID;           /* unique client ID - announced by client */
tui32 g_device_id;          /* unique device ID - announced by client */
tui16 g_client_rdp_version; /* returned by client                     */
struct stream *g_input_stream = NULL;

/*
 * Local functions called from devredir_proc_device_iocompletion()
 */
static void devredir_proc_cid_rmdir_or_file(IRP *irp, enum NTSTATUS IoStatus);
static void devredir_proc_cid_rmdir_or_file_resp(IRP *irp,
        enum NTSTATUS IoStatus);
static void devredir_proc_cid_rename_file(IRP *irp, enum NTSTATUS IoStatus);
static void devredir_proc_cid_rename_file_resp(IRP *irp,
        enum NTSTATUS IoStatus);
static void devredir_proc_cid_lookup(  IRP *irp,
                                       struct stream *s_in,
                                       enum NTSTATUS IoStatus);
static void devredir_proc_cid_setattr( IRP *irp,
                                       struct stream *s_in,
                                       enum NTSTATUS IoStatus);
/* Other local functions */
static void devredir_send_server_core_cap_req(void);
static void devredir_send_server_clientID_confirm(void);
static void devredir_send_server_user_logged_on(void);

static void devredir_proc_client_core_cap_resp(struct stream *s);
static void devredir_proc_client_devlist_announce_req(struct stream *s);
static void devredir_proc_client_devlist_remove_req(struct stream *s);
static void devredir_proc_device_iocompletion(struct stream *s);
static void devredir_proc_query_dir_response(IRP *irp,
        struct stream *s_in,
        tui32 DeviceId,
        tui32 CompletionId,
        enum NTSTATUS IoStatus);

static void devredir_cvt_slash(char *path);
static void devredir_cvt_to_unicode(char *unicode, const char *path);
static void devredir_cvt_from_unicode_len(char *path, char *unicode, int len);
static int  devredir_string_ends_with(const char *string, char c);

/*****************************************************************************/
int
devredir_init(void)
{
    struct  stream *s;
    int     bytes;
    tui32 clientID;

    /* get a random number that will act as a unique clientID */
    g_random((char *) &clientID, sizeof(clientID));

    /* setup stream */
    xstream_new(s, 1024);

    /* initiate drive redirection protocol by sending Server Announce Req  */
    xstream_wr_u16_le(s, RDPDR_CTYP_CORE);
    xstream_wr_u16_le(s, PAKID_CORE_SERVER_ANNOUNCE);
    xstream_wr_u16_le(s, 0x0001);  /* server major ver                      */
    xstream_wr_u16_le(s, 0x000C);  /* server minor ver  - pretend 2 b Win 7 */
    xstream_wr_u32_le(s, clientID); /* unique ClientID                      */

    /* send data to client */
    bytes = xstream_len(s);
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);

    xstream_free(s);
    return 0;
}

/*****************************************************************************/
int
devredir_deinit(void)
{
    scard_deinit();
    return 0;
}

/*****************************************************************************/

/*
 * Convert a COMPLETION_TYPE to a string
 */
const char *completion_type_to_str(enum COMPLETION_TYPE cid)
{
    return
        (cid == CID_CREATE_DIR_REQ)     ?  "CID_CREATE_DIR_REQ" :
        (cid == CID_DIRECTORY_CONTROL)  ?  "CID_DIRECTORY_CONTROL" :
        (cid == CID_CREATE_REQ)         ?  "CID_CREATE_REQ" :
        (cid == CID_OPEN_REQ)           ?  "CID_OPEN_REQ" :
        (cid == CID_READ)               ?  "CID_READ" :
        (cid == CID_WRITE)              ?  "CID_WRITE" :
        (cid == CID_CLOSE)              ?  "CID_CLOSE" :
        (cid == CID_FILE_CLOSE)         ?  "CID_FILE_CLOSE" :
        (cid == CID_RMDIR_OR_FILE)      ?  "CID_RMDIR_OR_FILE" :
        (cid == CID_RMDIR_OR_FILE_RESP) ?  "CID_RMDIR_OR_FILE_RESP" :
        (cid == CID_RENAME_FILE)        ?  "CID_RENAME_FILE" :
        (cid == CID_RENAME_FILE_RESP)   ?  "CID_RENAME_FILE_RESP" :
        (cid == CID_LOOKUP)             ?  "CID_LOOKUP" :
        (cid == CID_SETATTR)            ?  "CID_SETATTR" :
        /* default */                      "<unknown>";
};

/*****************************************************************************/

/*
 * Convert Windows permssions to Linux permissions.
 *
 * We can't curently support group or other permissions as separate from the
 * owner (not that there's much point). We'll assume our caller will provide
 * a umask if appropriate
 */
static tui32
WindowsToLinuxFilePerm(tui32 wperm)
{
    tui32 result;
    if (wperm & W_FILE_ATTRIBUTE_DIRECTORY)
    {
        result = S_IFDIR | 0555; /* dirs are always readable and executable */
    }
    else
    {
        result = S_IFREG | 0444; /* files are always readable */
        if (wperm & W_FILE_ATTRIBUTE_SYSTEM)
        {
            result |= 0111;    /* Executable */
        }
    }

    if ((wperm & W_FILE_ATTRIBUTE_READONLY) == 0)
    {
        result |= 0222;
    }

    return result;
}

/*****************************************************************************/
static tui32
LinuxToWindowsFilePerm(tui32 lperm)
{
    tui32 result = 0;

    /* Writeable flag is common to files and directories */
    if ((lperm & S_IWUSR) == 0)
    {
        result |= W_FILE_ATTRIBUTE_READONLY;
    }

    if (lperm & S_IFDIR)
    {
        result |= W_FILE_ATTRIBUTE_DIRECTORY;
    }
    else
    {
        /* For normal files the system attribute is used to store the owner
           executable bit */
        if (lperm & S_IXUSR)
        {
            result |= W_FILE_ATTRIBUTE_SYSTEM;
        }
        if (result == 0)
        {
            /* See MS-FSCC section 2.6 */
            result = W_FILE_ATTRIBUTE_NORMAL;
        }
    }

    return result;
}


/**
 * @brief process incoming data
 *
 * @return 0 on success, -1 on failure
 *****************************************************************************/

int
devredir_data_in(struct stream *s, int chan_id, int chan_flags, int length,
                 int total_length)
{
    struct stream *ls;
    tui16          comp_type;
    tui16          pktID;
    tui16          minor_ver;
    int            rv = 0;

    /*
     * handle packet fragmentation
     */
    if ((chan_flags & 3) == 3)
    {
        /* all data contained in one packet */
        ls = s;
    }
    else
    {
        /* is this is the first packet? */
        if (chan_flags & 1)
        {
            xstream_new(g_input_stream, total_length);
        }

        xstream_copyin(g_input_stream, s->p, length);

        /* in last packet, chan_flags & 0x02 will be true */
        if ((chan_flags & 2) == 0)
        {
            return 0;
        }

        s_mark_end(g_input_stream);
        g_input_stream->p = g_input_stream->data;
        ls = g_input_stream;
    }

    /* read header from incoming data */
    xstream_rd_u16_le(ls, comp_type);
    xstream_rd_u16_le(ls, pktID);

    /* for now we only handle core type, not printers */
    if (comp_type != RDPDR_CTYP_CORE)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "invalid component type in response; expected 0x%x got 0x%x",
                  RDPDR_CTYP_CORE, comp_type);

        rv = -1;
        goto done;
    }

    /* figure out what kind of response we have gotten */
    switch (pktID)
    {
        case PAKID_CORE_CLIENTID_CONFIRM:
            xstream_seek(ls, 2);  /* major version, we ignore it */
            xstream_rd_u16_le(ls, minor_ver);
            xstream_rd_u32_le(ls, g_clientID);

            g_client_rdp_version = minor_ver;

            switch (minor_ver)
            {
                case RDP_CLIENT_50:
                    break;

                case RDP_CLIENT_51:
                    break;

                case RDP_CLIENT_52:
                    break;

                case RDP_CLIENT_60_61:
                    break;
            }
            // LK_TODO devredir_send_server_clientID_confirm();
            break;

        case PAKID_CORE_CLIENT_NAME:
            /* client is telling us its computer name; do we even care? */

            /* let client know login was successful */
            devredir_send_server_user_logged_on();
            usleep(1000 * 100);

            /* let client know our capabilities */
            devredir_send_server_core_cap_req();

            /* send confirm clientID */
            devredir_send_server_clientID_confirm();
            break;

        case PAKID_CORE_CLIENT_CAPABILITY:
            devredir_proc_client_core_cap_resp(ls);
            break;

        case PAKID_CORE_DEVICELIST_ANNOUNCE:
            devredir_proc_client_devlist_announce_req(ls);
            break;

        case PAKID_CORE_DEVICELIST_REMOVE:
            devredir_proc_client_devlist_remove_req(ls);
            break;

        case PAKID_CORE_DEVICE_IOCOMPLETION:
            devredir_proc_device_iocompletion(ls);
            break;

        default:
            LOG_DEVEL(LOG_LEVEL_ERROR, "got unknown response 0x%x", pktID);
            break;
    }

done:

    if (g_input_stream)
    {
        xstream_free(g_input_stream);
        g_input_stream = NULL;
    }

    return rv;
}

/*****************************************************************************/
int
devredir_get_wait_objs(tbus *objs, int *count, int *timeout)
{
    if (g_is_smartcard_redir_supported)
    {
        return scard_get_wait_objs(objs, count, timeout);
    }
    return 0;
}

/*****************************************************************************/
int
devredir_check_wait_objs(void)
{
    if (g_is_smartcard_redir_supported)
    {
        return scard_check_wait_objs();
    }
    return 0;
}

/**
 * @brief let client know our capabilities
 *****************************************************************************/

static void
devredir_send_server_core_cap_req(void)
{
    struct stream *s;
    int            bytes;

    xstream_new(s, 1024);

    /* setup header */
    xstream_wr_u16_le(s, RDPDR_CTYP_CORE);
    xstream_wr_u16_le(s, PAKID_CORE_SERVER_CAPABILITY);

    xstream_wr_u16_le(s, 5);                /* num of caps we are sending     */
    xstream_wr_u16_le(s, 0x0000);           /* padding                        */

    /* setup general capability */
    xstream_wr_u16_le(s, CAP_GENERAL_TYPE); /* CapabilityType                 */
    xstream_wr_u16_le(s, 44);               /* CapabilityLength - len of this */
    /* CAPABILITY_SET in bytes, inc   */
    /* the header                     */
    xstream_wr_u32_le(s, 2);                /* Version                        */
    xstream_wr_u32_le(s, 2);                /* O.S type                       */
    xstream_wr_u32_le(s, 0);                /* O.S version                    */
    xstream_wr_u16_le(s, 1);                /* protocol major version         */
    xstream_wr_u16_le(s, g_client_rdp_version); /* protocol minor version     */
    xstream_wr_u32_le(s, 0xffff);           /* I/O code 1                     */
    xstream_wr_u32_le(s, 0);                /* I/O code 2                     */
    xstream_wr_u32_le(s, 7);                /* Extended PDU                   */
    xstream_wr_u32_le(s, 0);                /* extra flags 1                  */
    xstream_wr_u32_le(s, 0);                /* extra flags 2                  */
    xstream_wr_u32_le(s, 2);                /* special type device cap        */

    /* setup printer capability */
    xstream_wr_u16_le(s, CAP_PRINTER_TYPE);
    xstream_wr_u16_le(s, 8);
    xstream_wr_u32_le(s, 1);

    /* setup serial port capability */
    xstream_wr_u16_le(s, CAP_PORT_TYPE);
    xstream_wr_u16_le(s, 8);
    xstream_wr_u32_le(s, 1);

    /* setup file system capability */
    xstream_wr_u16_le(s, CAP_DRIVE_TYPE);   /* CapabilityType                 */
    xstream_wr_u16_le(s, 8);                /* CapabilityLength - len of this */
    /* CAPABILITY_SET in bytes, inc   */
    /* the header                     */
    xstream_wr_u32_le(s, 2);                /* Version                        */

    /* setup smart card capability */
    xstream_wr_u16_le(s, CAP_SMARTCARD_TYPE);
    xstream_wr_u16_le(s, 8);
    xstream_wr_u32_le(s, 1);

    /* send to client */
    bytes = xstream_len(s);
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);

    xstream_free(s);
}

static void
devredir_send_server_clientID_confirm(void)
{
    struct stream *s;
    int            bytes;

    xstream_new(s, 1024);

    /* setup stream */
    xstream_wr_u16_le(s, RDPDR_CTYP_CORE);
    xstream_wr_u16_le(s, PAKID_CORE_CLIENTID_CONFIRM);
    xstream_wr_u16_le(s, 0x0001);
    xstream_wr_u16_le(s, g_client_rdp_version);
    xstream_wr_u32_le(s, g_clientID);

    /* send to client */
    bytes = xstream_len(s);
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);

    xstream_free(s);
}

static void
devredir_send_server_user_logged_on(void)
{
    struct stream *s;
    int            bytes;

    xstream_new(s, 1024);

    /* setup stream */
    xstream_wr_u16_le(s, RDPDR_CTYP_CORE);
    xstream_wr_u16_le(s, PAKID_CORE_USER_LOGGEDON);

    /* send to client */
    bytes = xstream_len(s);
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);

    xstream_free(s);
}

static void
devredir_send_server_device_announce_resp(tui32 device_id,
        enum NTSTATUS result_code)
{
    struct stream *s;
    int            bytes;

    xstream_new(s, 1024);

    /* setup stream */
    xstream_wr_u16_le(s, RDPDR_CTYP_CORE);
    xstream_wr_u16_le(s, PAKID_CORE_DEVICE_REPLY);
    xstream_wr_u32_le(s, device_id);
    xstream_wr_u32_le(s, (tui32)result_code);

    /* send to client */
    bytes = xstream_len(s);
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);

    xstream_free(s);
}

/**
 * @return 0 on success, -1 on failure
 *****************************************************************************/

static int
devredir_send_drive_create_request(tui32 device_id,
                                   const char *path,
                                   tui32 DesiredAccess,
                                   tui32 CreateOptions,
                                   tui32 FileAttributes,
                                   tui32 CreateDisposition,
                                   tui32 completion_id)
{
    struct stream *s;
    int            bytes;
    int            len;
    tui32          SharedAccess;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "device_id=%d path=\"%s\""
              " DesiredAccess=0x%x CreateDisposition=0x%x"
              " FileAttributes=0x%x CreateOptions=0x%x"
              " CompletionId=%d",
              device_id, path,
              DesiredAccess, CreateDisposition,
              FileAttributes, CreateOptions,
              completion_id);

    /* path in unicode needs this much space */
    len = ((g_mbstowcs(NULL, path, 0) * sizeof(twchar)) / 2) + 2;

    xstream_new(s, 1024 + len);

    /* FILE_SHARE_DELETE allows files to be renamed while in use
       (in some circumstances) */
    SharedAccess = SA_FILE_SHARE_READ | SA_FILE_SHARE_WRITE |
                   SA_FILE_SHARE_DELETE;

    devredir_insert_DeviceIoRequest(s,
                                    device_id,
                                    0,
                                    completion_id,
                                    IRP_MJ_CREATE,
                                    IRP_MN_NONE);

    xstream_wr_u32_le(s, DesiredAccess);     /* DesiredAccess              */
    xstream_wr_u32_le(s, 0);                 /* AllocationSize high unused */
    xstream_wr_u32_le(s, 0);                 /* AllocationSize low  unused */
    xstream_wr_u32_le(s, FileAttributes);    /* FileAttributes             */
    xstream_wr_u32_le(s, SharedAccess);      /* SharedAccess               */
    xstream_wr_u32_le(s, CreateDisposition); /* CreateDisposition          */
    xstream_wr_u32_le(s, CreateOptions);     /* CreateOptions              */
    xstream_wr_u32_le(s, len);               /* PathLength                 */
    devredir_cvt_to_unicode(s->p, path);     /* path in unicode            */
    xstream_seek(s, len);

    /* send to client */
    bytes = xstream_len(s);
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);

    xstream_free(s);
    return 0;
}

/**
 * Close a request previously created by devredir_send_drive_create_request()
 *****************************************************************************/

static int
devredir_send_drive_close_request(tui16 Component, tui16 PacketId,
                                  tui32 DeviceId,
                                  tui32 FileId,
                                  tui32 CompletionId,
                                  enum IRP_MJ MajorFunction,
                                  enum IRP_MN MinorFunc,
                                  int pad_len)
{
    struct stream *s;
    int            bytes;

    xstream_new(s, 1024);

    devredir_insert_DeviceIoRequest(s, DeviceId, FileId, CompletionId,
                                    MajorFunction, MinorFunc);

    if (pad_len)
    {
        xstream_seek(s, pad_len);
    }

    /* send to client */
    bytes = xstream_len(s);
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);

    xstream_free(s);
    LOG_DEVEL(LOG_LEVEL_DEBUG, "sent close request; expect CID_FILE_CLOSE");
    return 0;
}

/**
 * @brief ask client to enumerate directory
 *
 * Server Drive Query Directory Request
 * DR_DRIVE_QUERY_DIRECTORY_REQ
 *
 *****************************************************************************/
// LK_TODO Path needs to be Unicode
static void
devredir_send_drive_dir_request(IRP *irp, tui32 device_id,
                                tui32 InitialQuery, char *Path)
{
    struct stream *s;
    int            bytes;
    char           upath[4096]; // LK_TODO need to malloc this
    int            path_len = 0;

    /* during Initial Query, Path cannot be NULL */
    if (InitialQuery)
    {
        if (Path == NULL)
        {
            return;
        }

        /* Path in unicode needs this much space */
        path_len = ((g_mbstowcs(NULL, Path, 0) * sizeof(twchar)) / 2) + 2;
        devredir_cvt_to_unicode(upath, Path);
    }

    xstream_new(s, 1024 + path_len);

    irp->completion_type = CID_DIRECTORY_CONTROL;
    devredir_insert_DeviceIoRequest(s,
                                    device_id,
                                    irp->FileId,
                                    irp->CompletionId,
                                    IRP_MJ_DIRECTORY_CONTROL,
                                    IRP_MN_QUERY_DIRECTORY);

    xstream_wr_u32_le(s, FileDirectoryInformation);  /* FsInformationClass */
    xstream_wr_u8(s, InitialQuery);                  /* InitialQuery       */

    if (!InitialQuery)
    {
        xstream_wr_u32_le(s, 0);                     /* PathLength         */
        xstream_seek(s, 23);
    }
    else
    {
        xstream_wr_u32_le(s, path_len);              /* PathLength         */
        xstream_seek(s, 23);                         /* Padding            */
        xstream_wr_string(s, upath, path_len);
    }

    /* send to client */
    bytes = xstream_len(s);
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);

    xstream_free(s);
}

/******************************************************************************
**                   process data received from client                       **
******************************************************************************/

/**
 * @brief process client's response to our core_capability_req() msg
 *
 * @param   s   stream containing client's response
 *****************************************************************************/
static void
devredir_proc_client_core_cap_resp(struct stream *s)
{
    int i;
    tui16 num_caps;
    tui16 cap_type;
    tui16 cap_len;
    tui32 cap_version;
    char *holdp;

    xstream_rd_u16_le(s, num_caps);
    xstream_seek(s, 2);  /* padding */

    for (i = 0; i < num_caps; i++)
    {
        holdp = s->p;
        xstream_rd_u16_le(s, cap_type);
        xstream_rd_u16_le(s, cap_len);
        xstream_rd_u32_le(s, cap_version);

        switch (cap_type)
        {
            case CAP_GENERAL_TYPE:
                LOG_DEVEL(LOG_LEVEL_DEBUG, "got CAP_GENERAL_TYPE");
                break;

            case CAP_PRINTER_TYPE:
                LOG_DEVEL(LOG_LEVEL_DEBUG, "got CAP_PRINTER_TYPE");
                g_is_printer_redir_supported = 1;
                break;

            case CAP_PORT_TYPE:
                LOG_DEVEL(LOG_LEVEL_DEBUG, "got CAP_PORT_TYPE");
                g_is_port_redir_supported = 1;
                break;

            case CAP_DRIVE_TYPE:
                LOG_DEVEL(LOG_LEVEL_DEBUG, "got CAP_DRIVE_TYPE");
                g_is_drive_redir_supported = 1;
                if (cap_version == 2)
                {
                    g_drive_redir_version = 2;
                }
                break;

            case CAP_SMARTCARD_TYPE:
                LOG_DEVEL(LOG_LEVEL_DEBUG, "got CAP_SMARTCARD_TYPE");
                g_is_smartcard_redir_supported = 1;
                scard_init();
                break;
        }
        s->p = holdp + cap_len;
    }
}

static void
devredir_proc_client_devlist_announce_req(struct stream *s)
{
    unsigned int i;
    int   j;
    tui32 device_count;
    tui32 device_type;
    tui32 device_data_len;
    char  preferred_dos_name[9];
    enum NTSTATUS response_status;

    /* get number of devices being announced */
    xstream_rd_u32_le(s, device_count);

    LOG_DEVEL(LOG_LEVEL_DEBUG, "num of devices announced: %d", device_count);

    for (i = 0; i < device_count; i++)
    {
        xstream_rd_u32_le(s, device_type);
        xstream_rd_u32_le(s, g_device_id);
        /* get preferred DOS name
         * DOS names that are 8 chars long are not NULL terminated */
        for (j = 0; j < 8; j++)
        {
            preferred_dos_name[j] = *s->p++;
        }
        preferred_dos_name[8] = 0;

        /* Assume this device isn't supported by us */
        response_status = STATUS_NOT_SUPPORTED;

        switch (device_type)
        {
            case RDPDR_DTYP_FILESYSTEM:
                /* get device data len */
                xstream_rd_u32_le(s, device_data_len);
                if (device_data_len)
                {
                    xstream_rd_string(g_full_name_for_filesystem, s,
                                      device_data_len);
                }

                LOG_DEVEL(LOG_LEVEL_DEBUG, "device_type=FILE_SYSTEM device_id=0x%x dosname=%s "
                          "device_data_len=%d full_name=%s", g_device_id,
                          preferred_dos_name,
                          device_data_len, g_full_name_for_filesystem);

                response_status = STATUS_SUCCESS;

                /* create share directory in xrdp file system;    */
                /* think of this as the mount point for this share */
                xfuse_create_share(g_device_id, preferred_dos_name);
                break;

            case RDPDR_DTYP_SMARTCARD:
                /* for smart cards, device data len always 0 */

                LOG_DEVEL(LOG_LEVEL_DEBUG, "device_type=SMARTCARD device_id=0x%x dosname=%s",
                          g_device_id, preferred_dos_name);

                response_status = STATUS_SUCCESS;

                scard_device_announce(g_device_id);
                break;

            case RDPDR_DTYP_SERIAL:
                LOG_DEVEL(LOG_LEVEL_DEBUG,
                          "device_type=SERIAL device_id=0x%x dosname=%s",
                          g_device_id, preferred_dos_name);
                break;

            case RDPDR_DTYP_PARALLEL:
                LOG_DEVEL(LOG_LEVEL_DEBUG,
                          "device_type=PARALLEL device_id=0x%x dosname=%s",
                          g_device_id, preferred_dos_name);
                break;

            case RDPDR_DTYP_PRINT:
                LOG_DEVEL(LOG_LEVEL_DEBUG,
                          "device_type=PRINT device_id=0x%x dosname=%s",
                          g_device_id, preferred_dos_name);
                break;

            default:
                LOG_DEVEL(LOG_LEVEL_DEBUG,
                          "device_type=UNKNOWN device_id=0x%x dosname=%s",
                          g_device_id, preferred_dos_name);
                break;
        }

        /* Tell the client wheth or not we're supporting this one */
        devredir_send_server_device_announce_resp(g_device_id,
                response_status);
    }
}

static void
devredir_proc_client_devlist_remove_req(struct stream *s)
{
    unsigned int i;
    tui32 device_count;
    tui32 device_id;

    /* get number of devices being announced */
    xstream_rd_u32_le(s, device_count);

    LOG_DEVEL(LOG_LEVEL_DEBUG, "num of devices removed: %d", device_count);
    {
        for (i = 0; i < device_count; i++)
        {
            xstream_rd_u32_le(s, device_id);
            xfuse_delete_share(device_id);
        }
    }
}

static void
devredir_proc_device_iocompletion(struct stream *s)
{
    IRP       *irp       = NULL;

    tui32      DeviceId;
    tui32      CompletionId;
    tui32      IoStatus32;
    tui32      Length;
    enum COMPLETION_TYPE comp_type;

    xstream_rd_u32_le(s, DeviceId);
    xstream_rd_u32_le(s, CompletionId);
    xstream_rd_u32_le(s, IoStatus32);
    enum NTSTATUS IoStatus = (enum NTSTATUS) IoStatus32; /* Needed by C++ */

    if ((irp = devredir_irp_find(CompletionId)) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "IRP with completion ID %d not found", CompletionId);
    }
    else if (irp->callback)
    {
        /* Callback has been set -  call it */
        (*irp->callback)(s, irp, DeviceId, CompletionId, IoStatus);
    }
    else
    {
        comp_type = (enum COMPLETION_TYPE) irp->completion_type;
        /* Log something about the IRP */
        if (IoStatus == STATUS_SUCCESS ||
                IoStatus == STATUS_NO_MORE_FILES ||
                (IoStatus == STATUS_NO_SUCH_FILE && comp_type == CID_LOOKUP))
        {
            /* Successes or common occurrences - debug logging only */
            LOG_DEVEL(LOG_LEVEL_DEBUG, "got %s", completion_type_to_str(comp_type));
        }
        else
        {
            LOG_DEVEL(LOG_LEVEL_ERROR, "CompletionType = %s, IoStatus=%08x "
                      "Pathname = %s",
                      completion_type_to_str(comp_type),
                      IoStatus,
                      (irp->pathname) ? irp->pathname : "<none>");
        }

        switch (comp_type)
        {
            case CID_CREATE_DIR_REQ:
                if (IoStatus != STATUS_SUCCESS)
                {
                    xfuse_devredir_cb_enum_dir_done(
                        (struct state_dirscan *) irp->fuse_info, IoStatus);
                    devredir_irp_delete(irp);
                }
                else
                {
                    xstream_rd_u32_le(s, irp->FileId);
                    devredir_send_drive_dir_request(irp, DeviceId,
                                                    1, irp->pathname);
                }
                break;

            case CID_CREATE_REQ:
                xstream_rd_u32_le(s, irp->FileId);

                xfuse_devredir_cb_create_file(
                    (struct state_create *) irp->fuse_info,
                    IoStatus, DeviceId, irp->FileId);
                if (irp->gen.create.creating_dir || IoStatus != STATUS_SUCCESS)
                {
                    devredir_irp_delete(irp);
                }
                break;

            case CID_OPEN_REQ:
                xstream_rd_u32_le(s, irp->FileId);

                xfuse_devredir_cb_open_file((struct state_open *) irp->fuse_info,
                                            IoStatus, DeviceId, irp->FileId);
                if (IoStatus != STATUS_SUCCESS)
                {
                    devredir_irp_delete(irp);
                }
                break;

            case CID_READ:
                xstream_rd_u32_le(s, Length);
                xfuse_devredir_cb_read_file((struct state_read *) irp->fuse_info,
                                            IoStatus,
                                            s->p, Length);
                devredir_irp_delete(irp);
                break;

            case CID_WRITE:
                xstream_rd_u32_le(s, Length);
                xfuse_devredir_cb_write_file((struct state_write *) irp->fuse_info,
                                             IoStatus,
                                             irp->gen.write.offset, Length);
                devredir_irp_delete(irp);
                break;

            case CID_CLOSE:
                devredir_irp_delete(irp);
                break;

            case CID_FILE_CLOSE:
                xfuse_devredir_cb_file_close((struct state_close *) irp->fuse_info);
                devredir_irp_delete(irp);
                break;

            case CID_DIRECTORY_CONTROL:
                devredir_proc_query_dir_response(irp, s, DeviceId,
                                                 CompletionId, IoStatus);
                break;

            case CID_RMDIR_OR_FILE:
                xstream_rd_u32_le(s, irp->FileId);
                devredir_proc_cid_rmdir_or_file(irp, IoStatus);
                break;

            case CID_RMDIR_OR_FILE_RESP:
                devredir_proc_cid_rmdir_or_file_resp(irp, IoStatus);
                break;

            case CID_RENAME_FILE:
                xstream_rd_u32_le(s, irp->FileId);
                devredir_proc_cid_rename_file(irp, IoStatus);
                break;

            case CID_RENAME_FILE_RESP:
                devredir_proc_cid_rename_file_resp(irp, IoStatus);
                break;

            case CID_LOOKUP:
                devredir_proc_cid_lookup(irp, s, IoStatus);
                break;

            case CID_SETATTR:
                devredir_proc_cid_setattr(irp, s, IoStatus);
                break;

            default:
                LOG_DEVEL(LOG_LEVEL_ERROR, "got unknown CompletionID: DeviceId=0x%x "
                          "CompletionId=0x%x IoStatus=0x%x",
                          DeviceId, CompletionId, IoStatus);
                break;
        }
    }
}

static void
devredir_proc_query_dir_response(IRP *irp,
                                 struct stream *s_in,
                                 tui32 DeviceId,
                                 tui32 CompletionId,
                                 enum NTSTATUS IoStatus)
{
    tui32 Length;
    xstream_rd_u32_le(s_in, Length);

    if (IoStatus == STATUS_SUCCESS)
    {
        unsigned int i;
        /* process FILE_DIRECTORY_INFORMATION structures */
        for (i = 0 ; i < Length ; ++i)
        {
            char  filename[256];
            tui64 LastAccessTime;
            tui64 LastWriteTime;
            tui64 EndOfFile;
            tui32 FileAttributes;
            tui32 FileNameLength;
            struct file_attr fattr;

            xstream_seek(s_in, 4);  /* NextEntryOffset */
            xstream_seek(s_in, 4);  /* FileIndex */
            xstream_seek(s_in, 8);  /* CreationTime */
            xstream_rd_u64_le(s_in, LastAccessTime);
            xstream_rd_u64_le(s_in, LastWriteTime);
            xstream_seek(s_in, 8);  /* ChangeTime */
            xstream_rd_u64_le(s_in, EndOfFile);
            xstream_seek(s_in, 8);  /* AllocationSize */
            xstream_rd_u32_le(s_in, FileAttributes);
            xstream_rd_u32_le(s_in, FileNameLength);

            devredir_cvt_from_unicode_len(filename, s_in->p, FileNameLength);

            i += 64 + FileNameLength;

            //LOG_DEVEL(LOG_LEVEL_DEBUG, "LastAccessTime:    0x%llx", LastAccessTime);
            //LOG_DEVEL(LOG_LEVEL_DEBUG, "LastWriteTime:     0x%llx", LastWriteTime);
            //LOG_DEVEL(LOG_LEVEL_DEBUG, "EndOfFile:         %lld", EndOfFile);
            //LOG_DEVEL(LOG_LEVEL_DEBUG, "FileAttributes:    0x%x", FileAttributes);
            //LOG_DEVEL(LOG_LEVEL_DEBUG, "FileNameLength:    %d", FileNameLength);
            LOG_DEVEL(LOG_LEVEL_DEBUG, "FileName:          %s", filename);

            fattr.mode = WindowsToLinuxFilePerm(FileAttributes);
            fattr.size = (size_t) EndOfFile;
            fattr.atime = WINDOWS_TO_LINUX_TIME(LastAccessTime);
            fattr.mtime = WINDOWS_TO_LINUX_TIME(LastWriteTime);

            /* add this entry to xrdp file system */
            xfuse_devredir_cb_enum_dir_add_entry(
                (struct state_dirscan *) irp->fuse_info,
                filename, &fattr);
        }

        /* Ask for more directory entries */
        devredir_send_drive_dir_request(irp, DeviceId, 0, NULL);
    }
    else
    {
        if (IoStatus == STATUS_NO_MORE_FILES)
        {
            IoStatus = STATUS_SUCCESS;
        }
        xfuse_devredir_cb_enum_dir_done((struct state_dirscan *)irp->fuse_info,
                                        IoStatus);
        irp->completion_type = CID_CLOSE;
        devredir_send_drive_close_request(RDPDR_CTYP_CORE,
                                          PAKID_CORE_DEVICE_IOREQUEST,
                                          DeviceId,
                                          irp->FileId,
                                          irp->CompletionId,
                                          IRP_MJ_CLOSE, IRP_MN_NONE, 32);
    }
}

/**
 * FUSE calls this function whenever it wants us to enumerate a dir
 *
 * @param fusep     opaque data struct that we just pass back to FUSE when done
 * @param device_id device_id of the redirected share
 * @param path      the dir path to enumerate
 *
 * @return 0 on success, -1 on failure
 *****************************************************************************/

int
devredir_get_dir_listing(struct state_dirscan *fusep, tui32 device_id,
                         const char *path)
{
    tui32  DesiredAccess;
    tui32  CreateOptions;
    tui32  CreateDisposition;
    int    rval = -1;
    IRP   *irp;

    /*
     * We need to be able to append two additional characters to the
     * path after we create the IRP
     */
    if ((irp = devredir_irp_with_pathnamelen_new(strlen(path) + 2)) != NULL)
    {
        /* convert / to windows compatible \ */
        strcpy(irp->pathname, path);
        devredir_cvt_slash(irp->pathname);

        irp->CompletionId = g_completion_id++;
        irp->completion_type = CID_CREATE_DIR_REQ;
        irp->DeviceId = device_id;
        irp->fuse_info = fusep;

        DesiredAccess = DA_FILE_READ_DATA | DA_SYNCHRONIZE;
        CreateOptions = CO_FILE_DIRECTORY_FILE |
                        CO_FILE_SYNCHRONOUS_IO_NONALERT;
        CreateDisposition = CD_FILE_OPEN;

        rval = devredir_send_drive_create_request(device_id, irp->pathname,
                DesiredAccess, CreateOptions,
                0, CreateDisposition,
                irp->CompletionId);

        LOG_DEVEL(LOG_LEVEL_DEBUG, "looking for device_id=%d path=%s", device_id, irp->pathname);

        /* when we get a response to devredir_send_drive_create_request(), we
         * call devredir_send_drive_dir_request(), which needs the following
         * at the end of the path argument */
        if (devredir_string_ends_with(irp->pathname, '\\'))
        {
            strcat(irp->pathname, "*");
        }
        else
        {
            strcat(irp->pathname, "\\*");
        }
    }
    return rval;
}

/**
 * FUSE calls this function whenever it wants us to lookup a file or directory
 *
 * @param fusep     opaque data struct that we just pass back to FUSE when done
 * @param device_id device_id of the redirected share
 * @param path      the name of the directory containing the file
 * @param file      the filename
 *
 * @return 0 on success, -1 on failure
 *****************************************************************************/

int
devredir_lookup_entry(struct state_lookup *fusep, tui32 device_id,
                      const char *path)
{
    tui32  DesiredAccess;
    tui32  CreateOptions;
    tui32  CreateDisposition;
    int    rval = -1;
    IRP   *irp;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "fusep=%p", fusep);

    if ((irp = devredir_irp_with_pathname_new(path)) != NULL)
    {
        /* convert / to windows compatible \ */
        devredir_cvt_slash(irp->pathname);

        /*
         * Allocate an IRP to open the file, read the basic attributes,
         * read the standard attributes, and then close the file
         */
        irp->CompletionId = g_completion_id++;
        irp->completion_type = CID_LOOKUP;
        irp->DeviceId = device_id;
        irp->gen.lookup.state = E_LOOKUP_GET_FH;
        irp->fuse_info = fusep;

        DesiredAccess = DA_FILE_READ_ATTRIBUTES | DA_SYNCHRONIZE;
        CreateOptions = 0;
        CreateDisposition = CD_FILE_OPEN;

        LOG_DEVEL(LOG_LEVEL_DEBUG, "lookup for device_id=%d path=%s CompletionId=%d",
                  device_id, irp->pathname, irp->CompletionId);

        rval = devredir_send_drive_create_request(device_id,
                irp->pathname,
                DesiredAccess, CreateOptions,
                0, CreateDisposition,
                irp->CompletionId);
    }

    return rval;
}

/**
 * FUSE calls this function whenever it wants us to set the attributes for
 * a file or directory.
 *
 * @param fusep     opaque data struct that we just pass back to FUSE when done
 * @param device_id device_id of the redirected share
 * @param filename  the name of the file
 * @param fattr     the file attributes to set for the file
 * @param to_set    Which bits of the file attributes have changed
 *
 * @return 0 on success, -1 on failure
 *****************************************************************************/
int
devredir_setattr_for_entry(struct state_setattr *fusep, tui32 device_id,
                           const char *filename,
                           const struct file_attr *fattr,
                           tui32 to_set)
{
    tui32  DesiredAccess;
    tui32  CreateOptions;
    tui32  CreateDisposition;
    int    rval = -1;
    IRP   *irp;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "fusep=%p", fusep);

    if ((irp = devredir_irp_with_pathname_new(filename)) != NULL)
    {
        /* convert / to windows compatible \ */
        devredir_cvt_slash(irp->pathname);

        /*
         * Allocate an IRP to open the file, update the attributes
         * and close the file.
         */
        irp->CompletionId = g_completion_id++;
        irp->completion_type = CID_SETATTR;
        irp->DeviceId = device_id;
        irp->fuse_info = fusep;

        irp->gen.setattr.state = E_SETATTR_GET_FH;
        irp->gen.setattr.to_set = to_set;
        irp->gen.setattr.fattr = *fattr;

        /*
         * Don't set DA_FILE_WRITE_DATA unless we're changing the
         * EndOfFile pointer. Otherwise we can't change the attributes
         * of read-only files! */
        DesiredAccess = DA_FILE_WRITE_ATTRIBUTES;
        if (to_set & TO_SET_SIZE)
        {
            DesiredAccess |= DA_FILE_WRITE_DATA;
        }
        CreateOptions = 0;
        CreateDisposition = CD_FILE_OPEN;

        LOG_DEVEL(LOG_LEVEL_DEBUG, "lookup for device_id=%d path=%s",
                  device_id, irp->pathname);

        rval = devredir_send_drive_create_request(device_id,
                irp->pathname,
                DesiredAccess, CreateOptions,
                0, CreateDisposition,
                irp->CompletionId);
    }

    return rval;
}

int
devredir_file_create(struct state_create *fusep, tui32 device_id,
                     const char *path, int mode)
{
    tui32  DesiredAccess;
    tui32  CreateOptions;
    tui32  FileAttributes = 0;
    tui32  CreateDisposition;
    int    rval = -1;
    IRP   *irp;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "device_id=%d path=%s mode=0%o", device_id, path, mode);

    if ((irp = devredir_irp_with_pathname_new(path)) != NULL)
    {
        /* convert / to windows compatible \ */
        devredir_cvt_slash(irp->pathname);

        irp->completion_type = CID_CREATE_REQ;
        irp->CompletionId = g_completion_id++;
        irp->DeviceId = device_id;
        irp->fuse_info = fusep;


        DesiredAccess = 0x0016019f; /* got this value from windows */
        FileAttributes = LinuxToWindowsFilePerm(mode);
        if (mode & S_IFDIR)
        {
            LOG_DEVEL(LOG_LEVEL_DEBUG, "creating dir");
            CreateOptions = CO_FILE_DIRECTORY_FILE | CO_FILE_SYNCHRONOUS_IO_NONALERT;
            irp->gen.create.creating_dir = 1;
        }
        else
        {
            LOG_DEVEL(LOG_LEVEL_DEBUG, "creating file");
            CreateOptions = 0x44; /* got this value from windows */
            irp->gen.create.creating_dir = 0;
        }

        //CreateDisposition = CD_FILE_CREATE;
        CreateDisposition  = 0x02; /* got this value from windows */

        rval = devredir_send_drive_create_request(device_id, path,
                DesiredAccess, CreateOptions,
                FileAttributes,
                CreateDisposition,
                irp->CompletionId);
    }

    return rval;
}

int
devredir_file_open(struct state_open *fusep, tui32 device_id,
                   const char *path, int flags)
{
    tui32  DesiredAccess;
    tui32  CreateOptions;
    tui32  FileAttributes = 0;
    tui32  CreateDisposition;
    int    rval = -1;
    IRP   *irp;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "device_id=%d path=%s flags=0%x",
              device_id, path, flags);

    if ((irp = devredir_irp_with_pathname_new(path)) != NULL)
    {
        /* convert / to windows compatible \ */
        devredir_cvt_slash(irp->pathname);

        irp->completion_type = CID_OPEN_REQ;
        irp->CompletionId = g_completion_id++;
        irp->DeviceId = device_id;

        irp->fuse_info = fusep;

        switch (flags & O_ACCMODE)
        {
            case O_RDONLY:
                LOG_DEVEL(LOG_LEVEL_DEBUG, "open file in O_RDONLY");
                DesiredAccess = DA_FILE_READ_DATA | DA_SYNCHRONIZE;
                break;

            case O_WRONLY:
                LOG_DEVEL(LOG_LEVEL_DEBUG, "open file in O_WRONLY");
                DesiredAccess = DA_FILE_WRITE_DATA | DA_SYNCHRONIZE;
                break;

            default:
                /*
                 * The access mode could conceivably be invalid here,
                 * but we assume this has been checked by the caller
                 */
                LOG_DEVEL(LOG_LEVEL_DEBUG, "open file in O_RDWR");
                /* without the 0x00000010 rdesktop opens files in */
                /* O_RDONLY instead of O_RDWR mode                */
                DesiredAccess = DA_FILE_READ_DATA | DA_FILE_WRITE_DATA |
                                DA_SYNCHRONIZE | 0x00000010;
        }

        CreateOptions = CO_FILE_SYNCHRONOUS_IO_NONALERT;
        CreateDisposition = CD_FILE_OPEN; // WAS 1

        rval = devredir_send_drive_create_request(device_id, path,
                DesiredAccess, CreateOptions,
                FileAttributes,
                CreateDisposition,
                irp->CompletionId);
    }

    return rval;
}

int devredir_file_close(struct state_close *fusep, tui32 device_id,
                        tui32 FileId)
{
    IRP *irp;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "entered: fusep=%p device_id=%d FileId=%d",
              fusep, device_id, FileId);

#if 0
    if ((irp = devredir_irp_new()) == NULL)
    {
        return -1;
    }

    irp->CompletionId = g_completion_id++;
#else
    if ((irp = devredir_irp_find_by_fileid(FileId)) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "no IRP found with FileId = %d", FileId);
        return -1;
    }
#endif
    irp->completion_type = CID_FILE_CLOSE;
    irp->DeviceId = device_id;
    irp->fuse_info = fusep;

    return devredir_send_drive_close_request(RDPDR_CTYP_CORE,
            PAKID_CORE_DEVICE_IOREQUEST,
            device_id,
            FileId,
            irp->CompletionId,
            IRP_MJ_CLOSE,
            IRP_MN_NONE, 32);
}

/**
 * Remove (delete) a directory or file
 *****************************************************************************/

int
devredir_rmdir_or_file(struct state_remove *fusep, tui32 device_id,
                       const char *path)
{
    tui32  DesiredAccess;
    tui32  CreateOptions;
    tui32  CreateDisposition;
    int    rval = -1;
    IRP   *irp;

    if ((irp = devredir_irp_with_pathname_new(path)) != NULL)
    {
        /* convert / to windows compatible \ */
        devredir_cvt_slash(irp->pathname);

        irp->CompletionId = g_completion_id++;
        irp->completion_type = CID_RMDIR_OR_FILE;
        irp->DeviceId = device_id;

        irp->fuse_info = fusep;

        //DesiredAccess = DA_DELETE | DA_FILE_READ_ATTRIBUTES | DA_SYNCHRONIZE;
        DesiredAccess = 0x00100080; /* got this value from windows */

        //CreateOptions = CO_FILE_DELETE_ON_CLOSE | CO_FILE_DIRECTORY_FILE |
        //                CO_FILE_SYNCHRONOUS_IO_NONALERT;
        CreateOptions = 0x020; /* got this value from windows */

        //CreateDisposition = CD_FILE_OPEN; // WAS 1
        CreateDisposition = 0x01; /* got this value from windows */

        rval = devredir_send_drive_create_request(device_id, path,
                DesiredAccess, CreateOptions,
                0, CreateDisposition,
                irp->CompletionId);
    }

    return rval;
}

/**
 * Read data from previously opened file
 *
 * Errors are reported via xfuse_devredir_cb_read_file()
 *****************************************************************************/

void
devredir_file_read(struct state_read *fusep, tui32 DeviceId, tui32 FileId,
                   tui32 Length, tui64 Offset)
{
    struct stream *s;
    IRP           *irp;
    IRP           *new_irp;
    int            bytes;

    xstream_new(s, 1024);

    /* Check we've got an open IRP for this file already */
    if ((irp = devredir_irp_find_by_fileid(FileId)) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "no IRP found with FileId = %d", FileId);
        xfuse_devredir_cb_read_file(fusep, STATUS_UNSUCCESSFUL, NULL, 0);
        xstream_free(s);
    }
    /* create a new IRP for this request */
    else if ((new_irp = devredir_irp_new()) == NULL)
    {
        /* system out of memory */
        xfuse_devredir_cb_read_file(fusep, STATUS_UNSUCCESSFUL, NULL, 0);
        xstream_free(s);
    }
    else
    {
        new_irp->DeviceId = DeviceId;
        new_irp->FileId = FileId;
        new_irp->completion_type = CID_READ;
        new_irp->CompletionId = g_completion_id++;
        new_irp->fuse_info = fusep;

        devredir_insert_DeviceIoRequest(s,
                                        DeviceId,
                                        FileId,
                                        new_irp->CompletionId,
                                        IRP_MJ_READ,
                                        IRP_MN_NONE);

        xstream_wr_u32_le(s, Length);
        xstream_wr_u64_le(s, Offset);
        xstream_seek(s, 20);

        /* send to client */
        bytes = xstream_len(s);
        send_channel_data(g_rdpdr_chan_id, s->data, bytes);
        xstream_free(s);
    }
}

/**
 * Read data from previously opened file
 *
 * Errors are reported via xfuse_devredir_cb_write_file()
 *****************************************************************************/

void
devredir_file_write(struct state_write *fusep, tui32 DeviceId, tui32 FileId,
                    const char *buf, int Length, tui64 Offset)
{
    struct stream *s;
    IRP           *irp;
    IRP           *new_irp;
    int            bytes;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "DeviceId=%d FileId=%d Length=%d Offset=%lld",
              DeviceId, FileId, Length, (long long)Offset);

    xstream_new(s, 1024 + Length);

    if ((irp = devredir_irp_find_by_fileid(FileId)) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "no IRP found with FileId = %d", FileId);
        xfuse_devredir_cb_write_file(fusep, STATUS_UNSUCCESSFUL, 0, 0);
        xstream_free(s);
    }
    /* create a new IRP for this request */
    else if ((new_irp = devredir_irp_new()) == NULL)
    {
        /* system out of memory */
        xfuse_devredir_cb_write_file(fusep, STATUS_UNSUCCESSFUL, 0, 0);
        xstream_free(s);
    }
    else
    {
        new_irp->DeviceId = DeviceId;
        new_irp->FileId = FileId;
        new_irp->completion_type = CID_WRITE;
        new_irp->CompletionId = g_completion_id++;
        new_irp->fuse_info = fusep;
        /* Offset needed after write to calculate new EOF */
        new_irp->gen.write.offset = Offset;

        devredir_insert_DeviceIoRequest(s,
                                        DeviceId,
                                        FileId,
                                        new_irp->CompletionId,
                                        IRP_MJ_WRITE,
                                        IRP_MN_NONE);

        xstream_wr_u32_le(s, Length);
        xstream_wr_u64_le(s, Offset);
        xstream_seek(s, 20); /* padding */

        /* now insert real data */
        xstream_copyin(s, buf, Length);

        /* send to client */
        bytes = xstream_len(s);
        send_channel_data(g_rdpdr_chan_id, s->data, bytes);
        xstream_free(s);
    }

}


int devredir_file_rename(struct state_rename *fusep, tui32 device_id,
                         const char *old_name,
                         const char *new_name)
{
    tui32  DesiredAccess;
    tui32  CreateOptions;
    tui32  FileAttributes = 0;
    tui32  CreateDisposition;
    int    rval = -1;
    IRP   *irp;
    unsigned int len;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "device_id=%d old_name=%s new_name=%s",
              device_id, old_name, new_name);

    /*
     * Allocate an IRP with enough space for both the old and new names.
     * We'll store the new name after the old name:-
     *
     *                 | n | a | m | e | 1 | \0 | n | a | m | e | 2 | \0 |
     *                   ^                        ^
     * irp->pathname ----+                        |
     * irp->gen.rename.new_name ------------------+
     */
    len = strlen(old_name) + 1 + strlen(new_name);
    if ((irp = devredir_irp_with_pathnamelen_new(len)) != NULL)
    {
        /* Set up pointer to new name string */
        irp->gen.rename.new_name = irp->pathname + strlen(old_name) + 1;

        /* Copy both strings, and change'/' to '\\' characters */
        strcpy(irp->pathname, old_name);
        devredir_cvt_slash(irp->pathname);
        strcpy(irp->gen.rename.new_name, new_name);
        devredir_cvt_slash(irp->gen.rename.new_name);

        irp->completion_type = CID_RENAME_FILE;
        irp->CompletionId = g_completion_id++;
        irp->DeviceId = device_id;

        irp->fuse_info = fusep;

        DesiredAccess = DA_FILE_WRITE_ATTRIBUTES | DA_DELETE;
        CreateOptions = 0;
        CreateDisposition = CD_FILE_OPEN; // WAS 1

        rval = devredir_send_drive_create_request(device_id, old_name,
                DesiredAccess, CreateOptions,
                FileAttributes,
                CreateDisposition,
                irp->CompletionId);
    }

    return rval;
}


/******************************************************************************
**                           miscellaneous stuff                             **
******************************************************************************/

void
devredir_insert_DeviceIoRequest(struct stream *s,
                                tui32 DeviceId,
                                tui32 FileId,
                                tui32 CompletionId,
                                enum IRP_MJ MajorFunction,
                                enum IRP_MN MinorFunction)
{
    /* setup DR_DEVICE_IOREQUEST header */
    xstream_wr_u16_le(s, RDPDR_CTYP_CORE);
    xstream_wr_u16_le(s, PAKID_CORE_DEVICE_IOREQUEST);
    xstream_wr_u32_le(s, DeviceId);
    xstream_wr_u32_le(s, FileId);
    xstream_wr_u32_le(s, CompletionId);
    xstream_wr_u32_le(s, MajorFunction);
    xstream_wr_u32_le(s, MinorFunction);
}

/**
 * Convert / to windows compatible \
 *****************************************************************************/

static void
devredir_cvt_slash(char *path)
{
    char *cptr = path;

    while (*cptr != 0)
    {
        if (*cptr == '/')
        {
            *cptr = '\\';
        }
        cptr++;
    }
}

static void
devredir_cvt_to_unicode(char *unicode, const char *path)
{
    char *dest;
    char *src;
    int   rv;
    int   i;

    rv = g_mbstowcs((twchar *) unicode, path, strlen(path));

    /* unicode is typically 4 bytes, but microsoft only uses 2 bytes */

    src  = unicode + sizeof(twchar); /* skip 1st unicode char        */
    dest = unicode + 2;              /* first char already in  place */

    for (i = 1; i < rv; i++)
    {
        *dest++ = *src++;
        *dest++ = *src++;
        src += 2;
    }

    *dest++ = 0;
    *dest++ = 0;
}

static void
devredir_cvt_from_unicode_len(char *path, char *unicode, int len)
{
    char *dest;
    char *dest_saved;
    char *src;
    int   i;
    int   bytes_to_alloc;
    int   max_bytes;

    bytes_to_alloc = (((len / 2) * sizeof(twchar)) + sizeof(twchar));

    src = unicode;
    dest = g_new0(char, bytes_to_alloc);
    dest_saved = dest;

    for (i = 0; i < len; i += 2)
    {
        *dest++ = *src++;
        *dest++ = *src++;
        dest += 2;
    }
    *dest++ = 0;
    *dest++ = 0;
    *dest++ = 0;
    *dest++ = 0;

    max_bytes = wcstombs(NULL, (wchar_t *) dest_saved, 0);
    if (max_bytes > 0)
    {
        wcstombs(path, (wchar_t *) dest_saved, max_bytes);
        path[max_bytes] = 0;
    }

    g_free(dest_saved);
}

static int
devredir_string_ends_with(const char *string, char c)
{
    size_t len;

    len = strlen(string);
    return (len > 0 && string[len - 1] == c) ? 1 : 0;
}

static void
devredir_proc_cid_rmdir_or_file(IRP *irp, enum NTSTATUS IoStatus)
{
    struct stream *s;
    int            bytes;

    if (IoStatus != STATUS_SUCCESS)
    {
        xfuse_devredir_cb_rmdir_or_file((struct state_remove *) irp->fuse_info,
                                        IoStatus);
        devredir_irp_delete(irp);
        return;
    }

    xstream_new(s, 1024);

    irp->completion_type = CID_RMDIR_OR_FILE_RESP;
    devredir_insert_DeviceIoRequest(s, irp->DeviceId, irp->FileId,
                                    irp->CompletionId,
                                    IRP_MJ_SET_INFORMATION, IRP_MN_NONE);

    xstream_wr_u32_le(s, FileDispositionInformation);
    xstream_wr_u32_le(s, 0); /* length is zero */
    xstream_seek(s, 24);     /* padding        */

    /* send to client */
    bytes = xstream_len(s);
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);
    xstream_free(s);

    return;
}

static void
devredir_proc_cid_rmdir_or_file_resp(IRP *irp, enum NTSTATUS IoStatus)
{
    xfuse_devredir_cb_rmdir_or_file((struct state_remove *)irp->fuse_info,
                                    IoStatus);

    if (IoStatus != STATUS_SUCCESS)
    {
        devredir_irp_delete(irp);
        return;
    }

    irp->completion_type = CID_CLOSE;
    devredir_send_drive_close_request(RDPDR_CTYP_CORE,
                                      PAKID_CORE_DEVICE_IOREQUEST,
                                      irp->DeviceId,
                                      irp->FileId,
                                      irp->CompletionId,
                                      IRP_MJ_CLOSE, IRP_MN_NONE, 32);
}

static void
devredir_proc_cid_rename_file(IRP *irp, enum NTSTATUS IoStatus)
{
    struct stream *s;
    int            bytes;
    int            sblen; /* SetBuffer length */
    int            flen;  /* FileNameLength   */


    if (IoStatus != STATUS_SUCCESS)
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "rename returned with IoStatus=0x%x", IoStatus);

        xfuse_devredir_cb_rename_file((struct state_rename *)irp->fuse_info,
                                      IoStatus);
        devredir_irp_delete(irp);
        return;
    }

    /* Path in unicode needs this much space */
    flen = ((g_mbstowcs(NULL, irp->gen.rename.new_name, 0)
             * sizeof(twchar)) / 2) + 2;
    sblen = 6 + flen;

    xstream_new(s, 1024 + flen);

    irp->completion_type = CID_RENAME_FILE_RESP;
    devredir_insert_DeviceIoRequest(s, irp->DeviceId, irp->FileId,
                                    irp->CompletionId,
                                    IRP_MJ_SET_INFORMATION, IRP_MN_NONE);

    xstream_wr_u32_le(s, FileRenameInformation);
    xstream_wr_u32_le(s, sblen);     /* number of bytes after padding */
    xstream_seek(s, 24);             /* padding                       */
    xstream_wr_u8(s, 1);             /* ReplaceIfExists               */
    xstream_wr_u8(s, 0);             /* RootDirectory                 */
    xstream_wr_u32_le(s, flen);      /* FileNameLength                */

    /* filename in unicode */
    devredir_cvt_to_unicode(s->p, irp->gen.rename.new_name); /* UNICODE_TODO */
    xstream_seek(s, flen);

    /* send to client */
    bytes = xstream_len(s);
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);
    xstream_free(s);

    return;
}

static void
devredir_proc_cid_rename_file_resp(IRP *irp, enum NTSTATUS IoStatus)
{
    xfuse_devredir_cb_rename_file((struct state_rename *)irp->fuse_info,
                                  IoStatus);

    if (IoStatus != STATUS_SUCCESS)
    {
        devredir_irp_delete(irp);
        return;
    }

    irp->completion_type = CID_CLOSE;
    devredir_send_drive_close_request(RDPDR_CTYP_CORE,
                                      PAKID_CORE_DEVICE_IOREQUEST,
                                      irp->DeviceId,
                                      irp->FileId,
                                      irp->CompletionId,
                                      IRP_MJ_CLOSE, IRP_MN_NONE, 32);
}


/*
 * Re-uses the specified IRP to issue a request to get file attributes
 * of varying types
 *
 * References : [MS-RDPEFS] 2.2.3.3.9  [MS-FSCC] 2.4
 *****************************************************************************/
static void issue_lookup(IRP *irp, int lookup_type)
{
    struct stream *s;
    int  bytes;

    bytes =
        lookup_type == FileBasicInformation    ? FILE_BASIC_INFORMATION_SIZE :
        lookup_type == FileStandardInformation ? FILE_STD_INFORMATION_SIZE :
        0;
    xstream_new(s, 1024);
    devredir_insert_DeviceIoRequest(s, irp->DeviceId, irp->FileId,
                                    irp->CompletionId,
                                    IRP_MJ_QUERY_INFORMATION, IRP_MN_NONE);

    xstream_wr_u32_le(s, lookup_type);
    xstream_wr_u32_le(s, bytes);     /* buffer length                 */
    xstream_seek(s, 24);             /* padding                       */
    xstream_seek(s, bytes);          /* buffer                        */

    /* send to client */
    bytes = xstream_len(s);
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);
    xstream_free(s);
}

/*
 * Parses an incoming FileBasicInformation structure
 *****************************************************************************/
static void lookup_read_basic_attributes(IRP *irp, struct stream *s_in)
{
    tui64 LastAccessTime;
    tui64 LastWriteTime;
    tui32 FileAttributes;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "processing FILE_BASIC_INFORMATION");

    xstream_seek(s_in, 8);  /* CreationTime */
    xstream_rd_u64_le(s_in, LastAccessTime);
    xstream_rd_u64_le(s_in, LastWriteTime);
    xstream_seek(s_in, 8);  /* ChangeTime */
    xstream_rd_u32_le(s_in, FileAttributes);

    //LOG_DEVEL(LOG_LEVEL_DEBUG, "LastAccessTime:    0x%llx",
    //          (unsigned long long)LastAccessTime);
    //LOG_DEVEL(LOG_LEVEL_DEBUG, "LastWriteTime:     0x%llx",
    //          (unsigned long long)LastWriteTime);
    //LOG_DEVEL(LOG_LEVEL_DEBUG, "ChangeTime:        0x%llx",
    //          (unsigned long long)ChangeTime);
    //LOG_DEVEL(LOG_LEVEL_DEBUG, "FileAttributes:    0x%x", (unsigned int)FileAttributes);

    /* Save the basic attributes in the IRP */
    irp->gen.lookup.fattr.mode = WindowsToLinuxFilePerm(FileAttributes);
    irp->gen.lookup.fattr.atime = WINDOWS_TO_LINUX_TIME(LastAccessTime);
    irp->gen.lookup.fattr.mtime = WINDOWS_TO_LINUX_TIME(LastWriteTime);
}

/*
 * Parses an incoming FileStandardInformation structure
 *****************************************************************************/
static void lookup_read_standard_attributes(IRP *irp, struct stream *s_in)
{
    tui64 EndOfFile;
    LOG_DEVEL(LOG_LEVEL_DEBUG, "processing FILE_STD_INFORMATION");
    xstream_seek(s_in, 8);  /* AllocationSize */
    xstream_rd_u64_le(s_in, EndOfFile);
    //LOG_DEVEL(LOG_LEVEL_DEBUG, "EndOfFile:         %lld",
    //          (unsigned long long)EndOfFile);

    irp->gen.lookup.fattr.size = EndOfFile;
}

/*
 * Completes a lookup request and returns status to the caller.
 *
 * Unless IoStatus is STATUS_SUCCESS, the lookup has failed.
 *****************************************************************************/
static void lookup_done(IRP *irp, enum NTSTATUS IoStatus)
{
    LOG_DEVEL(LOG_LEVEL_DEBUG, "Lookup with completion_id=%d returning 0x%x",
              irp->CompletionId, IoStatus);
    xfuse_devredir_cb_lookup_entry((struct state_lookup *)irp->fuse_info,
                                   IoStatus,
                                   &irp->gen.lookup.fattr);

    if (irp->FileId == 0)
    {
        /* Open failed - no file handle */
        devredir_irp_delete(irp);
    }
    else
    {
        /* Close the file handle */
        irp->completion_type = CID_CLOSE;
        devredir_send_drive_close_request(RDPDR_CTYP_CORE,
                                          PAKID_CORE_DEVICE_IOREQUEST,
                                          irp->DeviceId,
                                          irp->FileId,
                                          irp->CompletionId,
                                          IRP_MJ_CLOSE, IRP_MN_NONE, 32);
    }
}


/*
 * lookup has a mini state machine built-in, as it needs to issue
 * multiple I/O requests, but unlike lookup these are not always the same.
 */
static void
devredir_proc_cid_lookup(IRP *irp,
                         struct stream *s_in,
                         enum NTSTATUS IoStatus)
{
    tui32 Length;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "entry state is %d", irp->gen.lookup.state);
    if (IoStatus != STATUS_SUCCESS)
    {
        /* This is common to all setattr states */
        LOG_DEVEL(LOG_LEVEL_DEBUG, "last lookup returned with IoStatus=0x%08x", IoStatus);
        lookup_done(irp, IoStatus);
    }
    else
    {
        /* Read and validate any data we've got queued up */
        switch (irp->gen.lookup.state)
        {
            case E_LOOKUP_GET_FH:
                /* We've been sent the file ID */
                xstream_rd_u32_le(s_in, irp->FileId);
                issue_lookup(irp, FileBasicInformation);
                irp->gen.lookup.state = E_LOOKUP_CHECK_BASIC;
                break;

            case E_LOOKUP_CHECK_BASIC:
                /* Returned length what we expected? */
                xstream_rd_u32_le(s_in, Length);
                if (Length != FILE_BASIC_INFORMATION_SIZE)
                {
                    LOG_DEVEL(LOG_LEVEL_ERROR, "Expected FILE_BASIC_INFORMATION length"
                              "%d, got len=%d",
                              FILE_BASIC_INFORMATION_SIZE, Length);
                    IoStatus = STATUS_UNSUCCESSFUL;
                    lookup_done(irp, IoStatus);
                }
                else
                {
                    lookup_read_basic_attributes(irp, s_in);
                    issue_lookup(irp, FileStandardInformation);
                    irp->gen.lookup.state = E_LOOKUP_CHECK_EOF;
                }
                break;

            case E_LOOKUP_CHECK_EOF:
                /* Returned length what we expected? */
                xstream_rd_u32_le(s_in, Length);
                if (Length != FILE_STD_INFORMATION_SIZE)
                {
                    LOG_DEVEL(LOG_LEVEL_ERROR, "Expected FILE_STD_INFORMATION length"
                              "%d, got len=%d",
                              FILE_STD_INFORMATION_SIZE, Length);
                    IoStatus = STATUS_UNSUCCESSFUL;
                }
                else
                {
                    lookup_read_standard_attributes(irp, s_in);
                }
                lookup_done(irp, IoStatus);
                break;
        }
    }
}


/*
 * Re-uses the specified IRP to issue a request to set basic file attributes
 *
 * References : [MS-RDPEFS] 2.2.3.3.9  [MS-FSCC] 2.4.7
 *****************************************************************************/
static void issue_setattr_basic(IRP *irp)
{
    struct stream *s;
    int  bytes;
    const struct file_attr *fattr = &irp->gen.setattr.fattr;
    tui32 to_set = irp->gen.setattr.to_set;

    tui32 FileAttributes = 0;
    tui64 atime = 0;
    tui64 mtime = 0;

    if (to_set & TO_SET_MODE)
    {
        FileAttributes = LinuxToWindowsFilePerm(fattr->mode);
    }
    if (to_set & TO_SET_ATIME)
    {
        atime = LINUX_TO_WINDOWS_TIME(fattr->atime);
    }
    if (to_set & TO_SET_MTIME)
    {
        mtime = LINUX_TO_WINDOWS_TIME(fattr->mtime);
    }

    xstream_new(s, 1024);
    devredir_insert_DeviceIoRequest(s, irp->DeviceId, irp->FileId,
                                    irp->CompletionId,
                                    IRP_MJ_SET_INFORMATION, IRP_MN_NONE);

    xstream_wr_u32_le(s, FileBasicInformation);
    xstream_wr_u32_le(s, FILE_BASIC_INFORMATION_SIZE);
    /* buffer length                 */
    xstream_seek(s, 24);             /* padding                       */

    xstream_wr_u64_le(s, 0LL);            /* CreationTime */
    xstream_wr_u64_le(s, atime);          /* LastAccessTime */
    xstream_wr_u64_le(s, mtime);          /* LastWriteTime */
    xstream_wr_u64_le(s, 0LL);            /* ChangeTime */
    xstream_wr_u32_le(s, FileAttributes); /* FileAttributes */

    /* send to client */
    bytes = xstream_len(s);
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);
    xstream_free(s);
}


/*
 * Re-uses the specified IRP to issue a request to set file EOF
 *
 * References : [MS-RDPEFS] 2.2.3.3.9  [MS-FSCC] 2.4.13
 *****************************************************************************/
static void issue_setattr_eof(IRP *irp)
{
    struct stream *s;
    int  bytes;

    xstream_new(s, 1024);
    devredir_insert_DeviceIoRequest(s, irp->DeviceId, irp->FileId,
                                    irp->CompletionId,
                                    IRP_MJ_SET_INFORMATION, IRP_MN_NONE);

    xstream_wr_u32_le(s, FileEndOfFileInformation);
    xstream_wr_u32_le(s, FILE_END_OF_FILE_INFORMATION_SIZE);
    /* buffer length             */
    xstream_seek(s, 24);         /* padding                   */
    xstream_wr_u64_le(s, (tui64)irp->gen.setattr.fattr.size);
    /* File size                 */
    /* send to client */
    bytes = xstream_len(s);
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);
    xstream_free(s);
}

/*
 * Completes a setattr request and returns status to the caller.
 *****************************************************************************/
static void setattr_done(IRP *irp, enum NTSTATUS IoStatus)
{
    xfuse_devredir_cb_setattr((struct state_setattr *) irp->fuse_info,
                              IoStatus);

    if (irp->FileId == 0)
    {
        /* Open failed - no file handle */
        devredir_irp_delete(irp);
    }
    else
    {
        /* Close the file handle */
        irp->completion_type = CID_CLOSE;
        devredir_send_drive_close_request(RDPDR_CTYP_CORE,
                                          PAKID_CORE_DEVICE_IOREQUEST,
                                          irp->DeviceId,
                                          irp->FileId,
                                          irp->CompletionId,
                                          IRP_MJ_CLOSE, IRP_MN_NONE, 32);
    }
}


/*
 * setattr has a mini state machine built-in, as it needs to issue
 * multiple I/O requests, but unlike lookup these are not always the same.
 */
static void
devredir_proc_cid_setattr(IRP *irp,
                          struct stream *s_in,
                          enum NTSTATUS IoStatus)
{
#define TO_SET_BASIC_ATTRS (TO_SET_MODE | \
                            TO_SET_ATIME | TO_SET_MTIME)
    tui32 Length;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "entry state is %d", irp->gen.setattr.state);
    if (IoStatus != STATUS_SUCCESS)
    {
        /* This is common to all setattr states */
        LOG_DEVEL(LOG_LEVEL_DEBUG, "last setattr returned with IoStatus=0x%08x", IoStatus);
        setattr_done(irp, IoStatus);
    }
    else
    {
        /* Read and validate any data we've got queued up */
        switch (irp->gen.setattr.state)
        {
            case E_SETATTR_GET_FH:
                /* We've been sent the file ID */
                xstream_rd_u32_le(s_in, irp->FileId);
                break;

            case E_SETATTR_CHECK_BASIC:
                /* Returned length what we expected? */
                xstream_rd_u32_le(s_in, Length);
                if (Length != FILE_BASIC_INFORMATION_SIZE)
                {
                    LOG_DEVEL(LOG_LEVEL_ERROR, "Expected FILE_BASIC_INFORMATION length"
                              "%d, got len=%d",
                              FILE_BASIC_INFORMATION_SIZE, Length);
                }

                /* Clear the basic bits so we don't end up in here again */
                irp->gen.setattr.to_set &= ~TO_SET_BASIC_ATTRS;
                break;

            case E_SETATTR_CHECK_EOF:
                /* Returned length what we expected? */
                xstream_rd_u32_le(s_in, Length);
                if (Length != FILE_END_OF_FILE_INFORMATION_SIZE)
                {
                    LOG_DEVEL(LOG_LEVEL_ERROR, "Expected FILE_END_OF_FILE_INFORMATION length"
                              "%d, got len=%d",
                              FILE_END_OF_FILE_INFORMATION_SIZE, Length);
                }

                /* Clear the size bits so we don't end up in here again */
                irp->gen.setattr.to_set &= ~TO_SET_SIZE;
                break;
        }

        /* Work out the next call to issue */
        if (irp->gen.setattr.to_set & TO_SET_BASIC_ATTRS)
        {
            issue_setattr_basic(irp);
            irp->gen.setattr.state = E_SETATTR_CHECK_BASIC;
        }
        else if (irp->gen.setattr.to_set & TO_SET_SIZE)
        {
            issue_setattr_eof(irp);
            irp->gen.setattr.state = E_SETATTR_CHECK_EOF;
        }
        else
        {
            setattr_done(irp, IoStatus);
        }
    }
#undef TO_SET_BASIC_ATTRS
}
