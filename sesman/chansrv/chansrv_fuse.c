/**
 * xrdp: A Remote Desktop Protocol server.
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
 *      o need to support sym links
 *      o when creating dir/file, ensure it does not already exist
 *      o enable changing metadata for files/folders (time, owner, mode etc)
 *      o do not allow dirs to be created in ino==1 except for .clipbard and share mounts
 *      o xrdp_fs needs to grow dynamically - currently it is fixed at 1k or 4k
 *      o fix the HACK where I have to use my own buf instead of g_buffer
 *        this is in func xfuse_check_wait_objs()
 *      o if fuse mount point is already mounted, I get segfault
 *      o in open, check for modes such as O_TRUNC, O_APPEND
 *      o copying over an existing file does not work
 *      o are we calling close?
 *      o need to keep track of open files, reqd during rename
 *      o fuse ops to support
 *          o rename (mv)
 *          o touch does not work
 *          o mknod (may not be required if create is correctly implemented)
 *          o symlink
 *          o keep track of lookup_count
 *          o chmod must work
 *          o cat >> file is not working
 *
 */

/* FUSE mount point */
char g_fuse_root_path[256] = "";

#ifndef XRDP_FUSE

/******************************************************************************
**                                                                           **
**                   when FUSE is NOT enabled in xrdp                        **
**                                                                           **
******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arch.h"
#include "chansrv_fuse.h"

/* dummy calls when XRDP_FUSE is not defined */
int xfuse_init()                {}
int xfuse_deinit()              {}
int xfuse_check_wait_objs(void) {}
int xfuse_get_wait_objs(tbus *objs, int *count, int *timeout) {}
int xfuse_clear_clip_dir(void)  {}
int xfuse_file_contents_range(int stream_id, char *data, int data_bytes)     {}
int xfuse_file_contents_size(int stream_id, int file_size)                   {}
int xfuse_add_clip_dir_item(char *filename, int flags, int size, int lindex) {}
int xfuse_create_share(tui32 device_id, char *dirname)                       {}
void xfuse_devredir_cb_open_file(void *vp, tui32 DeviceId, tui32 FileId)     {}
void xfuse_devredir_cb_write_file(void *vp, char *buf, size_t length)        {}
void xfuse_devredir_cb_read_file(void *vp, char *buf, size_t length)         {}
void xfuse_devredir_cb_enum_dir(void *vp, struct xrdp_inode *xinode)         {}
void xfuse_devredir_cb_enum_dir_done(void *vp, tui32 IoStatus)               {}
void xfuse_devredir_cb_rmdir_or_file(void *vp, tui32 IoStatus)               {}
void xfuse_devredir_cb_file_close(void *vp)                                  {}

#else

/******************************************************************************
**                                                                           **
**                     when FUSE is enabled in xrdp                          **
**                                                                           **
******************************************************************************/

#define FUSE_USE_VERSION 26
#define _FILE_OFFSET_BITS 64

#include <fuse/fuse_lowlevel.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sched.h>

#include "arch.h"
#include "os_calls.h"
#include "chansrv_fuse.h"

#define min(x, y) ((x) < (y) ? (x) : (y))

#define XFUSE_ATTR_TIMEOUT      1.0
#define XFUSE_ENTRY_TIMEOUT     1.0

#define DOTDOT_INODE    0
#define DOT_INODE       0
#define FIRST_INODE     1

/* module based logging */
#define LOG_ERROR   0
#define LOG_INFO    1
#define LOG_DEBUG   2
#define LOG_LEVEL   LOG_DEBUG

#define log_error(_params...)                           \
{                                                       \
    g_write("[%10.10u]: FUSE       %s: %d : ERROR: ",   \
            g_time3(), __func__, __LINE__);             \
    g_writeln (_params);                                \
}

#define log_info(_params...)                            \
{                                                       \
    if (LOG_INFO <= LOG_LEVEL)                          \
    {                                                   \
        g_write("[%10.10u]: FUSE       %s: %d : ",      \
                g_time3(), __func__, __LINE__);         \
        g_writeln (_params);                            \
    }                                                   \
}

#define log_debug(_params...)                           \
{                                                       \
    if (LOG_DEBUG <= LOG_LEVEL)                        \
    {                                                   \
        g_write("[%10.10u]: FUSE       %s: %d : ",      \
                g_time3(), __func__, __LINE__);         \
        g_writeln (_params);                            \
    }                                                   \
}

/* the xrdp file system in memory */
struct xrdp_fs
{
    struct xrdp_inode **inode_table; /* a table of entries; can grow         */
    unsigned int max_entries;        /* size of inode_table[]                */
    unsigned int num_entries;        /* num entries available in inode_table */
    unsigned int next_node;          /* next free node number                */
};

struct dirbuf
{
    char *p;
    size_t size;
};

/* FUSE reply types */
#define RT_FUSE_REPLY_OPEN      1
#define RT_FUSE_REPLY_CREATE    2

struct xfuse_info
{
    struct fuse_file_info *fi;
    fuse_req_t             req;
    fuse_ino_t             inode;
    int                    invoke_fuse;
    char                   name[1024];
    tui32                  device_id;
    int                    reply_type;
    int                    mode;
    int                    type;
};
typedef struct xfuse_info XFUSE_INFO;

struct xfuse_handle
{
    tui32 DeviceId;
    tui32 FileId;
};
typedef struct xfuse_handle XFUSE_HANDLE;

/* globals */

static struct xrdp_fs g_xrdp_fs;             /* an inst of xrdp file system */
static char *g_mount_point = 0;              /* our FUSE mount point        */
static struct fuse_lowlevel_ops g_xfuse_ops; /* setup FUSE callbacks        */
static int g_xfuse_inited = 0;               /* true when FUSE is inited    */
static struct fuse_chan *g_ch = 0;
static struct fuse_session *g_se = 0;
static char *g_buffer = 0;
static int g_fd = 0;
static tintptr g_bufsize = 0;

/* forward declarations for internal access */
static int xfuse_init_xrdp_fs();
static int xfuse_deinit_xrdp_fs();
static int xfuse_init_lib(struct fuse_args *args);
static int xfuse_is_inode_valid(int ino);

// LK_TODO
#if 0
static void xfuse_create_file(fuse_req_t req, fuse_ino_t parent,
                              const char *name, mode_t mode, int type);
#endif

static void xfuse_dump_fs();
static void xfuse_dump_xrdp_inode(struct xrdp_inode *xino);
static tui32 xfuse_get_device_id_for_inode(tui32 ino, char *full_path);
static void fuse_reverse_pathname(char *full_path, char *reverse_path);

static struct xrdp_inode * xfuse_get_inode_from_pinode_name(tui32 pinode,
                                                            const char *name);

static struct xrdp_inode * xfuse_create_file_in_xrdp_fs(tui32 device_id,
                                                        int pinode, char *name,
                                                        int type);

static int xfuse_does_file_exist(int parent, char *name);

/* forward declarations for calls we make into devredir */
int dev_redir_get_dir_listing(void *fusep, tui32 device_id, char *path);

int dev_redir_file_open(void *fusep, tui32 device_id, char *path,
                        int mode, int type);

int dev_redir_file_read(void *fusep, tui32 device_id, tui32 FileId,
                        tui32 Length, tui64 Offset);

int dev_redir_file_write(void *fusep, tui32 device_id, tui32 FileId,
                         const char *buf, tui32 Length, tui64 Offset);

/* forward declarations for FUSE callbacks */
static void xfuse_cb_lookup(fuse_req_t req, fuse_ino_t parent,
                            const char *name);

static void xfuse_cb_getattr(fuse_req_t req, fuse_ino_t ino,
                             struct fuse_file_info *fi);

/* this is not a callback, but its's used by xfuse_cb_readdir() */
static void xfuse_dirbuf_add(fuse_req_t req, struct dirbuf *b,
                             const char *name, fuse_ino_t ino);

static void xfuse_cb_readdir(fuse_req_t req, fuse_ino_t ino, size_t size,
                             off_t off, struct fuse_file_info *fi);

static void xfuse_cb_mkdir(fuse_req_t req, fuse_ino_t parent,
                           const char *name, mode_t mode);

static void xfuse_cb_rmdir(fuse_req_t req, fuse_ino_t parent,
                           const char *name);

static void xfuse_cb_unlink(fuse_req_t req, fuse_ino_t parent,
                            const char *name);

/* this is not a callback, but it is used by the above two functions */
static void xfuse_remove_dir_or_file(fuse_req_t req, fuse_ino_t parent,
                                     const char *name, int type);

static void xfuse_create_dir_or_file(fuse_req_t req, fuse_ino_t parent,
                                     const char *name, mode_t mode,
                                     struct fuse_file_info *fi, int type);

