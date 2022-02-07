/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2012
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

/* MS-RDPECLIP
 * http://msdn.microsoft.com/en-us/library/cc241066%28prot.20%29.aspx
 *
 * CLIPRDR_FILEDESCRIPTOR
 * http://msdn.microsoft.com/en-us/library/ff362447%28prot.20%29.aspx */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <sys/time.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xfixes.h>
#include "arch.h"
#include "parse.h"
#include "os_calls.h"
#include "string_calls.h"
#include "list.h"
#include "chansrv.h"
#include "clipboard.h"
#include "clipboard_file.h"
#include "clipboard_common.h"
#include "xcommon.h"
#include "chansrv_fuse.h"
#include "ms-rdpeclip.h"

extern int g_cliprdr_chan_id; /* in chansrv.c */

extern struct clip_s2c g_clip_s2c; /* in clipboard.c */
extern struct clip_c2s g_clip_c2s; /* in clipboard.c */

extern char g_fuse_clipboard_path[];

struct cb_file_info
{
    char pathname[256];
    char filename[256];
    int flags;
    int size;
    tui64 time;
};

static struct list *g_files_list = 0;

/* used when server is asking for file info from the client */
static int g_file_request_sent_type = 0;

/* number of seconds from 1 Jan. 1601 00:00 to 1 Jan 1970 00:00 UTC */
#define CB_EPOCH_DIFF 11644473600LL

/*****************************************************************************/
#if 0
static tui64
timeval2wintime(struct timeval *tv)
{
    tui64 result;

    result = CB_EPOCH_DIFF;
    result += tv->tv_sec;
    result *= 10000000LL;
    result += tv->tv_usec * 10;
    return result;
}
#endif

/*****************************************************************************/
/* this will replace %20 or any hex with the space or correct char
 * returns error */
static int
clipboard_check_file(char *filename)
{
    char lfilename[256];
    char jchr[8];
    int lindex;
    int index;

    g_memset(lfilename, 0, 256);
    lindex = 0;
    index = 0;
    while (filename[index] != 0)
    {
        if (filename[index] == '%')
        {
            jchr[0] = filename[index + 1];
            jchr[1] = filename[index + 2];
            jchr[2] = 0;
            index += 3;
            lfilename[lindex] = g_htoi(jchr);
            lindex++;
        }
        else
        {
            lfilename[lindex] = filename[index];
            lindex++;
            index++;
        }
    }
    LOG_DEVEL(LOG_LEVEL_DEBUG, "[%s] [%s]", filename, lfilename);
    g_strcpy(filename, lfilename);
    return 0;
}

/*****************************************************************************/
static int
clipboard_get_file(const char *file, int bytes)
{
    int sindex;
    int pindex;
    int flags;
    char full_fn[256]; /* /etc/xrdp/xrdp.ini */
    char filename[256]; /* xrdp.ini */
    char pathname[256]; /* /etc/xrdp */
    struct cb_file_info *cfi;

    /* x-special/gnome-copied-files */
    if ((g_strncmp(file, "copy", 4) == 0) && (bytes == 4))
    {
        return 0;
    }
    if ((g_strncmp(file, "cut", 3) == 0) && (bytes == 3))
    {
        return 0;
    }
    sindex = 0;
    flags = CB_FILE_ATTRIBUTE_ARCHIVE;
    /* text/uri-list */
    /* x-special/gnome-copied-files */
    if (g_strncmp(file, "file://", 7) == 0)
    {
        sindex = 7;
    }
    pindex = bytes;
    while (pindex > sindex)
    {
        if (file[pindex] == '/')
        {
            break;
        }
        pindex--;
    }
    g_memset(pathname, 0, 256);
    g_memset(filename, 0, 256);
    g_memcpy(pathname, file + sindex, pindex - sindex);
    if (pathname[0] == 0)
    {
        pathname[0] = '/';
    }
    g_memcpy(filename, file + pindex + 1, (bytes - 1) - pindex);
    /* this should replace %20 with space */
    clipboard_check_file(pathname);
    clipboard_check_file(filename);
    g_snprintf(full_fn, 255, "%s/%s", pathname, filename);
    if (g_directory_exist(full_fn))
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "clipboard_get_file: file [%s] is a directory, "
                  "not supported", full_fn);
        flags |= CB_FILE_ATTRIBUTE_DIRECTORY;
        return 1;
    }
    if (!g_file_exist(full_fn))
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "clipboard_get_file: file [%s] does not exist",
                  full_fn);
        return 1;
    }
    else
    {
        cfi = (struct cb_file_info *)g_malloc(sizeof(struct cb_file_info), 1);
        list_add_item(g_files_list, (tintptr)cfi);
        g_strcpy(cfi->filename, filename);
        g_strcpy(cfi->pathname, pathname);
        cfi->size = g_file_get_size(full_fn);
        cfi->flags = flags;
        cfi->time = (g_time1() + CB_EPOCH_DIFF) * 10000000LL;
        LOG_DEVEL(LOG_LEVEL_DEBUG, "ok filename [%s] pathname [%s] size [%d]",
                  cfi->filename, cfi->pathname, cfi->size);
    }
    return 0;
}

