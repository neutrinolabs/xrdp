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

#include "devredir.h"

/* globals */
extern int g_rdpdr_chan_id; /* in chansrv.c */
int g_is_printer_redir_supported = 0;
int g_is_port_redir_supported = 0;
int g_is_drive_redir_supported = 0;
int g_is_smartcard_redir_supported = 0;
int g_drive_redir_version = 1;
char g_preferred_dos_name_for_filesystem[9];
char g_full_name_for_filesystem[1024];
tui32 g_completion_id = 1;

tui32 g_clientID;           /* unique client ID - announced by client */
tui32 g_device_id;          /* unique device ID - announced by client */
tui16 g_client_rdp_version; /* returned by client                     */
IRP *g_irp_head = NULL;
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
    stream_new(s, 1024);

    /* initiate drive redirection protocol by sending Server Announce Req  */
    stream_wr_u16_le(s, RDPDR_CTYP_CORE);
    stream_wr_u16_le(s, PAKID_CORE_SERVER_ANNOUNCE);
    stream_wr_u16_le(s, 0x0001);  /* server major ver                      */
    stream_wr_u16_le(s, 0x000C);  /* server minor ver  - pretend 2 b Win 7 */
    stream_wr_u32_le(s, u.clientID); /* unique ClientID                    */

    /* send data to client */
    bytes = stream_len(s);
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);

    stream_free(s);
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
            stream_new(g_input_stream, total_length);

        stream_copyin(g_input_stream, s->p, length);

        /* in last packet, chan_flags & 0x02 will be true */
        if ((chan_flags & 2) == 0)
            return 0;

        g_input_stream->p = g_input_stream->data;
        ls = g_input_stream;
    }

    /* read header from incoming data */
    stream_rd_u16_le(ls, comp_type);
    stream_rd_u16_le(ls, pktID);

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
            stream_seek(ls, 2);  /* major version, we ignore it */
            stream_rd_u16_le(ls, minor_ver);
            stream_rd_u32_le(ls, g_clientID);

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
            dev_redir_proc_client_devlist_announce_req(ls);
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
        stream_free(g_input_stream);
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

    stream_new(s, 1024);

    /* setup header */
    stream_wr_u16_le(s, RDPDR_CTYP_CORE);
    stream_wr_u16_le(s, PAKID_CORE_SERVER_CAPABILITY);

    stream_wr_u16_le(s, 5);                /* num of caps we are sending     */
    stream_wr_u16_le(s, 0x0000);           /* padding                        */

    /* setup general capability */
    stream_wr_u16_le(s, CAP_GENERAL_TYPE); /* CapabilityType                 */
    stream_wr_u16_le(s, 44);               /* CapabilityLength - len of this */
                                           /* CAPABILITY_SET in bytes, inc   */
                                           /* the header                     */
    stream_wr_u32_le(s, 2);                /* Version                        */
    stream_wr_u32_le(s, 2);                /* O.S type                       */
    stream_wr_u32_le(s, 0);                /* O.S version                    */
    stream_wr_u16_le(s, 1);                /* protocol major version         */
    stream_wr_u16_le(s, g_client_rdp_version); /* protocol minor version     */
    stream_wr_u32_le(s, 0xffff);           /* I/O code 1                     */
    stream_wr_u32_le(s, 0);                /* I/O code 2                     */
    stream_wr_u32_le(s, 7);                /* Extended PDU                   */
    stream_wr_u32_le(s, 0);                /* extra flags 1                  */
    stream_wr_u32_le(s, 0);                /* extra flags 2                  */
    stream_wr_u32_le(s, 2);                /* special type device cap        */

    /* setup printer capability */
    stream_wr_u16_le(s, CAP_PRINTER_TYPE);
    stream_wr_u16_le(s, 8);
    stream_wr_u32_le(s, 1);

    /* setup serial port capability */
    stream_wr_u16_le(s, CAP_PORT_TYPE);
    stream_wr_u16_le(s, 8);
    stream_wr_u32_le(s, 1);

    /* setup file system capability */
    stream_wr_u16_le(s, CAP_DRIVE_TYPE);   /* CapabilityType                 */
    stream_wr_u16_le(s, 8);                /* CapabilityLength - len of this */
                                           /* CAPABILITY_SET in bytes, inc   */
                                           /* the header                     */
    stream_wr_u32_le(s, 2);                /* Version                        */

    /* setup smart card capability */
    stream_wr_u16_le(s, CAP_SMARTCARD_TYPE);
    stream_wr_u16_le(s, 8);
    stream_wr_u32_le(s, 1);

    /* send to client */
    bytes = stream_len(s);
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);

    stream_free(s);
}

