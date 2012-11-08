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

#ifdef XRDP_FUSE

#define FUSE_USE_VERSION 26
#define _FILE_OFFSET_BITS 64

#include <fuse/fuse_lowlevel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include "arch.h"
#include "parse.h"
#include "list.h"
#include "os_calls.h"
#include "chansrv.h"
#include "chansrv_fuse.h"
#include "clipboard_file.h"

#define LLOG_LEVEL 1
#define LLOGLN(_level, _args) \
  do \
  { \
    if (_level < LLOG_LEVEL) \
    { \
      g_write("chansrv:fuse [%10.10u]: ", g_time3()); \
      g_writeln _args ; \
    } \
  } \
  while (0)

char g_fuse_root_path[256] = "";

static struct fuse_chan *g_ch = 0;
static struct fuse_session *g_se = 0;
static char *g_mountpoint = 0;
static tintptr g_bufsize = 0;
static char *g_buffer = 0;
static int g_fd = 0;
static time_t g_time = 0;
static int g_uid = 0;
static int g_gid = 0;

/* used for file data request sent to client */
struct req_list_item
{
    fuse_req_t req;
    int stream_id;
    int lindex;
    int off;
    int size;
};
static struct list *g_req_list = 0;

struct dirbuf
{
    char *p;
    int size;
    int alloc_bytes;
};

struct xfuse_file_info
{
    int ino;
    int lindex;
    char pathname[256];
    char filename[256];
    int flags;
    int size;
    tui64 time;
    struct xfuse_file_info* child;
    struct xfuse_file_info* parent;
    struct xfuse_file_info* next;
    struct xfuse_file_info* prev;
};

static struct xfuse_file_info *g_fuse_files = 0;
static struct fuse_lowlevel_ops g_xrdp_ll_oper;
static int g_ino = 2;

/*****************************************************************************/
static struct xfuse_file_info *APP_CC
fuse_find_file_info_by_name(struct xfuse_file_info *ffi, const char *filename)
{
    struct xfuse_file_info *rv;
    struct xfuse_file_info *rv1;

    rv = ffi;
    while (rv != 0)
    {
        if (g_strcmp(rv->filename, filename) == 0)
        {
            return rv;
        }
        if (rv->flags & 1)
        {
            rv1 = fuse_find_file_info_by_name(rv->child, filename);
            if (rv1 != 0)
            {
                return rv1;
            }
        }
        rv = rv->next;
    }
    return 0;
}

/*****************************************************************************/
static struct xfuse_file_info *APP_CC
fuse_find_file_info_by_ino(struct xfuse_file_info *ffi, int ino)
{
    struct xfuse_file_info *rv;
    struct xfuse_file_info *rv1;

    rv = ffi;
    while (rv != 0)
    {
        if (rv->ino == ino)
        {
            return rv;
        }
        if (rv->flags & 1)
        {
            rv1 = fuse_find_file_info_by_ino(rv->child, ino);
            if (rv1 != 0)
            {
                return rv1;
            }
        }
        rv = rv->next;
    }
    return 0;
}

/*****************************************************************************/
static int APP_CC
xrdp_ffi2stat(struct xfuse_file_info *ffi, struct stat *stbuf)
{
    stbuf->st_ino = ffi->ino;
    if (ffi->flags & 1)
    {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        stbuf->st_uid = g_uid;
        stbuf->st_gid = g_gid;
        stbuf->st_atime = g_time;
        stbuf->st_mtime = g_time;
        stbuf->st_ctime = g_time;
    }
    else
    {
        stbuf->st_mode = S_IFREG | 0664;
        stbuf->st_nlink = 1;
        stbuf->st_size = ffi->size;
        stbuf->st_uid = g_uid;
        stbuf->st_gid = g_gid;
        stbuf->st_atime = g_time;
        stbuf->st_mtime = g_time;
        stbuf->st_ctime = g_time;
    }
    return 0;
}

/*****************************************************************************/
static void DEFAULT_CC
xrdp_ll_lookup(fuse_req_t req, fuse_ino_t parent, const char *name)
{
    struct xfuse_file_info *ffi;
    struct fuse_entry_param e;

    LLOGLN(0, ("xrdp_ll_lookup: name %s", name));
    if (parent != 1)
    {
        fuse_reply_err(req, ENOENT);
    }
    else
    {
        ffi = fuse_find_file_info_by_name(g_fuse_files, name);
        if (ffi != 0)
        {
            LLOGLN(0, ("xrdp_ll_lookup: name %s ino %d", name, ffi->ino));
            g_memset(&e, 0, sizeof(e));
            e.ino = ffi->ino;
            e.attr_timeout = 1.0;
            e.entry_timeout = 1.0;
            xrdp_ffi2stat(ffi, &e.attr);
            fuse_reply_entry(req, &e);
            return;
        }
    }
    fuse_reply_err(req, ENOENT);
}