/*****************************************************************************/
static int
clipboard_get_files(const char *files, int bytes)
{
    int index;
    int file_index;
    char file[512];

    file_index = 0;
    for (index = 0; index < bytes; index++)
    {
        if (files[index] == '\n' || files[index] == '\r')
        {
            if (file_index > 0)
            {
                if (clipboard_get_file(file, file_index) == 0)
                {
                }
                file_index = 0;
            }
        }
        else
        {
            file[file_index] = files[index];
            file_index++;
        }
    }
    if (file_index > 0)
    {
        if (clipboard_get_file(file, file_index) == 0)
        {
        }
    }
    if (g_files_list->count < 1)
    {
        return 1;
    }
    return 0;
}

/*****************************************************************************/
/* server to client */
/* response to client asking for clipboard contents that is file list */
int
clipboard_send_data_response_for_file(const char *data, int data_size)
{
    struct stream *s;
    int size;
    int rv;
    int bytes_after_header;
    int cItems;
    int flags;
    int index;
    tui32 ui32;
    char fn[256];
    struct cb_file_info *cfi;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "clipboard_send_data_response_for_file: data_size %d",
              data_size);
    LOG_DEVEL_HEXDUMP(LOG_LEVEL_TRACE, "", data, data_size);
    if (g_files_list == 0)
    {
        g_files_list = list_create();
        g_files_list->auto_free = 1;
    }
    list_clear(g_files_list);
    clipboard_get_files(data, data_size);
    cItems = g_files_list->count;
    bytes_after_header = cItems * 592 + 4;
    make_stream(s);
    init_stream(s, 64 + bytes_after_header);
    out_uint16_le(s, CB_FORMAT_DATA_RESPONSE); /* 5 CLIPRDR_DATA_RESPONSE */
    out_uint16_le(s, CB_RESPONSE_OK); /* 1 status */
    out_uint32_le(s, bytes_after_header);
    out_uint32_le(s, cItems);
    for (index = 0; index < cItems; index++)
    {
        cfi = (struct cb_file_info *)list_get_item(g_files_list, index);
        flags = CB_FD_ATTRIBUTES | CB_FD_FILESIZE | CB_FD_WRITESTIME | CB_FD_PROGRESSUI;
        out_uint32_le(s, flags);
        out_uint8s(s, 32); /* reserved1 */
        flags = cfi->flags;
        out_uint32_le(s, flags);
        out_uint8s(s, 16); /* reserved2 */
        /* file time */
        /* 100-nanoseconds intervals since 1 January 1601 */
        //out_uint32_le(s, 0x2c305d08); /* 25 October 2009, 21:17 */
        //out_uint32_le(s, 0x01ca55f3);
        ui32 = cfi->time & 0xffffffff;
        out_uint32_le(s, ui32);
        ui32 = cfi->time >> 32;
        out_uint32_le(s, ui32);
        /* file size */
        out_uint32_le(s, 0);
        out_uint32_le(s, cfi->size);
        g_snprintf(fn, 255, "%s", cfi->filename);
        clipboard_out_unicode(s, fn, 256);
        out_uint8s(s, 8); /* pad */
    }
    out_uint32_le(s, 0);
    s_mark_end(s);
    size = (int)(s->end - s->data);
    rv = send_channel_data(g_cliprdr_chan_id, s->data, size);
    free_stream(s);
    return rv;
}