void dev_redir_send_server_clientID_confirm()
{
    struct stream *s;
    int            bytes;

    stream_new(s, 1024);

    /* setup stream */
    stream_wr_u16_le(s, RDPDR_CTYP_CORE);
    stream_wr_u16_le(s, PAKID_CORE_CLIENTID_CONFIRM);
    stream_wr_u16_le(s, 0x0001);
    stream_wr_u16_le(s, g_client_rdp_version);
    stream_wr_u32_le(s, g_clientID);

    /* send to client */
    bytes = stream_len(s);
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);

    stream_free(s);
}

void dev_redir_send_server_user_logged_on()
{
    struct stream *s;
    int            bytes;

    stream_new(s, 1024);

    /* setup stream */
    stream_wr_u16_le(s, RDPDR_CTYP_CORE);
    stream_wr_u16_le(s, PAKID_CORE_USER_LOGGEDON);

    /* send to client */
    bytes = stream_len(s);
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);

    stream_free(s);
}

void dev_redir_send_server_device_announce_resp(tui32 device_id)
{
    struct stream *s;
    int            bytes;

    stream_new(s, 1024);

    /* setup stream */
    stream_wr_u16_le(s, RDPDR_CTYP_CORE);
    stream_wr_u16_le(s, PAKID_CORE_DEVICE_REPLY);
    stream_wr_u32_le(s, device_id);
    stream_wr_u32_le(s, 0); /* ResultCode */

    /* send to client */
    bytes = stream_len(s);
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);

    stream_free(s);
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

    stream_new(s, 1024 + len);

    dev_redir_insert_dev_io_req_header(s,
                                       device_id,
                                       0,
                                       completion_id,
                                       IRP_MJ_CREATE,
                                       0);

    stream_wr_u32_le(s, DesiredAccess);     /* DesiredAccess   */
    stream_wr_u32_le(s, 0);                 /* AllocationSize high unused */
    stream_wr_u32_le(s, 0);                 /* AllocationSize low  unused */
    stream_wr_u32_le(s, 0);                 /* FileAttributes             */
    stream_wr_u32_le(s, 0);                 /* SharedAccess               */
    stream_wr_u32_le(s, CreateDisposition); /* CreateDisposition          */
    stream_wr_u32_le(s, CreateOptions);     /* CreateOptions              */
    stream_wr_u32_le(s, len);               /* PathLength                 */
    devredir_cvt_to_unicode(s->p, path);    /* path in unicode            */
    stream_seek(s, len);

    /* send to client */
    bytes = stream_len(s);
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);

    stream_free(s);
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

    stream_new(s, 1024);

    dev_redir_insert_dev_io_req_header(s, DeviceId, FileId, CompletionId,
                                       MajorFunction, MinorFunc);

    if (pad_len)
        stream_seek(s, pad_len);

    /* send to client */
    bytes = stream_len(s);
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);

    stream_free(s);
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

    stream_new(s, 1024 + path_len);

    irp->completion_type = CID_DIRECTORY_CONTROL;
    dev_redir_insert_dev_io_req_header(s,
                                       device_id,
                                       irp->FileId,
                                       irp->completion_id,
                                       IRP_MJ_DIRECTORY_CONTROL,
                                       IRP_MN_QUERY_DIRECTORY);

