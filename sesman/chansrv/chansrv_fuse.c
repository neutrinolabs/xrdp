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

#define XRDP_USE_FUSE 1

#ifdef XRDP_USE_FUSE

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
#include "os_calls.h"
#include "chansrv.h"
#include "chansrv_fuse.h"

#define LLOG_LEVEL 1
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

static struct fuse_chan *g_ch = 0;
static struct fuse_session *g_se = 0;
static char *g_mountpoint = 0;
static tintptr g_bufsize = 0;
static char *g_buffer = 0;
static int g_fd = 0;

struct file_item *g_file_items = 0;
int g_file_items_count = 0;

static time_t g_time = 0;
static int g_uid = 0;
static int g_gid = 0;

struct dirbuf
{
    char *p;
    int size;
    int alloc_bytes;
};

/*****************************************************************************/
static int APP_CC
xrdp_stat(fuse_ino_t ino, struct stat *stbuf)
{
    int index;

    LLOGLN(10, ("xrdp_stat: ino %d", (int)ino));
    stbuf->st_ino = ino;
    switch (ino)
    {
        case 1: /* . */
        case 2: /* .. */
            stbuf->st_mode = S_IFDIR | 0755;
            stbuf->st_nlink = 2;
            stbuf->st_uid = g_uid;
            stbuf->st_gid = g_gid;
            stbuf->st_atime = g_time;
            stbuf->st_mtime = g_time;
            stbuf->st_ctime = g_time;
            break;

        default:
            index = ino - 3;
            if (index < 0 || index >= g_file_items_count)
            {
                return -1;
            }
            stbuf->st_mode = S_IFREG | 0444;
            stbuf->st_nlink = 1;
            stbuf->st_size = g_file_items[index].data_bytes;
            stbuf->st_uid = g_uid;
            stbuf->st_gid = g_gid;
            stbuf->st_atime = g_time;
            stbuf->st_mtime = g_time;
            stbuf->st_ctime = g_time;
            break;
    }
    return 0;
}

/*****************************************************************************/
static void DEFAULT_CC
xrdp_ll_lookup(fuse_req_t req, fuse_ino_t parent, const char *name)
{
    struct fuse_entry_param e;
    int index;

    LLOGLN(10, ("xrdp_ll_lookup: name %s", name));
    if (parent != 1)
    {
        fuse_reply_err(req, ENOENT);
    }
    else
    {
        for (index = 0; index < g_file_items_count; index++)
        {
            if (g_strcmp(name, g_file_items[index].filename) == 0)
            {
                g_memset(&e, 0, sizeof(e));
                e.ino = g_file_items[index].ino;
                e.attr_timeout = 1.0;
                e.entry_timeout = 1.0;
                xrdp_stat(e.ino, &e.attr);
                fuse_reply_entry(req, &e);
                return;
            }
        }
    }
    fuse_reply_err(req, ENOENT);
}

/*****************************************************************************/
static void DEFAULT_CC
xrdp_ll_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
    struct stat stbuf;

    g_memset(&stbuf, 0, sizeof(stbuf));
    if (xrdp_stat(ino, &stbuf) == -1)
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
    fuse_add_direntry(req, b->p + oldsize, b->size - oldsize, name, &stbuf,
                      b->size);
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
    struct dirbuf b;
    int index;

    if (ino != 1)
    {
        fuse_reply_err(req, ENOTDIR);
    }
    else
    {
        g_memset(&b, 0, sizeof(b));
        dirbuf_add(req, &b, ".", 1);
        dirbuf_add(req, &b, "..", 2);
        for (index = 0; index < g_file_items_count; index++)
        {
            dirbuf_add(req, &b, g_file_items[index].filename,
                       g_file_items[index].ino);
        }
        reply_buf_limited(req, b.p, b.size, off, size);
        g_free(b.p);
    }
}

/*****************************************************************************/
static void DEFAULT_CC
xrdp_ll_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
    LLOGLN(10, ("xrdp_ll_open: ino %d", (int)ino));
    if (ino == 1 || ino == 2)
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
    int index;

    LLOGLN(10, ("xrdp_ll_read: %d %d %d", (int)ino, (int)off, (int)size));
    index = ino - 3;
    if (index < 0 || index >= g_file_items_count)
    {
    }
    else
    {
        reply_buf_limited(req, g_file_items[index].data,
                          g_file_items[index].data_bytes,
                          off, size);
    }
}

static struct fuse_lowlevel_ops xrdp_ll_oper =
{
    .lookup         = xrdp_ll_lookup,
    .getattr        = xrdp_ll_getattr,
    .readdir        = xrdp_ll_readdir,
    .open           = xrdp_ll_open,
    .read           = xrdp_ll_read,
};

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
    g_se = fuse_lowlevel_new(&args, &xrdp_ll_oper,
                             sizeof(xrdp_ll_oper), 0);
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
/* returns error */
int APP_CC
fuse_set_dir_item(int index, char *filename, int flags, char *data,
                  int data_bytes, int ino)
{
    g_strncpy(g_file_items[index].filename, filename, 255);
    g_file_items[index].flags = flags;
    g_memcpy(g_file_items[index].data, data, data_bytes);
    g_file_items[index].data_bytes = data_bytes;
    g_file_items[index].ino = ino;
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
    char root_path[256];

    g_snprintf(root_path, 255, "%s/xrdp_client", g_getenv("HOME"));
    LLOGLN(0, ("fuse_init: using root_path [%s]", root_path));
    if (g_ch != 0)
    {
        return 0;
    }
    g_time = g_time1();
    g_uid = g_getuid();
    g_gid = g_getgid();
    argv[0] = param0;
    argv[1] = root_path;
    argv[2] = 0;

    g_file_items = g_malloc(sizeof(struct file_item) * 3, 1);
    fuse_set_dir_item(0, "File1", 0, "1\n", 2, 3);
    fuse_set_dir_item(1, "File2", 0, "2\n", 2, 4);
    fuse_set_dir_item(2, "File3", 0, "3\n", 2, 5);
    g_file_items_count = 3;

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
    if (g_file_items != 0)
    {
        g_free(g_file_items);
        g_file_items = 0;
        g_file_items_count = 0;
    }
    return 0;
}

#else

#include "arch.h"

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
fuse_set_dir_item(int index, char *filename, int flags, char *data,
                  int data_bytes, int ino)
{
    return 0;
}

#endif
