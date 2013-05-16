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
#include "log.h"
#include "chansrv_fuse.h"
#include "devredir.h"
#include "smartcard.h"

/* module based logging */
#define LOG_ERROR   0
#define LOG_INFO    1
#define LOG_DEBUG   2

#ifndef LOG_LEVEL
#define LOG_LEVEL   LOG_DEBUG
#endif

#define log_error(_params...)                           \
{                                                       \
    g_write("[%10.10u]: DEV_REDIR  %s: %d : ERROR: ",   \
            g_time3(), __func__, __LINE__);             \
    g_writeln (_params);                                \
}

#define log_info(_params...)                            \
{                                                       \
    if (LOG_INFO <= LOG_LEVEL)                         \
    {                                                   \
        g_write("[%10.10u]: DEV_REDIR  %s: %d : ",      \
                g_time3(), __func__, __LINE__);         \
        g_writeln (_params);                            \
    }                                                   \
}

#define log_debug(_params...)                           \
{                                                       \
    if (LOG_DEBUG <= LOG_LEVEL)                        \
    {                                                   \
        g_write("[%10.10u]: DEV_REDIR  %s: %d : ",      \
                g_time3(), __func__, __LINE__);         \
        g_writeln (_params);                            \
    }                                                   \
}

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

void xfuse_devredir_cb_write_file(void *vp, char *buf, size_t length);

/*****************************************************************************/
int APP_CC
dev_redir_init(void)
{
    struct  stream *s;
    int     bytes;
    int     fd;

    union _u
    {
        tui32 clientID;
        char buf[4];
    } u;

    /* get a random number that will act as a unique clientID */
    if ((fd = open("/dev/urandom", O_RDONLY)))
    {
        read(fd, u.buf, 4);
        close(fd);
    }
    else
    {
        /* /dev/urandom did not work - use address of struct s */
        tui64 u64 = (tui64) &s;
        u.clientID = (tui32) u64;
    }

    /* setup stream */
    xstream_new(s, 1024);

    /* initiate drive redirection protocol by sending Server Announce Req  */
    xstream_wr_u16_le(s, RDPDR_CTYP_CORE);
    xstream_wr_u16_le(s, PAKID_CORE_SERVER_ANNOUNCE);
    xstream_wr_u16_le(s, 0x0001);  /* server major ver                      */
    xstream_wr_u16_le(s, 0x000C);  /* server minor ver  - pretend 2 b Win 7 */
    xstream_wr_u32_le(s, u.clientID); /* unique ClientID                    */

    /* send data to client */
    bytes = xstream_len(s);
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);

    xstream_free(s);
    return 0;
}

/*****************************************************************************/
int APP_CC
dev_redir_deinit(void)
{
    return 0;
}

/**
 * @brief process incoming data
 *
 * @return 0 on success, -1 on failure
 *****************************************************************************/