#ifdef USE_SHORT_NAMES_IN_DIR_LISTING
    stream_wr_u32_le(s, FileBothDirectoryInformation); /* FsInformationClass */
#else
    stream_wr_u32_le(s, FileDirectoryInformation);  /* FsInformationClass */
#endif
    stream_wr_u8(s, InitialQuery);                  /* InitialQuery       */

    if (!InitialQuery)
    {
        stream_wr_u32_le(s, 0);                     /* PathLength         */
        stream_seek(s, 23);
    }
    else
    {
        stream_wr_u32_le(s, path_len);              /* PathLength         */
        stream_seek(s, 23);                         /* Padding            */
        stream_wr_string(s, upath, path_len);
    }

    /* send to client */
    bytes = stream_len(s);
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);

    stream_free(s);
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

    stream_rd_u16_le(s, num_caps);
    stream_seek(s, 2);  /* padding */

    for (i = 0; i < num_caps; i++)
    {
        stream_rd_u16_le(s, cap_type);
        stream_rd_u16_le(s, cap_len);
        stream_rd_u32_le(s, cap_version);

        /* remove header length and version */
        cap_len -= 8;

        switch (cap_type)
        {
            case CAP_GENERAL_TYPE:
                log_debug("got CAP_GENERAL_TYPE");
                stream_seek(s, cap_len);
                break;

            case CAP_PRINTER_TYPE:
                log_debug("got CAP_PRINTER_TYPE");
                g_is_printer_redir_supported = 1;
                stream_seek(s, cap_len);
                break;

            case CAP_PORT_TYPE:
                log_debug("got CAP_PORT_TYPE");
                g_is_port_redir_supported = 1;
                stream_seek(s, cap_len);
                break;

            case CAP_DRIVE_TYPE:
                log_debug("got CAP_DRIVE_TYPE");
                g_is_drive_redir_supported = 1;
                if (cap_version == 2)
                    g_drive_redir_version = 2;
                stream_seek(s, cap_len);
                break;

            case CAP_SMARTCARD_TYPE:
                log_debug("got CAP_SMARTCARD_TYPE");
                g_is_smartcard_redir_supported = 1;
                stream_seek(s, cap_len);
                break;
        }
    }
}