/*****************************************************************************/
static void DEFAULT_CC
xrdp_ll_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
    struct stat stbuf;
    struct xfuse_file_info *ffi;

    LLOGLN(0, ("xrdp_ll_getattr: ino %d", ino));
    g_memset(&stbuf, 0, sizeof(stbuf));
    if (ino == 1)
    {
        stbuf.st_mode = S_IFDIR | 0755;
        stbuf.st_nlink = 2;
        stbuf.st_uid = g_uid;
        stbuf.st_gid = g_gid;
        stbuf.st_atime = g_time;
        stbuf.st_mtime = g_time;
        stbuf.st_ctime = g_time;
        fuse_reply_attr(req, &stbuf, 1.0);
        return;
    }
    ffi = fuse_find_file_info_by_ino(g_fuse_files, ino);
    if (ffi == 0)
    {
        LLOGLN(0, ("xrdp_ll_getattr: fuse_find_file_info_by_ino failed ino %d", ino));
        fuse_reply_err(req, ENOENT);
    }
    else if (xrdp_ffi2stat(ffi, &stbuf) == -1)
    {
        fuse_reply_err(req, ENOENT);
    }
    else
    {
        fuse_reply_attr(req, &stbuf, 1.0);
    }
}

/*****************************************************************************/
static void APP_CC
dirbuf_add(fuse_req_t req, struct dirbuf *b, const char *name, fuse_ino_t ino)
{
    struct stat stbuf;
    char *newp;
    int oldsize;

    oldsize = b->size;
    b->size += fuse_add_direntry(req, 0, 0, name, 0, 0);
    LLOGLN(10, ("1 %d %d %d", b->alloc_bytes, b->size, oldsize));
    if (b->size > b->alloc_bytes)
    {
        b->alloc_bytes = (b->size + 1023) & (~1023);
        LLOGLN(10, ("2 %d %d %d", b->alloc_bytes, b->size, oldsize));
        newp = g_malloc(b->alloc_bytes, 0);
        g_memcpy(newp, b->p, oldsize);
        g_free(b->p);
        b->p = newp;
    }
    g_memset(&stbuf, 0, sizeof(stbuf));
    stbuf.st_ino = ino;
    fuse_add_direntry(req, b->p + oldsize, b->size - oldsize, name,
                      &stbuf, b->size);
}

#define lmin(x, y) ((x) < (y) ? (x) : (y))

/*****************************************************************************/
static int APP_CC
reply_buf_limited(fuse_req_t req, const char *buf, size_t bufsize,
                  off_t off, size_t maxsize)
{
    LLOGLN(10, ("reply_buf_limited: %d", maxsize));
    if (off < bufsize)
    {
        return fuse_reply_buf(req, buf + off,
                              lmin(bufsize - off, maxsize));
    }
    else
    {
        return fuse_reply_buf(req, 0, 0);
    }
}

/*****************************************************************************/
static void DEFAULT_CC
xrdp_ll_readdir(fuse_req_t req, fuse_ino_t ino, size_t size,
                off_t off, struct fuse_file_info *fi)
{
    struct xfuse_file_info *ffi;
    struct dirbuf b;

    LLOGLN(0, ("xrdp_ll_readdir: ino %d", ino));
    if (ino != 1)
    {
        fuse_reply_err(req, ENOTDIR);
    }
    else
    {
        ffi = g_fuse_files;
        g_memset(&b, 0, sizeof(b));
        dirbuf_add(req, &b, ".", 1);
        dirbuf_add(req, &b, "..", 1);
        while (ffi != 0)
        {
            LLOGLN(10, ("xrdp_ll_readdir: %s", ffi->filename));
            dirbuf_add(req, &b, ffi->filename, ffi->ino);
            ffi = ffi->next;
        }
        reply_buf_limited(req, b.p, b.size, off, size);
        g_free(b.p);
    }
}

/*****************************************************************************/
static void DEFAULT_CC
xrdp_ll_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
    LLOGLN(0, ("xrdp_ll_open: ino %d", (int)ino));
    if (ino == 1)
    {
        fuse_reply_err(req, EISDIR);
    }
    else if ((fi->flags & 3) != O_RDONLY)
    {
        fuse_reply_err(req, EACCES);
    }
    else
    {
        fuse_reply_open(req, fi);
    }
}