static void xfuse_cb_open(fuse_req_t req, fuse_ino_t ino,
                          struct fuse_file_info *fi);

static void xfuse_cb_flush(fuse_req_t req, fuse_ino_t ino, struct
                           fuse_file_info *fi);

static void xfuse_cb_read(fuse_req_t req, fuse_ino_t ino, size_t size,
                          off_t off, struct fuse_file_info *fi);

static void xfuse_cb_write(fuse_req_t req, fuse_ino_t ino, const char *buf,
                           size_t size, off_t off, struct fuse_file_info *fi);

static void xfuse_cb_create(fuse_req_t req, fuse_ino_t parent,
                            const char *name, mode_t mode,
                            struct fuse_file_info *fi);

// LK_TODO may not need to be implemented
#if 0
static void xfuse_cb_statfs(fuse_req_t req, fuse_ino_t ino);

static void xfuse_cb_setxattr(fuse_req_t req, fuse_ino_t ino, const char *name,
                     const char *value, size_t size, int flags);

static void xfuse_cb_getxattr(fuse_req_t req, fuse_ino_t ino, const char *name,
                     size_t size);

static void xfuse_cb_listxattr(fuse_req_t req, fuse_ino_t ino, size_t size);

static void xfuse_cb_access(fuse_req_t req, fuse_ino_t ino, int mask);

static void xfuse_cb_getlk(fuse_req_t req, fuse_ino_t ino,
                           struct fuse_file_info *fi, struct flock *lock);

static void xfuse_cb_setlk(fuse_req_t req, fuse_ino_t ino,
                           struct fuse_file_info *fi, struct flock *lock,
                           int sleep);

static void xfuse_cb_ioctl(fuse_req_t req, fuse_ino_t ino, int cmd, void *arg,
                           struct fuse_file_info *fi, unsigned flags,
                           const void *in_buf, size_t in_bufsz,
                           size_t out_bufsz);

static void xfuse_cb_poll(fuse_req_t req, fuse_ino_t ino,
                          struct fuse_file_info *fi,
                          struct fuse_pollhandle *ph);
#endif

static void xfuse_cb_setattr(fuse_req_t req, fuse_ino_t ino, struct stat *attr,
                             int to_set, struct fuse_file_info *fi);

/*****************************************************************************
**                                                                          **
**         public functions - can be called from any code path              **
**                                                                          **
*****************************************************************************/

/**
 * Initialize FUSE subsystem
 *
 * @return 0 on success, -1 on failure
 *****************************************************************************/

int xfuse_init()
{
    struct fuse_args args = FUSE_ARGS_INIT(0, NULL);
    char   opt[1024];

    /* if already inited, just return */
    if (g_xfuse_inited)
    {
        log_debug("already inited");
        return 1;
    }

    if (g_ch != 0)
    {
        log_error("g_ch is not zero");
        return -1;
    }

    /* define FUSE mount point to ~/xrdp_client */
    g_snprintf(g_fuse_root_path, 255, "%s/xrdp_client", g_getenv("HOME"));

    /* if FUSE mount point does not exist, create it */
    if (!g_directory_exist(g_fuse_root_path))
    {
        if (!g_create_dir(g_fuse_root_path))
        {
            log_error("mkdir %s failed. If %s is already mounted, you must "
                      "first unmount it", g_fuse_root_path, g_fuse_root_path);
            return -1;
        }
    }

    /* setup xrdp file system */
    if (xfuse_init_xrdp_fs())
        return -1;

    /* setup FUSE callbacks */
    g_memset(&g_xfuse_ops, 0, sizeof(g_xfuse_ops));
    g_xfuse_ops.lookup  = xfuse_cb_lookup;
    g_xfuse_ops.readdir = xfuse_cb_readdir;
    g_xfuse_ops.mkdir   = xfuse_cb_mkdir;
    g_xfuse_ops.rmdir   = xfuse_cb_rmdir;
    g_xfuse_ops.unlink  = xfuse_cb_unlink;
    g_xfuse_ops.open    = xfuse_cb_open;
    g_xfuse_ops.flush   = xfuse_cb_flush;
    g_xfuse_ops.read    = xfuse_cb_read;
    g_xfuse_ops.write   = xfuse_cb_write;
    g_xfuse_ops.create  = xfuse_cb_create;
    g_xfuse_ops.getattr = xfuse_cb_getattr;
    g_xfuse_ops.setattr = xfuse_cb_setattr;

#if 0
    g_xfuse_ops.statfs  = xfuse_cb_statfs;
    g_xfuse_ops.listxattr  = xfuse_cb_listxattr;
    g_xfuse_ops.getlk = xfuse_cb_getlk;
    g_xfuse_ops.setlk = xfuse_cb_setlk;
    g_xfuse_ops.ioctl = xfuse_cb_ioctl;
    g_xfuse_ops.poll = xfuse_cb_poll;
    g_xfuse_ops.access = xfuse_cb_access;
    g_xfuse_ops.setxattr  = xfuse_cb_setxattr;
    g_xfuse_ops.getxattr  = xfuse_cb_getxattr;
#endif

    fuse_opt_add_arg(&args, "xrdp-chansrv");
    fuse_opt_add_arg(&args, g_fuse_root_path);
#if 0
    sprintf(opt, "-o uid=%d,gid=%d", g_getuid(), g_getgid());
    fuse_opt_add_arg(&args, opt);
#endif

    if (xfuse_init_lib(&args))
    {
        xfuse_deinit();
        return -1;
    }

    g_xfuse_inited = 1;
    return 0;
}

/**
 * De-initialize FUSE subsystem
 *
 * @return 0 on success, -1 on failure
 *****************************************************************************/

int xfuse_deinit()
{
    xfuse_deinit_xrdp_fs();

    if (g_ch != 0)
    {
        fuse_session_remove_chan(g_ch);
        fuse_unmount(g_mount_point, g_ch);
        g_ch = 0;
    }

    if (g_se != 0)
    {
        fuse_session_destroy(g_se);
        g_se = 0;
    }

    if (g_buffer != 0)
    {
        g_free(g_buffer);
        g_buffer = 0;
    }

    g_xfuse_inited = 0;
    return 0;
}

/**
 *
 *
 * @return 0 on success, -1 on failure
 *****************************************************************************/
int xfuse_check_wait_objs(void)
{
    struct fuse_chan *tmpch;
    int               rval;

#define HACK

#ifdef HACK
char buf[135168];
#endif

    if (g_ch == 0)
        return 0;

    if (g_tcp_select(g_fd, 0) & 1)
    {
        tmpch = g_ch;

#ifdef HACK
        rval = fuse_chan_recv(&tmpch, buf, g_bufsize);
#else
        rval = fuse_chan_recv(&tmpch, g_buffer, g_bufsize);
#endif
        if (rval == -EINTR)
            return -1;

        if (rval == -ENODEV)
            return -1;

        if (rval <= 0)
            return -1;

#ifdef HACK
        fuse_session_process(g_se, buf, rval, tmpch);
#else
        fuse_session_process(g_se, g_buffer, rval, tmpch);
#endif
    }

    return 0;
}

/**
 *
 *
 * @return 0 on success, -1 on failure
 *****************************************************************************/

int xfuse_get_wait_objs(tbus *objs, int *count, int *timeout)
{
    int lcount;

    if (g_ch == 0)
        return 0;

    lcount = *count;
    objs[lcount] = g_fd;
    lcount++;
    *count = lcount;

    return 0;
}

/**
 * @brief Create specified share directory.
 *
 * This code gets called from devredir
 *
 * @return 0 on success, -1 on failure
 *****************************************************************************/

int xfuse_create_share(tui32 device_id, char *dirname)
{
    /* LK_TODO need to specify parent dir, mode */

    XFUSE_INFO  *fip;
    XRDP_INODE  *xinode;
    tui32        saved_inode;

    if (dirname == NULL || strlen(dirname) == 0)
        return -1;

    if ((xinode = calloc(1, sizeof(struct xrdp_inode))) == NULL)
    {
        log_debug("calloc() failed");
        return -1;
    }

    if ((fip = calloc(1, sizeof(XFUSE_INFO))) == NULL)
    {
        log_error("system out of memory");
        return -1;
    }

    /* create directory entry */
    xinode->parent_inode = 1;
    xinode->inode = g_xrdp_fs.next_node++;
    xinode->mode = 0755 | S_IFDIR;
    xinode->nlink = 1;
    xinode->uid = getuid();
    xinode->gid = getgid();
    xinode->size = 0;
    xinode->atime = time(0);
    xinode->mtime = time(0);
    xinode->ctime = time(0);
    strcpy(xinode->name, dirname);
    xinode->device_id = device_id;

    g_xrdp_fs.num_entries++;
    saved_inode = xinode->inode;

    /* insert it in xrdp fs */
    g_xrdp_fs.inode_table[xinode->inode] = xinode;
    log_debug("created new share named %s at inode_table[%d]",
              dirname, (int) xinode->inode);

    /* update nentries in parent inode */
    xinode = g_xrdp_fs.inode_table[1];
    if (xinode == NULL)
        return -1;
    xinode->nentries++;

     /* enumerate root dir, do not call FUSE when done */
    fip->req = NULL;
    fip->inode = 1; // LK_TODO saved_inode;
    strncpy(fip->name, dirname, 1024);
    fip->name[1023] = 0;
    fip->device_id = device_id;

    dev_redir_get_dir_listing((void *) fip, device_id, "\\");

    return 0;
}