void dev_redir_proc_client_devlist_announce_req(struct stream *s)
{
    int   i;
    int   j;
    tui32 device_count;
    tui32 device_type;
    tui32 device_data_len;

    /* get number of devices being announced */
    stream_rd_u32_le(s, device_count);

    log_debug("num of devices announced: %d", device_count);

    for (i = 0; i < device_count; i++)
    {
        stream_rd_u32_le(s, device_type);
        stream_rd_u32_le(s, g_device_id); /* LK_TODO need to support */
                                          /* multiple drives         */

        switch (device_type)
        {
            case RDPDR_DTYP_FILESYSTEM:
                /* get preferred DOS name */
                for (j = 0; j < 8; j++)
                {
                    g_preferred_dos_name_for_filesystem[j] = *s->p++;
                }

                /* DOS names that are 8 chars long are not NULL terminated */
                g_preferred_dos_name_for_filesystem[8] = 0;

                /* LK_TODO need to check for invalid chars in DOS name */
                /* see section 2.2.1.3 of the protocol documentation   */

                /* get device data len */
                stream_rd_u32_le(s, device_data_len);
                if (device_data_len)
                {
                    stream_rd_string(g_full_name_for_filesystem, s,
                                     device_data_len);
                }

                log_debug("device_type=FILE_SYSTEM device_id=0x%x dosname=%s "
                          "device_data_len=%d full_name=%s", g_device_id,
                          g_preferred_dos_name_for_filesystem,
                          device_data_len, g_full_name_for_filesystem);

                dev_redir_send_server_device_announce_resp(g_device_id);

                /* create share directory in xrdp file system;    */
                /* think of this as the mount point for this share */
                xfuse_create_share(g_device_id,
                                   g_preferred_dos_name_for_filesystem);
                break;

            /* we don't yet support these devices */
            case RDPDR_DTYP_SERIAL:
            case RDPDR_DTYP_PARALLEL:
            case RDPDR_DTYP_PRINT:
            case RDPDR_DTYP_SMARTCARD:
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

    stream_rd_u32_le(s, DeviceId);
    stream_rd_u32_le(s, CompletionId);
    stream_rd_u32_le(s, IoStatus);

    /* LK_TODO need to check for IoStatus */

    log_debug("entered: IoStatus=0x%x CompletionId=%d", IoStatus, CompletionId);

    if ((irp = dev_redir_irp_find(CompletionId)) == NULL)
    {
        log_error("IRP with completion ID %d not found", CompletionId);
        return;
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
            dev_redir_irp_delete(irp);
            return;
        }

        stream_rd_u32_le(s, irp->FileId);
        log_debug("got CID_CREATE_DIR_REQ IoStatus=0x%x FileId=%d",
                  IoStatus, irp->FileId);

        dev_redir_send_drive_dir_request(irp, DeviceId, 1, irp->pathname);
        break;

    case CID_CREATE_OPEN_REQ:
        stream_rd_u32_le(s, irp->FileId);
        log_debug("got CID_CREATE_OPEN_REQ IoStatus=0x%x FileId=%d",
                  IoStatus, irp->FileId);
        fuse_data = dev_redir_fuse_data_dequeue(irp);
        xfuse_devredir_cb_open_file(fuse_data->data_ptr,
                                    DeviceId, irp->FileId);
        if (irp->type == S_IFDIR)
            dev_redir_irp_delete(irp);
        break;

    case CID_READ:
        log_debug("got CID_READ");
        stream_rd_u32_le(s, Length);
        fuse_data = dev_redir_fuse_data_dequeue(irp);
        xfuse_devredir_cb_read_file(fuse_data->data_ptr, s->p, Length);
        break;

    case CID_WRITE:
        log_debug("got CID_WRITE");
        stream_rd_u32_le(s, Length);
        fuse_data = dev_redir_fuse_data_dequeue(irp);
        xfuse_devredir_cb_write_file(fuse_data->data_ptr, s->p, Length);
        break;

    case CID_CLOSE:
        log_debug("got CID_CLOSE");
        log_debug("deleting irp with completion_id=%d comp_type=%d",
                  irp->completion_id, irp->completion_type);
        dev_redir_irp_delete(irp);
        break;

    case CID_FILE_CLOSE:
        log_debug("got CID_FILE_CLOSE");
        fuse_data = dev_redir_fuse_data_dequeue(irp);
        xfuse_devredir_cb_file_close(fuse_data->data_ptr);
        dev_redir_irp_delete(irp);
        break;

    case CID_DIRECTORY_CONTROL:
        log_debug("got CID_DIRECTORY_CONTROL");

        dev_redir_proc_query_dir_response(irp, s, DeviceId,
                                          CompletionId, IoStatus);
        break;

    case CID_RMDIR_OR_FILE:
        log_debug("got CID_RMDIR_OR_FILE");
        stream_rd_u32_le(s, irp->FileId);
        devredir_proc_cid_rmdir_or_file(irp, IoStatus);
        return;
        break;

    case CID_RMDIR_OR_FILE_RESP:
        log_debug("got CID_RMDIR_OR_FILE_RESP");
        devredir_proc_cid_rmdir_or_file_resp(irp, IoStatus);
        break;

    case CID_RENAME_FILE:
        log_debug("got CID_RENAME_FILE");
        stream_rd_u32_le(s, irp->FileId);
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

    stream_rd_u32_le(s_in, Length);

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
                                           irp->completion_id,
                                           IRP_MJ_CLOSE, 0, 32);
        free(fuse_data);
        return;
    }

    /* TODO check status for errors */

    /* process FILE_DIRECTORY_INFORMATION structures */
    while (i < Length)
    {
        log_debug("processing FILE_DIRECTORY_INFORMATION structs");

        stream_rd_u32_le(s_in, NextEntryOffset);
        stream_seek(s_in, 4);  /* FileIndex */
        stream_rd_u64_le(s_in, CreationTime);
        stream_rd_u64_le(s_in, LastAccessTime);
        stream_rd_u64_le(s_in, LastWriteTime);
        stream_rd_u64_le(s_in, ChangeTime);
        stream_rd_u64_le(s_in, EndOfFile);
        stream_seek(s_in, 8);  /* AllocationSize */
        stream_rd_u32_le(s_in, FileAttributes);
        stream_rd_u32_le(s_in, FileNameLength);

#ifdef USE_SHORT_NAMES_IN_DIR_LISTING
        stream_rd_u32_le(s_in, EaSize);
        stream_rd_u8(s_in, ShortNameLength);
        stream_rd_u8(s_in, Reserved);
        stream_seek(s_in, 23);  /* ShortName in Unicode */
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

    if ((irp = dev_redir_irp_new()) == NULL)
        return -1;

    /* cvt / to windows compatible \ */
    devredir_cvt_slash(path);

    irp->completion_id = g_completion_id++;
    irp->completion_type = CID_CREATE_DIR_REQ;
    irp->device_id = device_id;
    strcpy(irp->pathname, path);
    dev_redir_fuse_data_enqueue(irp, fusep);

    DesiredAccess = DA_FILE_READ_DATA | DA_SYNCHRONIZE;
    CreateOptions = CO_FILE_DIRECTORY_FILE | CO_FILE_SYNCHRONOUS_IO_NONALERT;
    CreateDisposition = CD_FILE_OPEN;

    rval = dev_redir_send_drive_create_request(device_id, path,
                                               DesiredAccess, CreateOptions,
                                               CreateDisposition,
                                               irp->completion_id);

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

    if ((irp = dev_redir_irp_new()) == NULL)
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

    irp->completion_id = g_completion_id++;
    irp->device_id = device_id;
    strcpy(irp->pathname, path);
    dev_redir_fuse_data_enqueue(irp, fusep);

    if (mode & O_CREAT)
    {
        log_debug("open file in O_CREAT");
        DesiredAccess = DA_FILE_READ_DATA | DA_FILE_WRITE_DATA | DA_SYNCHRONIZE;

        if (type & S_IFDIR)
        {
            log_debug("creating dir");
            CreateOptions = CO_FILE_DIRECTORY_FILE | CO_FILE_SYNCHRONOUS_IO_NONALERT;
            irp->type = S_IFDIR;
        }
        else
        {
            log_debug("creating file");
            CreateOptions = CO_FILE_SYNCHRONOUS_IO_NONALERT;
        }

        CreateDisposition = CD_FILE_CREATE;
    }
    else //if (mode & O_RDWR)
    {
        log_debug("open file in O_RDWR");
        DesiredAccess = DA_FILE_READ_DATA | DA_FILE_WRITE_DATA | DA_SYNCHRONIZE;
        CreateOptions = CO_FILE_SYNCHRONOUS_IO_NONALERT;
        CreateDisposition = CD_FILE_OPEN; // WAS 1
    }
#if 0
    else
    {
        log_debug("open file in O_RDONLY");
        DesiredAccess = DA_FILE_READ_DATA | DA_SYNCHRONIZE;
        CreateOptions = CO_FILE_SYNCHRONOUS_IO_NONALERT;
        CreateDisposition = CD_FILE_OPEN;
    }
#endif
    rval = dev_redir_send_drive_create_request(device_id, path,
                                               DesiredAccess, CreateOptions,
                                               CreateDisposition,
                                               irp->completion_id);

    return rval;
}

int devredir_file_close(void *fusep, tui32 device_id, tui32 FileId)
{
    IRP *irp;

#if 0
    if ((irp = dev_redir_irp_new()) == NULL)
        return -1;

    irp->completion_id = g_completion_id++;
#else
    if ((irp = dev_redir_irp_find_by_fileid(FileId)) == NULL)
    {
        log_error("no IRP found with FileId = %d", FileId);
        xfuse_devredir_cb_read_file(fusep, NULL, 0);
        return -1;
    }
#endif
    irp->completion_type = CID_FILE_CLOSE;
    irp->device_id = device_id;
    dev_redir_fuse_data_enqueue(irp, fusep);

    return dev_redir_send_drive_close_request(RDPDR_CTYP_CORE,
                                              PAKID_CORE_DEVICE_IOREQUEST,
                                              device_id,
                                              FileId,
                                              irp->completion_id,
                                              IRP_MJ_CLOSE,
                                              0, 32);
}

/**
 * Remove (delete) a directory
 *****************************************************************************/

int devredir_rmdir_or_file(void *fusep, tui32 device_id, char *path, int mode)
{
    tui32  DesiredAccess;
    tui32  CreateOptions;
    tui32  CreateDisposition;
    int    rval;
    IRP   *irp;

    if ((irp = dev_redir_irp_new()) == NULL)
        return -1;

    irp->completion_id = g_completion_id++;
    irp->completion_type = CID_RMDIR_OR_FILE;
    irp->device_id = device_id;
    strcpy(irp->pathname, path);
    dev_redir_fuse_data_enqueue(irp, fusep);

    // LK_TODO
    //DesiredAccess = DA_DELETE | DA_FILE_READ_DATA | DA_FILE_WRITE_DATA | DA_SYNCHRONIZE;
    DesiredAccess = DA_DELETE | DA_FILE_READ_ATTRIBUTES | DA_SYNCHRONIZE;

    CreateOptions = CO_FILE_DELETE_ON_CLOSE | CO_FILE_DIRECTORY_FILE |
                    CO_FILE_SYNCHRONOUS_IO_NONALERT;

    CreateDisposition = CD_FILE_OPEN; // WAS 1

    rval = dev_redir_send_drive_create_request(device_id, path,
                                               DesiredAccess, CreateOptions,
                                               CreateDisposition,
                                               irp->completion_id);

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

    stream_new(s, 1024);

    if ((irp = dev_redir_irp_find_by_fileid(FileId)) == NULL)
    {
        log_error("no IRP found with FileId = %d", FileId);
        xfuse_devredir_cb_read_file(fusep, NULL, 0);
        return -1;
    }

    irp->completion_type = CID_READ;
    dev_redir_fuse_data_enqueue(irp, fusep);
    dev_redir_insert_dev_io_req_header(s,
                                       DeviceId,
                                       FileId,
                                       irp->completion_id,
                                       IRP_MJ_READ,
                                       0);

    stream_wr_u32_le(s, Length);
    stream_wr_u64_le(s, Offset);
    stream_seek(s, 20);

    /* send to client */
    bytes = stream_len(s);
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);
    stream_free(s);

    return 0;
}