/*****************************************************************************/
static void DEFAULT_CC
xrdp_ll_read(fuse_req_t req, fuse_ino_t ino, size_t size,
             off_t off, struct fuse_file_info *fi)
{
    char *data;
    int stream_id;
    struct xfuse_file_info *ffi;
    struct req_list_item *rli;

    LLOGLN(0, ("xrdp_ll_read: %d %d %d", (int)ino, (int)off, (int)size));
    ffi = fuse_find_file_info_by_ino(g_fuse_files, ino);
    if (ffi != 0)
    {
        /* reply later */
        stream_id = 0;
        rli = (struct req_list_item *)
              g_malloc(sizeof(struct req_list_item), 1);
        rli->req = req;
        rli->stream_id = stream_id;
        rli->lindex = ffi->lindex;
        rli->off = off;
        rli->size = size;
        list_add_item(g_req_list, (tbus)rli);
        if (g_req_list->count == 1)
        {
            clipboard_request_file_data(rli->stream_id, rli->lindex,
                                        rli->off, rli->size);
        }
        return;
    }
    LLOGLN(0, ("xrdp_ll_read: fuse_find_file_info_by_ino failed ino %d", (int)ino));
    data = (char *)g_malloc(size, 1);
    fuse_reply_buf(req, data, size);
    g_free(data);

}

/*****************************************************************************/
/* returns error */
static int APP_CC
fuse_init_lib(int argc, char **argv)
{
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    int error;

    error = fuse_parse_cmdline(&args, &g_mountpoint, 0, 0);
    if (error == -1)
    {
        LLOGLN(0, ("fuse_init_lib: fuse_parse_cmdline failed"));
        fuse_opt_free_args(&args);
        return 1;
    }
    g_ch = fuse_mount(g_mountpoint, &args);
    if (g_ch == 0)
    {
        LLOGLN(0, ("fuse_init_lib: fuse_mount failed"));
        fuse_opt_free_args(&args);
        return 1;
    }
    g_se = fuse_lowlevel_new(&args, &g_xrdp_ll_oper,
                             sizeof(g_xrdp_ll_oper), 0);
    if (g_se == 0)
    {
        LLOGLN(0, ("fuse_init_lib: fuse_lowlevel_new failed"));
        fuse_unmount(g_mountpoint, g_ch);
        g_ch = 0;
        fuse_opt_free_args(&args);
        return 1;
    }
    fuse_opt_free_args(&args);
    fuse_session_add_chan(g_se, g_ch);
    g_bufsize = fuse_chan_bufsize(g_ch);
    g_buffer = g_malloc(g_bufsize, 0);
    g_fd = fuse_chan_fd(g_ch);
    return 0;
}

/*****************************************************************************/
static int APP_CC
fuse_delete_dir_items(struct xfuse_file_info *ffi)
{
    struct xfuse_file_info *ffi1;

    while (ffi != 0)
    {
        if (ffi->flags & 1)
        {
            fuse_delete_dir_items(ffi->child);
        }
        ffi1 = ffi;
        ffi = ffi->next;
        g_free(ffi1);
    }
    return 0;
}