int APP_CC
dev_redir_data_in(struct stream *s, int chan_id, int chan_flags, int length,
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
            xstream_new(g_input_stream, total_length);

        xstream_copyin(g_input_stream, s->p, length);

        /* in last packet, chan_flags & 0x02 will be true */
        if ((chan_flags & 2) == 0)
            return 0;

        g_input_stream->p = g_input_stream->data;
        ls = g_input_stream;
    }

    /* read header from incoming data */
    xstream_rd_u16_le(ls, comp_type);
    xstream_rd_u16_le(ls, pktID);

    /* for now we only handle core type, not printers */
    if (comp_type != RDPDR_CTYP_CORE)
    {
        log_error("invalid component type in response; expected 0x%x got 0x%x",
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
            // LK_TODO dev_redir_send_server_clientID_confirm();
            break;

        case PAKID_CORE_CLIENT_NAME:
            /* client is telling us its computer name; do we even care? */

            /* let client know loggin was successful */
            dev_redir_send_server_user_logged_on();
            usleep(1000 * 100);

            /* let client know our capabilites */
            dev_redir_send_server_core_cap_req();

            /* send confirm clientID */
            dev_redir_send_server_clientID_confirm();
            break;

        case PAKID_CORE_CLIENT_CAPABILITY:
            dev_redir_proc_client_core_cap_resp(ls);
            break;

        case PAKID_CORE_DEVICELIST_ANNOUNCE:
            devredir_proc_client_devlist_announce_req(ls);
            break;

        case PAKID_CORE_DEVICE_IOCOMPLETION:
            dev_redir_proc_device_iocompletion(ls);
            break;

        default:
            log_error("got unknown response 0x%x", pktID);
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
int APP_CC
dev_redir_get_wait_objs(tbus *objs, int *count, int *timeout)
{
    return 0;
}

/*****************************************************************************/
int APP_CC
dev_redir_check_wait_objs(void)
{
    return 0;
}

/**
 * @brief let client know our capabilities
 *****************************************************************************/

void dev_redir_send_server_core_cap_req()
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

void dev_redir_send_server_clientID_confirm()
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

void dev_redir_send_server_user_logged_on()
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

void devredir_send_server_device_announce_resp(tui32 device_id)
{
    struct stream *s;
    int            bytes;

    xstream_new(s, 1024);

    /* setup stream */
    xstream_wr_u16_le(s, RDPDR_CTYP_CORE);
    xstream_wr_u16_le(s, PAKID_CORE_DEVICE_REPLY);
    xstream_wr_u32_le(s, device_id);
    xstream_wr_u32_le(s, 0); /* ResultCode */

    /* send to client */
    bytes = xstream_len(s);
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);

    xstream_free(s);
}

/**
 * @return 0 on success, -1 on failure
 *****************************************************************************/

int dev_redir_send_drive_create_request(tui32 device_id, char *path,
                                        tui32 DesiredAccess,
                                        tui32 CreateOptions,
                                        tui32 CreateDisposition,
                                        tui32 completion_id)
{
    struct stream *s;
    int            bytes;
    int            len;

    log_debug("DesiredAccess=0x%x CreateDisposition=0x%x CreateOptions=0x%x",
              DesiredAccess, CreateDisposition, CreateOptions);

    /* to store path as unicode */
    len = strlen(path) * 2 + 2;

    xstream_new(s, 1024 + len);

    devredir_insert_DeviceIoRequest(s,
                                    device_id,
                                    0,
                                    completion_id,
                                    IRP_MJ_CREATE,
                                    0);

    xstream_wr_u32_le(s, DesiredAccess);     /* DesiredAccess              */
    xstream_wr_u32_le(s, 0);                 /* AllocationSize high unused */
    xstream_wr_u32_le(s, 0);                 /* AllocationSize low  unused */
    xstream_wr_u32_le(s, 0);                 /* FileAttributes             */
    xstream_wr_u32_le(s, 3);                 /* SharedAccess LK_TODO       */
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
 * Close a request previously created by dev_redir_send_drive_create_request()
 *****************************************************************************/

int dev_redir_send_drive_close_request(tui16 Component, tui16 PacketId,
                                       tui32 DeviceId,
                                       tui32 FileId,
                                       tui32 CompletionId,
                                       tui32 MajorFunction,
                                       tui32 MinorFunc,
                                       int pad_len)
{
    struct stream *s;
    int            bytes;

    xstream_new(s, 1024);

    devredir_insert_DeviceIoRequest(s, DeviceId, FileId, CompletionId,
                                    MajorFunction, MinorFunc);

    if (pad_len)
        xstream_seek(s, pad_len);

    /* send to client */
    bytes = xstream_len(s);
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);

    xstream_free(s);
    log_debug("sent close request; expect CID_FILE_CLOSE");
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
void dev_redir_send_drive_dir_request(IRP *irp, tui32 device_id,
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
            return;

        path_len = strlen(Path) * 2 + 2;
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

#ifdef USE_SHORT_NAMES_IN_DIR_LISTING
    xstream_wr_u32_le(s, FileBothDirectoryInformation); /* FsInformationClass */
#else
    xstream_wr_u32_le(s, FileDirectoryInformation);  /* FsInformationClass */
#endif
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
 * @brief process client's repsonse to our core_capability_req() msg
 *
 * @param   s   stream containing client's response
 *****************************************************************************/
void dev_redir_proc_client_core_cap_resp(struct stream *s)
{
    int i;
    tui16 num_caps;
    tui16 cap_type;
    tui16 cap_len;
    tui32 cap_version;

    xstream_rd_u16_le(s, num_caps);
    xstream_seek(s, 2);  /* padding */

    for (i = 0; i < num_caps; i++)
    {
        xstream_rd_u16_le(s, cap_type);
        xstream_rd_u16_le(s, cap_len);
        xstream_rd_u32_le(s, cap_version);

        /* remove header length and version */
        cap_len -= 8;

        switch (cap_type)
        {
            case CAP_GENERAL_TYPE:
                log_debug("got CAP_GENERAL_TYPE");
                xstream_seek(s, cap_len);
                break;

            case CAP_PRINTER_TYPE:
                log_debug("got CAP_PRINTER_TYPE");
                g_is_printer_redir_supported = 1;
                xstream_seek(s, cap_len);
                break;

            case CAP_PORT_TYPE:
                log_debug("got CAP_PORT_TYPE");
                g_is_port_redir_supported = 1;
                xstream_seek(s, cap_len);
                break;

            case CAP_DRIVE_TYPE:
                log_debug("got CAP_DRIVE_TYPE");
                g_is_drive_redir_supported = 1;
                if (cap_version == 2)
                    g_drive_redir_version = 2;
                xstream_seek(s, cap_len);
                break;

            case CAP_SMARTCARD_TYPE:
                log_debug("got CAP_SMARTCARD_TYPE");
                g_is_smartcard_redir_supported = 1;
                xstream_seek(s, cap_len);
                break;
        }
    }
}

void devredir_proc_client_devlist_announce_req(struct stream *s)
{
    int   i;
    int   j;
    tui32 device_count;
    tui32 device_type;
    tui32 device_data_len;
    char  preferred_dos_name[9];

    /* get number of devices being announced */
    xstream_rd_u32_le(s, device_count);

    log_debug("num of devices announced: %d", device_count);

    for (i = 0; i < device_count; i++)
    {
        xstream_rd_u32_le(s, device_type);
        xstream_rd_u32_le(s, g_device_id);

        switch (device_type)
        {
            case RDPDR_DTYP_FILESYSTEM:
                /* get preferred DOS name */
                for (j = 0; j < 8; j++)
                {
                    preferred_dos_name[j] = *s->p++;
                }

                /* DOS names that are 8 chars long are not NULL terminated */
                preferred_dos_name[8] = 0;

                /* get device data len */
                xstream_rd_u32_le(s, device_data_len);
                if (device_data_len)
                {
                    xstream_rd_string(g_full_name_for_filesystem, s,
                                     device_data_len);
                }

                log_debug("device_type=FILE_SYSTEM device_id=0x%x dosname=%s "
                          "device_data_len=%d full_name=%s", g_device_id,
                          preferred_dos_name,
                          device_data_len, g_full_name_for_filesystem);

                devredir_send_server_device_announce_resp(g_device_id);

                /* create share directory in xrdp file system;    */
                /* think of this as the mount point for this share */
                xfuse_create_share(g_device_id, preferred_dos_name);
                break;

            case RDPDR_DTYP_SMARTCARD:
                /* get preferred DOS name */
                for (j = 0; j < 8; j++)
                {
                    preferred_dos_name[j] = *s->p++;
                }

                /* DOS names that are 8 chars long are not NULL terminated */
                preferred_dos_name[8] = 0;

                /* for smart cards, device data len always 0 */

                log_debug("device_type=SMARTCARD device_id=0x%x dosname=%s "
                          "device_data_len=%d",
                          g_device_id, preferred_dos_name, device_data_len);

                devredir_send_server_device_announce_resp(g_device_id);
                scard_device_announce(g_device_id);
                break;

            /* we don't yet support these devices */
            case RDPDR_DTYP_SERIAL:
            case RDPDR_DTYP_PARALLEL:
            case RDPDR_DTYP_PRINT:
                log_debug("unsupported dev: 0x%x", device_type);
                break;
        }
    }
}

void dev_redir_proc_device_iocompletion(struct stream *s)
{
    FUSE_DATA *fuse_data = NULL;
    IRP       *irp       = NULL;

    tui32      DeviceId;
    tui32      CompletionId;
    tui32      IoStatus;
    tui32      Length;

    xstream_rd_u32_le(s, DeviceId);
    xstream_rd_u32_le(s, CompletionId);
    xstream_rd_u32_le(s, IoStatus);

    /* LK_TODO need to check for IoStatus */

    log_debug("entered: IoStatus=0x%x CompletionId=%d", IoStatus, CompletionId);

    if ((irp = devredir_irp_find(CompletionId)) == NULL)
    {
        log_error("IRP with completion ID %d not found", CompletionId);
        return;
    }

    /* if callback has been set, call it */
    if (irp->callback)
    {
        (*irp->callback)(s, irp, DeviceId, CompletionId, IoStatus);
        goto done;
    }

    switch (irp->completion_type)
    {
    case CID_CREATE_DIR_REQ:
        log_debug("got CID_CREATE_DIR_REQ");
        if (IoStatus != NT_STATUS_SUCCESS)
        {
            /* we were trying to create a request to enumerate a dir */
            /* that does not exist; let FUSE know                    */
            fuse_data = dev_redir_fuse_data_dequeue(irp);
            if (fuse_data)
            {
                xfuse_devredir_cb_enum_dir_done(fuse_data->data_ptr,
                                                IoStatus);
                free(fuse_data);
            }
            devredir_irp_delete(irp);
            return;
        }

        xstream_rd_u32_le(s, irp->FileId);
        log_debug("got CID_CREATE_DIR_REQ IoStatus=0x%x FileId=%d",
                  IoStatus, irp->FileId);

        dev_redir_send_drive_dir_request(irp, DeviceId, 1, irp->pathname);
        break;

    case CID_CREATE_OPEN_REQ:
        xstream_rd_u32_le(s, irp->FileId);
        log_debug("got CID_CREATE_OPEN_REQ IoStatus=0x%x FileId=%d",
                  IoStatus, irp->FileId);
        fuse_data = dev_redir_fuse_data_dequeue(irp);
        xfuse_devredir_cb_open_file(fuse_data->data_ptr,
                                    DeviceId, irp->FileId);
        if (irp->type == S_IFDIR)
            devredir_irp_delete(irp);
        break;

    case CID_READ:
        log_debug("got CID_READ");
        xstream_rd_u32_le(s, Length);
        fuse_data = dev_redir_fuse_data_dequeue(irp);
        xfuse_devredir_cb_read_file(fuse_data->data_ptr, s->p, Length);
        break;

    case CID_WRITE:
        log_debug("got CID_WRITE");
        xstream_rd_u32_le(s, Length);
        fuse_data = dev_redir_fuse_data_dequeue(irp);
        xfuse_devredir_cb_write_file(fuse_data->data_ptr, s->p, Length);
        break;

    case CID_CLOSE:
        log_debug("got CID_CLOSE");
        log_debug("deleting irp with completion_id=%d comp_type=%d",
                  irp->CompletionId, irp->completion_type);
        devredir_irp_delete(irp);
        break;

    case CID_FILE_CLOSE:
        log_debug("got CID_FILE_CLOSE");
        fuse_data = dev_redir_fuse_data_dequeue(irp);
        xfuse_devredir_cb_file_close(fuse_data->data_ptr);
        devredir_irp_delete(irp);
        break;

    case CID_DIRECTORY_CONTROL:
        log_debug("got CID_DIRECTORY_CONTROL");

        dev_redir_proc_query_dir_response(irp, s, DeviceId,
                                          CompletionId, IoStatus);
        break;

    case CID_RMDIR_OR_FILE:
        log_debug("got CID_RMDIR_OR_FILE");
        xstream_rd_u32_le(s, irp->FileId);
        devredir_proc_cid_rmdir_or_file(irp, IoStatus);
        return;
        break;

    case CID_RMDIR_OR_FILE_RESP:
        log_debug("got CID_RMDIR_OR_FILE_RESP");
        devredir_proc_cid_rmdir_or_file_resp(irp, IoStatus);
        break;

    case CID_RENAME_FILE:
        log_debug("got CID_RENAME_FILE");
        xstream_rd_u32_le(s, irp->FileId);
        devredir_proc_cid_rename_file(irp, IoStatus);
        return;
        break;

    case CID_RENAME_FILE_RESP:
        log_debug("got CID_RENAME_FILE_RESP");
        devredir_proc_cid_rename_file_resp(irp, IoStatus);
        break;

    default:
        log_error("got unknown CompletionID: DeviceId=0x%x "
                  "CompletionId=0x%x IoStatus=0x%x",
                  DeviceId, CompletionId, IoStatus);
        break;
    }

done:

    if (fuse_data)
        free(fuse_data);

    log_debug("exiting");
}

void dev_redir_proc_query_dir_response(IRP *irp,
                                       struct stream *s_in,
                                       tui32 DeviceId,
                                       tui32 CompletionId,
                                       tui32 IoStatus)
{
    FUSE_DATA  *fuse_data = NULL;
    XRDP_INODE *xinode    = NULL;

    tui32 Length;
    tui32 NextEntryOffset;
    tui64 CreationTime;
    tui64 LastAccessTime;
    tui64 LastWriteTime;
    tui64 ChangeTime;
    tui64 EndOfFile;
    tui32 FileAttributes;
    tui32 FileNameLength;
    tui32 status;

#ifdef USE_SHORT_NAMES_IN_DIR_LISTING
    tui32 EaSize;
    tui8  ShortNameLength;
    tui8  Reserved;
#endif

    char  filename[256];
    int   i = 0;

    xstream_rd_u32_le(s_in, Length);

    if ((IoStatus == NT_STATUS_UNSUCCESSFUL) ||
        (IoStatus == STATUS_NO_MORE_FILES))
    {
        status = (IoStatus == STATUS_NO_MORE_FILES) ? 0 : IoStatus;
        fuse_data = dev_redir_fuse_data_dequeue(irp);
        xfuse_devredir_cb_enum_dir_done(fuse_data->data_ptr, status);
        irp->completion_type = CID_CLOSE;
        dev_redir_send_drive_close_request(RDPDR_CTYP_CORE,
                                           PAKID_CORE_DEVICE_IOREQUEST,
                                           DeviceId,
                                           irp->FileId,
                                           irp->CompletionId,
                                           IRP_MJ_CLOSE, 0, 32);
        free(fuse_data);
        return;
    }

    /* TODO check status for errors */

    /* process FILE_DIRECTORY_INFORMATION structures */
    while (i < Length)
    {
        log_debug("processing FILE_DIRECTORY_INFORMATION structs");

        xstream_rd_u32_le(s_in, NextEntryOffset);
        xstream_seek(s_in, 4);  /* FileIndex */
        xstream_rd_u64_le(s_in, CreationTime);
        xstream_rd_u64_le(s_in, LastAccessTime);
        xstream_rd_u64_le(s_in, LastWriteTime);
        xstream_rd_u64_le(s_in, ChangeTime);
        xstream_rd_u64_le(s_in, EndOfFile);
        xstream_seek(s_in, 8);  /* AllocationSize */
        xstream_rd_u32_le(s_in, FileAttributes);
        xstream_rd_u32_le(s_in, FileNameLength);

#ifdef USE_SHORT_NAMES_IN_DIR_LISTING
        xstream_rd_u32_le(s_in, EaSize);
        xstream_rd_u8(s_in, ShortNameLength);
        xstream_rd_u8(s_in, Reserved);
        xstream_seek(s_in, 23);  /* ShortName in Unicode */
#endif
        devredir_cvt_from_unicode_len(filename, s_in->p, FileNameLength);

#ifdef USE_SHORT_NAMES_IN_DIR_LISTING
        i += 70 + 23 + FileNameLength;
#else
        i += 64 + FileNameLength;
#endif
        //log_debug("NextEntryOffset:   0x%x", NextEntryOffset);
        //log_debug("CreationTime:      0x%llx", CreationTime);
        //log_debug("LastAccessTime:    0x%llx", LastAccessTime);
        //log_debug("LastWriteTime:     0x%llx", LastWriteTime);
        //log_debug("ChangeTime:        0x%llx", ChangeTime);
        //log_debug("EndOfFile:         %lld", EndOfFile);
        //log_debug("FileAttributes:    0x%x", FileAttributes);
#ifdef USE_SHORT_NAMES_IN_DIR_LISTING
        //log_debug("ShortNameLength:   %d", ShortNameLength);
#endif
        //log_debug("FileNameLength:    %d", FileNameLength);
        log_debug("FileName:          %s", filename);

        if ((xinode = calloc(1, sizeof(struct xrdp_inode))) == NULL)
        {
            log_error("system out of memory");
            fuse_data = dev_redir_fuse_data_peek(irp);
            xfuse_devredir_cb_enum_dir(fuse_data->data_ptr, NULL);
            return;
        }

        strcpy(xinode->name, filename);
        xinode->size = (size_t) EndOfFile;
        xinode->mode = WINDOWS_TO_LINUX_FILE_PERM(FileAttributes);
        xinode->atime = WINDOWS_TO_LINUX_TIME(LastAccessTime);
        xinode->mtime = WINDOWS_TO_LINUX_TIME(LastWriteTime);
        xinode->ctime = WINDOWS_TO_LINUX_TIME(CreationTime);

        /* add this entry to xrdp file system */
        fuse_data = dev_redir_fuse_data_peek(irp);
        xfuse_devredir_cb_enum_dir(fuse_data->data_ptr, xinode);
    }

    dev_redir_send_drive_dir_request(irp, DeviceId, 0, NULL);
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

int dev_redir_get_dir_listing(void *fusep, tui32 device_id, char *path)
{
    tui32  DesiredAccess;
    tui32  CreateOptions;
    tui32 CreateDisposition;
    int    rval;
    IRP   *irp;

    log_debug("fusep=%p", fusep);

    if ((irp = devredir_irp_new()) == NULL)
        return -1;

    /* cvt / to windows compatible \ */
    devredir_cvt_slash(path);

    irp->CompletionId = g_completion_id++;
    irp->completion_type = CID_CREATE_DIR_REQ;
    irp->DeviceId = device_id;
    strcpy(irp->pathname, path);
    dev_redir_fuse_data_enqueue(irp, fusep);

    DesiredAccess = DA_FILE_READ_DATA | DA_SYNCHRONIZE;
    CreateOptions = CO_FILE_DIRECTORY_FILE | CO_FILE_SYNCHRONOUS_IO_NONALERT;
    CreateDisposition = CD_FILE_OPEN;

    rval = dev_redir_send_drive_create_request(device_id, path,
                                               DesiredAccess, CreateOptions,
                                               CreateDisposition,
                                               irp->CompletionId);

    log_debug("looking for device_id=%d path=%s", device_id, path);

    /* when we get a respone to dev_redir_send_drive_create_request(), we    */
    /* call dev_redir_send_drive_dir_request(), which needs the following    */
    /* at the end of the path argument                                       */
    if (dev_redir_string_ends_with(irp->pathname, '\\'))
        strcat(irp->pathname, "*");
    else
        strcat(irp->pathname, "\\*");

    return rval;
}

int dev_redir_file_open(void *fusep, tui32 device_id, char *path,
                        int mode, int type, char *gen_buf)
{
    tui32  DesiredAccess;
    tui32  CreateOptions;
    tui32  CreateDisposition;
    int    rval;
    IRP   *irp;

    log_debug("device_id=%d path=%s mode=0x%x", device_id, path, mode);

    if ((irp = devredir_irp_new()) == NULL)
        return -1;

    if (type & OP_RENAME_FILE)
    {
        irp->completion_type = CID_RENAME_FILE;
        strcpy(irp->gen_buf, gen_buf);
    }
    else
    {
        irp->completion_type = CID_CREATE_OPEN_REQ;
    }

    irp->CompletionId = g_completion_id++;
    irp->DeviceId = device_id;
    strcpy(irp->pathname, path);
    dev_redir_fuse_data_enqueue(irp, fusep);

    if (mode & O_CREAT)
    {
        log_debug("open file in O_CREAT");
        DesiredAccess = 0x0016019f; /* got this value from windows */

        if (type & S_IFDIR)
        {
            log_debug("creating dir");
            CreateOptions = CO_FILE_DIRECTORY_FILE | CO_FILE_SYNCHRONOUS_IO_NONALERT;
            irp->type = S_IFDIR;
        }
        else
        {
            log_debug("creating file");
            CreateOptions = 0x44; /* got this value from windows */
        }

        //CreateDisposition = CD_FILE_CREATE;
        CreateDisposition  = 0x02; /* got this value from windows */
    }
    else //if (mode & O_RDWR)
    {
        log_debug("open file in O_RDWR");
#if 1
        DesiredAccess = DA_FILE_READ_DATA | DA_FILE_WRITE_DATA | DA_SYNCHRONIZE;
        CreateOptions = CO_FILE_SYNCHRONOUS_IO_NONALERT;
        CreateDisposition = CD_FILE_OPEN; // WAS 1
#else
        /* got this value from windows */
        DesiredAccess = 0x00120089;
        CreateOptions = 0x20060;
        CreateDisposition = 0x01;
#endif
    }

    rval = dev_redir_send_drive_create_request(device_id, path,
                                               DesiredAccess, CreateOptions,
                                               CreateDisposition,
                                               irp->CompletionId);

    return rval;
}

int devredir_file_close(void *fusep, tui32 device_id, tui32 FileId)
{
    IRP *irp;

    log_debug("entered");

#if 0
    if ((irp = devredir_irp_new()) == NULL)
        return -1;

    irp->CompletionId = g_completion_id++;
#else
    if ((irp = devredir_irp_find_by_fileid(FileId)) == NULL)
    {
        log_error("no IRP found with FileId = %d", FileId);
        return -1;
    }
#endif
    irp->completion_type = CID_FILE_CLOSE;
    irp->DeviceId = device_id;
    dev_redir_fuse_data_enqueue(irp, fusep);

    return dev_redir_send_drive_close_request(RDPDR_CTYP_CORE,
                                              PAKID_CORE_DEVICE_IOREQUEST,
                                              device_id,
                                              FileId,
                                              irp->CompletionId,
                                              IRP_MJ_CLOSE,
                                              0, 32);
}

/**
 * Remove (delete) a directory or file
 *****************************************************************************/

int devredir_rmdir_or_file(void *fusep, tui32 device_id, char *path, int mode)
{
    tui32  DesiredAccess;
    tui32  CreateOptions;
    tui32  CreateDisposition;
    int    rval;
    IRP   *irp;

    if ((irp = devredir_irp_new()) == NULL)
        return -1;

    irp->CompletionId = g_completion_id++;
    irp->completion_type = CID_RMDIR_OR_FILE;
    irp->DeviceId = device_id;
    strcpy(irp->pathname, path);
    dev_redir_fuse_data_enqueue(irp, fusep);

    //DesiredAccess = DA_DELETE | DA_FILE_READ_ATTRIBUTES | DA_SYNCHRONIZE;
    DesiredAccess = 0x00100080; /* got this value from windows */

    //CreateOptions = CO_FILE_DELETE_ON_CLOSE | CO_FILE_DIRECTORY_FILE |
    //                CO_FILE_SYNCHRONOUS_IO_NONALERT;
    CreateOptions = 0x020; /* got this value from windows */

    //CreateDisposition = CD_FILE_OPEN; // WAS 1
    CreateDisposition = 0x01; /* got this value from windows */

    rval = dev_redir_send_drive_create_request(device_id, path,
                                               DesiredAccess, CreateOptions,
                                               CreateDisposition,
                                               irp->CompletionId);

    return rval;
}

/**
 * Read data from previously opened file
 *
 * @return 0 on success, -1 on failure
 *****************************************************************************/

int dev_redir_file_read(void *fusep, tui32 DeviceId, tui32 FileId,
                        tui32 Length, tui64 Offset)
{
    struct stream *s;
    IRP           *irp;
    int            bytes;

    xstream_new(s, 1024);

    if ((irp = devredir_irp_find_by_fileid(FileId)) == NULL)
    {
        log_error("no IRP found with FileId = %d", FileId);
        xfuse_devredir_cb_read_file(fusep, NULL, 0);
        return -1;
    }

    irp->completion_type = CID_READ;
    dev_redir_fuse_data_enqueue(irp, fusep);
    devredir_insert_DeviceIoRequest(s,
                                    DeviceId,
                                    FileId,
                                    irp->CompletionId,
                                    IRP_MJ_READ,
                                    0);

    xstream_wr_u32_le(s, Length);
    xstream_wr_u64_le(s, Offset);
    xstream_seek(s, 20);

    /* send to client */
    bytes = xstream_len(s);
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);
    xstream_free(s);

    return 0;
}

int dev_redir_file_write(void *fusep, tui32 DeviceId, tui32 FileId,
                         const char *buf, tui32 Length, tui64 Offset)
{
    struct stream *s;
    IRP           *irp;
    int            bytes;

    log_debug("DeviceId=%d FileId=%d Length=%d Offset=%lld",
              DeviceId, FileId, Length, Offset);

    xstream_new(s, 1024 + Length);

    if ((irp = devredir_irp_find_by_fileid(FileId)) == NULL)
    {
        log_error("no IRP found with FileId = %d", FileId);
        xfuse_devredir_cb_write_file(fusep, NULL, 0);
        return -1;
    }

    irp->completion_type = CID_WRITE;
    dev_redir_fuse_data_enqueue(irp, fusep);
    devredir_insert_DeviceIoRequest(s,
                                    DeviceId,
                                    FileId,
                                    irp->CompletionId,
                                    IRP_MJ_WRITE,
                                    0);

    xstream_wr_u32_le(s, Length);
    xstream_wr_u64_le(s, Offset);
    xstream_seek(s, 20); /* padding */

    /* now insert real data */
    xstream_copyin(s, buf, Length);

    /* send to client */
    bytes = xstream_len(s);
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);
    xstream_free(s);

    return 0;
}

/******************************************************************************
**                            FIFO for FUSE_DATA                             **
******************************************************************************/

/**
 * Return FUSE_DATA at the head of the queue without removing it
 *
 * @return FUSE_DATA on success, or NULL on failure
 *****************************************************************************/

void *dev_redir_fuse_data_peek(IRP *irp)
{
    return irp->fd_head;
}

/**
 * Return oldest FUSE_DATA from queue
 *
 * @return FUSE_DATA on success, NULL on failure
 *****************************************************************************/

void *dev_redir_fuse_data_dequeue(IRP *irp)
{
    FUSE_DATA *head;

    if ((irp == NULL) || (irp->fd_head == NULL))
        return NULL;

    if (irp->fd_head->next == NULL)
    {
        /* only one element in queue */
        head = irp->fd_head;
        irp->fd_head = NULL;
        irp->fd_tail = NULL;
        return head;
    }

    /* more than one element in queue */
    head = irp->fd_head;
    irp->fd_head = head->next;

    return head;
}

/**
 * Insert specified FUSE_DATA at the end of our queue
 *
 * @return 0 on success, -1 on failure
 *****************************************************************************/

int dev_redir_fuse_data_enqueue(IRP *irp, void *vp)
{
    FUSE_DATA *fd;
    FUSE_DATA *tail;

    if (irp == NULL)
        return -1;

    if ((fd = calloc(1, sizeof(FUSE_DATA))) == NULL)
        return -1;

    fd->data_ptr = vp;
    fd->next = NULL;

    if (irp->fd_tail == NULL)
    {
        /* queue is empty, insert at head */
        irp->fd_head = fd;
        irp->fd_tail = fd;
        return 0;
    }

    /* queue is not empty, insert at tail end */
    tail = irp->fd_tail;
    tail->next = fd;
    irp->fd_tail = fd;
    return 0;
}

/******************************************************************************
**                           miscellaneous stuff                             **
******************************************************************************/

void devredir_insert_DeviceIoRequest(struct stream *s,
                                     tui32 DeviceId,
                                     tui32 FileId,
                                     tui32 CompletionId,
                                     tui32 MajorFunction,
                                     tui32 MinorFunction)
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

void devredir_cvt_slash(char *path)
{
    char *cptr = path;

    while (*cptr != 0)
    {
        if (*cptr == '/')
            *cptr = '\\';
        cptr++;
    }
}

void devredir_cvt_to_unicode(char *unicode, char *path)
{
    int len = strlen(path);
    int i;
    int j = 0;

    for (i = 0; i < len; i++)
    {
        unicode[j++] = path[i];
        unicode[j++] = 0x00;
    }
    unicode[j++] = 0x00;
    unicode[j++] = 0x00;
}

void devredir_cvt_from_unicode_len(char *path, char *unicode, int len)
{
    int i;
    int j;

    for (i = 0, j = 0; i < len; i += 2)
    {
        path[j++] = unicode[i];
    }
    path[j] = 0;
}

int dev_redir_string_ends_with(char *string, char c)
{
    int len;

    len = strlen(string);
    return (string[len - 1] == c) ? 1 : 0;
}

void devredir_insert_RDPDR_header(struct stream *s, tui16 Component,
                                   tui16 PacketId)
{
    xstream_wr_u16_le(s, Component);
    xstream_wr_u16_le(s, PacketId);
}

void devredir_proc_cid_rmdir_or_file(IRP *irp, tui32 IoStatus)
{
    struct stream *s;
    int            bytes;

    if (IoStatus != NT_STATUS_SUCCESS)
    {
        FUSE_DATA *fuse_data = dev_redir_fuse_data_dequeue(irp);
        if (fuse_data)
        {
            xfuse_devredir_cb_rmdir_or_file(fuse_data->data_ptr, IoStatus);
            free(fuse_data);
        }
        devredir_irp_delete(irp);
        return;
    }

    xstream_new(s, 1024);

    irp->completion_type = CID_RMDIR_OR_FILE_RESP;
    devredir_insert_DeviceIoRequest(s, irp->DeviceId, irp->FileId,
                                    irp->CompletionId,
                                    IRP_MJ_SET_INFORMATION, 0);

    xstream_wr_u32_le(s, FileDispositionInformation);
    xstream_wr_u32_le(s, 0); /* length is zero */
    xstream_seek(s, 24);     /* padding        */

    /* send to client */
    bytes = xstream_len(s);
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);
    xstream_free(s);

    return;
}

void devredir_proc_cid_rmdir_or_file_resp(IRP *irp, tui32 IoStatus)
{
    FUSE_DATA *fuse_data;

    fuse_data = dev_redir_fuse_data_dequeue(irp);
    if (fuse_data)
    {
        xfuse_devredir_cb_rmdir_or_file(fuse_data->data_ptr, IoStatus);
        free(fuse_data);
    }

    if (IoStatus != NT_STATUS_SUCCESS)
    {
        devredir_irp_delete(irp);
        return;
    }

    irp->completion_type = CID_CLOSE;
    dev_redir_send_drive_close_request(RDPDR_CTYP_CORE,
                                       PAKID_CORE_DEVICE_IOREQUEST,
                                       irp->DeviceId,
                                       irp->FileId,
                                       irp->CompletionId,
                                       IRP_MJ_CLOSE, 0, 32);
}

void devredir_proc_cid_rename_file(IRP *irp, tui32 IoStatus)
{
    struct stream *s;
    int            bytes;
    int            sblen; /* SetBuffer length */
    int            flen;  /*FileNameLength    */


    if (IoStatus != NT_STATUS_SUCCESS)
    {
        log_debug("rename returned with IoStatus=0x%x", IoStatus);

        FUSE_DATA *fuse_data = dev_redir_fuse_data_dequeue(irp);
        if (fuse_data)
        {
            xfuse_devredir_cb_rename_file(fuse_data->data_ptr, IoStatus);
            free(fuse_data);
        }
        devredir_irp_delete(irp);
        return;
    }

    xstream_new(s, 1024);

    irp->completion_type = CID_RENAME_FILE_RESP;
    devredir_insert_DeviceIoRequest(s, irp->DeviceId, irp->FileId,
                                    irp->CompletionId,
                                    IRP_MJ_SET_INFORMATION, 0);

    flen = strlen(irp->gen_buf) * 2 + 2;
    sblen = 6 + flen;

    xstream_wr_u32_le(s, FileRenameInformation);
    xstream_wr_u32_le(s, sblen);     /* Length          */
    xstream_seek(s, 24);             /* padding         */
    xstream_wr_u8(s, 1);             /* ReplaceIfExists */
    xstream_wr_u8(s, 0);             /* RootDirectory   */
    xstream_wr_u32_le(s, flen);      /* FileNameLength  */

    /* filename in unicode */
    devredir_cvt_to_unicode(s->p, irp->gen_buf);
    xstream_seek(s, flen);

    /* send to client */
    bytes = xstream_len(s);
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);
    xstream_free(s);

    return;
}

void devredir_proc_cid_rename_file_resp(IRP *irp, tui32 IoStatus)
{
    FUSE_DATA *fuse_data;

    log_debug("entered");

    fuse_data = dev_redir_fuse_data_dequeue(irp);
    if (fuse_data)
    {
        xfuse_devredir_cb_rename_file(fuse_data->data_ptr, IoStatus);
        free(fuse_data);
    }

    if (IoStatus != NT_STATUS_SUCCESS)
    {
        devredir_irp_delete(irp);
        return;
    }

    irp->completion_type = CID_CLOSE;
    dev_redir_send_drive_close_request(RDPDR_CTYP_CORE,
                                       PAKID_CORE_DEVICE_IOREQUEST,
                                       irp->DeviceId,
                                       irp->FileId,
                                       irp->CompletionId,
                                       IRP_MJ_CLOSE, 0, 32);
}