/**
 *
 *
 * @return 0 on success, -1 on failure
 *****************************************************************************/

int xfuse_clear_clip_dir(void)
{
    return 0; // CLIPBOARD_TODO
}

/**
 *
 *
 * @return 0 on success, -1 on failure
 *****************************************************************************/

int xfuse_file_contents_range(int stream_id, char *data, int data_bytes)
{
    return 0; // CLIPBOARD_TODO
}

/**
 *
 *
 * @return 0 on success, -1 on failure
 *****************************************************************************/

int xfuse_add_clip_dir_item(char *filename, int flags, int size, int lindex)
{
    return 0; // CLIPBOARD_TODO
}

/**
 *
 *
 * @return 0 on success, -1 on failure
 *****************************************************************************/

int xfuse_file_contents_size(int stream_id, int file_size)
{
    return 0; // CLIPBOARD_TODO
}

/*****************************************************************************
**                                                                          **
**       private functions - can only be called from within this file       **
**                                                                          **
*****************************************************************************/

/**
 * Initialize FUSE library
 *
 * @return 0 on success, -1 on failure
 *****************************************************************************/

static int xfuse_init_lib(struct fuse_args *args)
{
    // LK_TODO
    {
    int i;

    for (i = 0; i < args->argc; i++)
        log_debug("+++++++++++++ argc=%d argv=%s", i, args->argv[i]);
    }

    if (fuse_parse_cmdline(args, &g_mount_point, 0, 0) < 0)
    {
        log_error("fuse_parse_cmdline() failed");
        fuse_opt_free_args(args);
        return -1;
    }

    if ((g_ch = fuse_mount(g_mount_point, args)) == 0)
    {
        log_error("fuse_mount() failed");
        fuse_opt_free_args(args);
        return -1;
    }

    g_se = fuse_lowlevel_new(args, &g_xfuse_ops, sizeof(g_xfuse_ops), 0);
    if (g_se == 0)
    {
        log_error("fuse_lowlevel_new() failed");
        fuse_unmount(g_mount_point, g_ch);
        g_ch = 0;
        fuse_opt_free_args(args);
        return -1;
    }

    fuse_opt_free_args(args);
    fuse_session_add_chan(g_se, g_ch);
    g_bufsize = fuse_chan_bufsize(g_ch);

    g_buffer = calloc(g_bufsize, 1);
    g_fd = fuse_chan_fd(g_ch);

    return 0;
}

/**
 * Initialize xrdp file system
 *
 * @return 0 on success, -1 on failure
 *
 *****************************************************************************/

static int xfuse_init_xrdp_fs()
{
    struct xrdp_inode *xino;

    g_xrdp_fs.inode_table = calloc(4096, sizeof(struct xrdp_inode *));
    if (g_xrdp_fs.inode_table == NULL)
    {
        log_error("system out of memory");
        return -1;
    }

    /*
     * index 0 is our .. dir
     */

    if ((xino = calloc(1, sizeof(struct xrdp_inode))) == NULL)
    {
        log_error("system out of memory");
        free(g_xrdp_fs.inode_table);
        return -1;
    }
    g_xrdp_fs.inode_table[0] = xino;
    xino->parent_inode = 0;
    xino->inode = 0;
    xino->mode = S_IFDIR | 0755;
    xino->nentries = 1;
    xino->uid = getuid();
    xino->gid = getgid();
    xino->size = 0;
    xino->atime = time(0);
    xino->mtime = time(0);
    xino->ctime = time(0);
    strcpy(xino->name, "..");

    /*
     * index 1 is our . dir
     */

    if ((xino = calloc(1, sizeof(struct xrdp_inode))) == NULL)
    {
        log_error("system out of memory");
        free(g_xrdp_fs.inode_table[0]);
        free(g_xrdp_fs.inode_table);
        return -1;
    }
    g_xrdp_fs.inode_table[1] = xino;
    xino->parent_inode = 0;
    xino->inode = 1;
    xino->mode = S_IFDIR | 0755;
    xino->nentries = 1;
    xino->uid = getuid();
    xino->gid = getgid();
    xino->size = 0;
    xino->atime = time(0);
    xino->mtime = time(0);
    xino->ctime = time(0);
    strcpy(xino->name, ".");

    /*
     * index 2 is for clipboard use
     */

    if ((xino = calloc(1, sizeof(struct xrdp_inode))) == NULL)
    {
        log_error("system out of memory");
        free(g_xrdp_fs.inode_table[0]);
        free(g_xrdp_fs.inode_table[1]);
        free(g_xrdp_fs.inode_table);
        return -1;
    }

    g_xrdp_fs.inode_table[2] = xino;
    xino->parent_inode = 1;
    xino->inode = 2;
    xino->nentries = 1;
    xino->mode = S_IFDIR | 0755;
    xino->uid = getuid();
    xino->gid = getgid();
    xino->size = 0;
    xino->atime = time(0);
    xino->mtime = time(0);
    xino->ctime = time(0);
    strcpy(xino->name, ".clipboard");

    g_xrdp_fs.max_entries = 1024;
    g_xrdp_fs.num_entries = 3;
    g_xrdp_fs.next_node = 3;

    return 0;
}

/**
 * zap the xrdp file system
 *
 * @return 0 on success, -1 on failure
 *****************************************************************************/

static int xfuse_deinit_xrdp_fs()
{
    return 0;
}

/**
 * determine if specified ino exists in xrdp file system
 *
 * @return 1 if it does, 0 otherwise
 *****************************************************************************/

static int xfuse_is_inode_valid(int ino)
{
    /* our lowest ino is FIRST_INODE */
    if (ino < FIRST_INODE)
        return 0;

    /* is ino present in our table? */
    if (ino >= g_xrdp_fs.next_node)
        return 0;

    return 1;
}

/**
 * @brief Create a directory or regular file.
 *****************************************************************************/

// LK_TODO
#if 0
static void xfuse_create_file(fuse_req_t req, fuse_ino_t parent,
                              const char *name, mode_t mode, int type)
{
    struct xrdp_inode        *xinode;
    struct fuse_entry_param   e;

    log_debug("parent=%d name=%s", (int) parent, name);

    /* do we have a valid parent inode? */
    if (!xfuse_is_inode_valid(parent))
    {
        log_error("inode %d is not valid", parent);
        fuse_reply_err(req, EBADF);
    }

    if ((xinode = calloc(1, sizeof(struct xrdp_inode))) == NULL)
    {
        log_error("calloc() failed");
        fuse_reply_err(req, ENOMEM);
    }

    /* create directory entry */
    xinode->parent_inode = parent;
    xinode->inode = g_xrdp_fs.next_node++; /* TODO should be thread safe */
    xinode->mode = mode | type;
    xinode->uid = getuid();
    xinode->gid = getgid();
    xinode->size = 0;
    xinode->atime = time(0);
    xinode->mtime = time(0);
    xinode->ctime = time(0);
    strcpy(xinode->name, name);

    g_xrdp_fs.num_entries++;

    /* insert it in xrdp fs */
    g_xrdp_fs.inode_table[xinode->inode] = xinode;
    log_debug("inserted new dir at inode_table[%d]", (int) xinode->inode);

    xfuse_dump_fs();

    log_debug("new inode=%d", (int) xinode->inode);

    /* setup return value */
    memset(&e, 0, sizeof(e));
    e.ino = xinode->inode;
    e.attr_timeout = XFUSE_ATTR_TIMEOUT;
    e.entry_timeout = XFUSE_ENTRY_TIMEOUT;
    e.attr.st_ino = xinode->inode;
    e.attr.st_mode = xinode->mode;
    e.attr.st_nlink = xinode->nlink;
    e.attr.st_uid = xinode->uid;
    e.attr.st_gid = xinode->gid;
    e.attr.st_size = 0;
    e.attr.st_atime = xinode->atime;
    e.attr.st_mtime = xinode->mtime;
    e.attr.st_ctime = xinode->ctime;
    e.generation = 1;

    fuse_reply_entry(req, &e);
}
#endif