/*****************************************************************************/
/* send the file size from server to the client */
static int
clipboard_send_file_size(int streamId, int lindex)
{
    struct stream *s;
    int size;
    int rv;
    int file_size;
    struct cb_file_info *cfi;

    if (g_files_list == 0)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "clipboard_send_file_size: error g_files_list is nil");
        return 1;
    }
    cfi = (struct cb_file_info *)list_get_item(g_files_list, lindex);
    if (cfi == 0)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "clipboard_send_file_size: error cfi is nil");
        return 1;
    }
    file_size = cfi->size;
    LOG_DEVEL(LOG_LEVEL_DEBUG, "clipboard_send_file_size: streamId %d file_size %d",
              streamId, file_size);
    make_stream(s);
    init_stream(s, 8192);
    out_uint16_le(s, CB_FILECONTENTS_RESPONSE); /* 9 */
    out_uint16_le(s, CB_RESPONSE_OK); /* 1 status */
    out_uint32_le(s, 12);
    out_uint32_le(s, streamId);
    out_uint32_le(s, file_size);
    out_uint32_le(s, 0);
    out_uint32_le(s, 0);
    s_mark_end(s);
    size = (int)(s->end - s->data);
    rv = send_channel_data(g_cliprdr_chan_id, s->data, size);
    free_stream(s);
    return rv;
}

/*****************************************************************************/
/* ask the client to send the file size */
int
clipboard_request_file_size(int stream_id, int lindex)
{
    struct stream *s;
    int size;
    int rv;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "clipboard_request_file_size:");
    if (g_file_request_sent_type != 0)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "clipboard_request_file_size: warning, still waiting "
                  "for CB_FILECONTENTS_RESPONSE");
    }
    make_stream(s);
    init_stream(s, 8192);
    out_uint16_le(s, CB_FILECONTENTS_REQUEST); /* 8 */
    out_uint16_le(s, 0);
    out_uint32_le(s, 28);
    out_uint32_le(s, stream_id);
    out_uint32_le(s, lindex);
    out_uint32_le(s, CB_FILECONTENTS_SIZE);
    out_uint32_le(s, 0); /* nPositionLow */
    out_uint32_le(s, 0); /* nPositionHigh */
    out_uint32_le(s, 0); /* cbRequested */
    out_uint32_le(s, 0); /* clipDataId */
    out_uint32_le(s, 0);
    s_mark_end(s);
    size = (int)(s->end - s->data);
    rv = send_channel_data(g_cliprdr_chan_id, s->data, size);
    free_stream(s);
    g_file_request_sent_type = CB_FILECONTENTS_SIZE;
    return rv;
}

/*****************************************************************************/
/* send a chunk of the file from server to client */
static int
clipboard_send_file_data(int streamId, int lindex,
                         int nPositionLow, int cbRequested)
{
    struct stream *s;
    int size;
    int rv;
    int fd;
    char full_fn[256];
    struct cb_file_info *cfi;

    if (g_files_list == 0)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "clipboard_send_file_data: error g_files_list is nil");
        return 1;
    }
    cfi = (struct cb_file_info *)list_get_item(g_files_list, lindex);
    if (cfi == 0)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "clipboard_send_file_data: error cfi is nil");
        return 1;
    }
    LOG_DEVEL(LOG_LEVEL_DEBUG, "clipboard_send_file_data: streamId %d lindex %d "
              "nPositionLow %d cbRequested %d", streamId, lindex,
              nPositionLow, cbRequested);
    g_snprintf(full_fn, 255, "%s/%s", cfi->pathname, cfi->filename);
    fd = g_file_open_ex(full_fn, 1, 0, 0, 0);
    if (fd == -1)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "clipboard_send_file_data: file open [%s] failed",
                  full_fn);
        return 1;
    }
    if (g_file_seek(fd, nPositionLow) < 0)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "clipboard_send_file_data: seek error "
                  "in file: %s", full_fn);
        g_file_close(fd);
        return 1;
    }
    make_stream(s);
    init_stream(s, cbRequested + 64);
    size = g_file_read(fd, s->data + 12, cbRequested);
    if (size < 1)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "clipboard_send_file_data: read error, want %d got %d",
                  cbRequested, size);
        free_stream(s);
        g_file_close(fd);
        return 1;
    }
    out_uint16_le(s, CB_FILECONTENTS_RESPONSE); /* 9 */
    out_uint16_le(s, CB_RESPONSE_OK); /* 1 status */
    out_uint32_le(s, size + 4);
    out_uint32_le(s, streamId);
    s->p += size;
    out_uint32_le(s, 0);
    s_mark_end(s);
    size = (int)(s->end - s->data);
    rv = send_channel_data(g_cliprdr_chan_id, s->data, size);
    free_stream(s);
    g_file_close(fd);

    /* Log who transferred which file via clipboard for the purpose of audit */
    LOG(LOG_LEVEL_INFO, "S2C: Transfered a file: filename=%s, uid=%d", full_fn, g_getuid());

    return rv;
}