/*****************************************************************************/
int APP_CC
fuse_clear_clip_dir(void)
{
    fuse_delete_dir_items(g_fuse_files);
    g_fuse_files = 0;
    return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
fuse_add_clip_dir_item(char *filename, int flags, int size, int lindex)
{
    struct xfuse_file_info *ffi;
    struct xfuse_file_info *ffi1;

    LLOGLN(0, ("fuse_add_clip_dir_item: adding %s ino %d", filename, g_ino));
    ffi = g_fuse_files;
    if (ffi == 0)
    {
        ffi1 = (struct xfuse_file_info *)
               g_malloc(sizeof(struct xfuse_file_info), 1);
        ffi1->flags = flags;
        ffi1->ino = g_ino++;
        ffi1->lindex = lindex;
        ffi1->size = size;
        g_strncpy(ffi1->filename, filename, 255);
        g_fuse_files = ffi1;
        return 0;
    }
    while (ffi->next != 0)
    {
        ffi = ffi->next;
    }
    ffi1 = (struct xfuse_file_info *)
           g_malloc(sizeof(struct xfuse_file_info), 1);
    ffi1->flags = flags;
    ffi1->ino = g_ino++;
    ffi1->lindex = lindex;
    ffi1->size = size;
    g_strncpy(ffi1->filename, filename, 255);
    ffi->next = ffi1;
    return 0;
}

/*****************************************************************************/
int APP_CC
fuse_get_wait_objs(tbus *objs, int *count, int *timeout)
{
    int lcount;

    LLOGLN(10, ("fuse_get_wait_objs:"));
    if (g_ch == 0)
    {
        return 0;
    }

    lcount = *count;
    objs[lcount] = g_fd;
    lcount++;
    *count = lcount;
    return 0;
}

/*****************************************************************************/
int APP_CC
fuse_check_wait_objs(void)
{
    struct fuse_chan *tmpch;
    int res;

    LLOGLN(10, ("fuse_check_wait_objs:"));
    if (g_ch == 0)
    {
        return 0;
    }
    if (g_tcp_select(g_fd, 0) & 1)
    {
        LLOGLN(10, ("fuse_check_wait_objs: fd is set"));
        tmpch = g_ch;
        res = fuse_chan_recv(&tmpch, g_buffer, g_bufsize);
        if (res == -EINTR)
        {
            return 0;
        }
        if (res <= 0)
        {
            return 1;
        }
        fuse_session_process(g_se, g_buffer, res, tmpch);
    }
    return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
fuse_init(void)
{
    char *param0 = "xrdp-chansrv";
    char *argv[4];

    if (g_ch != 0)
    {
        return 0;
    }
    g_snprintf(g_fuse_root_path, 255, "%s/xrdp_client", g_getenv("HOME"));
    LLOGLN(0, ("fuse_init: using root_path [%s]", g_fuse_root_path));
    if (!g_directory_exist(g_fuse_root_path))
    {
        if (!g_create_dir(g_fuse_root_path))
        {
            LLOGLN(0, ("fuse_init: g_create_dir failed [%s]",
                       g_fuse_root_path));
            return 1;
        }
    }
    g_time = g_time1();
    g_uid = g_getuid();
    g_gid = g_getgid();
    argv[0] = param0;
    argv[1] = g_fuse_root_path;
    argv[2] = 0;

    g_memset(&g_xrdp_ll_oper, 0, sizeof(g_xrdp_ll_oper));
    g_xrdp_ll_oper.lookup = xrdp_ll_lookup;
    g_xrdp_ll_oper.getattr = xrdp_ll_getattr;
    g_xrdp_ll_oper.readdir = xrdp_ll_readdir;
    g_xrdp_ll_oper.open = xrdp_ll_open;
    g_xrdp_ll_oper.read = xrdp_ll_read;

    g_req_list = list_create();
    g_req_list->auto_free = 1;

    return fuse_init_lib(2, argv);
}

/*****************************************************************************/
/* returns error */
int APP_CC
fuse_deinit(void)
{
    LLOGLN(0, ("fuse_deinit:"));
    if (g_ch != 0)
    {
        LLOGLN(0, ("fuse_deinit: calling fuse_unmount"));
        fuse_session_remove_chan(g_ch);
        fuse_unmount(g_mountpoint, g_ch);
        g_ch = 0;
    }
    if (g_se != 0)
    {
        LLOGLN(0, ("fuse_deinit: calling fuse_session_destroy"));
        fuse_session_destroy(g_se);
        g_se = 0;
    }
    if (g_buffer != 0)
    {
        g_free(g_buffer);
        g_buffer = 0;
    }
    if (g_req_list != 0)
    {
        list_delete(g_req_list);
        g_req_list = 0;
    }
    return 0;
}

/*****************************************************************************/
int APP_CC
fuse_file_contents_size(int stream_id, int file_size)
{
    LLOGLN(0, ("fuse_file_contents_size: file_size %d", file_size));
    return 0;
}

/*****************************************************************************/
int APP_CC
fuse_file_contents_range(int stream_id, char *data, int data_bytes)
{
    struct req_list_item *rli;

    LLOGLN(0, ("fuse_file_contents_range: data_bytes %d", data_bytes));
    rli = (struct req_list_item *)list_get_item(g_req_list, 0);
    if (rli != 0)
    {
        fuse_reply_buf(rli->req, data, data_bytes);
        list_remove_item(g_req_list, 0);
        if (g_req_list->count > 0)
        {
            /* send next request */
            rli = (struct req_list_item *)list_get_item(g_req_list, 0);
            if (rli != 0)
            {
                clipboard_request_file_data(rli->stream_id, rli->lindex,
                                            rli->off, rli->size);
            }
            else
            {
                LLOGLN(0, ("fuse_file_contents_range: error"));
            }
        }
    }
    else
    {
        LLOGLN(0, ("fuse_file_contents_range: error"));
    }
    return 0;
}

#else

#include "arch.h"

char g_fuse_root_path[256] = "";

/*****************************************************************************/
int APP_CC
fuse_get_wait_objs(tbus *objs, int *count, int *timeout)
{
    return 0;
}

/*****************************************************************************/
int APP_CC
fuse_check_wait_objs(void)
{
    return 0;
}

/*****************************************************************************/
int APP_CC
fuse_init(void)
{
    return 0;
}

/*****************************************************************************/
int APP_CC
fuse_deinit(void)
{
    return 0;
}

/*****************************************************************************/
int APP_CC
fuse_clear_clip_dir(void)
{
    return 0;
}

/*****************************************************************************/
int APP_CC
fuse_add_clip_dir_item(char *filename, int flags, int size, int lindex)
{
    return 0;
}

/*****************************************************************************/
int APP_CC
fuse_file_contents_size(int stream_id, int file_size)
{
    return 0;
}

/*****************************************************************************/
int APP_CC
fuse_file_contents_range(int stream_id, char *data, int data_bytes)
{
    return 0;
}

#endif