static void xfuse_dump_fs()
{
    int i;
    struct xrdp_inode *xinode;

    log_debug("found %d entries", g_xrdp_fs.num_entries - FIRST_INODE);

    for (i = FIRST_INODE; i < g_xrdp_fs.num_entries; i++)
    {
        if ((xinode = g_xrdp_fs.inode_table[i]) == NULL)
            continue;

        log_debug("pinode=%d inode=%d nentries=%d mode=0x%x name=%s",
                  (int) xinode->parent_inode, (int) xinode->inode,
                  xinode->nentries, xinode->mode, xinode->name);
    }
    log_debug("");
}

/**
 * Dump contents of xinode structure
 *
 * @param xino xinode structure to dump
 *****************************************************************************/

static void xfuse_dump_xrdp_inode(struct xrdp_inode *xino)
{
    log_debug("--- dumping struct xinode ---");
    log_debug("name:          %s", xino->name);
    log_debug("parent_inode:  %ld", xino->parent_inode);
    log_debug("inode:         %ld", xino->inode);
    log_debug("mode:          %o", xino->mode);
    log_debug("nlink:         %d", xino->nlink);
    log_debug("uid:           %d", xino->uid);
    log_debug("gid:           %d", xino->gid);
    log_debug("size:          %ld", xino->size);
    log_debug("device_id:     %d", xino->device_id);
    log_debug("");
}

/**
 * Return the device_id associated with specified inode and copy the
 * full path to the specified inode into full_path
 *
 * @param ino the inode
 * @param full_path full path to the inode
 *
 * @return the device_id of specified inode
 *****************************************************************************/

static tui32 xfuse_get_device_id_for_inode(tui32 ino, char *full_path)
{
    tui32   parent_inode = 0;
    tui32   child_inode  = ino;
    char    reverse_path[4096];

    reverse_path[0] = 0;
    full_path[0] = 0;

    /* ino == 1 is a special case; we already know that it is not */
    /* associated with any device redirection                     */
    if (ino == 1)
        return 0;

    while (1)
    {
        strcat(reverse_path, g_xrdp_fs.inode_table[child_inode]->name);

        parent_inode = g_xrdp_fs.inode_table[child_inode]->parent_inode;
        if (parent_inode == 1)
            break;

        strcat(reverse_path, "/");
        child_inode = parent_inode;
    }

    fuse_reverse_pathname(full_path, reverse_path);

    return g_xrdp_fs.inode_table[child_inode]->device_id;
}

/**
 * Reverse the pathname in 'reverse_path' and insert it into 'full_path'
 *
 * Example: abba/music/share1 becomes share1/music/abba
 *
 * @param full_path path name in the correct order
 * @param reverse_path path name in the reverse order
 *****************************************************************************/

static void fuse_reverse_pathname(char *full_path, char *reverse_path)
{
    char *cptr;

    full_path[0] = 0;

    while ((cptr = strrchr(reverse_path, '/')) != NULL)
    {
        strcat(full_path, cptr + 1);
        strcat(full_path, "/");
        cptr[0] = 0;
    }
    strcat(full_path, reverse_path);
}

/**
 * Return the inode that matches the name and parent inode
 *****************************************************************************/

static struct xrdp_inode * xfuse_get_inode_from_pinode_name(tui32 pinode,
                                                            const char *name)
{
    int i;
    struct xrdp_inode * xinode;

    for (i = FIRST_INODE; i < g_xrdp_fs.num_entries; i++)
    {
        if ((xinode = g_xrdp_fs.inode_table[i]) == NULL)
            continue;

        /* match parent inode */
        if (xinode->parent_inode != pinode)
            continue;

        /* match name */
        if (strcmp(xinode->name, name) != 0)
            continue;

        return xinode;
    }
    return NULL;
}

/**
 * Create file in xrdp file system
 *
 * @param pinode the parent inode
 * @param name   filename
 *
 * @return XRDP_INODE on success, NULL on failure
 *****************************************************************************/

static struct xrdp_inode * xfuse_create_file_in_xrdp_fs(tui32 device_id,
                                                        int pinode, char *name,
                                                        int type)
{
    XRDP_INODE *xinode;
    XRDP_INODE *xinodep;

    if ((name == NULL) || (strlen(name) == 0))
        return NULL;

    if ((xinode = calloc(1, sizeof(XRDP_INODE))) == NULL)
    {
        log_error("system out of memory");
        return NULL;
    }

    log_debug("S_IFDIR=0x%x S_IFREG=0x%x type=0x%x", S_IFDIR, S_IFREG, type);

    xinode->parent_inode = pinode;
    xinode->inode = g_xrdp_fs.next_node++; /* TODO should be thread safe */
    xinode->nlink = 1;
    xinode->uid = getuid();
    xinode->gid = getgid();
    xinode->atime = time(0);
    xinode->mtime = time(0);
    xinode->ctime = time(0);
    xinode->device_id = device_id;
    xinode->is_synced = 1;
    strcpy(xinode->name, name);

    if (type == S_IFDIR)
    {
        xinode->mode = 0755 | type;
        xinode->size = 4096;
    }
    else
    {
        xinode->mode = 0644 | type;
        xinode->size = 0;
    }

    g_xrdp_fs.inode_table[xinode->inode] = xinode;
    g_xrdp_fs.num_entries++; /* TODO should be thread safe */

    /* bump up lookup count in parent dir */
    xinodep = g_xrdp_fs.inode_table[pinode];
    xinodep->nentries++;

    log_debug("LK_TODO: incremented nentries; parent=%d nentries=%d",
              pinode, xinodep->nentries);

    /* LK_TODO */
    xfuse_dump_fs();

    return xinode;
}

/**
 * Check if specified file exists
 *
 * @param parent parent inode of file
 * @param name   flilename or dirname
 *
 * @return 1 if specified file exists, 0 otherwise
 *****************************************************************************/

static int xfuse_does_file_exist(int parent, char *name)
{
    int         i;
    XRDP_INODE *xinode;

    for (i = FIRST_INODE; i < g_xrdp_fs.num_entries; i++)
    {
        if ((xinode = g_xrdp_fs.inode_table[i]) == NULL)
            continue;

        if ((xinode->parent_inode == parent) &&
            (strcmp(xinode->name, name) == 0))
        {
            return 1;
        }
    }

    return 0;
}

/******************************************************************************
**                                                                           **
**                         callbacks for devredir                            **
**                                                                           **
******************************************************************************/

/**
 * Add a file or directory to xrdp file system
 *****************************************************************************/

void xfuse_devredir_cb_enum_dir(void *vp, struct xrdp_inode *xinode)
{
    XFUSE_INFO  *fip = (XFUSE_INFO *) vp;
    XRDP_INODE  *target_inode;

    if ((fip == NULL) || (xinode == NULL))
    {
        log_error("fip or xinode are NULL");
        return;
    }

    /* do we have a valid inode? */
    if (!xfuse_is_inode_valid(fip->inode))
    {
        log_error("inode %d is not valid", fip->inode);
        return;
    }

    /* if filename is . or .. don't add it */
    if ((strcmp(xinode->name, ".") == 0) || (strcmp(xinode->name, "..") == 0))
    {
        free(xinode);
        return;
    }

    /* we have a parent inode and a dir name; what we need is the xinode */
    /* that matches the parent inode and the dir name                    */
    target_inode = xfuse_get_inode_from_pinode_name(fip->inode, fip->name);
    if (target_inode == 0)
        return;

    xinode->parent_inode = target_inode->inode;
    xinode->inode = g_xrdp_fs.next_node++;
    xinode->uid = getuid();
    xinode->gid = getgid();
    xinode->device_id = fip->device_id;

    g_xrdp_fs.num_entries++;

    /* insert it in xrdp fs */
    g_xrdp_fs.inode_table[xinode->inode] = xinode;

    /* bump up lookup count */
    xinode = g_xrdp_fs.inode_table[target_inode->inode];
    xinode->nentries++;

    log_debug("added %s to pinode=%d, nentries=%d target_inode->inode=%d",
              fip->name, fip->inode, xinode->nentries, target_inode->inode);
}

/**
 *****************************************************************************/