int dev_redir_file_write(void *fusep, tui32 DeviceId, tui32 FileId,
                         const char *buf, tui32 Length, tui64 Offset)
{
    struct stream *s;
    IRP           *irp;
    int            bytes;

    stream_new(s, 1024 + Length);

    if ((irp = dev_redir_irp_find_by_fileid(FileId)) == NULL)
    {
        log_error("no IRP found with FileId = %d", FileId);
        xfuse_devredir_cb_write_file(fusep, NULL, 0);
        return -1;
    }

    irp->completion_type = CID_WRITE;
    dev_redir_fuse_data_enqueue(irp, fusep);
    dev_redir_insert_dev_io_req_header(s,
                                       DeviceId,
                                       FileId,
                                       irp->completion_id,
                                       IRP_MJ_WRITE,
                                       0);

    stream_wr_u32_le(s, Length);
    stream_wr_u64_le(s, Offset);
    stream_seek(s, 20); /* padding */

    /* now insert real data */
    stream_copyin(s, buf, Length);

    /* send to client */
    bytes = stream_len(s);
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);
    stream_free(s);

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
**                                IRP stuff                                  **
******************************************************************************/

/**
 * Create a new IRP and append to linked list
 *
 * @return new IRP or NULL on error
 *****************************************************************************/

