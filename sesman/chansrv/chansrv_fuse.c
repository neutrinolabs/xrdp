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
 *      o when creating dir/file, ensure it does not already exist
 *      o do not allow dirs to be created in ino==1 except for .clipboard and share mounts
 *      o fix the HACK where I have to use my own buf instead of g_buffer
 *        this is in func xfuse_check_wait_objs()
 *      o if fuse mount point is already mounted, I get segfault
 *      o in open, check for modes such as O_TRUNC, O_APPEND
 *      o copying over an existing file does not work
 *      o after a dir is created, the device cannot be unmounted on the client side
 *        so something is holding it up
 *      o in thunar, when I move a file by dragging to another folder, the file
 *        is getting copied instead of being moved
 *      o unable to edit files in vi
 *      o fuse ops to support
 *          o touch does not work
 *          o chmod must work
 *          o cat >> file is not working
 *
 */

//#define USE_SYNC_FLAG

/* FUSE mount point */
char g_fuse_root_path[256] = "";
char g_fuse_clipboard_path[256] = ""; /* for clipboard use */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#ifndef XRDP_FUSE

/******************************************************************************
**                                                                           **
**                   when FUSE is NOT enabled in xrdp                        **
**                                                                           **
******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "arch.h"
#include "chansrv_fuse.h"

/* dummy calls when XRDP_FUSE is not defined */
int xfuse_init(void)            { return 0; }
int xfuse_deinit(void)          { return 0; }
int xfuse_check_wait_objs(void) { return 0; }
int xfuse_get_wait_objs(tbus *objs, int *count, int *timeout) { return 0; }
int xfuse_clear_clip_dir(void)  { return 0; }
int xfuse_file_contents_range(int stream_id, const char *data, int data_bytes)     { return 0; }
int xfuse_file_contents_size(int stream_id, int file_size)                   { return 0; }
int xfuse_add_clip_dir_item(const char *filename, int flags, int size, int lindex) { return 0; }
int xfuse_create_share(tui32 device_id, const char *dirname)                       { return 0; }
void xfuse_devredir_cb_open_file(void *vp, tui32 IoStatus, tui32 DeviceId, tui32 FileId)     {}
void xfuse_devredir_cb_write_file(void *vp, const char *buf, size_t length)        {}
void xfuse_devredir_cb_read_file(void *vp, const char *buf, size_t length)         {}
int  xfuse_devredir_cb_enum_dir(void *vp, struct xrdp_inode *xinode)         { return 0; }
void xfuse_devredir_cb_enum_dir_done(void *vp, tui32 IoStatus)               {}
void xfuse_devredir_cb_rmdir_or_file(void *vp, tui32 IoStatus)               {}
void xfuse_devredir_cb_rename_file(void *vp, tui32 IoStatus)                 {}
void xfuse_devredir_cb_file_close(void *vp)                                  {}

#else

/******************************************************************************
**                                                                           **
**                     when FUSE is enabled in xrdp                          **
**                                                                           **
******************************************************************************/

#define FUSE_USE_VERSION 26

#include <fuse_lowlevel.h>
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
#include "clipboard_file.h"
#include "chansrv_fuse.h"
#include "devredir.h"
#include "list.h"
#include "fifo.h"
#include "file.h"

#ifndef EREMOTEIO
#define EREMOTEIO EIO
#endif

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
#define LOG_LEVEL   LOG_ERROR

#define log_error(_params...)                           \
{                                                       \
    g_write("[%10.10u]: FUSE       %s: %d : ERROR: ",   \
            g_time3(), __func__, __LINE__);             \
    g_writeln (_params);                                \
}