void xfuse_devredir_cb_enum_dir_done(void *vp, tui32 IoStatus)
{
    struct xrdp_inode       *xinode;
    struct fuse_entry_param  e;
    int                      i;

    XFUSE_INFO *fip = (XFUSE_INFO *) vp;

    xfuse_dump_fs();

    if (fip == NULL)
    {
        log_debug("fip is NULL");
        goto done;
    }

    if (IoStatus != 0)
    {
        /* command failed */
        if (fip->invoke_fuse)
            fuse_reply_err(fip->req, ENOENT);
        goto done;
    }

    /* do we have a valid inode? */
    if (!xfuse_is_inode_valid(fip->inode))
    {
        log_error("inode %d is not valid", fip->inode);
        if (fip->invoke_fuse)
            fuse_reply_err(fip->req, EBADF);
        goto done;
    }

    log_debug("looking for parent_inode=%d name=%s", fip->inode, fip->name);

    for (i = FIRST_INODE; i < g_xrdp_fs.num_entries; i++)
    {
        if ((xinode = g_xrdp_fs.inode_table[i]) == NULL)
            continue;

        /* match parent inode */
        if (xinode->parent_inode != fip->inode)
            continue;

        /* match name */
        if (strcmp(xinode->name, fip->name) != 0)
            continue;

        memset(&e, 0, sizeof(e));
        e.ino = xinode->inode;
        e.attr_timeout = XFUSE_ATTR_TIMEOUT;
        e.entry_timeout = XFUSE_ENTRY_TIMEOUT;
        e.attr.st_ino = xinode->inode;
		e.attr.st_mode = xinode->mode;
		e.attr.st_nlink = xinode->nlink;
        e.attr.st_uid = xinode->uid;
        e.attr.st_gid = xinode->gid;
		e.attr.st_size = xinode->size;
        e.attr.st_atime = xinode->atime;
        e.attr.st_mtime = xinode->mtime;
        e.attr.st_ctime = xinode->ctime;
        e.generation = 1;

        xinode->is_synced = 1;

        if (fip->invoke_fuse)
            fuse_reply_entry(fip->req, &e);

        break;
    }

    if (i == g_xrdp_fs.num_entries)
    {
        /* requested entry not found */
        log_debug("did NOT find entry");
        if (fip->invoke_fuse)
            fuse_reply_err(fip->req, ENOENT);
    }

done:

    free(fip);
}

void xfuse_devredir_cb_open_file(void *vp, tui32 DeviceId, tui32 FileId)
{
    XFUSE_HANDLE *fh;
    XRDP_INODE   *xinode;

    XFUSE_INFO *fip = (XFUSE_INFO *) vp;
    if (fip == NULL)
    {
        log_debug("fip is NULL");
        goto done;
    }

    if (fip->fi != NULL)
    {
        /* LK_TODO fH NEEDS TO BE RELEASED WHEN THE FILE IS CLOSED */
        /* LK_TODO nopen needs to be decremented when file is closed */
        if ((fh = calloc(1, sizeof(XFUSE_HANDLE))) == NULL)
        {
            log_error("system out of memory");
            free(fip);
            if (fip->invoke_fuse)
                fuse_reply_err(fip->req, ENOMEM);
            return;
        }

        /* save file handle for later use */
        fh->DeviceId = DeviceId;
        fh->FileId = FileId;

        fip->fi->fh = (uint64_t) ((long) fh);
    }

    if (fip->invoke_fuse)
    {
        if (fip->reply_type == RT_FUSE_REPLY_OPEN)
        {
            log_debug("LK_TODO sending fuse_reply_open(); "
                      "DeviceId=%d FileId=%d req=%p fi=%p",
                      fh->DeviceId, fh->FileId, fip->req, fip->fi);

            /* update open count */
            if ((xinode = g_xrdp_fs.inode_table[fip->inode]) != NULL)
                xinode->nopen++;

            fuse_reply_open(fip->req, fip->fi);
        }
        else if (fip->reply_type == RT_FUSE_REPLY_CREATE)
        {
            struct fuse_entry_param  e;

// LK_TODO
#if 0
            if ((xinode = g_xrdp_fs.inode_table[fip->inode]) == NULL)
            {
                log_error("inode at inode_table[%d] is NULL", fip->inode);
                fuse_reply_err(fip->req, EBADF);
                goto done;
            }
#else
            /* create entry in xrdp file system */
            xinode = xfuse_create_file_in_xrdp_fs(fip->device_id, fip->inode,
                                                  fip->name, fip->mode);
            if (xinode == NULL)
            {
                fuse_reply_err(fip->req, ENOMEM);
                return;
            }
#endif
            memset(&e, 0, sizeof(struct fuse_entry_param));

            e.ino = xinode->inode;
            e.attr_timeout = XFUSE_ATTR_TIMEOUT;
            e.entry_timeout = XFUSE_ENTRY_TIMEOUT;
            e.attr.st_ino = xinode->inode;
            e.attr.st_mode = xinode->mode;
            e.attr.st_nlink = xinode->nlink;
            e.attr.st_uid = xinode->uid;
            e.attr.st_gid = xinode->gid;
            e.attr.st_size = xinode->size;
            e.attr.st_atime = xinode->atime;
            e.attr.st_mtime = xinode->mtime;
            e.attr.st_ctime = xinode->ctime;
            e.generation = 1;

            if (fip->mode == S_IFDIR)
                fuse_reply_entry(fip->req, &e);
            else
                fuse_reply_create(fip->req, &e, fip->fi);
        }
        else
        {
            log_error("invalid reply type: %d", fip->reply_type);
        }
    }

done:

    free(fip);
}

void xfuse_devredir_cb_read_file(void *vp, char *buf, size_t length)
{
    XFUSE_HANDLE *fh;
    XFUSE_INFO   *fip;

    fip = (XFUSE_INFO *) vp;
    if (fip == NULL)
        return;

    fuse_reply_buf(fip->req, buf, length);
    free(fip);
}

void xfuse_devredir_cb_write_file(void *vp, char *buf, size_t length)
{
    XRDP_INODE   *xinode;
    XFUSE_HANDLE *fh;
    XFUSE_INFO   *fip;

    fip = (XFUSE_INFO *) vp;
    if (fip == NULL)
        return;

    fuse_reply_write(fip->req, length);

    /* update file size */
    if ((xinode = g_xrdp_fs.inode_table[fip->inode]) != NULL)
        xinode->size += length;
    else
        log_error("inode at inode_table[%d] is NULL", fip->inode);

    free(fip);
}

void xfuse_devredir_cb_rmdir_or_file(void *vp, tui32 IoStatus)
{
    XFUSE_INFO   *fip;
    XRDP_INODE   *xinode;

    fip = (XFUSE_INFO *) vp;
    if (fip == NULL)
        return;

    if (IoStatus != 0)
    {
        fuse_reply_err(fip->req, EBADF);
        free(fip);
        return;
    }

    /* now delete the item in xrdp fs */
    xinode = xfuse_get_inode_from_pinode_name(fip->inode, fip->name);
    if (xinode == NULL)
    {
        fuse_reply_err(fip->req, EBADF);
        free(fip);
        return;
    }

    g_xrdp_fs.inode_table[xinode->inode] = NULL;
    free(xinode);

    /* update parent */
    xinode = g_xrdp_fs.inode_table[fip->inode];
    xinode->nentries--;

    fuse_reply_err(fip->req, 0);
    free(fip);
}

void xfuse_devredir_cb_file_close(void *vp)
{
    XFUSE_INFO   *fip;
    XRDP_INODE   *xinode;

    fip = (XFUSE_INFO *) vp;
    if (fip == NULL)
        return;

    if ((xinode = g_xrdp_fs.inode_table[fip->inode]) == NULL)
        fuse_reply_err(fip->req, EBADF);

    log_debug("before: inode=%d nopen=%d", xinode->inode, xinode->nopen);

    if (xinode->nopen > 0)
        xinode->nopen--;

    log_debug("after: inode=%d nopen=%d", xinode->inode, xinode->nopen);

    fuse_reply_err(fip->req, 0);
}

/******************************************************************************
**                                                                           **
**                           callbacks for fuse                              **
**                                                                           **
******************************************************************************/

/**
 * Look up a directory entry by name and get its attributes
 *
 *****************************************************************************/