IRP * dev_redir_irp_new()
{
    IRP *irp;
    IRP *irp_last;

    log_debug("=== entered");

    /* create new IRP */
    if ((irp = calloc(1, sizeof(IRP))) == NULL)
    {
        log_error("system out of memory!");
        return NULL;
    }

    /* insert at end of linked list */
    if ((irp_last = dev_redir_irp_get_last()) == NULL)
    {
        /* list is empty, this is the first entry */
        g_irp_head = irp;
    }
    else
    {
        irp_last->next = irp;
        irp->prev = irp_last;
    }

    return irp;
}

/**
 * Delete specified IRP from linked list
 *
 * @return 0 on success, -1 on failure
 *****************************************************************************/

int dev_redir_irp_delete(IRP *irp)
{
    IRP *lirp = g_irp_head;

    log_debug("=== entered; completion_id=%d type=%d",
              irp->completion_id, irp->completion_type);

    if ((irp == NULL) || (lirp == NULL))
        return -1;

    dev_redir_irp_dump(); // LK_TODO

    while (lirp)
    {
        if (lirp == irp)
            break;

        lirp = lirp->next;
    }

    if (lirp == NULL)
        return -1; /* did not find specified irp */

    if (lirp->prev == NULL)
    {
        /* we are at head of linked list */
        if (lirp->next == NULL)
        {
            /* only one element in list */
            free(lirp);
            g_irp_head = NULL;
            dev_redir_irp_dump(); // LK_TODO
            return 0;
        }

        lirp->next->prev = NULL;
        g_irp_head = lirp->next;
        free(lirp);
    }
    else if (lirp->next == NULL)
    {
        /* we are at tail of linked list */
        lirp->prev->next = NULL;
        free(lirp);
    }
    else
    {
        /* we are in between */
        lirp->prev->next = lirp->next;
        lirp->next->prev = lirp->prev;
        free(lirp);
    }

    dev_redir_irp_dump(); // LK_TODO

    return 0;
}