/*****************************************************************************/
/* ask the client to send the file size */
int
clipboard_request_file_data(int stream_id, int lindex, int offset,
                            int request_bytes)
{
    struct stream *s;
    int size;
    int rv;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "clipboard_request_file_data: stream_id=%d lindex=%d off=%d request_bytes=%d",
              stream_id, lindex, offset, request_bytes);

    if (g_file_request_sent_type != 0)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "clipboard_request_file_data: warning, still waiting "
                  "for CB_FILECONTENTS_RESPONSE");
    }
    make_stream(s);
    init_stream(s, 8192);
    out_uint16_le(s, CB_FILECONTENTS_REQUEST); /* 8 */
    out_uint16_le(s, 0);
    out_uint32_le(s, 28);
    out_uint32_le(s, stream_id);
    out_uint32_le(s, lindex);
    out_uint32_le(s, CB_FILECONTENTS_RANGE);
    out_uint32_le(s, offset); /* nPositionLow */
    out_uint32_le(s, 0); /* nPositionHigh */
    out_uint32_le(s, request_bytes); /* cbRequested */
    out_uint32_le(s, 0); /* clipDataId */
    out_uint32_le(s, 0);
    s_mark_end(s);
    size = (int)(s->end - s->data);
    rv = send_channel_data(g_cliprdr_chan_id, s->data, size);
    free_stream(s);
    g_file_request_sent_type = CB_FILECONTENTS_RANGE;
    return rv;
}


/*****************************************************************************/
/* client is asking from info about a file */
int
clipboard_process_file_request(struct stream *s, int clip_msg_status,
                               int clip_msg_len)
{
    int streamId;
    int lindex;
    int dwFlags;
    int nPositionLow;
    int cbRequested;
    //int clipDataId;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "clipboard_process_file_request:");
    LOG_DEVEL_HEXDUMP(LOG_LEVEL_TRACE, "", s->p, clip_msg_len);
    in_uint32_le(s, streamId);
    in_uint32_le(s, lindex);
    in_uint32_le(s, dwFlags);
    in_uint32_le(s, nPositionLow);
    in_uint8s(s, 4); /* nPositionHigh */
    in_uint32_le(s, cbRequested);
    //in_uint32_le(s, clipDataId); /* options, used when locking */
    if (dwFlags & CB_FILECONTENTS_SIZE)
    {
        clipboard_send_file_size(streamId, lindex);
    }
    if (dwFlags & CB_FILECONTENTS_RANGE)
    {
        clipboard_send_file_data(streamId, lindex, nPositionLow, cbRequested);
    }
    return 0;
}

/*****************************************************************************/
/* server requested info about the file and this is the response
   it's either the file size or file data */
int
clipboard_process_file_response(struct stream *s, int clip_msg_status,
                                int clip_msg_len)
{
    int streamId;
    int file_size;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "clipboard_process_file_response:");
    if (g_file_request_sent_type == CB_FILECONTENTS_SIZE)
    {
        g_file_request_sent_type = 0;
        in_uint32_le(s, streamId);
        in_uint32_le(s, file_size);
        LOG_DEVEL(LOG_LEVEL_DEBUG, "clipboard_process_file_response: streamId %d "
                  "file_size %d", streamId, file_size);
        xfuse_file_contents_size(streamId, file_size);
    }
    else if (g_file_request_sent_type == CB_FILECONTENTS_RANGE)
    {
        g_file_request_sent_type = 0;
        in_uint32_le(s, streamId);
        xfuse_file_contents_range(streamId, s->p, clip_msg_len - 4);
    }
    else
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "clipboard_process_file_response: error");
        g_file_request_sent_type = 0;
    }
    return 0;
}