static void xfuse_cb_lookup(fuse_req_t req, fuse_ino_t parent, const char *name)
{
    XFUSE_INFO              *fip;
    XRDP_INODE              *xinode;
    struct fuse_entry_param  e;
    tui32                    device_id;
    char                     full_path[4096];
    char                    *cptr;

    log_debug("ENTERED: looking for parent=%d name=%s", (int) parent, name);

    xfuse_dump_fs();

    if (!xfuse_is_inode_valid(parent))
    {
        log_error("inode %d is not valid", parent);
        fuse_reply_err(req, EBADF);
        return;
    }

// LK_TODO
#if 0
    for (i = FIRST_INODE; i < g_xrdp_fs.num_entries; i++)
    {
        if ((xinode = g_xrdp_fs.inode_table[i]) == NULL)
            continue;

        /* match parent inode */
        if (xinode->parent_inode != parent)
            continue;

        /* match name */
        if (strcmp(xinode->name, name) != 0)
            continue;

        /* got a full match; if this dir is located on a remote device */
        /* and is not synced, do a remote look up                      */
        if ((xinode->device_id != 0) && (!xinode->is_synced))
            goto do_remote_lookup;

        memset(&e, 0, sizeof(e));
        e.ino = xinode->inode;
        e.attr_timeout = XFUSE_ATTR_TIMEOUT;
        e.entry_timeout = XFUSE_ENTRY_TIMEOUT;
        e.attr.st_ino = xinode->inode;
		e.attr.st_mode = xinode->mode;
		e.attr.st_nlink = xinode->nlink;
        e.attr.st_uid = xinode->uid;
        e.attr.st_gid = xinode->gid;
		e.attr.st_size = xinode->size;
        e.attr.st_atime = xinode->atime;
        e.attr.st_mtime = xinode->mtime;
        e.attr.st_ctime = xinode->ctime;
        e.generation = 1;

        fuse_reply_entry(req, &e);
        log_debug("found entry in xrdp fs; returning");
        return;
    }
#else
    if ((xinode = xfuse_get_inode_from_pinode_name(parent, name)) != NULL)
    {
        /* got a full match; if this dir is located on a remote device */
        /* and is not synced, do a remote look up                      */
        if ((xinode->device_id != 0) && (!xinode->is_synced))
            goto do_remote_lookup;

        memset(&e, 0, sizeof(e));
        e.ino = xinode->inode;
        e.attr_timeout = XFUSE_ATTR_TIMEOUT;
        e.entry_timeout = XFUSE_ENTRY_TIMEOUT;
        e.attr.st_ino = xinode->inode;
		e.attr.st_mode = xinode->mode;
		e.attr.st_nlink = xinode->nlink;
        e.attr.st_uid = xinode->uid;
        e.attr.st_gid = xinode->gid;
		e.attr.st_size = xinode->size;
        e.attr.st_atime = xinode->atime;
        e.attr.st_mtime = xinode->mtime;
        e.attr.st_ctime = xinode->ctime;
        e.generation = 1;

        fuse_reply_entry(req, &e);
        log_debug("found entry in xrdp fs; returning");
        return;
    }

#endif

do_remote_lookup:

    /* if ino belongs to a redirected share, pass the call to devredir;      */
    /* when done, devredir will invoke xfuse_devredir_cb_enum_dir_done(...)  */
    device_id = xfuse_get_device_id_for_inode((tui32) parent, full_path);
    if (device_id != 0)
    {
        log_debug("did not find entry; redirecting call to dev_redir");

        if ((fip = calloc(1, sizeof(XFUSE_INFO))) == NULL)
        {
            log_error("system out of memory");
            fuse_reply_err(req, ENOMEM);
            return;
        }
        fip->req = req;
        fip->inode = parent;
        strncpy(fip->name, name, 1024);
        fip->name[1023] = 0;
        fip->invoke_fuse = 1;
        fip->device_id = device_id;

        strcat(full_path, "/");
        strcat(full_path, name);

        /* we want path minus 'root node of the share' */
        if ((cptr = strchr(full_path, '/')) == NULL)
        {
            /* enumerate root dir */
            if (dev_redir_get_dir_listing((void *) fip, device_id, "\\"))
            {
                log_error("failed to send dev_redir_get_dir_listing() cmd");
                fuse_reply_buf(req, NULL, 0);
            }
            else
            {
                log_debug("dev_redir_get_dir_listing() called");
            }

        }
        else
        {
            if (dev_redir_get_dir_listing((void *) fip, device_id, cptr))
            {
                log_error("failed to send dev_redir_get_dir_listing() cmd");
                fuse_reply_buf(req, NULL, 0);
            }
        }

        log_debug("cmd sent; reting");
        return;
    }

    log_debug("parent=%d name=%s not found", (int) parent, name);
    fuse_reply_err(req, ENOENT);
}

/**
 * Get file attributes
 *****************************************************************************/

static void xfuse_cb_getattr(fuse_req_t req, fuse_ino_t ino,
                            struct fuse_file_info *fi)
{
    struct xrdp_inode *xino;
    struct stat stbuf;

    (void) fi;

    log_debug("ino=%d", (int) ino);

    /* if ino is not valid, just return */
    if (!xfuse_is_inode_valid(ino))
    {
        log_error("inode %d is not valid", ino);
        fuse_reply_err(req, EBADF);
        return;
    }

    xino = g_xrdp_fs.inode_table[ino];
    xfuse_dump_xrdp_inode(xino);

    memset(&stbuf, 0, sizeof(stbuf));
	stbuf.st_ino = ino;
    stbuf.st_mode = xino->mode;
    stbuf.st_nlink = xino->nlink;
    stbuf.st_size = xino->size;

    fuse_reply_attr(req, &stbuf, 1.0);
}

/**
 *
 *****************************************************************************/

static void xfuse_dirbuf_add(fuse_req_t req, struct dirbuf *b,
                             const char *name, fuse_ino_t ino)
{
    struct stat stbuf;
    size_t oldsize = b->size;

    log_debug("adding ino=%d name=%s", (int) ino, name);

    b->size += fuse_add_direntry(req, NULL, 0, name, NULL, 0);
    b->p = (char *) realloc(b->p, b->size);

    memset(&stbuf, 0, sizeof(stbuf));
    stbuf.st_ino = ino;
    fuse_add_direntry(req, b->p + oldsize, b->size - oldsize, name, &stbuf,
                      b->size);
}

/**
 *
 *****************************************************************************/

static void xfuse_cb_readdir(fuse_req_t req, fuse_ino_t ino, size_t size,
                             off_t off, struct fuse_file_info *fi)
{
    struct xrdp_inode *xinode;
    struct dirbuf      b;
    int                i;

    (void) fi;

    log_debug("looking for dir with inode=%d", ino);

    if (!xfuse_is_inode_valid(ino))
    {
        log_error("inode %d is not valid", ino);
        fuse_reply_err(req, EBADF);
        return;
    }

    /* does this dir have any entries? */
    xinode = g_xrdp_fs.inode_table[ino];
    memset(&b, 0, sizeof(b));
    if (ino == 1)
    {
        xfuse_dirbuf_add(req, &b, ".", 1);
        xfuse_dirbuf_add(req, &b, "..", 1);
    }
    else
    {
        xfuse_dirbuf_add(req, &b, ".", xinode->inode);
        xfuse_dirbuf_add(req, &b, "..", xinode->parent_inode);
    }

    for (i = FIRST_INODE; i < g_xrdp_fs.num_entries; i++)
    {
        if ((xinode = g_xrdp_fs.inode_table[i]) == NULL)
            continue;

        if (xinode->parent_inode == ino)
            xfuse_dirbuf_add(req, &b, xinode->name, xinode->inode);
    }

    if (off < b.size)
        fuse_reply_buf(req, b.p + off, min(b.size - off, size));
    else
        fuse_reply_buf(req, NULL, 0);

    free(b.p);
}

/**
 * Create a directory
 *****************************************************************************/

static void xfuse_cb_mkdir(fuse_req_t req, fuse_ino_t parent,
                           const char *name, mode_t mode)
{
    XRDP_INODE               *xinode;
    struct fuse_entry_param   e;

    if ((xinode = xfuse_get_inode_from_pinode_name(parent, name)) != NULL)
    {
        /* dir already exists, just return it */
        memset(&e, 0, sizeof(struct fuse_entry_param));

        e.ino = xinode->inode;
        e.attr_timeout = XFUSE_ATTR_TIMEOUT;
        e.entry_timeout = XFUSE_ENTRY_TIMEOUT;
        e.attr.st_ino = xinode->inode;
        e.attr.st_mode = xinode->mode;
        e.attr.st_nlink = xinode->nlink;
        e.attr.st_uid = xinode->uid;
        e.attr.st_gid = xinode->gid;
        e.attr.st_size = xinode->size;
        e.attr.st_atime = xinode->atime;
        e.attr.st_mtime = xinode->mtime;
        e.attr.st_ctime = xinode->ctime;
        e.generation = 1;

        fuse_reply_entry(req, &e);
        return;
    }

    /* dir does not exist, create it */
    xfuse_create_dir_or_file(req, parent, name, mode, NULL, S_IFDIR);
}

/**
 * Remove specified dir
 *****************************************************************************/

static void xfuse_cb_rmdir(fuse_req_t req, fuse_ino_t parent,
                           const char *name)
{
    xfuse_remove_dir_or_file(req, parent, name, 1);
}

static void xfuse_cb_unlink(fuse_req_t req, fuse_ino_t parent,
                            const char *name)
{
    xfuse_remove_dir_or_file(req, parent, name, 2);
}

/**
 * Remove a dir or file
 *
 * @param type 1=dir, 2=file
 *****************************************************************************/