/**
 * Return IRP containing specified completion_id
 *****************************************************************************/

IRP *dev_redir_irp_find(tui32 completion_id)
{
    IRP *irp = g_irp_head;

    while (irp)
    {
        if (irp->completion_id == completion_id)
            return irp;

        irp = irp->next;
    }

    return NULL;
}

IRP * dev_redir_irp_find_by_fileid(tui32 FileId)
{
    IRP *irp = g_irp_head;

    while (irp)
    {
        if (irp->FileId == FileId)
            return irp;

        irp = irp->next;
    }

    return NULL;
}

/**
 * Return last IRP in linked list
 *****************************************************************************/

IRP * dev_redir_irp_get_last()
{
    IRP *irp = g_irp_head;

    while (irp)
    {
        if (irp->next == NULL)
            break;

        irp = irp->next;
    }

    return irp;
}

void dev_redir_irp_dump()
{
    IRP *irp = g_irp_head;

    log_debug("------- dumping IRPs --------");
    while (irp)
    {
        log_debug("        completion_id=%d\tcompletion_type=%d\tFileId=%d",
                  irp->completion_id, irp->completion_type, irp->FileId);

        irp = irp->next;
    }
    log_debug("------- dumping IRPs done ---");
}

/******************************************************************************
**                           miscellaneous stuff                             **
******************************************************************************/

