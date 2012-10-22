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

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xfixes.h>
#include "arch.h"
#include "parse.h"
#include "os_calls.h"
#include "chansrv.h"
#include "clipboard.h"
#include "clipboard_file.h"
#include "clipboard_common.h"
#include "xcommon.h"

#define LLOG_LEVEL 11
#define LLOGLN(_level, _args) \
  do \
  { \
    if (_level < LLOG_LEVEL) \
    { \
      g_write("chansrv:clip [%10.10u]: ", g_time3()); \
      g_writeln _args ; \
    } \
  } \
  while (0)

extern int g_cliprdr_chan_id; /* in chansrv.c */

extern struct clip_s2c g_clip_s2c; /* in clipboard.c */
extern struct clip_c2s g_clip_c2s; /* in clipboard.c */

struct cb_file_info
{
    char pathname[256];
    char filename[256];
    int flags;
    int size;
};

static struct cb_file_info g_files[64];
static int g_num_files = 0;

/*****************************************************************************/
static int APP_CC
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
    LLOGLN(0, ("[%s] [%s]", filename, lfilename));
    g_strcpy(filename, lfilename);
    return 0;
}

/*****************************************************************************/
static int APP_CC
clipboard_get_file(char* file, int bytes)
{
    int sindex;
    int pindex;
    char full_fn[256]; /* /etc/xrdp/xrdp.ini */
    char filename[256]; /* xrdp.ini */
    char pathname[256]; /* /etc/xrdp */

    sindex = 0;
    if (g_strncmp(file, "file:///", 8) == 0)
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
    clipboard_check_file(filename);
    g_snprintf(full_fn, 255, "%s/%s", pathname, filename);
    if (g_directory_exist(full_fn))
    {
        LLOGLN(0, ("clipboard_get_file: file [%s] is a directory, "
                   "not supported", full_fn));
        return 1;
    }
    if (!g_file_exist(full_fn))
    {
        LLOGLN(0, ("clipboard_get_file: file [%s] does not exist",
                   full_fn));
        return 1;
    }
    else
    {
        g_strcpy(g_files[g_num_files].filename, filename);
        g_strcpy(g_files[g_num_files].pathname, pathname);
        g_files[g_num_files].size = g_file_get_size(full_fn);
        g_writeln("ok filename [%s] pathname [%s] size [%d]",
                  g_files[g_num_files].filename,
                  g_files[g_num_files].pathname,
                  g_files[g_num_files].size);
        g_num_files++;
    }
    return 0;
}

/*****************************************************************************/
static int APP_CC
clipboard_get_files(char *files, int bytes)
{
    int index;
    int file_index;
    char file[512];

    g_num_files = 0;
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
        if (g_num_files > 60)
        {
            break;
        }
    }
    if (file_index > 0)
    {
        if (clipboard_get_file(file, file_index) == 0)
        {
        }
    }
    if (g_num_files < 1)
    {
        return 1;
    }
    return 0;
}