static void xfuse_remove_dir_or_file(fuse_req_t req, fuse_ino_t parent,
                                     const char *name, int type)
{
    XFUSE_INFO *fip;
    XRDP_INODE *xinode;
    char       *cptr;
    char        full_path[4096];
    tui32       device_id;

    log_debug("entered: parent=%d name=%s", parent, name);

    /* is parent inode valid? */
    if (!xfuse_is_inode_valid(parent))
    {
        log_error("inode %d is not valid", parent);
        fuse_reply_err(req, EBADF);
        return;
    }

    if ((xinode = xfuse_get_inode_from_pinode_name(parent, name)) == NULL)
    {
        log_error("did not find file with pinode=%d name=%s", parent, name);
        fuse_reply_err(req, EBADF);
        return;
    }

    device_id = xfuse_get_device_id_for_inode(parent, full_path);

    log_debug("path=%s nentries=%d", full_path, xinode->nentries);

    if ((type == 1) && (xinode->nentries != 0))
    {
        log_debug("cannot rmdir; lookup count is %d", xinode->nentries);
        fuse_reply_err(req, ENOTEMPTY);
        return;
    }
    else if ((type == 2) && (xinode->nopen != 0))
    {
        log_debug("cannot unlink; open count is %d", xinode->nopen);
        fuse_reply_err(req, EBUSY);
        return;
    }

    strcat(full_path, "/");
    strcat(full_path, name);

    if (device_id == 0)
    {
        /* specified file is a local resource */
        //XFUSE_HANDLE *fh;

        log_debug("LK_TODO: this is still a TODO");
        fuse_reply_err(req, EINVAL);
        return;
    }

    /* specified file resides on redirected share */

    if ((fip = calloc(1, sizeof(XFUSE_INFO))) == NULL)
    {
        log_error("system out of memory");
        fuse_reply_err(req, ENOMEM);
        return;
    }

    fip->req = req;
    fip->inode = parent;
    fip->invoke_fuse = 1;
    fip->device_id = device_id;
    strncpy(fip->name, name, 1024);
    fip->name[1023] = 0;
    fip->type = type;

    /* we want path minus 'root node of the share' */
    if ((cptr = strchr(full_path, '/')) == NULL)
    {
        /* get dev_redir to open the remote file */
        if (devredir_rmdir_or_file((void *) fip, device_id, "\\", O_RDWR))
        {
            log_error("failed to send dev_redir_open_file() cmd");
            fuse_reply_err(req, EREMOTEIO);
            free(fip);
            return;
        }
    }
    else
    {
        if (devredir_rmdir_or_file((void *) fip, device_id, cptr, O_RDWR))
        {
            log_error("failed to send dev_redir_get_dir_listing() cmd");
            fuse_reply_err(req, EREMOTEIO);
            free(fip);
            return;
        }
    }
}

/**
 * Create a directory or file
 *
 * @param req    opaque FUSE object
 * @param parent parent inode
 * @param name   name of dir or file to create
 * @param mode   creation mode
 * @param fi     for storing file handles
 * @param type   S_IFDIR for dir and S_IFREG for file
 *****************************************************************************/

static void xfuse_create_dir_or_file(fuse_req_t req, fuse_ino_t parent,
                                     const char *name, mode_t mode,
                                     struct fuse_file_info *fi, int type)
{
    XFUSE_INFO        *fip;    // LK_TODO use only XFUSE_INFO instead of struct
    char              *cptr;
    char               full_path[1024];
    tui32              device_id;

    full_path[0] = 0;

    log_debug("entered: type = %s", (type == S_IFDIR) ? "dir" : "file");

    /* name must be valid */
    if ((name == NULL) || (strlen(name) == 0))
    {
        log_error("invalid name");
        fuse_reply_err(req, EBADF);
        return;
    }

    /* is parent inode valid? */
    if (!xfuse_is_inode_valid(parent))
    {
        log_error("inode %d is not valid", parent);
        fuse_reply_err(req, EBADF);
        return;
    }

    device_id = xfuse_get_device_id_for_inode(parent, full_path);
    strcat(full_path, "/");
    strcat(full_path, name);

    if (device_id == 0)
    {
        /* specified file is a local resource */
        //XFUSE_HANDLE *fh;

        log_debug("LK_TODO: this is still a TODO");
        fuse_reply_err(req, EINVAL);
        return;
    }

    /* specified file resides on redirected share */

    if ((fip = calloc(1, sizeof(XFUSE_INFO))) == NULL)
    {
       log_error("system out of memory");
       fuse_reply_err(req, ENOMEM);
       return;
    }

    fip->req = req;
    fip->fi = fi;
    fip->inode = parent;
    fip->invoke_fuse = 1;
    fip->device_id = device_id;
    fip->mode = type;
    fip->reply_type = RT_FUSE_REPLY_CREATE;
    strncpy(fip->name, name, 1024);
    fip->name[1023] = 0;

    /* LK_TODO need to handle open permissions */

    /* we want path minus 'root node of the share' */
    if ((cptr = strchr(full_path, '/')) == NULL)
    {
       /* get dev_redir to open the remote file */
       if (dev_redir_file_open((void *) fip, device_id, "\\", O_CREAT, type))
       {
           log_error("failed to send dev_redir_open_file() cmd");
           fuse_reply_err(req, EREMOTEIO);
       }
    }
    else
    {
       if (dev_redir_file_open((void *) fip, device_id, cptr, O_CREAT, type))
       {
           log_error("failed to send dev_redir_get_dir_listing() cmd");
           fuse_reply_err(req, EREMOTEIO);
       }
    }
}

/**
 * Open specified file
 *****************************************************************************/

static void xfuse_cb_open(fuse_req_t req, fuse_ino_t ino,
                          struct fuse_file_info *fi)
{
    XRDP_INODE        *xinode;
    XFUSE_INFO        *fip;
    char              *cptr;
    char               full_path[4096];
    tui32              device_id;

    log_debug("LK_TODO: open_flags=0x%x req=%p fi=%p",
              fi->flags, req, fi);

    if (!xfuse_is_inode_valid(ino))
    {
        log_error("inode %d is not valid", ino);
        fuse_reply_err(req, EBADF);
        return;
    }

    /* if ino points to a dir, fail the open request */
    xinode = g_xrdp_fs.inode_table[ino];
    if (xinode->mode & S_IFDIR)
    {
        log_debug("reading a dir not allowed!");
        fuse_reply_err(req, EISDIR);
        return;
    }

    device_id = xfuse_get_device_id_for_inode((tui32) ino, full_path);
    if (device_id)
    {
         /* specified file resides on redirected share */

        log_debug("LK_TODO looking for file %s in DeviceId=%d", full_path, device_id);

        if ((fip = calloc(1, sizeof(XFUSE_INFO))) == NULL)
        {
            log_error("system out of memory");
            fuse_reply_err(req, ENOMEM);
            return;
        }

        fip->req = req;
        fip->inode = ino;
        fip->invoke_fuse = 1;
        fip->device_id = device_id;
        fip->fi = fi;
        strncpy(fip->name, full_path, 1024);
        fip->name[1023] = 0;
        fip->reply_type = RT_FUSE_REPLY_OPEN;

        /* LK_TODO need to handle open permissions */

        /* we want path minus 'root node of the share' */
        if ((cptr = strchr(full_path, '/')) == NULL)
        {
            /* get dev_redir to open the remote file */
            if (dev_redir_file_open((void *) fip, device_id, "\\",
                                    fi->flags, S_IFREG))
            {
                log_error("failed to send dev_redir_open_file() cmd");
                fuse_reply_err(req, EREMOTEIO);
            }
        }
        else
        {
            if (dev_redir_file_open((void *) fip, device_id, cptr,
                                    fi->flags, S_IFREG))
            {
                log_error("failed to send dev_redir_get_dir_listing() cmd");
                fuse_reply_err(req, EREMOTEIO);
            }
        }
    }
    else
    {
        /* specified file is a local resource */
        //XFUSE_HANDLE *fh;

        log_debug("LK_TODO: this is still a TODO");
        fuse_reply_err(req, EINVAL);
    }
}

static void xfuse_cb_flush(fuse_req_t req, fuse_ino_t ino, struct
                           fuse_file_info *fi)
{
    XFUSE_INFO   *fip    = NULL;
    XFUSE_HANDLE *handle = (XFUSE_HANDLE *) fi->fh;

    if (!xfuse_is_inode_valid(ino))
    {
        log_error("inode %d is not valid", ino);
        fuse_reply_err(req, EBADF);
        return;
    }

    if (handle->DeviceId == 0)
    {
        /* specified file is a local resource */
        log_debug("LK_TODO: this is still a TODO");
        fuse_reply_err(req, EBADF);
        return;
    }

    /* specified file resides on redirected share */

    if ((fip = calloc(1, sizeof(XFUSE_INFO))) == NULL)
    {
        log_error("system out of memory");
        fuse_reply_err(req, ENOMEM);
        return;
    }

    fip->req = req;
    fip->inode = ino;
    fip->invoke_fuse = 1;
    fip->device_id = handle->DeviceId;
    fip->fi = fi;

    if (devredir_file_close((void *) fip, fip->device_id, handle->FileId))
    {
        log_error("failed to send devredir_close_file() cmd");
        fuse_reply_err(req, EREMOTEIO);
    }
}