/*****************************************************************************/
/* read in CLIPRDR_FILEDESCRIPTOR */
static int
clipboard_c2s_in_file_info(struct stream *s, struct clip_file_desc *cfd)
{
    int num_chars;
    int ex_bytes;

    in_uint32_le(s, cfd->flags);
    in_uint8s(s, 32); /* reserved1 */
    in_uint32_le(s, cfd->fileAttributes);
    in_uint8s(s, 16); /* reserved2 */
    in_uint32_le(s, cfd->lastWriteTimeLow);
    in_uint32_le(s, cfd->lastWriteTimeHigh);
    in_uint32_le(s, cfd->fileSizeHigh);
    in_uint32_le(s, cfd->fileSizeLow);
    num_chars = 256;
    clipboard_in_unicode(s, cfd->cFileName, &num_chars);
    ex_bytes = 512 - num_chars * 2;
    ex_bytes -= 2;
    in_uint8s(s, ex_bytes);
    in_uint8s(s, 8); /* pad */
    LOG_DEVEL(LOG_LEVEL_DEBUG, "clipboard_c2s_in_file_info:");
    LOG_DEVEL(LOG_LEVEL_DEBUG, "  flags 0x%8.8x", cfd->flags);
    LOG_DEVEL(LOG_LEVEL_DEBUG, "  fileAttributes 0x%8.8x", cfd->fileAttributes);
    LOG_DEVEL(LOG_LEVEL_DEBUG, "  lastWriteTime 0x%8.8x%8.8x", cfd->lastWriteTimeHigh,
              cfd->lastWriteTimeLow);
    LOG_DEVEL(LOG_LEVEL_DEBUG, "  fileSize 0x%8.8x%8.8x", cfd->fileSizeHigh,
              cfd->fileSizeLow);
    LOG_DEVEL(LOG_LEVEL_DEBUG, "  num_chars %d cFileName [%s]", num_chars, cfd->cFileName);
    return 0;
}

/*****************************************************************************/
int
clipboard_c2s_in_files(struct stream *s, char *file_list)
{
    int cItems;
    int lindex;
    int str_len;
    int file_count;
    struct clip_file_desc *cfd;
    char *ptr;

    if (!s_check_rem(s, 4))
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "clipboard_c2s_in_files: parse error");
        return 1;
    }
    in_uint32_le(s, cItems);
    if (cItems > 64 * 1024) /* sanity check */
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "clipboard_c2s_in_files: error cItems %d too big", cItems);
        return 1;
    }
    xfuse_clear_clip_dir();
    LOG_DEVEL(LOG_LEVEL_DEBUG, "clipboard_c2s_in_files: cItems %d", cItems);
    cfd = (struct clip_file_desc *)
          g_malloc(sizeof(struct clip_file_desc), 0);
    file_count = 0;
    ptr = file_list;
    for (lindex = 0; lindex < cItems; lindex++)
    {
        g_memset(cfd, 0, sizeof(struct clip_file_desc));
        clipboard_c2s_in_file_info(s, cfd);
        if ((g_pos(cfd->cFileName, "\\") >= 0) ||
                (cfd->fileAttributes & CB_FILE_ATTRIBUTE_DIRECTORY))
        {
            LOG_DEVEL(LOG_LEVEL_ERROR, "clipboard_c2s_in_files: skipping directory not "
                      "supported [%s]", cfd->cFileName);
            continue;
        }
        if (xfuse_add_clip_dir_item(cfd->cFileName, 0, cfd->fileSizeLow, lindex) == -1)
        {
            LOG_DEVEL(LOG_LEVEL_ERROR, "clipboard_c2s_in_files: failed to add clip dir item");
            continue;
        }

        if (file_count > 0)
        {
            *ptr = '\n';
            ptr++;
        }
        file_count++;

        g_strcpy(ptr, "file://");
        ptr += 7;

        str_len = g_strlen(g_fuse_clipboard_path);
        g_strcpy(ptr, g_fuse_clipboard_path);
        ptr += str_len;

        *ptr = '/';
        ptr++;

        str_len = g_strlen(cfd->cFileName);
        g_strcpy(ptr, cfd->cFileName);
        ptr += str_len;
    }
    *ptr = 0;
    g_free(cfd);
    return 0;
}