/*****************************************************************************/
/* server to client */
int APP_CC
clipboard_send_data_response_for_file(char *data, int data_size)
{
    struct stream *s;
    int size;
    int rv;
    int bytes_after_header;
    int cItems;
    int flags;
    int index;
    char fn[256];

    LLOGLN(10, ("clipboard_send_data_response_for_file: data_size %d",
                data_size));
    //g_hexdump(data, data_size);
    clipboard_get_files(data, data_size);
    cItems = g_num_files;
    bytes_after_header = cItems * 592 + 4;
    make_stream(s);
    init_stream(s, 64 + bytes_after_header);
    out_uint16_le(s, CB_FORMAT_DATA_RESPONSE); /* 5 CLIPRDR_DATA_RESPONSE */
    out_uint16_le(s, CB_RESPONSE_OK); /* 1 status */
    out_uint32_le(s, bytes_after_header);
    out_uint32_le(s, cItems);
    for (index = 0; index < cItems; index++)
    {
        flags = CB_FD_ATTRIBUTES | CB_FD_FILESIZE | CB_FD_WRITESTIME | CB_FD_PROGRESSUI;
        out_uint32_le(s, flags);
        out_uint8s(s, 32); /* reserved1 */
        flags = CB_FILE_ATTRIBUTE_ARCHIVE;
        //flags = CB_FILE_ATTRIBUTE_NORMAL;
        out_uint32_le(s, flags);
        out_uint8s(s, 16); /* reserved2 */
        /* file time */
        /* 100-nanoseconds intervals since 1 January 1601 */
        out_uint32_le(s, 0x2c305d08); /* 25 October 2009, 21:17 */
        out_uint32_le(s, 0x01ca55f3);
        /* file size */
        out_uint32_le(s, 0);
        out_uint32_le(s, g_files[index].size);
        //g_writeln("jay size %d", g_files[index].size);
        g_snprintf(fn, 255, "%s", g_files[index].filename);
        clipboard_out_unicode(s, fn, 256);
        out_uint8s(s, 8); /* pad */
    }
    out_uint32_le(s, 0);
    s_mark_end(s);
    size = (int)(s->end - s->data);
    //g_hexdump(s->data, size);
    rv = send_channel_data(g_cliprdr_chan_id, s->data, size);
    free_stream(s);
    return rv;
}

/*****************************************************************************/
static int APP_CC
clipboard_send_file_size(int streamId, int lindex)
{
    struct stream *s;
    int size;
    int rv;
    int file_size;

    file_size = g_files[lindex].size;
    LLOGLN(10, ("clipboard_send_file_size: streamId %d file_size %d",
                streamId, file_size));
    make_stream(s);
    init_stream(s, 8192);
    out_uint16_le(s, CB_FILECONTENTS_RESPONSE); /* 9 */
    out_uint16_le(s, CB_RESPONSE_OK); /* 1 status */
    out_uint32_le(s, 12);
    out_uint32_le(s, streamId);
    out_uint32_le(s, file_size);
    g_writeln("file_size %d", file_size);
    out_uint32_le(s, 0);
    out_uint32_le(s, 0);
    s_mark_end(s);
    size = (int)(s->end - s->data);
    rv = send_channel_data(g_cliprdr_chan_id, s->data, size);
    free_stream(s);
    return rv;
}

/*****************************************************************************/
static int APP_CC
clipboard_send_file_data(int streamId, int lindex,
                         int nPositionLow, int cbRequested)
{
    struct stream *s;
    int size;
    int rv;
    int fd;
    char full_fn[256];

    LLOGLN(10, ("clipboard_send_file_data: streamId %d lindex %d "
                "nPositionLow %d cbRequested %d", streamId, lindex,
                nPositionLow, cbRequested));
    g_snprintf(full_fn, 255, "%s/%s", g_files[lindex].pathname,
               g_files[lindex].filename);
    fd = g_file_open_ex(full_fn, 1, 0, 0, 0);
    if (fd == -1)
    {
        LLOGLN(0, ("clipboard_send_file_data: file open [%s] failed",
                   full_fn));
        return 1;
    }
    g_file_seek(fd, nPositionLow);
    make_stream(s);
    init_stream(s, cbRequested + 64);
    //g_memset(s->data + 12, 26, cbRequested);
    size = g_file_read(fd, s->data + 12, cbRequested);
    //g_writeln("size %d", size);
    if (size < 1)
    {
        LLOGLN(10, ("clipboard_send_file_data: read error, want %d got %d",
                    cbRequested, size));
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
    return rv;
}

/*****************************************************************************/
int APP_CC
clipboard_process_file_request(struct stream *s, int clip_msg_status,
                               int clip_msg_len)
{
    int streamId;
    int lindex;
    int dwFlags;
    int nPositionLow;
    int nPositionHigh;
    int cbRequested;
    //int clipDataId;

    LLOGLN(10, ("clipboard_process_file_request:"));
    //g_hexdump(s->p, clip_msg_len);
    in_uint32_le(s, streamId);
    in_uint32_le(s, lindex);
    in_uint32_le(s, dwFlags);
    in_uint32_le(s, nPositionLow);
    in_uint32_le(s, nPositionHigh);
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