void dev_redir_insert_dev_io_req_header(struct stream *s,
                                        tui32 DeviceId,
                                        tui32 FileId,
                                        tui32 CompletionId,
                                        tui32 MajorFunction,
                                        tui32 MinorFunction)
{
    /* setup DR_DEVICE_IOREQUEST header */
    stream_wr_u16_le(s, RDPDR_CTYP_CORE);
    stream_wr_u16_le(s, PAKID_CORE_DEVICE_IOREQUEST);
    stream_wr_u32_le(s, DeviceId);
    stream_wr_u32_le(s, FileId);
    stream_wr_u32_le(s, CompletionId);
    stream_wr_u32_le(s, MajorFunction);
    stream_wr_u32_le(s, MinorFunction);
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

void dev_redir_insert_rdpdr_header(struct stream *s, tui16 Component,
                                   tui16 PacketId)
{
    stream_wr_u16_le(s, Component);
    stream_wr_u16_le(s, PacketId);
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
        dev_redir_irp_delete(irp);
        return;
    }

    stream_new(s, 1024);

    irp->completion_type = CID_RMDIR_OR_FILE_RESP;
    dev_redir_insert_dev_io_req_header(s, irp->device_id, irp->FileId,
                                       irp->completion_id,
                                       IRP_MJ_SET_INFORMATION, 0);

    stream_wr_u32_le(s, FileDispositionInformation);
    stream_wr_u32_le(s, 0); /* length is zero */
    stream_seek(s, 24);     /* padding        */

    /* send to client */
    bytes = stream_len(s);
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);
    stream_free(s);

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
        dev_redir_irp_delete(irp);
        return;
    }

    irp->completion_type = CID_CLOSE;
    dev_redir_send_drive_close_request(RDPDR_CTYP_CORE,
                                       PAKID_CORE_DEVICE_IOREQUEST,
                                       irp->device_id,
                                       irp->FileId,
                                       irp->completion_id,
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
        FUSE_DATA *fuse_data = dev_redir_fuse_data_dequeue(irp);
        if (fuse_data)
        {
            xfuse_devredir_cb_rename_file(fuse_data->data_ptr, IoStatus);
            free(fuse_data);
        }
        dev_redir_irp_delete(irp);
        return;
    }

    stream_new(s, 1024);

    irp->completion_type = CID_RENAME_FILE_RESP;
    dev_redir_insert_dev_io_req_header(s, irp->device_id, irp->FileId,
                                       irp->completion_id,
                                       IRP_MJ_SET_INFORMATION, 0);

    flen = strlen(irp->gen_buf) * 2 + 2;
    sblen = 6 + flen;

    stream_wr_u32_le(s, FileRenameInformation);
    stream_wr_u32_le(s, sblen);     /* Length          */
    stream_seek(s, 24);             /* padding         */
    stream_wr_u8(s, 1);             /* ReplaceIfExists */
    stream_wr_u8(s, 0);             /* RootDirectory   */
    stream_wr_u32_le(s, flen);      /* FileNameLength  */

    /* filename in unicode */
    devredir_cvt_to_unicode(s->p, irp->gen_buf);
    stream_seek(s, flen);

    /* send to client */
    bytes = stream_len(s);
    send_channel_data(g_rdpdr_chan_id, s->data, bytes);
    stream_free(s);

    return;
}

void devredir_proc_cid_rename_file_resp(IRP *irp, tui32 IoStatus)
{
    FUSE_DATA *fuse_data;

    fuse_data = dev_redir_fuse_data_dequeue(irp);
    if (fuse_data)
    {
        xfuse_devredir_cb_rename_file(fuse_data->data_ptr, IoStatus);
        free(fuse_data);
    }

    if (IoStatus != NT_STATUS_SUCCESS)
    {
        dev_redir_irp_delete(irp);
        return;
    }

    irp->completion_type = CID_CLOSE;
    dev_redir_send_drive_close_request(RDPDR_CTYP_CORE,
                                       PAKID_CORE_DEVICE_IOREQUEST,
                                       irp->device_id,
                                       irp->FileId,
                                       irp->completion_id,
                                       IRP_MJ_CLOSE, 0, 32);
}