#define log_always(_params...)                          \
{                                                       \
    g_write("[%10.10u]: FUSE       %s: %d : ALWAYS: ",  \
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
    if (LOG_DEBUG <= LOG_LEVEL)                         \
    {                                                   \
        g_write("[%10.10u]: FUSE       %s: %d : ",      \
                g_time3(), __func__, __LINE__);         \
        g_writeln (_params);                            \
    }                                                   \
}

#define OP_RENAME_FILE  0x01

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

struct dirbuf1
{
    char buf[4096];
    int  bytes_in_buf;
    int  first_time;
};

/* FUSE reply types */
#define RT_FUSE_REPLY_OPEN      1
#define RT_FUSE_REPLY_CREATE    2

struct xfuse_info
{
    struct fuse_file_info *fi;
    fuse_req_t             req;
    fuse_ino_t             inode;
    fuse_ino_t             new_inode;
    int                    invoke_fuse;
    char                   name[1024];
    char                   new_name[1024];
    tui32                  device_id;
    int                    reply_type;
    int                    mode;
    int                    type;
    size_t                 size;
    off_t                  off;
    struct dirbuf1         dirbuf1;
};
typedef struct xfuse_info XFUSE_INFO;

struct xfuse_handle
{
    tui32 DeviceId;
    tui32 FileId;
    int   is_loc_resource; /* this is not a redirected resource */
};
typedef struct xfuse_handle XFUSE_HANDLE;

/* used for file data request sent to client */
struct req_list_item
{
    fuse_req_t req;
    int stream_id;
    int lindex;
    int off;
    int size;
};

struct dir_info
{
    /* last index accessed in g_xrdp_fs.inode_table[] */
    int index;
};

/* queue FUSE opendir commands so we run only one at a time */
struct opendir_req
{
    fuse_req_t             req;
    fuse_ino_t             ino;
    struct fuse_file_info *fi;
};

static char g_fuse_mount_name[256] = "xrdp_client";

FIFO g_fifo_opendir;

static struct list *g_req_list = 0;
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
static int xfuse_init_xrdp_fs(void);
static int xfuse_deinit_xrdp_fs(void);
static int xfuse_init_lib(struct fuse_args *args);
static int xfuse_is_inode_valid(fuse_ino_t ino);

// LK_TODO
#if 0
static void xfuse_create_file(fuse_req_t req, fuse_ino_t parent,
                              const char *name, mode_t mode, int type);
#endif

static void xfuse_dump_fs(void);
static tui32 xfuse_get_device_id_for_inode(fuse_ino_t ino, char *full_path);
static void fuse_reverse_pathname(char *full_path, char *reverse_path);

static struct xrdp_inode * xfuse_get_inode_from_pinode_name(fuse_ino_t pinode,
                                                            const char *name);

static struct xrdp_inode *
xfuse_create_file_in_xrdp_fs(tui32 device_id, int pinode, const char *name,
                             int type);

static int  xfuse_does_file_exist(fuse_ino_t parent, char *name);
static int  xfuse_delete_file_with_xinode(XRDP_INODE *xinode);
static int  xfuse_delete_dir_with_xinode(XRDP_INODE *xinode);
static int  xfuse_recursive_delete_dir_with_xinode(XRDP_INODE *xinode);
static void xfuse_update_xrdpfs_size(void);

#ifdef USE_SYNC_FLAG
static void xfuse_enum_dir(fuse_req_t req, fuse_ino_t ino, size_t size,
                           off_t off, struct fuse_file_info *fi);
#endif

/* forward declarations for FUSE callbacks */
static void xfuse_cb_lookup(fuse_req_t req, fuse_ino_t parent,
                            const char *name);

static void xfuse_cb_getattr(fuse_req_t req, fuse_ino_t ino,
                             struct fuse_file_info *fi);

/* this is not a callback, but it's used by xfuse_cb_readdir() */
static int xfuse_dirbuf_add1(fuse_req_t req, struct dirbuf1 *b,
                             const char *name, fuse_ino_t ino);

static void xfuse_cb_readdir(fuse_req_t req, fuse_ino_t ino, size_t size,
                             off_t off, struct fuse_file_info *fi);

static void xfuse_cb_mkdir(fuse_req_t req, fuse_ino_t parent,
                           const char *name, mode_t mode);

static void xfuse_cb_rmdir(fuse_req_t req, fuse_ino_t parent,
                           const char *name);

static void xfuse_cb_unlink(fuse_req_t req, fuse_ino_t parent,
                            const char *name);

static void xfuse_cb_rename(fuse_req_t req,
                            fuse_ino_t old_parent, const char *old_name,
                            fuse_ino_t new_parent, const char *new_name);

/* this is not a callback, but it is used by the above two functions */
static void xfuse_remove_dir_or_file(fuse_req_t req, fuse_ino_t parent,
                                     const char *name, int type);

static void xfuse_create_dir_or_file(fuse_req_t req, fuse_ino_t parent,
                                     const char *name, mode_t mode,
                                     struct fuse_file_info *fi, int type);

static void xfuse_cb_open(fuse_req_t req, fuse_ino_t ino,
                          struct fuse_file_info *fi);

static void xfuse_cb_release(fuse_req_t req, fuse_ino_t ino, struct
                             fuse_file_info *fi);

static void xfuse_cb_read(fuse_req_t req, fuse_ino_t ino, size_t size,
                          off_t off, struct fuse_file_info *fi);

static void xfuse_cb_write(fuse_req_t req, fuse_ino_t ino, const char *buf,
                           size_t size, off_t off, struct fuse_file_info *fi);

static void xfuse_cb_create(fuse_req_t req, fuse_ino_t parent,
                            const char *name, mode_t mode,
                            struct fuse_file_info *fi);

#if 0
static void xfuse_cb_fsync(fuse_req_t req, fuse_ino_t ino, int datasync,
                           struct fuse_file_info *fi);
#endif

static void xfuse_cb_setattr(fuse_req_t req, fuse_ino_t ino, struct stat *attr,
                             int to_set, struct fuse_file_info *fi);

static void xfuse_cb_opendir(fuse_req_t req, fuse_ino_t ino,
                             struct fuse_file_info *fi);

static int xfuse_proc_opendir_req(fuse_req_t req, fuse_ino_t ino,
                                  struct fuse_file_info *fi);

static void xfuse_cb_releasedir(fuse_req_t req, fuse_ino_t ino,
                                struct fuse_file_info *fi);

/* misc calls */
static void xfuse_mark_as_stale(fuse_ino_t pinode);
static void xfuse_delete_stale_entries(fuse_ino_t pinode);

/*****************************************************************************/
int
load_fuse_config(void)
{
    int index;
    char cfg_file[256];
    struct list *items;
    struct list *values;
    char *item;
    char *value;

    items = list_create();
    items->auto_free = 1;
    values = list_create();
    values->auto_free = 1;
    g_snprintf(cfg_file, 255, "%s/sesman.ini", XRDP_CFG_PATH);
    file_by_name_read_section(cfg_file, "Chansrv", items, values);
    for (index = 0; index < items->count; index++)
    {
        item = (char *)list_get_item(items, index);
        value = (char *)list_get_item(values, index);
        if (g_strcasecmp(item, "FuseMountName") == 0)
        {
            g_strncpy(g_fuse_mount_name, value, 255);
        }
    }
    list_delete(items);
    list_delete(values);
    return 0;
}

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

int
xfuse_init(void)
{
    struct fuse_args args = FUSE_ARGS_INIT(0, NULL);

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

    load_fuse_config();

    /* define FUSE mount point to ~/xrdp_client, ~/thinclient_drives */
    g_snprintf(g_fuse_root_path, 255, "%s/%s", g_getenv("HOME"), g_fuse_mount_name);
    g_snprintf(g_fuse_clipboard_path, 255, "%s/.clipboard", g_fuse_root_path);

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

    /* setup FIFOs */
    fifo_init(&g_fifo_opendir, 30);

    /* setup FUSE callbacks */
    g_memset(&g_xfuse_ops, 0, sizeof(g_xfuse_ops));
    g_xfuse_ops.lookup      = xfuse_cb_lookup;
    g_xfuse_ops.readdir     = xfuse_cb_readdir;
    g_xfuse_ops.mkdir       = xfuse_cb_mkdir;
    g_xfuse_ops.rmdir       = xfuse_cb_rmdir;
    g_xfuse_ops.unlink      = xfuse_cb_unlink;
    g_xfuse_ops.rename      = xfuse_cb_rename;
    g_xfuse_ops.open        = xfuse_cb_open;
    g_xfuse_ops.release     = xfuse_cb_release;
    g_xfuse_ops.read        = xfuse_cb_read;
    g_xfuse_ops.write       = xfuse_cb_write;
    g_xfuse_ops.create      = xfuse_cb_create;
    //g_xfuse_ops.fsync     = xfuse_cb_fsync; /* LK_TODO delete this */
    g_xfuse_ops.getattr     = xfuse_cb_getattr;
    g_xfuse_ops.setattr     = xfuse_cb_setattr;
    g_xfuse_ops.opendir     = xfuse_cb_opendir;
    g_xfuse_ops.releasedir  = xfuse_cb_releasedir;

    fuse_opt_add_arg(&args, "xrdp-chansrv");
    fuse_opt_add_arg(&args, g_fuse_root_path);
    //fuse_opt_add_arg(&args, "-s"); /* single threaded mode */
    //fuse_opt_add_arg(&args, "-d"); /* debug mode           */

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

int
xfuse_deinit(void)
{
    xfuse_deinit_xrdp_fs();
    fifo_deinit(&g_fifo_opendir);

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

    if (g_req_list != 0)
    {
        list_delete(g_req_list);
        g_req_list = 0;
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

int xfuse_create_share(tui32 device_id, const char *dirname)
{
#if 0
    XFUSE_INFO  *fip;
#endif
    XRDP_INODE  *xinode;
    /* tui32        saved_inode; */

    if (dirname == NULL || strlen(dirname) == 0)
        return -1;

    /* Do we have an inode table yet? */
    if (xfuse_init_xrdp_fs())
    {
        return -1;
    }

    xinode = g_new0(struct xrdp_inode, 1);
    if (xinode == NULL)
    {
        log_debug("g_new0() failed");
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
    /* saved_inode = xinode->inode; */

    /* insert it in xrdp fs */
    g_xrdp_fs.inode_table[xinode->inode] = xinode;
    log_debug("created new share named %s at inode_table[%d]",
              dirname, xinode->inode);

    /* update nentries in parent inode */
    xinode = g_xrdp_fs.inode_table[1];
    if (xinode == NULL)
        return -1;
    xinode->nentries++;

#if 0
    fip = g_new0(XFUSE_INFO, 1);
    if (fip == NULL)
    {
        log_error("system out of memory");
        return -1;
    }

     /* enumerate root dir, do not call FUSE when done */
    fip->req = NULL;
    fip->inode = 1; /* TODO saved_inode; */
    strncpy(fip->name, dirname, 1024);
    fip->name[1023] = 0;
    fip->device_id = device_id;

    dev_redir_get_dir_listing((void *) fip, device_id, "\\");
#endif

    return 0;
}

/**
 * Clear all clipboard entries in xrdp_fs
 *
 * This function is called by clipboard code
 *
 * @return 0 on success, -1 on failure
 *****************************************************************************/

int xfuse_clear_clip_dir(void)
{
    fuse_ino_t i;
    XRDP_INODE *xinode;
    XRDP_INODE *xip;

    log_debug("entered");

    if (g_xrdp_fs.inode_table == NULL)
    {
        return 0;
    }

    /* xinode for .clipboard */
    xip = g_xrdp_fs.inode_table[2];

    for (i = FIRST_INODE; i < g_xrdp_fs.num_entries; i++)
    {
        if ((xinode = g_xrdp_fs.inode_table[i]) == NULL)
            continue;

        if (xinode->parent_inode == 2)
        {
            g_xrdp_fs.inode_table[i] = NULL;
            free(xinode);
            xip->nentries--;
        }
    }

    return 0;
}

/**
 * Return clipboard data to fuse
 *
 * @return 0 on success, -1 on failure
 *****************************************************************************/

int
xfuse_file_contents_range(int stream_id, const char *data, int data_bytes)
{
    log_debug("entered: stream_id=%d data_bytes=%d", stream_id, data_bytes);

    struct req_list_item *rli;

    if ((rli = (struct req_list_item *) list_get_item(g_req_list, 0)) == NULL)
    {
        log_error("range error!");
        return -1;
    }

    log_debug("lindex=%d off=%d size=%d", rli->lindex, rli->off, rli->size);

    fuse_reply_buf(rli->req, data, data_bytes);

    list_remove_item(g_req_list, 0);
    if (g_req_list->count <= 0)
    {
        log_debug("completed all requests");
        return 0;
    }

    /* send next request */
    rli = (struct req_list_item *) list_get_item(g_req_list, 0);
    if (rli == NULL)
    {
        log_error("range error!");
        return -1;
    }

    log_debug("requesting clipboard file data");

    clipboard_request_file_data(rli->stream_id, rli->lindex,
                                    rli->off, rli->size);

    return 0;
}

/**
 * Create a file in .clipboard dir
 *
 * This function is called by clipboard code
 *
 * @return 0 on success, -1 on failure
 *****************************************************************************/

int
xfuse_add_clip_dir_item(const char *filename, int flags, int size, int lindex)
{
    log_debug("entered: filename=%s flags=%d size=%d lindex=%d",
              filename, flags, size, lindex);

    /* add entry to xrdp_fs */
    XRDP_INODE *xinode = xfuse_create_file_in_xrdp_fs(0,    /* device id    */
                                                      2,    /* parent inode */
                                                      filename,
                                                      S_IFREG);
    if (xinode == NULL)
    {
        log_debug("failed to create file in xrdp filesystem");
        return -1;
    }
    xinode->size = size;
    xinode->lindex = lindex;
    xinode->is_loc_resource = 1;

    return 0;
}

/**
 *
 *
 * @return 0 on success, -1 on failure
 *****************************************************************************/

int xfuse_file_contents_size(int stream_id, int file_size)
{
    log_debug("entered: stream_id=%d file_size=%d", stream_id, file_size);
    return 0;
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

    g_buffer = g_new0(char, g_bufsize);
    g_fd = fuse_chan_fd(g_ch);

    g_req_list = list_create();
    g_req_list->auto_free = 1;

    return 0;
}

/**
 * Initialize xrdp file system
 *
 * @return 0 on success, -1 on failure
 *
 *****************************************************************************/

static int xfuse_init_xrdp_fs(void)
{
    struct xrdp_inode *xino;

    /* Already called? */
    if (g_xrdp_fs.inode_table != NULL)
    {
        return 0;
    }

    g_xrdp_fs.inode_table = g_new0(struct xrdp_inode *, 4096);
    if (g_xrdp_fs.inode_table == NULL)
    {
        log_error("system out of memory");
        return -1;
    }

    /*
     * index 0 is our .. dir
     */

    xino = g_new0(struct xrdp_inode, 1);
    if (xino == NULL)
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

    xino = g_new0(struct xrdp_inode, 1);
    if (xino == NULL)
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

    xino = g_new0(struct xrdp_inode, 1);
    if (xino == NULL)
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
    xino->nentries = 0;
    xino->mode = S_IFDIR | 0755;
    xino->uid = getuid();
    xino->gid = getgid();
    xino->size = 0;
    xino->atime = time(0);
    xino->mtime = time(0);
    xino->ctime = time(0);
    xino->is_loc_resource = 1;
    strcpy(xino->name, ".clipboard");

    g_xrdp_fs.max_entries = 4096;
    g_xrdp_fs.num_entries = 3;
    g_xrdp_fs.next_node = 3;

    return 0;
}

/**
 * zap the xrdp file system
 *
 * @return 0 on success, -1 on failure
 *****************************************************************************/

static int xfuse_deinit_xrdp_fs(void)
{
    return 0;
}

/**
 * determine if specified ino exists in xrdp file system
 *
 * @return 1 if it does, 0 otherwise
 *****************************************************************************/

static int xfuse_is_inode_valid(fuse_ino_t ino)
{
    /* is ino present in our table? */
    if ((ino < FIRST_INODE) || (ino >= g_xrdp_fs.next_node))
        return 0;

    if (g_xrdp_fs.inode_table[ino] == NULL)
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

    log_debug("parent=%ld name=%s", parent, name);

    /* do we have a valid parent inode? */
    if (!xfuse_is_inode_valid(parent))
    {
        log_error("inode %ld is not valid", parent);
        fuse_reply_err(req, EBADF);
    }

    xinode = g_new0(struct xrdp_inode, 1);
    if (xinode == NULL)
    {
        log_error("g_new0() failed");
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
    xfuse_update_xrdpfs_size();
    log_debug("inserted new dir at inode_table[%d]", xinode->inode);

    xfuse_dump_fs();

    log_debug("new inode=%d", xinode->inode);

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

static void xfuse_dump_fs(void)
{
    fuse_ino_t i;
    struct xrdp_inode *xinode;

    log_debug("found %d entries", g_xrdp_fs.num_entries - FIRST_INODE);

#if 0
    log_debug("not dumping xrdp fs");
    return;
#endif

    for (i = FIRST_INODE; i < g_xrdp_fs.num_entries; i++)
    {
        if ((xinode = g_xrdp_fs.inode_table[i]) == NULL)
            continue;

        log_debug("pinode=%d inode=%d nentries=%d nopen=%d is_synced=%d name=%s",
                  xinode->parent_inode, xinode->inode,
                  xinode->nentries, xinode->nopen, xinode->is_synced,
                  xinode->name);
    }
    log_debug("%s", "");
}

/**
 * Dump contents of xinode structure
 *
 * @param xino xinode structure to dump
 *****************************************************************************/

#if 0
static void xfuse_dump_xrdp_inode(struct xrdp_inode *xino)
{
    log_debug("--- dumping struct xinode ---");
    log_debug("name:          %s", xino->name);
    log_debug("parent_inode:  %d", xino->parent_inode);
    log_debug("inode:         %d", xino->inode);
    log_debug("mode:          %o", xino->mode);
    log_debug("nlink:         %d", xino->nlink);
    log_debug("uid:           %d", xino->uid);
    log_debug("gid:           %d", xino->gid);
    log_debug("size:          %zd", xino->size);
    log_debug("device_id:     %d", xino->device_id);
    log_debug("%s", "");
}
#endif

/**
 * Return the device_id associated with specified inode and copy the
 * full path to the specified inode into full_path
 *
 * @param ino the inode
 * @param full_path full path to the inode
 *
 * @return the device_id of specified inode
 *****************************************************************************/

static tui32 xfuse_get_device_id_for_inode(fuse_ino_t ino, char *full_path)
{
    fuse_ino_t parent_inode = 0;
    fuse_ino_t child_inode  = ino;
    char    reverse_path[4096];

    /* ino == 1 is a special case; we already know that it is not */
    /* associated with any device redirection                     */
    if (ino == 1)
    {
        /* just return the device_id for the file in full_path */
        log_debug("looking for file with pinode=%ld name=%s", ino, full_path);
        xfuse_dump_fs();

        XRDP_INODE *xinode = xfuse_get_inode_from_pinode_name(ino, full_path);
        full_path[0] = 0;
        if (xinode)
            return xinode->device_id;
        else
            return 0;
    }

    reverse_path[0] = 0;
    full_path[0] = 0;

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

static struct xrdp_inode * xfuse_get_inode_from_pinode_name(fuse_ino_t pinode,
                                                            const char *name)
{
    fuse_ino_t i;
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

static struct xrdp_inode *
xfuse_create_file_in_xrdp_fs(tui32 device_id, int pinode, const char *name,
                             int type)
{
    XRDP_INODE *xinode;
    XRDP_INODE *xinodep;

    if ((name == NULL) || (strlen(name) == 0))
        return NULL;

    xinode = g_new0(XRDP_INODE, 1);
    if (xinode == NULL)
    {
        log_error("system out of memory");
        return NULL;
    }

    xinode->parent_inode = pinode;
    xinode->inode = g_xrdp_fs.next_node++;
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
    g_xrdp_fs.num_entries++;

    /* bump up lookup count in parent dir */
    xinodep = g_xrdp_fs.inode_table[pinode];
    xinodep->nentries++;
    xfuse_update_xrdpfs_size();

    log_debug("incremented nentries; parent=%d nentries=%d",
              pinode, xinodep->nentries);

    return xinode;
}

/**
 * Check if specified file exists
 *
 * @param parent parent inode of file
 * @param name   filename or dirname
 *
 * @return 1 if specified file exists, 0 otherwise
 *****************************************************************************/

static int xfuse_does_file_exist(fuse_ino_t parent, char *name)
{
    fuse_ino_t i;
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

static int xfuse_delete_file_with_xinode(XRDP_INODE *xinode)
{
    /* make sure it is not a dir */
    if ((xinode == NULL) || (xinode->mode & S_IFDIR))
        return -1;

    log_always("deleting: inode=%d name=%s", xinode->inode, xinode->name);
    log_debug("deleting: inode=%d name=%s", xinode->inode, xinode->name);

    g_xrdp_fs.inode_table[xinode->parent_inode]->nentries--;
    g_xrdp_fs.inode_table[xinode->inode] = NULL;
    free(xinode);

    return 0;
}

static int xfuse_delete_dir_with_xinode(XRDP_INODE *xinode)
{
    XRDP_INODE *xip;
    fuse_ino_t i;

    /* make sure it is not a file */
    if ((xinode == NULL) || (xinode->mode & S_IFREG))
        return -1;

    for (i = FIRST_INODE; i < g_xrdp_fs.num_entries; i++)
    {
        if ((xip = g_xrdp_fs.inode_table[i]) == NULL)
            continue;

        /* look for child inodes */
        if (xip->parent_inode == xinode->inode)
        {
            /* got one, delete it */
            g_xrdp_fs.inode_table[xip->inode] = NULL;
            free(xip);
        }
    }

    /* our parent will have one less dir */
    g_xrdp_fs.inode_table[xinode->parent_inode]->nentries--;

    g_xrdp_fs.inode_table[xinode->inode] = NULL;
    free(xinode);

    return 0;
}

/**
 * Recursively delete dir with specified inode
 *****************************************************************************/

static int xfuse_recursive_delete_dir_with_xinode(XRDP_INODE *xinode)
{
    XRDP_INODE *xip;
    fuse_ino_t i;

    /* make sure it is not a file */
    if ((xinode == NULL) || (xinode->mode & S_IFREG))
        return -1;

    log_always("recursively deleting dir with inode=%d name=%s",
              xinode->inode, xinode->name);

    log_debug("recursively deleting dir with inode=%d name=%s",
              xinode->inode, xinode->name);

    for (i = FIRST_INODE; i < g_xrdp_fs.num_entries; i++)
    {
        if ((xip = g_xrdp_fs.inode_table[i]) == NULL)
            continue;

        /* look for child inodes */
        if (xip->parent_inode == xinode->inode)
        {
            /* got one */
            if (xip->mode & S_IFREG)
            {
                /* regular file */
                g_xrdp_fs.inode_table[xip->parent_inode]->nentries--;
                g_xrdp_fs.inode_table[xip->inode] = NULL;
                free(xip);
            }
            else
            {
                /* got another dir */
                xfuse_recursive_delete_dir_with_xinode(xip);
            }
        }
    }

    /* our parent will have one less dir */
    g_xrdp_fs.inode_table[xinode->parent_inode]->nentries--;

    g_xrdp_fs.inode_table[xinode->inode] = NULL;
    free(xinode);

    return 0;
}

static void xfuse_update_xrdpfs_size(void)
{
    void *vp;
    int diff;

    diff = g_xrdp_fs.max_entries - g_xrdp_fs.num_entries;
    if (diff > 100)
        return;

    /* extend memory */
    vp = realloc(g_xrdp_fs.inode_table,
                 (g_xrdp_fs.max_entries + 100) * sizeof(struct xrdp_inode *));

    if (vp == NULL)
    {
        log_error("system out of memory");
        return;
    }

    /* zero newly added memory */
    memset((char *) vp + g_xrdp_fs.max_entries * sizeof(struct xrdp_inode *),
           0,
           100 * sizeof(struct xrdp_inode *));

    g_xrdp_fs.max_entries += 100;
    g_xrdp_fs.inode_table = (struct xrdp_inode **) vp;
}

/******************************************************************************
**                                                                           **
**                         callbacks for devredir                            **
**                                                                           **
******************************************************************************/

/**
 * Add a file or directory to xrdp file system
 *****************************************************************************/

int xfuse_devredir_cb_enum_dir(void *vp, struct xrdp_inode *xinode)
{
    XFUSE_INFO *fip = (XFUSE_INFO *) vp;
    XRDP_INODE *xip = NULL;

    if ((fip == NULL) || (xinode == NULL))
    {
        log_error("fip or xinode are NULL");
        return -1;
    }

    if (!xfuse_is_inode_valid(fip->inode))
    {
        log_error("inode %ld is not valid", fip->inode);
        g_free(xinode);
        return -1;
    }

    log_debug("parent_inode=%ld name=%s", fip->inode, xinode->name);

    /* if filename is . or .. don't add it */
    if ((strcmp(xinode->name, ".") == 0) || (strcmp(xinode->name, "..") == 0))
    {
        g_free(xinode);
        return -1;
    }

    xfuse_dump_fs();

    if ((xip = xfuse_get_inode_from_pinode_name(fip->inode, xinode->name)) != NULL)
    {
        log_debug("inode=%ld name=%s already exists in xrdp_fs; not adding it",
                  fip->inode, xinode->name);
        g_free(xinode);
        xip->stale = 0;
        return -1;
    }

    xinode->parent_inode = fip->inode;
    xinode->inode = g_xrdp_fs.next_node++;
    xinode->uid = getuid();
    xinode->gid = getgid();
    xinode->device_id = fip->device_id;

    g_xrdp_fs.num_entries++;

    /* insert it in xrdp fs and update lookup count */
    g_xrdp_fs.inode_table[xinode->inode] = xinode;
    g_xrdp_fs.inode_table[fip->inode]->nentries++;
    xfuse_update_xrdpfs_size();

    xfuse_dump_fs();
    return 0;
}

/**
 *****************************************************************************/

void xfuse_devredir_cb_enum_dir_done(void *vp, tui32 IoStatus)
{
    XFUSE_INFO         *fip;
    struct dir_info    *di;
    struct opendir_req *odreq;

    log_debug("vp=%p IoStatus=0x%x", vp, IoStatus);

    fip = (XFUSE_INFO *) vp;
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
        log_error("inode %ld is not valid", fip->inode);
        if (fip->invoke_fuse)
            fuse_reply_err(fip->req, EBADF);
        goto done;
    }

    xfuse_delete_stale_entries(fip->inode);

    /* this will be used by xfuse_cb_readdir() */
    di = g_new0(struct dir_info, 1);
    di->index = FIRST_INODE;
    fip->fi->fh = (tintptr) di;

    fuse_reply_open(fip->req, fip->fi);

done:

    if (fip)
        free(fip);

    /* remove current request */
    g_free(fifo_remove(&g_fifo_opendir));

    while (1)
    {
        /* process next request */
        odreq = (struct opendir_req *) fifo_peek(&g_fifo_opendir);
        if (!odreq)
            return;

        if (xfuse_proc_opendir_req(odreq->req, odreq->ino, odreq->fi))
            g_free(fifo_remove(&g_fifo_opendir)); /* req failed */
        else
            break; /* req has been queued */
    }
}

void xfuse_devredir_cb_open_file(void *vp, tui32 IoStatus, tui32 DeviceId,
                                 tui32 FileId)
{
    XFUSE_HANDLE *fh;
    XRDP_INODE   *xinode;

    XFUSE_INFO *fip = (XFUSE_INFO *) vp;
    if (fip == NULL)
    {
        log_debug("fip is NULL");
        goto done;
    }

    log_debug("+++ XFUSE_INFO=%p XFUSE_INFO->fi=%p DeviceId=%d FileId=%d",
              fip, fip->fi, DeviceId, FileId);

    if (IoStatus != 0)
    {
        if (!fip->invoke_fuse)
            goto done;

        switch (IoStatus)
        {
        case 0xC0000022:
            fuse_reply_err(fip->req, EACCES);
            break;

        case 0xC0000033:
        case 0xC0000034:
            fuse_reply_err(fip->req, ENOENT);
            break;

        default:
            fuse_reply_err(fip->req, EIO);
            break;
        }

        goto done;
    }

    if (fip->fi != NULL)
    {
        fh = g_new0(XFUSE_HANDLE, 1);
        if (fh == NULL)
        {
            log_error("system out of memory");
            if (fip->invoke_fuse)
                fuse_reply_err(fip->req, ENOMEM);

            free(fip);
            return;
        }

        /* save file handle for later use */
        fh->DeviceId = DeviceId;
        fh->FileId = FileId;

        fip->fi->fh = (tintptr) fh;
        log_debug("+++ XFUSE_INFO=%p XFUSE_INFO->fi=%p XFUSE_INFO->fi->fh=0x%llx",
                  fip, fip->fi, (long long) fip->fi->fh);
    }

    if (fip->invoke_fuse)
    {
        if (fip->reply_type == RT_FUSE_REPLY_OPEN)
        {
            log_debug("sending fuse_reply_open(); "
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
                log_error("inode at inode_table[%ld] is NULL", fip->inode);
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
            {
                fuse_reply_entry(fip->req, &e);
            }
            else
            {
                xinode->nopen++;
                fuse_reply_create(fip->req, &e, fip->fi);
            }
        }
        else
        {
            log_error("invalid reply type: %d", fip->reply_type);
        }
    }

done:

    free(fip);
}

void xfuse_devredir_cb_read_file(void *vp, const char *buf, size_t length)
{
    XFUSE_INFO   *fip;

    fip = (XFUSE_INFO *) vp;
    if ((fip == NULL) || (fip->req == NULL))
    {
        log_error("fip for fip->req is NULL");
        return;
    }

    fuse_reply_buf(fip->req, buf, length);
    free(fip);
}

void xfuse_devredir_cb_write_file(void *vp, const char *buf, size_t length)
{
    XRDP_INODE   *xinode;
    XFUSE_INFO   *fip;

    fip = (XFUSE_INFO *) vp;
    if ((fip == NULL) || (fip->req == NULL) || (fip->fi == NULL))
    {
        log_error("fip, fip->req or fip->fi is NULL");
        return;
    }

    log_debug("+++ XFUSE_INFO=%p, XFUSE_INFO->fi=%p XFUSE_INFO->fi->fh=0x%llx",
              fip, fip->fi, (long long) fip->fi->fh);

    fuse_reply_write(fip->req, length);

    /* update file size */
    if ((xinode = g_xrdp_fs.inode_table[fip->inode]) != NULL)
        xinode->size += length;
    else
        log_error("inode at inode_table[%ld] is NULL", fip->inode);

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

void xfuse_devredir_cb_rename_file(void *vp, tui32 IoStatus)
{
    XFUSE_INFO   *fip;
    XRDP_INODE   *old_xinode;
    XRDP_INODE   *new_xinode;

    fip = (XFUSE_INFO *) vp;
    if (fip == NULL)
        return;

    if (IoStatus != 0)
    {
        fuse_reply_err(fip->req, EEXIST);
        free(fip);
        return;
    }

    /*
     * update xrdp file system
     */

    /* if destination dir/file exists, delete it */
    if (xfuse_does_file_exist(fip->new_inode, fip->new_name))
    {
        new_xinode = xfuse_get_inode_from_pinode_name(fip->new_inode,
                                                      fip->new_name);

        if (new_xinode)
        {
            if (new_xinode->mode & S_IFREG)
                xfuse_delete_file_with_xinode(new_xinode);
            else
                xfuse_delete_dir_with_xinode(new_xinode);

            new_xinode = NULL;
        }
    }

    old_xinode = xfuse_get_inode_from_pinode_name(fip->inode, fip->name);
    if (old_xinode == NULL)
    {
        fuse_reply_err(fip->req, EBADF);
        free(fip);
        return;
    }

    old_xinode->parent_inode = fip->new_inode;
    strncpy(old_xinode->name, fip->new_name, 1023);
    old_xinode->name[1023] = 0;

    if (fip->inode != fip->new_inode)
    {
        /* file has been moved to a different dir */
        old_xinode->is_synced = 1;
        g_xrdp_fs.inode_table[fip->inode]->nentries--;
        g_xrdp_fs.inode_table[fip->new_inode]->nentries++;
    }

    fuse_reply_err(fip->req, 0);
    free(fip);
}

void xfuse_devredir_cb_file_close(void *vp)
{
    XFUSE_INFO   *fip;
    XRDP_INODE   *xinode;

    fip = (XFUSE_INFO *) vp;
    if (fip == NULL)
    {
        log_error("fip is NULL");
        return;
    }

    if (fip->fi == NULL)
    {
        log_error("fip->fi is NULL");
        return;
    }

    log_debug("+++ XFUSE_INFO=%p XFUSE_INFO->fi=%p XFUSE_INFO->fi->fh=0x%llx",
              fip, fip->fi, (long long) fip->fi->fh);

    if ((xinode = g_xrdp_fs.inode_table[fip->inode]) == NULL)
    {
        log_debug("inode_table[%ld] is NULL", fip->inode);
        fuse_reply_err(fip->req, EBADF);
        return;
    }

    log_debug("before: inode=%d nopen=%d", xinode->inode, xinode->nopen);

    if (xinode->nopen > 0)
        xinode->nopen--;

    /* LK_TODO */
#if 0
    if ((xinode->nopen == 0) && fip->fi && fip->fi->fh)
    {
        printf("LK_TODO: ################################ fi=%p fi->fh=0x%llx\n",
               fip->fi, (long long) fip->fi->fh);

        free((char *) (tintptr) (fip->fi->fh));
        fip->fi->fh = 0;
    }
#endif

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
    XRDP_INODE              *xinode;
    struct fuse_entry_param  e;

    log_debug("looking for parent=%ld name=%s", parent, name);
    xfuse_dump_fs();

    if (!xfuse_is_inode_valid(parent))
    {
        log_error("inode %ld is not valid", parent);
        fuse_reply_err(req, EBADF);
        return;
    }

    xinode = xfuse_get_inode_from_pinode_name(parent, name);
    if (xinode == NULL)
    {
        log_debug("did not find entry for parent=%ld name=%s", parent, name);
        fuse_reply_err(req, ENOENT);
        return;
    }

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
    log_debug("found entry for parent=%ld name=%s uid=%d gid=%d",
              parent, name, xinode->uid, xinode->gid);
    return;
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

    log_debug("req=%p ino=%ld", req, ino);

    /* if ino is not valid, just return */
    if (!xfuse_is_inode_valid(ino))
    {
        log_error("inode %ld is not valid", ino);
        fuse_reply_err(req, EBADF);
        return;
    }

    xino = g_xrdp_fs.inode_table[ino];
    if (!xino)
    {
        log_debug("****** invalid ino=%ld", ino);
        fuse_reply_err(req, EBADF);
        return;
    }

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

#if 0
static void xfuse_dirbuf_add(fuse_req_t req, struct dirbuf *b,
                             const char *name, fuse_ino_t ino)
{
    struct stat stbuf;
    size_t oldsize = b->size;

    log_debug("adding ino=%ld name=%s", ino, name);

    b->size += fuse_add_direntry(req, NULL, 0, name, NULL, 0);
    b->p = (char *) realloc(b->p, b->size);

    memset(&stbuf, 0, sizeof(stbuf));
    stbuf.st_ino = ino;
    fuse_add_direntry(req, b->p + oldsize, b->size - oldsize, name, &stbuf,
                      b->size);
}
#endif

static int xfuse_dirbuf_add1(fuse_req_t req, struct dirbuf1 *b,
                             const char *name, fuse_ino_t ino)
{
    struct stat  stbuf;
    int          len;

    len = fuse_add_direntry(req, NULL, 0, name, NULL, 0);
    if (b->bytes_in_buf + len > 4096)
    {
        log_debug("not adding entry because dirbuf overflow would occur");
        return -1;
    }

    memset(&stbuf, 0, sizeof(stbuf));
    stbuf.st_ino = ino;

    fuse_add_direntry(req,
                      &b->buf[b->bytes_in_buf], /* index where new entry will be added to buf */
                      4096 - len,               /* remaining size of buf */
                      name,                     /* name of entry */
                      &stbuf,                   /* file attributes */
                      b->bytes_in_buf + len     /* offset of next entry */
                     );

    b->bytes_in_buf += len;
    return 0;
}

/**
 *
 *****************************************************************************/

static void xfuse_cb_readdir(fuse_req_t req, fuse_ino_t ino, size_t size,
                             off_t off, struct fuse_file_info *fi)
{
    XRDP_INODE      *xinode;
    XRDP_INODE      *ti;
    struct dir_info *di;
    struct dirbuf1   b;
    fuse_ino_t       i;
    int              first_time;

    log_debug("req=%p inode=%ld size=%zd offset=%lld", req, ino, size, (long long) off);

    /* do we have a valid inode? */
    if (!xfuse_is_inode_valid(ino))
    {
        log_error("inode %ld is not valid", ino);
        fuse_reply_err(req, EBADF);
        return;
    }

    di = (struct dir_info *) (tintptr) (fi->fh);
    if (di == NULL)
    {
        /* something seriously wrong somewhere! */
        fuse_reply_buf(req, 0, 0);
        return;
    }

    b.bytes_in_buf = 0;
    first_time = (di->index == FIRST_INODE) ? 1 : 0;

    for (i = di->index; i < g_xrdp_fs.num_entries; i++, di->index++)
    {
        if ((xinode = g_xrdp_fs.inode_table[i]) == NULL)
            continue;

        /* match parent inode */
        if (xinode->parent_inode != ino)
            continue;

        xinode->is_synced = 1;

        if (first_time)
        {
            first_time = 0;
            ti = g_xrdp_fs.inode_table[ino];
            if (!ti)
            {
                log_debug("****** g_xrdp_fs.inode_table[%ld] is NULL", ino);
                fuse_reply_buf(req, NULL, 0);
                return;
            }
            xfuse_dirbuf_add1(req, &b, ".", ino);
            xfuse_dirbuf_add1(req, &b, "..", ti->parent_inode);
        }

        if (xfuse_dirbuf_add1(req, &b, xinode->name, xinode->inode))
            break; /* buffer is full */
    }

    if (b.bytes_in_buf)
        fuse_reply_buf(req, b.buf, b.bytes_in_buf);
    else
        fuse_reply_buf(req, NULL, 0);
}

/**
 * Create a directory
 *****************************************************************************/

static void xfuse_cb_mkdir(fuse_req_t req, fuse_ino_t parent,
                           const char *name, mode_t mode)
{
    XRDP_INODE               *xinode;
    struct fuse_entry_param   e;

    log_debug("entered: parent_inode=%ld name=%s", parent, name);

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

    log_debug("entered: parent=%ld name=%s", parent, name);

    /* is parent inode valid? */
    if (!xfuse_is_inode_valid(parent))
    {
        log_error("inode %ld is not valid", parent);
        fuse_reply_err(req, EBADF);
        return;
    }

    if ((xinode = xfuse_get_inode_from_pinode_name(parent, name)) == NULL)
    {
        log_error("did not find file with pinode=%ld name=%s", parent, name);
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
    else if (type == 2)
    {
        if ((xinode->nopen > 1) || ((xinode->nopen == 1) &&
                                    (xinode->close_in_progress == 0)))
        {
            log_debug("cannot unlink; open count is %d", xinode->nopen);
            fuse_reply_err(req, EBUSY);
            return;
        }
    }

    strcat(full_path, "/");
    strncat(full_path, name, sizeof(full_path) - strlen(full_path) - 1);

    if (xinode->is_loc_resource)
    {
        /* specified file is a local resource */
        //XFUSE_HANDLE *fh;

        log_debug("LK_TODO: this is still a TODO");
        fuse_reply_err(req, EINVAL);
        return;
    }

    /* specified file resides on redirected share */

    fip = g_new0(XFUSE_INFO, 1);
    if (fip == NULL)
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

static void xfuse_cb_rename(fuse_req_t req,
                            fuse_ino_t old_parent, const char *old_name,
                            fuse_ino_t new_parent, const char *new_name)
{
    XRDP_INODE *old_xinode;
    XFUSE_INFO *fip;
    tui32       new_device_id;
    char       *cptr;
    char        old_full_path[1024];
    char        new_full_path[1024];
    const char *cp;

    tui32 device_id;

    log_debug("entered: old_parent=%ld old_name=%s new_parent=%ld new_name=%s",
              old_parent, old_name, new_parent, new_name);
    xfuse_dump_fs();

    /* is old_parent inode valid? */
    if (!xfuse_is_inode_valid(old_parent))
    {
        log_error("inode %ld is not valid", old_parent);
        fuse_reply_err(req, EINVAL);
        return;
    }

    /* is new_parent inode valid? */
    if (!xfuse_is_inode_valid(new_parent))
    {
        log_error("inode %ld is not valid", new_parent);
        fuse_reply_err(req, EINVAL);
        return;
    }

    if ((old_name == NULL) || (strlen(old_name) == 0))
    {
        fuse_reply_err(req, EINVAL);
        return;
    }

    if ((new_name == NULL) || (strlen(new_name) == 0))
    {
        fuse_reply_err(req, EINVAL);
        return;
    }

    old_xinode = xfuse_get_inode_from_pinode_name(old_parent, old_name);
    if (old_xinode  == NULL)
    {
        log_error("did not find file with pinode=%ld name=%s",
                  old_parent, old_name);
        fuse_reply_err(req, EBADF);
        return;
    }

    /* if file is open, cannot rename */
    if (old_xinode->nopen != 0)
    {
        fuse_reply_err(req, EBUSY);
        return;
    }

    /* rename across file systems not yet supported */
    new_device_id = xfuse_get_device_id_for_inode(new_parent, new_full_path);
    strcat(new_full_path, "/");
    strcat(new_full_path, new_name);

    if (new_device_id != old_xinode->device_id)
    {
        log_error("rename across file systems not supported");
        fuse_reply_err(req, EINVAL);
        return;
    }

    if (old_xinode->is_loc_resource)
    {
        /* specified file is a local resource */
        log_debug("LK_TODO: this is still a TODO");
        fuse_reply_err(req, EINVAL);
        return;
    }

    /* resource is on a redirected share */

    device_id = old_xinode->device_id;

    xfuse_get_device_id_for_inode(old_parent, old_full_path);
    strcat(old_full_path, "/");
    strcat(old_full_path, old_name);

    fip = g_new0(XFUSE_INFO, 1);
    if (fip == NULL)
    {
        log_error("system out of memory");
        fuse_reply_err(req, ENOMEM);
        return;
    }

    fip->req = req;
    fip->inode = old_parent;
    fip->new_inode = new_parent;
    strncpy(fip->name, old_name, 1024);
    strncpy(fip->new_name, new_name, 1024);
    fip->name[1023] = 0;
    fip->new_name[1023] = 0;
    fip->invoke_fuse = 1;
    fip->device_id = device_id;

    if ((cp = strchr(new_full_path, '/')) == NULL)
        cp = "\\";

    /* we want path minus 'root node of the share' */
    if ((cptr = strchr(old_full_path, '/')) == NULL)
    {
        /* get dev_redir to open the remote file */
        if (dev_redir_file_open((void *) fip, device_id, "\\",
                                O_RDWR, S_IFREG | OP_RENAME_FILE, cp))
        {
            log_error("failed to send dev_redir_file_open() cmd");
            fuse_reply_err(req, EREMOTEIO);
            free(fip);
            return;
        }
    }
    else
    {
        if (dev_redir_file_open((void *) fip, device_id, cptr,
                                O_RDWR, S_IFREG | OP_RENAME_FILE, cp))
        {
            log_error("failed to send dev_redir_file_open() cmd");
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
    XFUSE_INFO        *fip;
    char              *cptr;
    char               full_path[1024];
    tui32              device_id;

    full_path[0] = 0;

    log_debug("entered: parent_ino=%ld name=%s type=%s",
              parent, name, (type == S_IFDIR) ? "dir" : "file");

    /* name must be valid */
    if ((name == NULL) || (strlen(name) == 0))
    {
        log_error("invalid name");
        fuse_reply_err(req, EBADF);
        return;
    }

    /* is parent inode valid? */
    if ((parent == 1) || (!xfuse_is_inode_valid(parent)))
    {
        log_error("inode %ld is not valid", parent);
        fuse_reply_err(req, EBADF);
        return;
    }

    device_id = xfuse_get_device_id_for_inode(parent, full_path);
    strcat(full_path, "/");
    strcat(full_path, name);

    XRDP_INODE *xinode = g_xrdp_fs.inode_table[parent];
    if (xinode->is_loc_resource)
    {
        /* specified file is a local resource */
        //XFUSE_HANDLE *fh;

        log_debug("LK_TODO: this is still a TODO");
        fuse_reply_err(req, EINVAL);
        return;
    }

    /* specified file resides on redirected share */

    fip = g_new0(XFUSE_INFO, 1);
    if (fip == NULL)
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

    log_debug("+++ created XFUSE_INFO=%p XFUSE_INFO->fi=%p", fip, fip->fi);

    /* LK_TODO need to handle open permissions */

    /* we want path minus 'root node of the share' */
    if ((cptr = strchr(full_path, '/')) == NULL)
    {
       /* get dev_redir to open the remote file */
       if (dev_redir_file_open((void *) fip, device_id, "\\",
                               O_CREAT, type, NULL))
       {
           log_error("failed to send dev_redir_open_file() cmd");
           fuse_reply_err(req, EREMOTEIO);
       }
    }
    else
    {
       if (dev_redir_file_open((void *) fip, device_id, cptr,
                               O_CREAT, type, NULL))
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

    log_debug("entered: ino=%ld", ino);

    if (!xfuse_is_inode_valid(ino))
    {
        log_error("inode %ld is not valid", ino);
        fuse_reply_err(req, EBADF);
        return;
    }

    /* if ino points to a dir, fail the open request */
    xinode = g_xrdp_fs.inode_table[ino];
    if (!xinode)
    {
        log_debug("****** g_xrdp_fs.inode_table[%ld] is NULL", ino);
        fuse_reply_err(req, EBADF);
        return;
    }
    if (xinode->mode & S_IFDIR)
    {
        log_debug("reading a dir not allowed!");
        fuse_reply_err(req, EISDIR);
        return;
    }

    device_id = xfuse_get_device_id_for_inode(ino, full_path);

    if (xinode->is_loc_resource)
    {
        /* specified file is a local resource */
        XFUSE_HANDLE *fh = g_new0(XFUSE_HANDLE, 1);
        fh->is_loc_resource = 1;
        fi->fh = (tintptr) fh;
        fuse_reply_open(req, fi);
        return;
    }

    /* specified file resides on redirected share */

    fip = g_new0(XFUSE_INFO, 1);
    if (fip == NULL)
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

    log_debug("LK_TODO: fip->fi = %p", fip->fi);

    strncpy(fip->name, full_path, 1024);
    fip->name[1023] = 0;
    fip->reply_type = RT_FUSE_REPLY_OPEN;

    /* we want path minus 'root node of the share' */
    if ((cptr = strchr(full_path, '/')) == NULL)
    {
       /* get dev_redir to open the remote file */
       if (dev_redir_file_open((void *) fip, device_id, "\\",
                               fi->flags, S_IFREG, NULL))
       {
           log_error("failed to send dev_redir_open_file() cmd");
           fuse_reply_err(req, EREMOTEIO);
       }
    }
    else
    {
       if (dev_redir_file_open((void *) fip, device_id, cptr,
                               fi->flags, S_IFREG, NULL))
       {
           log_error("failed to send dev_redir_get_dir_listing() cmd");
           fuse_reply_err(req, EREMOTEIO);
       }
    }
}

static void xfuse_cb_release(fuse_req_t req, fuse_ino_t ino, struct
                             fuse_file_info *fi)
{
    XFUSE_INFO   *fip    = NULL;
    XFUSE_HANDLE *handle = (XFUSE_HANDLE *) (tintptr) (fi->fh);

    log_debug("entered: ino=%ld fi=%p fi->fh=0x%llx", ino, fi,
              (long long) fi->fh);

    if (!xfuse_is_inode_valid(ino))
    {
        log_error("inode %ld is not valid", ino);
        fuse_reply_err(req, EBADF);
        return;
    }

    XRDP_INODE *xinode = g_xrdp_fs.inode_table[ino];
    if (!xinode)
    {
        log_debug("****** g_xrdp_fs.inode_table[%ld] is NULL", ino);
        fuse_reply_err(req, 0);
        return;
    }
    if (xinode->is_loc_resource)
    {
        /* specified file is a local resource */
        fuse_reply_err(req, 0);
        return;
    }

    /* specified file resides on redirected share */

    log_debug("nopen=%d", xinode->nopen);

    /* if file is not opened, just return */
    if (xinode->nopen == 0)
    {
        log_debug("cannot close because file not opened");
        fuse_reply_err(req, 0);
        return;
    }

    fip = g_new0(XFUSE_INFO, 1);
    if (fip == NULL)
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

    log_debug(" +++ created XFUSE_INFO=%p XFUSE_INFO->fi=%p XFUSE_INFO->fi->fh=0x%llx",
              fip, fip->fi, (long long) fip->fi->fh);

    fip->fi->fh = 0;
    xinode->close_in_progress = 1;

    if (devredir_file_close((void *) fip, fip->device_id, handle->FileId))
    {
        log_error("failed to send devredir_close_file() cmd");
        fuse_reply_err(req, EREMOTEIO);
    }

    free(handle);
}

/**
 *****************************************************************************/

static void xfuse_cb_read(fuse_req_t req, fuse_ino_t ino, size_t size,
                          off_t off, struct fuse_file_info *fi)
{
    XFUSE_HANDLE          *fh;
    XFUSE_INFO            *fusep;
    XRDP_INODE            *xinode;
    struct req_list_item  *rli;
    long                   handle;

    log_debug("want_bytes %zd bytes at off %lld", size, (long long) off);

    if (fi->fh == 0)
    {
        fuse_reply_err(req, EINVAL);
        return;
    }

    handle = fi->fh;
    fh = (XFUSE_HANDLE *) handle;

    if (fh->is_loc_resource)
    {
        /* target file is in .clipboard dir */

        log_debug("target file is in .clipboard dir");

        if ((xinode = g_xrdp_fs.inode_table[ino]) == NULL)
        {
            log_error("ino does not exist in xrdp_fs");
            fuse_reply_buf(req, 0, 0);
            return;
        }

        rli = g_new0(struct req_list_item, 1);

        rli->stream_id = 0;
        rli->req = req;
        rli->lindex = xinode->lindex;
        rli->off = off;
        rli->size = size;
        list_add_item(g_req_list, (tbus) rli);

        if (g_req_list->count == 1)
        {
            log_debug("requesting clipboard file data lindex = %d off = %lld size = %zd",
                      rli->lindex, (long long) off, size);

            clipboard_request_file_data(rli->stream_id, rli->lindex,
                                        (int) off, (int) size);
        }

        return;
    }

    /* target file is on a remote device */

    fusep = g_new0(XFUSE_INFO, 1);
    if (fusep == NULL)
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

    devredir_file_read(fusep, fh->DeviceId, fh->FileId, size, off);
}

/**
 *****************************************************************************/

static void xfuse_cb_write(fuse_req_t req, fuse_ino_t ino, const char *buf,
                           size_t size, off_t off, struct fuse_file_info *fi)
{
    XFUSE_HANDLE *fh;
    XFUSE_INFO   *fusep;
    long          handle;

    log_debug("write %zd bytes at off %lld to inode=%ld",
              size, (long long) off, ino);

    if (fi->fh == 0)
    {
        log_error("file handle fi->fh is NULL");
        fuse_reply_err(req, EINVAL);
        return;
    }

    handle = fi->fh;
    fh = (XFUSE_HANDLE *) handle;

    if (fh->is_loc_resource)
    {
        /* target file is in .clipboard dir */
        log_debug("THIS IS STILL A TODO!");
        return;
    }

    /* target file is on a remote device */

    fusep = g_new0(XFUSE_INFO, 1);
    if (fusep == NULL)
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

    log_debug("+++ created XFUSE_INFO=%p XFUSE_INFO->fi=%p XFUSE_INFO->fi->fh=0x%llx",
              fusep, fusep->fi, (long long) fusep->fi->fh);

    dev_redir_file_write(fusep, fh->DeviceId, fh->FileId, buf, size, off);
    log_debug("exiting");
}

/**
 *****************************************************************************/

static void xfuse_cb_create(fuse_req_t req, fuse_ino_t parent,
                            const char *name, mode_t mode,
                            struct fuse_file_info *fi)
{
    log_debug("entered: parent_inode=%ld, name=%s fi=%p",
              parent, name, fi);

    xfuse_create_dir_or_file(req, parent, name, mode, fi, S_IFREG);
}

/**
 *****************************************************************************/

#if 0
static void xfuse_cb_fsync(fuse_req_t req, fuse_ino_t ino, int datasync,
                           struct fuse_file_info *fi)
{
    log_debug("#################### entered: ino=%ld datasync=%d", ino, datasync);
    log_debug("function not required");
    fuse_reply_err(req, EINVAL);
}
#endif

/**
 *****************************************************************************/

static void xfuse_cb_setattr(fuse_req_t req, fuse_ino_t ino, struct stat *attr,
                             int to_set, struct fuse_file_info *fi)
{
    XRDP_INODE   *xinode;
    struct stat  st;

    log_debug("entered to_set=0x%x", to_set);

    if (!xfuse_is_inode_valid(ino))
    {
        log_error("inode %ld is not valid", ino);
        fuse_reply_err(req, EBADF);
        return;
    }

    if ((xinode = g_xrdp_fs.inode_table[ino]) == NULL)
    {
        log_debug("g_xrdp_fs.inode_table[%ld] is NULL", ino);
        fuse_reply_err(req, EBADF);
        return;
    }

    if (to_set & FUSE_SET_ATTR_MODE)
    {
        xinode->mode = attr->st_mode;
        log_debug("FUSE_SET_ATTR_MODE");

    }

    if (to_set & FUSE_SET_ATTR_UID)
    {
        xinode->uid = attr->st_uid;
        log_debug("FUSE_SET_ATTR_UID");
    }

    if (to_set & FUSE_SET_ATTR_GID)
    {
        xinode->gid = attr->st_gid;
        log_debug("FUSE_SET_ATTR_GID");
    }

    if (to_set & FUSE_SET_ATTR_SIZE)
    {
        log_debug("previous file size: %lld", (long long) attr->st_size);
        xinode->size = attr->st_size;
        log_debug("returning file size: %zd", xinode->size);
    }

    if (to_set & FUSE_SET_ATTR_ATIME)
    {
        xinode->atime = attr->st_atime;
        log_debug("FUSE_SET_ATTR_ATIME");
    }

    if (to_set & FUSE_SET_ATTR_MTIME)
    {
        xinode->mtime = attr->st_mtime;
        log_debug("FUSE_SET_ATTR_MTIME");
    }

    if (to_set & FUSE_SET_ATTR_ATIME_NOW)
    {
        xinode->atime = attr->st_atime;
        log_debug("FUSE_SET_ATTR_ATIME_NOW");
    }

    if (to_set & FUSE_SET_ATTR_MTIME_NOW)
    {
        xinode->mtime = attr->st_mtime;
        log_debug("FUSE_SET_ATTR_MTIME_NOW");
    }

    memset(&st, 0, sizeof(st));
    st.st_ino   = xinode->inode;
    st.st_mode  = xinode->mode;
    st.st_size  = xinode->size;
    st.st_uid   = xinode->uid;
    st.st_gid   = xinode->gid;
    st.st_atime = xinode->atime;
    st.st_mtime = xinode->mtime;
    st.st_ctime = xinode->ctime;

    fuse_reply_attr(req, &st, 1.0); /* LK_TODO just faking for now */
}

/**
 * Get dir listing
 *****************************************************************************/
static void xfuse_cb_opendir(fuse_req_t req, fuse_ino_t ino,
                             struct fuse_file_info *fi)
{
    struct opendir_req *odreq;

    /* save request */
    odreq = g_new(struct opendir_req, 1);
    odreq->req = req;
    odreq->ino = ino;
    odreq->fi  = fi;

    if (fifo_is_empty(&g_fifo_opendir))
    {
        fifo_insert(&g_fifo_opendir, odreq);
        xfuse_proc_opendir_req(req, ino, fi);
    }
    else
    {
        /* place req in FIFO; xfuse_devredir_cb_enum_dir_done() will handle it */
        fifo_insert(&g_fifo_opendir, odreq);
    }
}

/**
 * Process the next opendir req
 *
 * @return 0 of the request was sent for remote lookup, -1 otherwise
 *****************************************************************************/
static int xfuse_proc_opendir_req(fuse_req_t req, fuse_ino_t ino,
                                  struct fuse_file_info *fi)
{
    struct dir_info *di;
    XRDP_INODE      *xinode;
    XFUSE_INFO      *fip;
    tui32            device_id;
    char             full_path[4096];
    char            *cptr;

    log_debug("inode=%ld", ino);

    if (!xfuse_is_inode_valid(ino))
    {
        log_error("inode %ld is not valid", ino);
        fuse_reply_err(req, EBADF);
        g_free(fifo_remove(&g_fifo_opendir));
        return -1;
    }

    if (ino == 1)
        goto done;  /* special case; enumerate top level dir */

    if ((xinode = g_xrdp_fs.inode_table[ino]) == NULL)
    {
        log_debug("g_xrdp_fs.inode_table[%ld] is NULL", ino);
        fuse_reply_err(req, EBADF);
        g_free(fifo_remove(&g_fifo_opendir));
        return -1;
    }

    if (xinode->is_loc_resource)
        goto done;

    /* enumerate resources on a remote device */

#ifdef USE_SYNC_FLAG
    if (xinode->is_synced)
    {
        xfuse_enum_dir(req, ino, size, off, fi);
        g_free(fifo_remove(&g_fifo_opendir));
        return -1;
    }
    else
    {
        goto do_remote_lookup;
    }

do_remote_lookup:
#endif

    xfuse_mark_as_stale(ino);

    log_debug("did not find entry; redirecting call to dev_redir");
    device_id = xfuse_get_device_id_for_inode(ino, full_path);

    log_debug("dev_id=%d ino=%ld full_path=%s", device_id, ino, full_path);

    fip = g_new0(XFUSE_INFO, 1);
    if (fip == NULL)
    {
        log_error("system out of memory");
        fuse_reply_err(req, ENOMEM);
        g_free(fifo_remove(&g_fifo_opendir));
        return -1;
    }

    fip->req = req;
    fip->inode = ino;
    fip->size = 0;
    fip->off = 0;
    fip->fi = fi;
    fip->dirbuf1.first_time = 1;
    fip->dirbuf1.bytes_in_buf = 0;

    fip->invoke_fuse = 1;
    fip->device_id = device_id;

    /* we want path minus 'root node of the share' */
    if ((cptr = strchr(full_path, '/')) == NULL)
    {
        /* enumerate root dir */
        if (dev_redir_get_dir_listing((void *) fip, device_id, "\\"))
        {
            log_error("failed to send dev_redir_get_dir_listing() cmd");
            fuse_reply_buf(req, NULL, 0);
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

    return 0;

done:

    di = g_new0(struct dir_info, 1);
    di->index = FIRST_INODE;
    fi->fh = (tintptr) di;
    fuse_reply_open(req, fi);
    g_free(fifo_remove(&g_fifo_opendir));
    return -1;
}

/**
 *****************************************************************************/

static void xfuse_cb_releasedir(fuse_req_t req, fuse_ino_t ino,
                                struct fuse_file_info *fi)
{
    struct dir_info *di;

    di = (struct dir_info *) (tintptr) fi->fh;
    if (di)
        free(di);

    fuse_reply_err(req, 0);
}

/******************************************************************************
 *                           miscellaneous functions
 *****************************************************************************/

/**
 * Mark all files with matching parent inode as stale
 *
 * @param  pinode  the parent inode
 *****************************************************************************/

static void
xfuse_mark_as_stale(fuse_ino_t pinode)
{
    fuse_ino_t i;
    XRDP_INODE  *xinode;

    if ((pinode < FIRST_INODE) || (pinode >=  g_xrdp_fs.num_entries))
        return;

    for (i = FIRST_INODE; i < g_xrdp_fs.num_entries; i++)
    {
        if ((xinode = g_xrdp_fs.inode_table[i]) == NULL)
            continue;

        /* match parent inode */
        if (xinode->parent_inode != pinode)
            continue;

        /* got a match */
        xinode->stale = 1;
    }
}

/**
 * Delete all files with matching parent inode that are marked as stale
 *
 * @param  pinode  the parent inode
 *****************************************************************************/

static void
xfuse_delete_stale_entries(fuse_ino_t pinode)
{
    fuse_ino_t i;
    XRDP_INODE  *xinode;

    if ((pinode < FIRST_INODE) || (pinode >=  g_xrdp_fs.num_entries))
        return;

    for (i = FIRST_INODE; i < g_xrdp_fs.num_entries; i++)
    {
        if ((xinode = g_xrdp_fs.inode_table[i]) == NULL)
            continue;

        /* match parent inode */
        if (xinode->parent_inode != pinode)
            continue;

        /* got a match, but is it stale? */
        if (!xinode->stale)
            continue;

        /* ok to delete */
        if (xinode->mode & S_IFREG)
            xfuse_delete_file_with_xinode(xinode);
        else
            xfuse_recursive_delete_dir_with_xinode(xinode);
    }
}

#endif /* end else #ifndef XRDP_FUSE */