/**
 *****************************************************************************/

static void xfuse_cb_read(fuse_req_t req, fuse_ino_t ino, size_t size,
                          off_t off, struct fuse_file_info *fi)
{
    XFUSE_HANDLE *fh;
    XFUSE_INFO   *fusep;
    long          handle;

    log_debug("want_bytes %d bytes at off %d", size, off);

    if (fi->fh == 0)
    {
        log_debug("LK_TODO: looks like fi->fh is corrupted");
        fuse_reply_err(req, EINVAL);
        return;
    }

    log_debug("$$$$$$$$$$$$$ LK_TODO: fh=0x%llx", fi->fh);

    handle = fi->fh;
    fh = (XFUSE_HANDLE *) handle;

    if (fh->DeviceId == 0)
    {
        /* target file is in .clipboard dir */
        log_debug(">>>>>>>>>>>>>>>>> THIS IS STILL A TODO!");
        return;
    }

    /* target file is on a remote device */

    if ((fusep = calloc(1, sizeof(XFUSE_INFO))) == NULL)
    {
        log_error("system out of memory");
        fuse_reply_err(req, ENOMEM);
        return;
    }
    fusep->req = req;
    fusep->inode = ino;
    fusep->invoke_fuse = 1;
    fusep->device_id = fh->DeviceId;
    fusep->fi = fi;

    dev_redir_file_read(fusep, fh->DeviceId, fh->FileId, size, off);
    log_debug("exiting");
}

static void xfuse_cb_write(fuse_req_t req, fuse_ino_t ino, const char *buf,
                           size_t size, off_t off, struct fuse_file_info *fi)
{
    XFUSE_HANDLE *fh;
    XFUSE_INFO   *fusep;
    long          handle;

    log_debug("write %d bytes at off %d", size, off);

    if (fi->fh == 0)
    {
        log_debug("LK_TODO: looks like fi->fh is corrupted");
        fuse_reply_err(req, EINVAL);
        return;
    }

    handle = fi->fh;
    fh = (XFUSE_HANDLE *) handle;

    if (fh->DeviceId == 0)
    {
        /* target file is in .clipboard dir */
        log_debug(">>>>>>>>>>>>>>>>> THIS IS STILL A TODO!");
        return;
    }

    /* target file is on a remote device */

    if ((fusep = calloc(1, sizeof(XFUSE_INFO))) == NULL)
    {
        log_error("system out of memory");
        fuse_reply_err(req, ENOMEM);
        return;
    }
    fusep->req = req;
    fusep->inode = ino;
    fusep->invoke_fuse = 1;
    fusep->device_id = fh->DeviceId;
    fusep->fi = fi;

    dev_redir_file_write(fusep, fh->DeviceId, fh->FileId, buf, size, off);
    log_debug("exiting");
}

static void xfuse_cb_create(fuse_req_t req, fuse_ino_t parent,
                            const char *name, mode_t mode,
                            struct fuse_file_info *fi)
{
    xfuse_create_dir_or_file(req, parent, name, mode, fi, S_IFREG);
}

// LK_TODO may not need to implement the following funcs

#if 0
static void xfuse_cb_statfs(fuse_req_t req, fuse_ino_t ino)
{
    log_debug(">>>>>>>>>>>>>>>> LK_TODO: entered");
    fuse_reply_err(req, ENOMEM);
}

static void xfuse_cb_setxattr(fuse_req_t req, fuse_ino_t ino, const char *name,
                     const char *value, size_t size, int flags)
{
    log_debug(">>>>>>>>>>>>>>>> LK_TODO: entered");
    fuse_reply_err(req, ENOMEM);

}

static void xfuse_cb_getxattr(fuse_req_t req, fuse_ino_t ino, const char *name,
                     size_t size)
{
    log_debug(">>>>>>>>>>>>>>>> LK_TODO: entered");
    fuse_reply_err(req, ENOMEM);

}

static void xfuse_cb_listxattr(fuse_req_t req, fuse_ino_t ino, size_t size)
{
    log_debug(">>>>>>>>>>>>>>>> LK_TODO: entered");
    fuse_reply_err(req, ENOMEM);

}

static void xfuse_cb_access(fuse_req_t req, fuse_ino_t ino, int mask)
{
    log_debug(">>>>>>>>>>>>>>>> LK_TODO: entered");
    fuse_reply_err(req, ENOMEM);

}

static void xfuse_cb_getlk(fuse_req_t req, fuse_ino_t ino,
                           struct fuse_file_info *fi, struct flock *lock)
{
    log_debug(">>>>>>>>>>>>>>>> LK_TODO: entered");
    fuse_reply_err(req, ENOMEM);

}

static void xfuse_cb_setlk(fuse_req_t req, fuse_ino_t ino,
                           struct fuse_file_info *fi, struct flock *lock,
                           int sleep)
{
    log_debug(">>>>>>>>>>>>>>>> LK_TODO: entered");
    fuse_reply_err(req, ENOMEM);

}

static void xfuse_cb_ioctl(fuse_req_t req, fuse_ino_t ino, int cmd, void *arg,
                           struct fuse_file_info *fi, unsigned flags,
                           const void *in_buf, size_t in_bufsz,
                           size_t out_bufsz)
{
    log_debug(">>>>>>>>>>>>>>>> LK_TODO: entered");
    fuse_reply_err(req, ENOMEM);

}

static void xfuse_cb_poll(fuse_req_t req, fuse_ino_t ino,
                          struct fuse_file_info *fi,
                          struct fuse_pollhandle *ph)
{
    log_debug(">>>>>>>>>>>>>>>> LK_TODO: entered");
    fuse_reply_err(req, ENOMEM);

}
#endif

static void xfuse_cb_setattr(fuse_req_t req, fuse_ino_t ino, struct stat *attr,
                             int to_set, struct fuse_file_info *fi)
{
    XRDP_INODE        *xinode;

    log_debug(">>>>>>>>>>>>>>>> LK_TODO: entered to_set=0x%x", to_set);

    if (!xfuse_is_inode_valid(ino))
    {
        log_error("inode %d is not valid", ino);
        fuse_reply_err(req, EBADF);
        return;
    }

    xinode = g_xrdp_fs.inode_table[ino];

    if (to_set & FUSE_SET_ATTR_MODE)
    {
        xinode->mode = attr->st_mode;
        log_debug(">>>>>>>>>>>>>>>> LK_TODO: FUSE_SET_ATTR_MODE");

    }

    if (to_set & FUSE_SET_ATTR_UID)
    {
        xinode->uid = attr->st_uid;
        log_debug(">>>>>>>>>>>>>>>> LK_TODO: FUSE_SET_ATTR_UID");
    }

    if (to_set & FUSE_SET_ATTR_GID)
    {
        xinode->gid = attr->st_gid;
        log_debug(">>>>>>>>>>>>>>>> LK_TODO: FUSE_SET_ATTR_GID");
    }

    if (to_set & FUSE_SET_ATTR_SIZE)
    {
        log_debug("previous file size: %d", attr->st_size);
        xinode->size = attr->st_size;
        log_debug("returning file size: %d", xinode->size);
    }

    if (to_set & FUSE_SET_ATTR_ATIME)
    {
        xinode->atime = attr->st_atime;
        log_debug(">>>>>>>>>>>>>>>> LK_TODO: FUSE_SET_ATTR_ATIME");
    }

    if (to_set & FUSE_SET_ATTR_MTIME)
    {
        xinode->mtime = attr->st_mtime;
        log_debug(">>>>>>>>>>>>>>>> LK_TODO: FUSE_SET_ATTR_MTIME");
    }

    if (to_set & FUSE_SET_ATTR_ATIME_NOW)
    {
        xinode->atime = attr->st_atime;
        log_debug(">>>>>>>>>>>>>>>> LK_TODO: FUSE_SET_ATTR_ATIME_NOW");
    }

    if (to_set & FUSE_SET_ATTR_MTIME_NOW)
    {
        xinode->mtime = attr->st_mtime;
        log_debug(">>>>>>>>>>>>>>>> LK_TODO: FUSE_SET_ATTR_MTIME_NOW");
    }

    fuse_reply_attr(req, attr, 1.0); /* LK_TODO just faking for now */
}

#endif /* end else #ifndef XRDP_FUSE */
