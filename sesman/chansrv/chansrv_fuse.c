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

/* FUSE mount point */
char g_fuse_root_path[256] = "";
#ifdef XRDP_FUSE
static const char *g_fuse_root_path_basename; /* See xfuse_path_in_xfuse_fs() */
static int g_fuse_root_parent_dev;   /* Ditto */
static int g_fuse_root_parent_ino;   /* Ditto */
#endif
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

#include "arch.h"
#include "chansrv_fuse.h"
#include "chansrv_xfs.h"

/* dummy calls when XRDP_FUSE is not defined */
int xfuse_init(void)
{
    return 0;
}
int xfuse_deinit(void)
{
    return 0;
}
int xfuse_check_wait_objs(void)
{
    return 0;
}
int xfuse_get_wait_objs(tbus *objs, int *count, int *timeout)
{
    return 0;
}
int xfuse_create_share(tui32 device_id, const char *dirname)
{
    return 0;
}
void xfuse_delete_share(tui32 share_id) {}
int xfuse_clear_clip_dir(void)
{
    return 0;
}
int xfuse_file_contents_range(int stream_id, const char *data, int data_bytes)
{
    return 0;
}
int xfuse_file_contents_size(int stream_id, int file_size)
{
    return 0;
}
int xfuse_add_clip_dir_item(const char *filename,
                            int flags, int size, int lindex)
{
    return 0;
}

void xfuse_devredir_cb_enum_dir_add_entry(
    struct state_dirscan *fip,
    const char *name,
    const struct file_attr *fattr)
{}
void xfuse_devredir_cb_enum_dir_done(struct state_dirscan *fip,
                                     enum NTSTATUS IoStatus)
{}
void xfuse_devredir_cb_lookup_entry(struct state_lookup *fip,
                                    enum NTSTATUS IoStatus,
                                    const struct file_attr *file_info)
{}
void xfuse_devredir_cb_setattr(struct state_setattr *fip,
                               enum NTSTATUS IoStatus)
{}
void xfuse_devredir_cb_create_file(struct state_create *fip,
                                   enum NTSTATUS IoStatus,
                                   tui32 DeviceId, tui32 FileId)
{}
void xfuse_devredir_cb_open_file(struct state_open *fip,
                                 enum NTSTATUS IoStatus,
                                 tui32 DeviceId, tui32 FileId)
{}
void xfuse_devredir_cb_read_file(struct state_read *fip,
                                 enum NTSTATUS IoStatus,
                                 const char *buf, size_t length)
{}
void xfuse_devredir_cb_write_file(
    struct state_write *fip,
    enum NTSTATUS IoStatus,
    off_t offset,
    size_t length)
{}
void xfuse_devredir_cb_rmdir_or_file(struct state_remove *fip,
                                     enum NTSTATUS IoStatus)
{}
void xfuse_devredir_cb_rename_file(struct state_rename *fip,
                                   enum NTSTATUS IoStatus)
{}
void xfuse_devredir_cb_file_close(struct state_close *fip)
{}

int xfuse_path_in_xfuse_fs(const char *path)
{
    return 0;
}

#else

/******************************************************************************
**                                                                           **
**                     when FUSE is enabled in xrdp                          **
**                                                                           **
******************************************************************************/

/* FUSE_USE_VERSION must be defined globally for other parts of
 * xrdp-chansrv which include <fuse_lowlevel.h> for definitions. Check
 * it's actually defined here */
#ifndef FUSE_USE_VERSION
#error Define FUSE_USE_VERSION in the make system and recompile
#endif

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
#include "string_calls.h"
#include "clipboard_file.h"
#include "chansrv_fuse.h"
#include "chansrv_xfs.h"
#include "chansrv.h"
#include "chansrv_config.h"
#include "devredir.h"
#include "list.h"
#include "file.h"

/* Check for FUSE features we may wish to use
 *
 * Note that FUSE_VERSION might be more useful for some features than
 * FUSE_USE_VERSION
 */
#if FUSE_VERSION >= FUSE_MAKE_VERSION(3,7)
#define FUSE_SET_LOG_FUNC_AVAILABLE
#endif

#ifndef EREMOTEIO
#define EREMOTEIO EIO
#endif

#define XFUSE_ATTR_TIMEOUT      5.0
#define XFUSE_ENTRY_TIMEOUT     5.0


/* Type of buffer used for fuse_add_direntry() calls */
struct dirbuf1
{
    char buf[4096];
    size_t  len;
};

/*
 * Record type used to maintain state when running a directory scan
 */
struct state_dirscan
{
    fuse_req_t             req;   /* Original FUSE request from opendir */
    struct fuse_file_info  fi;    /* File info struct passed to opendir */
    fuse_ino_t             pinum; /* inum of parent directory           */
};


/*
 * Record type used to maintain state when running an entry lookup
 */
struct state_lookup
{
    fuse_req_t        req;        /* Original FUSE request from lookup  */
    fuse_ino_t        pinum;      /* inum of parent directory           */
    char              name[XFS_MAXFILENAMELEN + 1];
    /* Name to look up                    */
    fuse_ino_t        existing_inum;
    /* inum of an existing entry          */
    tui32             existing_generation;
    /* generation of the above            */
};

/*
 * Record type used to maintain state when running an entry setattr
 */
struct state_setattr
{
    fuse_req_t        req;        /* Original FUSE request from lookup  */
    fuse_ino_t        inum;       /* inum of entry                      */
    struct file_attr  fattr;      /* File attributes to set             */
    tui32             change_mask; /* Attributes to set in fattr        */
};


/*
 * Record type used to maintain state when running an open
 */
struct state_open
{
    fuse_req_t        req;        /* Original FUSE request from lookup  */
    struct fuse_file_info fi;     /* File info struct passed to open    */
    fuse_ino_t        inum;       /* inum of file to open               */
};


/*
 * Record type used to maintain state when running a create
 */
struct state_create
{
    fuse_req_t        req;        /* Original FUSE request from lookup  */
    struct fuse_file_info fi;     /* File info struct passed to open    */
    fuse_ino_t        pinum;      /* inum of parent directory           */
    char              name[XFS_MAXFILENAMELEN + 1];
    /* Name of file in parent directory   */
    mode_t            mode;       /* Mode of file to create             */
};

/*
 * Record type used to maintain state when running a read
 */
struct state_read
{
    fuse_req_t        req;        /* Original FUSE request from lookup  */
};

/*
 * Record type used to maintain state when running a write
 */
struct state_write
{
    fuse_req_t        req;        /* Original FUSE request from lookup  */
    fuse_ino_t        inum;       /* inum of file we're writing         */
};

/*
 * Record type used to maintain state when running a remove
 */
struct state_remove
{
    fuse_req_t        req;        /* Original FUSE request from lookup  */
    fuse_ino_t        inum;       /* inum of file we're removing        */
};

/*
 * Record type used to maintain state when running a rename
 */
struct state_rename
{
    fuse_req_t        req;        /* Original FUSE request from lookup  */
    fuse_ino_t        pinum;      /* inum of parent of file             */
    fuse_ino_t        new_pinum;  /* inum of new parent of file         */
    char              name[XFS_MAXFILENAMELEN + 1];
    /* New name of file in new parent dir */
};

/*
 * Record type used to maintain state when running a close
 */
struct state_close
{
    fuse_req_t        req;        /* Original FUSE request from lookup  */
    struct fuse_file_info fi;     /* File info struct passed to open    */
    fuse_ino_t        inum;       /* inum of file to open               */
};

struct xfuse_handle
{
    tui32 DeviceId;
    tui32 FileId;
    int   is_loc_resource; /* this is not a redirected resource */

    /* a directory handle, if this xfuse_handle represents a directory.
     * NULL, if this xfuse_handle represents a file.
     *
     * Note: when this xfuse_handle represents a directory, then the other
     *       fields of this structure contain invalid values.
     */
    struct xfs_dir_handle *dir_handle;
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

extern struct config_chansrv *g_cfg; /* in chansrv.c */

static struct list *g_req_list = 0;
static struct xfs_fs *g_xfs;                 /* an inst of xrdp file system */
static ino_t g_clipboard_inum;               /* inode of clipboard dir      */
static struct fuse_lowlevel_ops g_xfuse_ops; /* setup FUSE callbacks        */
static int g_xfuse_inited = 0;               /* true when FUSE is inited    */
static struct fuse_session *g_se = 0;
// For the below, see the source for the fuse_session_loop() function
static struct fuse_buf g_buffer =
{
    .mem = NULL
};

/* forward declarations for internal access */
static int xfuse_init_xrdp_fs(void);
static int xfuse_deinit_xrdp_fs(void);
static int xfuse_init_lib(struct fuse_args *args);

/* forward declarations for FUSE callbacks */
static void xfuse_cb_lookup(fuse_req_t req, fuse_ino_t parent,
                            const char *name);

static void xfuse_cb_getattr(fuse_req_t req, fuse_ino_t ino,
                             struct fuse_file_info *fi);

/* this is not a callback, but it's used by xfuse_cb_readdir() */
static int xfuse_dirbuf_add1(fuse_req_t req, struct dirbuf1 *b,
                             XFS_INODE *xinode, off_t offset);

static void xfuse_cb_readdir(fuse_req_t req, fuse_ino_t ino, size_t size,
                             off_t off, struct fuse_file_info *fi);

static void xfuse_cb_mkdir(fuse_req_t req, fuse_ino_t parent,
                           const char *name, mode_t mode);

static void xfuse_cb_unlink(fuse_req_t req, fuse_ino_t parent,
                            const char *name);

static void xfuse_cb_rename(fuse_req_t req,
                            fuse_ino_t old_parent, const char *old_name,
                            fuse_ino_t new_parent, const char *new_name,
                            unsigned int flags);

/* Whether to create a dir of file depends on whether S_IFDIR is set in the
   mode field */
static void xfuse_create_dir_or_file(fuse_req_t req, fuse_ino_t parent,
                                     const char *name, mode_t mode,
                                     struct fuse_file_info *fi);

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

static void xfuse_cb_releasedir(fuse_req_t req, fuse_ino_t ino,
                                struct fuse_file_info *fi);

/* miscellaneous functions */
static void xfs_inode_to_fuse_entry_param(const XFS_INODE *xinode,
        struct fuse_entry_param *e);
static void make_fuse_entry_reply(fuse_req_t req, const XFS_INODE *xinode);
static void make_fuse_attr_reply(fuse_req_t req, const XFS_INODE *xinode);
static const char *filename_on_device(const char *full_path);
static void update_inode_file_attributes(const struct file_attr *fattr,
        tui32 change_mask, XFS_INODE *xinode);
static char *get_name_for_entry_in_parent(fuse_ino_t parent, const char *name);
static unsigned int format_user_info(char *dest, unsigned int len,
                                     const char *format);

/*****************************************************************************/
int
load_fuse_config(void)
{
    return 0;
}

/*****************************************************************************/
XFUSE_HANDLE *
xfuse_handle_create()
{
    return g_new0(XFUSE_HANDLE, 1);
}

/*****************************************************************************/
void
xfuse_handle_delete(XFUSE_HANDLE *self)
{
    if (self == NULL)
    {
        return;
    }

    if (self->dir_handle != NULL)
    {
        free(self->dir_handle);
    }
    free(self);
}

/*****************************************************************************/
uint64_t
xfuse_handle_to_fuse_handle(XFUSE_HANDLE *self)
{
    return (uint64_t) (tintptr) self;
}

/*****************************************************************************/
XFUSE_HANDLE *
xfuse_handle_from_fuse_handle(uint64_t handle)
{
    return (XFUSE_HANDLE *) (tintptr) handle;
}

/*****************************************************************************
**                                                                          **
**         public functions - can be called from any code path              **
**                                                                          **
*****************************************************************************/

/**
 * Initialize FUSE subsystem
 *
 * @return 0 on success, -1 on failure, 1 for feature disabled
 *****************************************************************************/

int
xfuse_init(void)
{
    struct fuse_args args = FUSE_ARGS_INIT(0, NULL);
    char *p;

    /* if already inited, just return */
    if (g_xfuse_inited)
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "already inited");
        return 0;
    }

    /* This feature may be disabled */
    if (!g_cfg->enable_fuse_mount)
    {
        /*
         * Only log the 'disabled mounts' message one time
         */
        static int disabled_mounts_msg_shown = 0;
        if (!disabled_mounts_msg_shown)
        {
            LOG(LOG_LEVEL_INFO, "FUSE mounts are disabled by config");
            disabled_mounts_msg_shown = 1;
        }
        return 1;
    }

    if (g_se != 0)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "g_se is not zero");
        return -1;
    }

    load_fuse_config();

    /* define FUSE mount point */
    if (g_cfg->fuse_mount_name[0] == '/')
    {
        /* String is an absolute path to the mount point containing
         * %u or %U characters */
        format_user_info(g_fuse_root_path, sizeof(g_fuse_root_path),
                         g_cfg->fuse_mount_name);
    }
    else
    {
        /* mount_name is relative to $HOME, e.g. ~/xrdp_client,
         * or ~/thinclient_drives */
        unsigned int len = g_snprintf(g_fuse_root_path, sizeof(g_fuse_root_path), "%s/", g_getenv("HOME"));
        if (len < sizeof(g_fuse_root_path))
        {
            format_user_info(g_fuse_root_path + len, sizeof(g_fuse_root_path) - len, g_cfg->fuse_mount_name);
        }

    }

    /* Remove all trailing '/' from the root path */
    p = g_fuse_root_path + g_strlen(g_fuse_root_path);
    while ( p > g_fuse_root_path && *(p - 1) == '/')
    {
        --p;
        *p = '\0';
    }

    /* This shouldn't happen */
    if (g_strlen(g_fuse_root_path) == 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "Fuse root path is empty after removing trailing '/'");
        return -1;
    }

    g_snprintf(g_fuse_clipboard_path, sizeof(g_fuse_clipboard_path),
               "%s/.clipboard", g_fuse_root_path);

    /* if FUSE mount point does not exist, create it */
    if (!g_directory_exist(g_fuse_root_path))
    {
        (void)g_create_path(g_fuse_root_path);
        if (!g_create_dir(g_fuse_root_path))
        {
            LOG(LOG_LEVEL_ERROR, "mkdir %s failed (%s)",
                g_fuse_root_path, g_get_strerror());
            return -1;
        }
    }

    /* Get the characteristics of the parent directory of the FUSE mount
     * point. Used by xfuse_path_in_xfuse_fs() */
    g_fuse_root_parent_dev = -1;
    g_fuse_root_parent_ino = -1;
    p = (char *)g_strrchr(g_fuse_root_path, '/');
    if (p != NULL)
    {
        /* Temporarily finish the root path at this point */
        *p = '\0';
        g_fuse_root_path_basename = p + 1;
        g_fuse_root_parent_dev = g_file_get_device_number(g_fuse_root_path);
        g_fuse_root_parent_ino = g_file_get_inode_num(g_fuse_root_path);
        *p = '/';
    }

    if (g_fuse_root_parent_dev == -1 || g_fuse_root_parent_ino == -1)
    {
        LOG(LOG_LEVEL_ERROR,
            "Unable to obtain characteristics of directory containing %s",
            g_fuse_root_path);
        return -1;
    }

    /* setup xrdp file system */
    if (xfuse_init_xrdp_fs())
    {
        return -1;
    }

    /* setup FUSE callbacks */
    g_memset(&g_xfuse_ops, 0, sizeof(g_xfuse_ops));
    g_xfuse_ops.lookup      = xfuse_cb_lookup;
    g_xfuse_ops.readdir     = xfuse_cb_readdir;
    g_xfuse_ops.mkdir       = xfuse_cb_mkdir;
    g_xfuse_ops.rmdir       = xfuse_cb_unlink;
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
    fuse_opt_add_arg(&args, "-o");
    fuse_opt_add_arg(&args, "fsname=xrdp-chansrv");
    //fuse_opt_add_arg(&args, "-s"); /* single threaded mode */
    //fuse_opt_add_arg(&args, "-d"); /* debug mode           */

    if (xfuse_init_lib(&args))
    {
        fuse_opt_free_args(&args);
        xfuse_deinit();
        return -1;
    }
    fuse_opt_free_args(&args);

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
    if (g_se != NULL)
    {
        fuse_session_unmount(g_se);
        fuse_session_destroy(g_se);
        g_se = NULL;
    }

    free(g_buffer.mem);
    g_buffer.mem = NULL;

    list_delete(g_req_list);
    g_req_list = 0;

    xfuse_deinit_xrdp_fs();

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
    if (g_se != NULL)
    {
        if (g_sck_can_recv(fuse_session_fd(g_se), 0))
        {
            int rval = fuse_session_receive_buf(g_se, &g_buffer);
            if (rval == -EINTR)
            {
                return -1;
            }

            if (rval == -ENODEV)
            {
                return -1;
            }

            if (rval <= 0)
            {
                return -1;
            }

            fuse_session_process_buf(g_se, &g_buffer);
        }
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
    if (g_se != NULL)
    {
        objs[*count] = fuse_session_fd(g_se);
        ++(*count);
    }

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
    int result = -1;
    XFS_INODE  *xinode;

    if (dirname != NULL && strlen(dirname) > 0 &&
            xfuse_init_xrdp_fs() == 0)
    {
        xinode = xfs_add_entry(g_xfs, FUSE_ROOT_ID, dirname, (0777 | S_IFDIR));
        if (xinode == NULL)
        {
            LOG_DEVEL(LOG_LEVEL_DEBUG, "xfs_add_entry() failed");
        }
        else
        {
            xinode->is_redirected = 1;
            xinode->device_id = device_id;
            result = 0;
        }
    }

    return result;
}

/**
 * @brief Remove specified share directory.
 *
 * This code gets called from devredir
 *****************************************************************************/

void xfuse_delete_share(tui32 device_id)
{
    xfs_delete_redirected_entries_with_device_id(g_xfs, device_id);
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
    int result = 0;

    if (g_xfs != NULL && g_clipboard_inum > 0)
    {
        xfs_remove_directory_contents(g_xfs, g_clipboard_inum);
    }

    return result;
}

/**
 * Return clipboard data to fuse
 *
 * @return 0 on success, -1 on failure
 *****************************************************************************/

int
xfuse_file_contents_range(int stream_id, const char *data, int data_bytes)
{
    LOG_DEVEL(LOG_LEVEL_DEBUG, "entered: stream_id=%d data_bytes=%d", stream_id, data_bytes);

    struct req_list_item *rli;

    if ((rli = (struct req_list_item *) list_get_item(g_req_list, 0)) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "range error!");
        return -1;
    }

    LOG_DEVEL(LOG_LEVEL_DEBUG, "lindex=%d off=%d size=%d", rli->lindex, rli->off, rli->size);

    fuse_reply_buf(rli->req, data, data_bytes);

    list_remove_item(g_req_list, 0);
    if (g_req_list->count <= 0)
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "completed all requests");
        return 0;
    }

    /* send next request */
    rli = (struct req_list_item *) list_get_item(g_req_list, 0);
    if (rli == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "range error!");
        return -1;
    }

    LOG_DEVEL(LOG_LEVEL_DEBUG, "requesting clipboard file data");

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
    LOG_DEVEL(LOG_LEVEL_DEBUG,
              "entered: filename=%s flags=%d size=%d lindex=%d",
              filename, flags, size, lindex);

    int result = -1;

    if (g_xfs == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR,
                  "xfuse_add_clip_dir_item() called with no filesystem");
    }
    else
    {
        /* add entry to xrdp_fs */
        XFS_INODE *xinode = xfs_add_entry( g_xfs,
                                           g_clipboard_inum, /* parent inode */
                                           filename,
                                           (0666 | S_IFREG));
        if (xinode == NULL)
        {
            LOG(LOG_LEVEL_INFO,
                "failed to create file %s in xrdp filesystem", filename);
        }
        else
        {
            xinode->size = size;
            xinode->lindex = lindex;
            result = 0;
        }
    }

    return result;
}

/**
 *
 *
 * @return 0 on success, -1 on failure
 *****************************************************************************/

int xfuse_file_contents_size(int stream_id, int file_size)
{
    LOG_DEVEL(LOG_LEVEL_DEBUG, "entered: stream_id=%d file_size=%d", stream_id, file_size);
    return 0;
}

/*****************************************************************************
**                                                                          **
**       private functions - can only be called from within this file       **
**                                                                          **
*****************************************************************************/

/*****************************************************************************/
/**
 * FUSE logging function
 *
 * Used to get errors from FUSE for the log file
 * @param fuse_log_level Logging level understood by FUSE
 * @param fmt Logging format string
 * @param ap Arguments for above
 */
#ifdef FUSE_SET_LOG_FUNC_AVAILABLE
static void
xfuse_log_func(enum fuse_log_level fuse_log_level, const char *fmt, va_list ap)
{
    char msg[512];
    enum logLevels level;
    switch (fuse_log_level)
    {
        case FUSE_LOG_ERR:
            level = LOG_LEVEL_ERROR;
            break;

        case FUSE_LOG_WARNING:
            level = LOG_LEVEL_WARNING;
            break;

        case FUSE_LOG_NOTICE:
        case FUSE_LOG_INFO:
            level = LOG_LEVEL_INFO;
            break;

        case FUSE_LOG_DEBUG:
            level = LOG_LEVEL_DEBUG;
            break;

        default:
            level = LOG_LEVEL_ALWAYS;
            break;
    }

    vsnprintf(msg, sizeof(msg), fmt, ap);
    LOG(level, "%s", msg);
}
#endif //FUSE_SET_LOG_FUNC_AVAILABLE

/**
 * Initialize FUSE library
 *
 * @return 0 on success, -1 on failure
 *****************************************************************************/
static int xfuse_init_lib(struct fuse_args *args)
{
    int rv = -1;

#ifdef FUSE_SET_LOG_FUNC_AVAILABLE
    fuse_set_log_func(xfuse_log_func);
#endif

    g_se = fuse_session_new(args, &g_xfuse_ops, sizeof(g_xfuse_ops), 0);
    if (g_se == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "fuse_session_new() failed");
    }
    else if (fuse_session_mount(g_se, g_fuse_root_path) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "FUSE mount on %s failed."
            " If %s is already mounted, you must first unmount it",
            g_fuse_root_path, g_fuse_root_path);
        fuse_session_destroy(g_se);
        g_se = NULL;
    }
    else
    {
        g_req_list = list_create();
        g_req_list->auto_free = 1;
        rv = 0;
    }

    return rv;
}

/**
 * Initialize xrdp file system
 *
 * @return 0 on success, -1 on failure
 *
 *****************************************************************************/

static int xfuse_init_xrdp_fs(void)
{
    XFS_INODE *xino;
    int result = -1;

    /* Already called? */
    if (g_xfs != NULL)
    {
        result = 0;
    }
    else if ((g_xfs = xfs_create_xfs_fs(0, g_getuid(), g_getgid())) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "system out of memory");
    }
    else
    {
        /* Need a top-level .clipboard directory */
        xino = xfs_add_entry(g_xfs, FUSE_ROOT_ID, ".clipboard",
                             (0777 | S_IFDIR));
        if (xino == NULL)
        {
            LOG_DEVEL(LOG_LEVEL_ERROR, "system out of memory");
            xfs_delete_xfs_fs(g_xfs);
            g_xfs = NULL;
        }
        else
        {
            g_clipboard_inum = xino->inum;
            result = 0;
        }
    }
    return result;
}

/**
 * zap the xrdp file system
 *
 * @return 0 on success, -1 on failure
 *****************************************************************************/

static int xfuse_deinit_xrdp_fs(void)
{
    xfs_delete_xfs_fs(g_xfs);
    g_xfs = NULL;
    g_clipboard_inum = 0;

    return 0;
}

/******************************************************************************
**                                                                           **
**                         callbacks for devredir                            **
**                                                                           **
******************************************************************************/


/**
 * Add a file or directory to xrdp file system as part of a
 * directory request
 *
 * If the file or directory already exists, no changes are made to it.
 *****************************************************************************/

void xfuse_devredir_cb_enum_dir_add_entry(
    struct state_dirscan *fip,
    const char *name,
    const struct file_attr *fattr)
{
    XFS_INODE *xinode = NULL;

    if (!xfs_get(g_xfs, fip->pinum))
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "inode %ld is not valid", fip->pinum);
    }
    else if ((strcmp(name, ".") == 0) ||
             (strcmp(name, "..") == 0))
    {
        ; /* filename is . or ..  - don't add it */
    }
    else
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "parent_inode=%ld name=%s", fip->pinum, name);

        /* Does the file already exist ? If it does it's important we
         * don't mess with it, as we're only enumerating the directory, and
         * we don't want to disrupt any existing operations on the file
         */
        xinode = xfs_lookup_in_dir(g_xfs, fip->pinum, name);
        if (xinode == NULL)
        {
            /* Add a new node to the file system */
            LOG_DEVEL(LOG_LEVEL_DEBUG, "Creating name=%s in parent=%ld in xrdp_fs",
                      name, fip->pinum);
            xinode = xfs_add_entry(g_xfs, fip->pinum, name, fattr->mode);
            if (xinode == NULL)
            {
                LOG_DEVEL(LOG_LEVEL_ERROR, "xfs_add_entry() failed");
            }
            else
            {
                xinode->size = fattr->size;
                xinode->atime = fattr->atime;
                xinode->mtime = fattr->mtime;
                /* Initially, set the attribute change time to the file data
                   change time */
                xinode->ctime = fattr->mtime;

                /* device_id is inherited from parent */
            }
        }
    }
}

/**
 * This routine is called by devredir when the opendir request has
 * completed
 *****************************************************************************/

void xfuse_devredir_cb_enum_dir_done(struct state_dirscan *fip,
                                     enum NTSTATUS IoStatus)
{
    LOG_DEVEL(LOG_LEVEL_DEBUG, "fip=%p IoStatus=0x%x", fip, IoStatus);

    /*
     * STATUS_NO_SUCH_FILE is returned for empty directories
     */
    if (IoStatus != STATUS_SUCCESS && IoStatus != STATUS_NO_SUCH_FILE)
    {
        int status;
        switch (IoStatus)
        {
            case STATUS_ACCESS_DENIED:
                status = EACCES;
                break;
            default:
                status = ENOENT;
        }
        fuse_reply_err(fip->req, status);
    }
    else if (!xfs_get(g_xfs, fip->pinum))
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "inode %ld is not valid", fip->pinum);
        fuse_reply_err(fip->req, ENOENT);
    }
    else
    {
        struct fuse_file_info *fi = &fip->fi;
        XFUSE_HANDLE *xhandle = xfuse_handle_create();

        if (xhandle == NULL
                || (xhandle->dir_handle = xfs_opendir(g_xfs, fip->pinum)) == NULL)
        {
            xfuse_handle_delete(xhandle);
            fuse_reply_err(fip->req, ENOMEM);
        }
        else
        {
            fi->fh = xfuse_handle_to_fuse_handle(xhandle);
            fuse_reply_open(fip->req, &fip->fi);
        }
    }

    free(fip);
}

/**
 * This routine is caused as a result of a devredir remote lookup
 * instigated by xfuse_cb_lookup()
 *****************************************************************************/

void xfuse_devredir_cb_lookup_entry(struct state_lookup *fip,
                                    enum NTSTATUS IoStatus,
                                    const struct file_attr *file_info)
{
    XFS_INODE *xinode = NULL;

    if (IoStatus != STATUS_SUCCESS)
    {
        switch (IoStatus)
        {
            case STATUS_SHARING_VIOLATION:
            /* This can happen when trying to read the attributes of
             * some system files (e.g. pagefile.sys) */
            case STATUS_ACCESS_DENIED:
                fuse_reply_err(fip->req, EACCES);
                break;

            case STATUS_UNSUCCESSFUL:
                /* Happens if we try to lookup an illegal filename (e.g.
                 * one with a '*' in it) */
                fuse_reply_err(fip->req, ENOENT);
                break;

            case STATUS_NO_SUCH_FILE:
                /* Remove our copy, if any */
                if (fip->existing_inum  &&
                        (xinode = xfs_get(g_xfs, fip->existing_inum)) != NULL &&
                        xinode->generation == fip->existing_generation)
                {
                    xfs_remove_entry(g_xfs, fip->existing_inum);
                }
                fuse_reply_err(fip->req, ENOENT);
                break;

            default:
                LOG_DEVEL(LOG_LEVEL_INFO, "Error code %08x - fallthrough", (int) IoStatus);
                fuse_reply_err(fip->req, EIO);
                break;
        }
    }
    else if (!xfs_get(g_xfs, fip->pinum))
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "parent inode %ld is not valid", fip->pinum);
        fuse_reply_err(fip->req, ENOENT);
    }
    else
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "parent_inode=%ld name=%s", fip->pinum, fip->name);

        /* Does the file already exist ? */
        xinode = xfs_lookup_in_dir(g_xfs, fip->pinum, fip->name);
        if (xinode != NULL)
        {
            /* Is the existing file the same type ? */
            if ((xinode->mode & (S_IFREG | S_IFDIR)) ==
                    (file_info->mode & (S_IFREG | S_IFDIR)))
            {
                LOG_DEVEL(LOG_LEVEL_DEBUG, "inode=%ld name=%s already exists in xrdp_fs as %ld",
                          fip->pinum, fip->name, xinode->inum);
                if (xfs_get_file_open_count(g_xfs, xinode->inum) > 0)
                {
                    /*
                     * Don't mess with open files. The local attributes are
                     * almost certainly more up-to-date. A worst case scenario
                     * would be truncating a file we're currently writing
                     * to, as the lookup value for the size is stale.
                     */
                    LOG_DEVEL(LOG_LEVEL_DEBUG, "inode=%ld is open - "
                              "preferring local attributes", xinode->inum);
                }
                else
                {
                    LOG_DEVEL(LOG_LEVEL_DEBUG, "Updating attributes of inode=%ld", xinode->inum);
                    update_inode_file_attributes(file_info, TO_SET_ALL, xinode);
                }
            }
            else
            {
                /* Type has changed from file to directory, or vice-versa */
                LOG_DEVEL(LOG_LEVEL_DEBUG, "inode=%ld name=%s of different type in xrdp_fs"
                          " - removing",
                          fip->pinum, xinode->name);
                xfs_remove_entry(g_xfs, xinode->inum);
                xinode = NULL;
            }
        }

        if (xinode == NULL)
        {
            /* Add a new node to the file system */
            LOG_DEVEL(LOG_LEVEL_DEBUG, "Creating name=%s in parent=%ld in xrdp_fs",
                      fip->name, fip->pinum);
            xinode = xfs_add_entry(g_xfs, fip->pinum, fip->name,
                                   file_info->mode);
            if (xinode == NULL)
            {
                LOG_DEVEL(LOG_LEVEL_DEBUG, "xfs_add_entry() failed");
            }
            else
            {
                xinode->size = file_info->size;
                xinode->atime = file_info->atime;
                xinode->mtime = file_info->mtime;
                /* Initially, set the attribute change time to the file data
                   change time */
                xinode->ctime = file_info->mtime;
                /* device_id is inherited from parent */
            }
        }
        if (xinode != NULL)
        {
            make_fuse_entry_reply(fip->req, xinode);
        }
        else
        {
            fuse_reply_err(fip->req, EIO);
        }
    }

    free(fip);
}

/**
 * This routine is caused as a result of a devredir remote setattr
 * instigated by xfuse_cb_setattr()
 *****************************************************************************/
void xfuse_devredir_cb_setattr(struct state_setattr *fip,
                               enum NTSTATUS IoStatus)
{
    XFS_INODE *xinode;

    if (IoStatus != STATUS_SUCCESS)
    {
        switch (IoStatus)
        {
            case STATUS_SHARING_VIOLATION:
            /* This can happen when trying to read the attributes of
             * some system files (e.g. pagefile.sys) */
            case STATUS_ACCESS_DENIED:
                fuse_reply_err(fip->req, EACCES);
                break;

            case STATUS_UNSUCCESSFUL:
            /* Happens if we try to lookup an illegal filename */
            case STATUS_NO_SUCH_FILE:
                fuse_reply_err(fip->req, ENOENT);
                break;

            default:
                LOG_DEVEL(LOG_LEVEL_INFO, "Error code %08x - fallthrough", (int) IoStatus);
                fuse_reply_err(fip->req, EIO);
                break;
        }
    }
    else if ((xinode = xfs_get(g_xfs, fip->inum)) == NULL)
    {
        fuse_reply_err(fip->req, ENOENT);
    }
    else
    {
        update_inode_file_attributes(&fip->fattr, fip->change_mask, xinode);
        make_fuse_attr_reply(fip->req, xinode);
    }
    free(fip);
}

/**
 * This routine is caused as a result of a file or directory
 * create request */
void xfuse_devredir_cb_create_file(struct state_create *fip,
                                   enum NTSTATUS IoStatus,
                                   tui32 DeviceId, tui32 FileId)
{
    XFUSE_HANDLE *fh = NULL;

    if (IoStatus != 0)
    {
        switch (IoStatus)
        {
            case STATUS_ACCESS_DENIED:
                fuse_reply_err(fip->req, EACCES);
                break;

            case STATUS_OBJECT_NAME_INVALID:
            case STATUS_OBJECT_NAME_NOT_FOUND:
                fuse_reply_err(fip->req, ENOENT);
                break;

            default:
                fuse_reply_err(fip->req, EIO);
                break;
        }
    }
    else
    {
        if ((fip->mode & S_IFREG) != 0)
        {
            /* We've created a regular file */
            /* Allocate an XFUSE_HANDLE for future file operations */
            if ((fh = xfuse_handle_create()) != NULL)
            {
                /* save file handle for later use */
                fh->DeviceId = DeviceId;
                fh->FileId = FileId;

                fip->fi.fh = xfuse_handle_to_fuse_handle(fh);
            }
        }

        if ((fip->mode & S_IFREG) != 0 && fh == NULL)
        {
            /* We failed to allocate a file handle */
            LOG_DEVEL(LOG_LEVEL_ERROR, "system out of memory");
            fuse_reply_err(fip->req, ENOMEM);
        }
        else
        {
            XFS_INODE   *xinode;
            /* create entry in xrdp file system */
            xinode = xfs_add_entry(g_xfs, fip->pinum, fip->name, fip->mode);
            if (xinode == NULL)
            {
                /* It's possible xfs_add_entry() has failed, as the
                 * file has already been added (by a lookup request)
                 * in the time between our request and the response
                 * This can happen if an 'ls' is happening in the same
                 * directory as this file is being created.
                 *
                 * We'll check for this before we fail the create */
                if ((xinode = xfs_lookup_in_dir(g_xfs,
                                                fip->pinum,
                                                fip->name)) != NULL)
                {
                    /*
                     * The mode should be correct anyway, but we'll
                     * set it to the mode requested at creation
                     */
                    xinode->mode = fip->mode;
                }
            }

            if (xinode == NULL)
            {
                LOG_DEVEL(LOG_LEVEL_ERROR, "Out of memory!");
                fuse_reply_err(fip->req, ENOMEM);
            }
            else
            {

                if ((fip->mode & S_IFDIR) != 0)
                {
                    make_fuse_entry_reply(fip->req, xinode);
                }
                else
                {
                    struct fuse_entry_param  e;
                    xfs_inode_to_fuse_entry_param(xinode, &e);
                    fuse_reply_create(fip->req, &e, &fip->fi);
                    xfs_increment_file_open_count(g_xfs, xinode->inum);
                }
            }
        }

    }

    free(fip);
}


/**
 * This routine is caused as a result of a file open request */
void xfuse_devredir_cb_open_file(struct state_open *fip,
                                 enum NTSTATUS IoStatus,
                                 tui32 DeviceId, tui32 FileId)
{
    XFUSE_HANDLE *fh;

    if (IoStatus != 0)
    {
        switch (IoStatus)
        {
            case STATUS_ACCESS_DENIED:
                fuse_reply_err(fip->req, EACCES);
                break;

            case STATUS_OBJECT_NAME_INVALID:
            case STATUS_OBJECT_NAME_NOT_FOUND:
                fuse_reply_err(fip->req, ENOENT);
                break;

            default:
                fuse_reply_err(fip->req, EIO);
                break;
        }
    }
    else
    {
        /* Allocate an XFUSE_HANDLE for future file operations */
        if ((fh = xfuse_handle_create()) == NULL)
        {
            LOG_DEVEL(LOG_LEVEL_ERROR, "system out of memory");
            fuse_reply_err(fip->req, ENOMEM);
        }
        else
        {
            /* save file handle for later use */
            fh->DeviceId = DeviceId;
            fh->FileId = FileId;

            fip->fi.fh = xfuse_handle_to_fuse_handle(fh);

            LOG_DEVEL(LOG_LEVEL_DEBUG, "sending fuse_reply_open(); "
                      "DeviceId=%d FileId=%d req=%p",
                      fh->DeviceId, fh->FileId, fip->req);

            /* update open count */
            xfs_increment_file_open_count(g_xfs, fip->inum);

            fuse_reply_open(fip->req, &fip->fi);
        }
    }

    free(fip);
}

void xfuse_devredir_cb_read_file(struct state_read *fip,
                                 enum NTSTATUS IoStatus,
                                 const char *buf, size_t length)
{
    if (IoStatus != STATUS_SUCCESS)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "Read NTSTATUS is %d", (int) IoStatus);
        fuse_reply_err(fip->req, EIO);
    }
    else
    {
        fuse_reply_buf(fip->req, buf, length);
    }
    free(fip);
}

void xfuse_devredir_cb_write_file(
    struct state_write *fip,
    enum NTSTATUS IoStatus,
    off_t offset,
    size_t length)
{
    XFS_INODE   *xinode;

    if (IoStatus != STATUS_SUCCESS)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "Write NTSTATUS is %d", (int) IoStatus);
        fuse_reply_err(fip->req, EIO);
    }
    else
    {
        off_t new_size = offset + length;
        fuse_reply_write(fip->req, length);

        /* update file size */
        if ((xinode = xfs_get(g_xfs, fip->inum)) != NULL)
        {
            if (new_size > xinode->size)
            {
                xinode->size = new_size;
            }
        }
        else
        {
            LOG_DEVEL(LOG_LEVEL_ERROR, "inode %ld is invalid", fip->inum);
        }
    }

    free(fip);
}

void xfuse_devredir_cb_rmdir_or_file(struct state_remove *fip,
                                     enum NTSTATUS IoStatus)
{
    XFS_INODE   *xinode = xfs_get(g_xfs, fip->inum);

    switch (IoStatus)
    {
        case STATUS_SUCCESS:
        case STATUS_NO_SUCH_FILE:
            xfs_remove_entry(g_xfs, xinode->inum); /* Remove local copy */
            fuse_reply_err(fip->req, 0);
            break;

        case STATUS_SHARING_VIOLATION:
        case STATUS_ACCESS_DENIED:
            fuse_reply_err(fip->req, EACCES);
            break;

        default:
            LOG_DEVEL(LOG_LEVEL_INFO, "Error code %08x - fallthrough", (int) IoStatus);
            fuse_reply_err(fip->req, EBADF);
            break;
    }
    free(fip);
}

void xfuse_devredir_cb_rename_file(struct state_rename *fip,
                                   enum NTSTATUS IoStatus)
{
    int status;

    if (IoStatus != STATUS_SUCCESS)
    {
        status =
            (IoStatus == STATUS_SHARING_VIOLATION) ? EBUSY  :
            (IoStatus == STATUS_ACCESS_DENIED)     ? EACCES :
            /* default */                        EEXIST ;
    }
    else
    {
        status = xfs_move_entry(g_xfs, fip->pinum,
                                fip->new_pinum, fip->name);
    }

    fuse_reply_err(fip->req, status);
    free(fip);
}

void xfuse_devredir_cb_file_close(struct state_close *fip)
{
    fuse_reply_err(fip->req, 0);
    xfs_decrement_file_open_count(g_xfs, fip->inum);

    free(fip);
}

/*
 * Determine is a file is in the FUSE filesystem
 *
 * The whole purpose of this function is to avoid deadlocks. If we try
 * to determine the size (or any other attribute) of a file in the FUSE
 * filesystem, the kernel will block the calling thread and send a message
 * to libfuse. Since we're single-threaded, this will result in deadlock.
 *
 * Ideally we'd use a stat() of the file to compare with a stat() of
 * the mountpoint. This won't work however, for the reasons outline above.
 *
 * A simple implementation might be to compare the start of the filename
 * with the FUSE mount point. This will work, but can easily be defeated,
 * (perhaps unintentionally) by symlinks in the path.
 *
 * The approach taken here is to record the basename of the FUSE mount
 * point directory (e.g. 'thinclient_drives') and look for that in the
 * passed-in filename as a directory element. If we find it, we look
 * at the device name and inode number of the parent directory of the
 * FUSE mount point. If these match we consider the filename to be in
 * the FUSE filesystem.
 */
int xfuse_path_in_xfuse_fs(const char *path)
{
    char *wpath = NULL; /* Writeable copy of path */
    char *p;
    int blen = g_strlen(g_fuse_root_path_basename);
    int rv = 0;

    if ((wpath = g_strdup(path)) == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "system out of memory");
    }
    else
    {
        /* Look ahead in the string for the basename of the FUSE mount point */
        for (p = wpath;
                (p = g_strstr(p, g_fuse_root_path_basename)) != NULL ;
                p = p + 1)
        {
            /* Is this string preceded by a '/' ? */
            if (p == wpath || *(p - 1) != '/')
            {
                continue;
            }

            /* Is the string followed by a '/' or '\0' ? We know it's
             * valid to look ahead this far in the string, as g_strstr()
             * tells us it is */
            if (*(p + blen) != '/' && *(p + blen) != '\0')
            {
                continue;
            }

            /* Temporarily terminate the string early, and check the
             * file system characteristics of the preceding directory */
            *(p - 1) = '\0';

            if (g_file_get_device_number(wpath) == g_fuse_root_parent_dev &&
                    g_file_get_inode_num(wpath) == g_fuse_root_parent_ino)
            {
                rv = 1;
                break;
            }
            *(p - 1) = '/';
        }
        g_free(wpath);
    }
    return rv;
}

/******************************************************************************
**                                                                           **
**                           callbacks for fuse                              **
**                                                                           **
******************************************************************************/

/**
 * Look up a directory entry by name and get its attributes
 *****************************************************************************/

static void xfuse_cb_lookup(fuse_req_t req, fuse_ino_t parent, const char *name)
{
    XFS_INODE *parent_xinode;
    XFS_INODE *xinode = NULL;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "looking for parent=%ld name=%s", parent, name);

    if (strlen(name) > XFS_MAXFILENAMELEN)
    {
        fuse_reply_err(req, ENAMETOOLONG);
    }
    else if ((parent_xinode = xfs_get(g_xfs, parent)) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "inode %ld is not valid", parent);
        fuse_reply_err(req, ENOENT);
    }
    else
    {
        if (!parent_xinode->is_redirected)
        {
            /* File cannot be remote - we either know about it or we don't */
            if ((xinode = xfs_lookup_in_dir(g_xfs, parent, name)) != NULL)
            {
                LOG_DEVEL(LOG_LEVEL_DEBUG, "found entry for parent=%ld name=%s",
                          parent, name);
                make_fuse_entry_reply(req, xinode);
            }
            else
            {
                fuse_reply_err(req, ENOENT);
            }
        }
        else
        {
            /* specified file resides on redirected share
             *
             * We always look these up, and relying on libfuse to do sane
             * caching */
            struct state_lookup *fip = g_new0(struct state_lookup, 1);
            char *full_path = get_name_for_entry_in_parent(parent, name);

            if (fip == NULL || full_path == NULL)
            {
                LOG_DEVEL(LOG_LEVEL_ERROR, "system out of memory");
                fuse_reply_err(req, ENOMEM);
                free(fip);
                free(full_path);
            }
            else
            {
                const char *cptr;

                fip->req = req;
                fip->pinum = parent;
                strcpy(fip->name, name);

                /* we want path minus 'root node of the share' */
                cptr = filename_on_device(full_path);

                /* If the file already exists on our side, save the inum
                 * and generation. If it's not remote any more this means we
                 * can remove it when we get the response
                 */
                if ((xinode = xfs_lookup_in_dir(g_xfs, parent, name)) != NULL)
                {
                    fip->existing_inum = xinode->inum;
                    fip->existing_generation = xinode->generation;
                }
                LOG_DEVEL(LOG_LEVEL_DEBUG, "Looking up %s in %s on %d", name, cptr,
                          parent_xinode->device_id);
                /*
                 * If this call succeeds, further request processing happens in
                 * xfuse_devredir_cb_lookup_entry()
                 */
                if (devredir_lookup_entry(fip, parent_xinode->device_id, cptr))
                {
                    LOG_DEVEL(LOG_LEVEL_ERROR, "failed to send devredir_lookup_entry() cmd");
                    fuse_reply_err(req, EREMOTEIO);
                    free(fip);
                }
                free(full_path);
            }
        }
    }
}

/**
 * Get file attributes
 * GOTCHA : For FUSE 2.9 at least, the 'fi' parameter is allocated on the
 *          stack by the caller, so must be copied if we're not using it
 *          to reply to FUSE immediately
 *****************************************************************************/

static void xfuse_cb_getattr(fuse_req_t req, fuse_ino_t ino,
                             struct fuse_file_info *fi)
{
    XFS_INODE *xino;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "req=%p ino=%ld", req, ino);

    /* if ino is not valid, just return */
    if ((xino = xfs_get(g_xfs, ino)) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "inode %ld is not valid", ino);
        fuse_reply_err(req, ENOENT);
    }
    else
    {
        make_fuse_attr_reply(req, xino);
    }
}

/**
 *
 *****************************************************************************/

/*
 * Adds an entry to the buffer, using fuse_add_direnty()
 *
 * Returns 1 for success, or zero if the entry couldn't be added
 */
static int xfuse_dirbuf_add1(fuse_req_t req, struct dirbuf1 *b,
                             XFS_INODE *xinode, off_t offset)
{
    struct stat  stbuf;
    size_t len;
    int result = 0;

    memset(&stbuf, 0, sizeof(stbuf));
    stbuf.st_ino = xinode->inum;
    stbuf.st_mode = xinode->mode & ~g_cfg->file_umask;

    /*
     * Try to add the entry. From the docs for fuse_add_direntry():-
     *   "Buffer needs to be large enough to hold the entry. If it's not,
     *    then the entry is not filled in but the size of the entry is
     *    still returned."
     */
    len = fuse_add_direntry(req,
                            &b->buf[b->len], /* index where new entry will be added to buf */
                            sizeof(b->buf) - b->len,  /* Space left */
                            xinode->name,             /* name of entry */
                            &stbuf,                   /* file attributes */
                            offset                    /* offset of next entry */
                           );
    if (len + b->len <= sizeof(b->buf))
    {
        /* Entry fitted in OK */
        b->len += len;
        result = 1;
    }

    return result;
}

/**
 * GOTCHA : For FUSE 2.9 at least, the 'fi' parameter is allocated on the
 *          stack by the caller, so must be copied if we're not using it
 *          to reply to FUSE immediately
 *****************************************************************************/

static void xfuse_cb_readdir(fuse_req_t req, fuse_ino_t ino, size_t size,
                             off_t off, struct fuse_file_info *fi)
{
    XFS_INODE      *xinode;
    XFUSE_HANDLE   *xhandle;
    struct xfs_dir_handle *dh;
    struct dirbuf1   b;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "req=%p inode=%ld size=%zd offset=%lld", req, ino, size, (long long) off);

    /* On the first call, check the inode is valid */
    if (off == 0 && !xfs_get(g_xfs, ino))
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "inode %ld is not valid", ino);
        fuse_reply_err(req, ENOENT);
    }
    else if ((xhandle = xfuse_handle_from_fuse_handle(fi->fh)) == NULL
             || (dh = xhandle->dir_handle) == NULL)
    {
        /* something seriously wrong somewhere! */
        fuse_reply_buf(req, 0, 0);
    }
    else
    {
        b.len = 0;

        off_t new_off = off;
        while ((xinode = xfs_readdir(g_xfs, dh, &new_off)) != NULL)
        {
            if (xfuse_dirbuf_add1(req, &b, xinode, new_off) == 0)
            {
                break; /* buffer is full */
            }
            /* Make sure we get the next entry next time */
            off = new_off;
        }

        fuse_reply_buf(req, b.buf, b.len);
    }
}


/**
 * Create a directory
 *****************************************************************************/

static void xfuse_cb_mkdir(fuse_req_t req, fuse_ino_t parent,
                           const char *name, mode_t mode)
{
    XFS_INODE               *xinode;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "entered: parent_inode=%ld name=%s", parent, name);

    if ((xinode = xfs_lookup_in_dir(g_xfs, parent, name)) != NULL)
    {
        /* dir already exists, just return it */
        make_fuse_entry_reply(req, xinode);
    }
    else
    {
        /* dir does not exist, create it */
        xfuse_create_dir_or_file(req, parent, name, mode | S_IFDIR, NULL);
    }
}

/**
 * Remove a dir or file
 *
 *****************************************************************************/

static void xfuse_cb_unlink(fuse_req_t req, fuse_ino_t parent,
                            const char *name)
{
    XFS_INODE *xinode;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "entered: parent=%ld name=%s", parent, name);

    if (strlen(name) > XFS_MAXFILENAMELEN)
    {
        fuse_reply_err(req, ENAMETOOLONG);
    }
    else if ((xinode = xfs_lookup_in_dir(g_xfs, parent, name)) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "did not find file with pinode=%ld name=%s", parent, name);
        fuse_reply_err(req, ENOENT);
    }

    else if ((xinode->mode & S_IFDIR) != 0 &&
             !xfs_is_dir_empty(g_xfs, xinode->inum))
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "cannot rmdir; directory is not empty");
        fuse_reply_err(req, ENOTEMPTY);
    }

    else if (!xinode->is_redirected)
    {
        /* specified file is a local resource */
        //XFUSE_HANDLE *fh;

        LOG_DEVEL(LOG_LEVEL_DEBUG, "LK_TODO: this is still a TODO");
        fuse_reply_err(req, EROFS);
    }
    else
    {
        /* specified file resides on redirected share */
        struct state_remove *fip = g_new0(struct state_remove, 1);
        char *full_path = xfs_get_full_path(g_xfs, xinode->inum);
        if (!full_path || !fip)
        {
            LOG_DEVEL(LOG_LEVEL_ERROR, "system out of memory");
            fuse_reply_err(req, ENOMEM);
            free(fip);
        }
        else
        {
            const char *cptr;

            fip->req = req;
            fip->inum = xinode->inum;

            /* we want path minus 'root node of the share' */
            cptr = filename_on_device(full_path);

            /* get devredir to open the remote file
             *
             * If this call succeeds, further request processing happens in
             * xfuse_devredir_cb_rmdir_or_file()
             */
            if (devredir_rmdir_or_file(fip, xinode->device_id, cptr))
            {
                LOG_DEVEL(LOG_LEVEL_ERROR, "failed to send devredir_rmdir_or_file() cmd");
                fuse_reply_err(req, EREMOTEIO);
                free(fip);
            }

        }
        free(full_path);
    }
}

static void xfuse_cb_rename(fuse_req_t req,
                            fuse_ino_t old_parent, const char *old_name,
                            fuse_ino_t new_parent, const char *new_name,
                            unsigned int flags)
{
    XFS_INODE *old_xinode;
    XFS_INODE *new_parent_xinode;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "entered: old_parent=%ld old_name=%s new_parent=%ld new_name=%s",
              old_parent, old_name, new_parent, new_name);

    // renameat2() flags (in stdio.h) are not supported
    if (flags != 0)
    {
        fuse_reply_err(req, EINVAL);
    }
    else if (strlen(old_name) > XFS_MAXFILENAMELEN ||
             strlen(new_name) > XFS_MAXFILENAMELEN)
    {
        fuse_reply_err(req, ENAMETOOLONG);
    }
    else if (!(old_xinode = xfs_lookup_in_dir(g_xfs, old_parent, old_name)))
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "did not find file with pinode=%ld name=%s",
                  old_parent, old_name);
        fuse_reply_err(req, ENOENT);
    }

    else if (!(new_parent_xinode = xfs_get(g_xfs, new_parent)))
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "inode %ld is not valid", new_parent);
        fuse_reply_err(req, ENOENT);
    }

    else if (!xfs_check_move_entry(g_xfs, old_xinode->inum,
                                   new_parent, new_name))
    {
        /* Catchall -see rename(2). Fix when logging is improved */
        fuse_reply_err(req, EINVAL);
    }

    else if (new_parent_xinode->is_redirected != old_xinode->is_redirected ||
             new_parent_xinode->device_id != old_xinode->device_id)
    {
        fuse_reply_err(req, EXDEV);
    }

    else if (!old_xinode->is_redirected)
    {
        /* specified file is a local resource */
        LOG_DEVEL(LOG_LEVEL_DEBUG, "LK_TODO: this is still a TODO");
        fuse_reply_err(req, EROFS);
    }

    else
    {
        /* resource is on a redirected share */
        struct state_rename *fip = g_new0(struct state_rename, 1);
        char *old_full_path = xfs_get_full_path(g_xfs, old_xinode->inum);
        char *new_full_path = get_name_for_entry_in_parent(new_parent,
                              new_name);

        if (!old_full_path || !new_full_path || !fip)
        {
            LOG_DEVEL(LOG_LEVEL_ERROR, "system out of memory");
            fuse_reply_err(req, ENOMEM);
            free(fip);
            free(old_full_path);
            free(new_full_path);
        }
        else
        {
            const char *cptr;
            const char *cp;

            fip->req = req;
            fip->pinum = old_xinode->inum;
            fip->new_pinum = new_parent;
            strcpy(fip->name, new_name);

            /* we want path minus 'root node of the share' */
            cptr = filename_on_device(old_full_path);
            cp = filename_on_device(new_full_path);

            /*
             * If this call succeeds, further request processing happens in
             * xfuse_devredir_cb_rename_file()
             */
            if (devredir_file_rename(fip, old_xinode->device_id, cptr, cp))
            {
                LOG_DEVEL(LOG_LEVEL_ERROR, "failed to send devredir_file_rename() cmd");
                fuse_reply_err(req, EREMOTEIO);
                free(fip);
            }
            free(old_full_path);
            free(new_full_path);
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
 * @param fi     File info from fuse_lowlevel_ops.create, or NULL.
 *               This may need to be copied if we're not replying immediately
 *****************************************************************************/

static void xfuse_create_dir_or_file(fuse_req_t req, fuse_ino_t parent,
                                     const char *name, mode_t mode,
                                     struct fuse_file_info *fi)
{
    XFS_INODE        *xinode;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "entered: parent_ino=%ld name=%s mode=%o type=%s",
              parent, name, mode, (mode & S_IFDIR) ? "dir" : "file");

    /* name must be valid */
    if (strlen(name) > XFS_MAXFILENAMELEN)
    {
        fuse_reply_err(req, ENAMETOOLONG);
    }
    else
    {
        /* Sanitise the mode
         *
         * Currently group/world write is not allowed.
         */
        if (mode & S_IFDIR)
        {
            mode = (mode & 0777) | S_IFDIR;
        }
        else
        {
            mode = (mode & 0777) | S_IFREG;
        }

        /* is parent inode valid? */
        if (parent == FUSE_ROOT_ID)
        {
            fuse_reply_err(req, EROFS);
        }
        else if ((xinode = xfs_get(g_xfs, parent)) == NULL)
        {
            fuse_reply_err(req, ENOENT);
        }
        else if ((xinode->mode & S_IFDIR) == 0)
        {
            fuse_reply_err(req, ENOTDIR);
        }
        else if (!xinode->is_redirected)
        {
            /* specified file is a local resource */
            //XFUSE_HANDLE *fh;

            LOG_DEVEL(LOG_LEVEL_DEBUG, "LK_TODO: this is still a TODO");
            fuse_reply_err(req, EROFS);
        }
        else
        {
            struct state_create *fip = g_new0(struct state_create, 1);
            char *full_path = get_name_for_entry_in_parent(parent, name);

            if (full_path == NULL || fip == NULL)
            {
                LOG_DEVEL(LOG_LEVEL_ERROR, "Out of memory");
                fuse_reply_err(req, ENOMEM);
                free(fip);
                free(full_path);
            }
            else
            {
                const char *cptr;

                fip->req = req;
                if (fi != NULL)
                {
                    fip->fi = *fi;
                }
                fip->pinum = parent;
                strcpy(fip->name, name);
                fip->mode = mode;

                /* we want path minus 'root node of the share' */
                cptr = filename_on_device(full_path);

                /*
                 * If this call succeeds, further request processing happens
                 * in xfuse_devredir_cb_create_file()
                 */
                if (devredir_file_create(fip, xinode->device_id, cptr, mode))
                {
                    LOG_DEVEL(LOG_LEVEL_ERROR, "failed to send devredir_file_create() cmd");
                    fuse_reply_err(req, EREMOTEIO);
                    free(fip);
                }
                free(full_path);
            }
        }
    }
}

/**
 * Open specified file
*
 * GOTCHA : For FUSE 2.9 at least, the 'fi' parameter is allocated on the
 *          stack by the caller, so must be copied if we're not using it
 *          to reply to FUSE immediately
 *****************************************************************************/

static void xfuse_cb_open(fuse_req_t req, fuse_ino_t ino,
                          struct fuse_file_info *fi)
{
    XFS_INODE        *xinode;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "entered: ino=%ld", ino);

    if (!(xinode = xfs_get(g_xfs, ino)))
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "inode %ld is not valid", ino);
        fuse_reply_err(req, ENOENT);
    }
    else if (xinode->mode & S_IFDIR)
    {
        /* Can't open directories like this */
        LOG_DEVEL(LOG_LEVEL_DEBUG, "reading/writing a dir not allowed!");
        fuse_reply_err(req, EISDIR);
    }
    else if ((fi->flags & O_ACCMODE) != O_RDONLY &&
             (fi->flags & O_ACCMODE) != O_WRONLY &&
             (fi->flags & O_ACCMODE) != O_RDWR)
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "Invalid access mode specified");
        fuse_reply_err(req, EINVAL);
    }
    else if (!xinode->is_redirected)
    {
        /* specified file is a local resource */
        if ((fi->flags & O_ACCMODE) != O_RDONLY)
        {
            fuse_reply_err(req, EROFS);
        }
        else
        {
            XFUSE_HANDLE *fh = xfuse_handle_create();
            fh->is_loc_resource = 1;
            fi->fh = xfuse_handle_to_fuse_handle(fh);
            fuse_reply_open(req, fi);
        }
    }
    else
    {
        /* specified file resides on redirected share */
        struct state_open *fip = g_new0(struct state_open, 1);
        char *full_path = xfs_get_full_path(g_xfs, ino);

        if (!full_path || !fip)
        {
            LOG_DEVEL(LOG_LEVEL_ERROR, "system out of memory");
            fuse_reply_err(req, ENOMEM);
            free(fip);
            free(full_path);
        }
        else
        {
            const char *cptr;

            fip->req = req;
            fip->fi = *fi;
            fip->inum = ino;

            if (g_cfg->fuse_direct_io)
            {
                fip->fi.direct_io = 1;
            }

            /* we want path minus 'root node of the share' */
            cptr = filename_on_device(full_path);

            /* get devredir to open the remote file
             *
             * If the caller has set O_TRUNC when writing the file,
             * fuse should call us back via fuse_cb_setattr() to set
             * the size to zero - we don't need to do this ourselves.
             *
             * If this call succeeds, further request processing happens in
             * xfuse_devredir_cb_open_file()
             */
            if (devredir_file_open(fip, xinode->device_id, cptr, fi->flags))
            {
                LOG_DEVEL(LOG_LEVEL_ERROR, "failed to send devredir_file_open() cmd");
                fuse_reply_err(req, EREMOTEIO);
                free(fip);
            }
            free(full_path);
        }
    }
}

/*
 * GOTCHA : For FUSE 2.9 at least, the 'fi' parameter is allocated on the
 *          stack by the caller, so must be copied if we're not using it
 *          to reply to FUSE immediately
 */
static void xfuse_cb_release(fuse_req_t req, fuse_ino_t ino, struct
                             fuse_file_info *fi)
{
    XFS_INODE   *xinode;

    XFUSE_HANDLE *handle = xfuse_handle_from_fuse_handle(fi->fh);

    LOG_DEVEL(LOG_LEVEL_DEBUG, "entered: ino=%ld fi=%p fi->fh=0x%llx", ino, fi,
              (long long) fi->fh);

    if ((xinode = xfs_get(g_xfs, ino)) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "inode %ld is not valid", ino);
        fuse_reply_err(req, ENOENT);
    }
    else if (!xinode->is_redirected)
    {
        /* specified file is a local resource */
        fuse_reply_err(req, 0);
    }
    else
    {
        /* specified file resides on redirected share */

        struct state_close *fip = g_new0(struct state_close, 1);
        if (fip == NULL)
        {
            LOG_DEVEL(LOG_LEVEL_ERROR, "system out of memory");
            fuse_reply_err(req, ENOMEM);
            return;
        }

        fip->req = req;
        fip->inum = ino;
        fip->fi = *fi;

        fi->fh = xfuse_handle_to_fuse_handle(NULL);

        /*
         * If this call succeeds, further request processing happens in
         * xfuse_devredir_cb_file_close()
         */
        if (devredir_file_close(fip, xinode->device_id, handle->FileId))
        {
            LOG_DEVEL(LOG_LEVEL_ERROR, "failed to send devredir_close_file() cmd");
            fuse_reply_err(req, EREMOTEIO);
            free(fip);
        }

        xfuse_handle_delete(handle);
    }
}

/**
 * GOTCHA : For FUSE 2.9 at least, the 'fi' parameter is allocated on the
 *          stack by the caller, so must be copied if we're not using it
 *          to reply to FUSE immediately
 *****************************************************************************/

static void xfuse_cb_read(fuse_req_t req, fuse_ino_t ino, size_t size,
                          off_t off, struct fuse_file_info *fi)
{
    XFUSE_HANDLE          *fh;
    struct state_read *fusep;
    XFS_INODE            *xinode;
    struct req_list_item  *rli;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "want_bytes %zd bytes at off %lld", size, (long long) off);

    if ((fh = xfuse_handle_from_fuse_handle(fi->fh)) == NULL)
    {
        fuse_reply_err(req, EINVAL);
    }
    else if (fh->is_loc_resource)
    {
        /* target file is in .clipboard dir */

        LOG_DEVEL(LOG_LEVEL_DEBUG, "target file is in .clipboard dir");

        if ((xinode = xfs_get(g_xfs, ino)) == NULL)
        {
            LOG_DEVEL(LOG_LEVEL_ERROR, "ino does not exist in xrdp_fs");
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
            LOG_DEVEL(LOG_LEVEL_DEBUG, "requesting clipboard file data lindex = %d off = %lld size = %zd",
                      rli->lindex, (long long) off, size);

            clipboard_request_file_data(rli->stream_id, rli->lindex,
                                        (int) off, (int) size);
        }
    }
    else
    {
        /* target file is on a remote device */

        fusep = g_new0(struct state_read, 1);
        if (fusep == NULL)
        {
            LOG_DEVEL(LOG_LEVEL_ERROR, "system out of memory");
            fuse_reply_err(req, ENOMEM);
        }
        else
        {
            fusep->req = req;

            /*
             * If this call succeeds, further request processing happens in
             * xfuse_devredir_cb_read_file()
             */
            devredir_file_read(fusep, fh->DeviceId, fh->FileId, size, off);
        }
    }
}

/**
 * GOTCHA : For FUSE 2.9 at least, the 'fi' parameter is allocated on the
 *          stack by the caller, so must be copied if we're not using it
 *          to reply to FUSE immediately
 *****************************************************************************/

static void xfuse_cb_write(fuse_req_t req, fuse_ino_t ino, const char *buf,
                           size_t size, off_t off, struct fuse_file_info *fi)
{
    XFUSE_HANDLE *fh;
    struct state_write *fusep;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "write %zd bytes at off %lld to inode=%ld",
              size, (long long) off, ino);

    if ((fh = xfuse_handle_from_fuse_handle(fi->fh)) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "file handle fi->fh is NULL");
        fuse_reply_err(req, EINVAL);
    }
    else if (fh->is_loc_resource)
    {
        /* target file is in .clipboard dir */
        LOG_DEVEL(LOG_LEVEL_DEBUG, "THIS IS STILL A TODO!");
        fuse_reply_err(req, EROFS);
    }
    else
    {
        /* target file is on a remote device */

        fusep = g_new0(struct state_write, 1);
        if (fusep == NULL)
        {
            LOG_DEVEL(LOG_LEVEL_ERROR, "system out of memory");
            fuse_reply_err(req, ENOMEM);
        }
        else
        {
            fusep->req = req;
            fusep->inum = ino;

            /*
             * If this call succeeds, further request processing happens in
             * xfuse_devredir_cb_write_file()
             */
            devredir_file_write(fusep, fh->DeviceId, fh->FileId, buf,
                                size, off);
        }
    }
}

/**
 *****************************************************************************/

/*
 * GOTCHA : For FUSE 2.9 at least, the 'fi' parameter is allocated on the
 *          stack by the caller, so must be copied if we're not using it
 *          to reply to FUSE immediately
 */
static void xfuse_cb_create(fuse_req_t req, fuse_ino_t parent,
                            const char *name, mode_t mode,
                            struct fuse_file_info *fi)
{
    LOG_DEVEL(LOG_LEVEL_DEBUG, "entered: parent_inode=%ld, name=%s fi=%p",
              parent, name, fi);

    xfuse_create_dir_or_file(req, parent, name, mode & ~S_IFDIR, fi);
}

/**
 *****************************************************************************/

#if 0
static void xfuse_cb_fsync(fuse_req_t req, fuse_ino_t ino, int datasync,
                           struct fuse_file_info *fi)
{
    LOG_DEVEL(LOG_LEVEL_DEBUG, "#################### entered: ino=%ld datasync=%d", ino, datasync);
    LOG_DEVEL(LOG_LEVEL_DEBUG, "function not required");
    fuse_reply_err(req, EINVAL);
}
#endif

/**
 * Sets attributes for a directory entry.
 *
 * If the file resides on a remote share, devredir
 * is asked to update it. This will result in the following
 * callbacks:-
 * - xfuse_devredir_cb_setattr() to update our copy and return status
 *
 * GOTCHA : For FUSE 2.9 at least, the 'fi' parameter is allocated on the
 *          stack by the caller, so must be copied if we're not using it
 *          to reply to FUSE immediately
 *****************************************************************************/

static void xfuse_cb_setattr(fuse_req_t req, fuse_ino_t ino, struct stat *attr,
                             int to_set, struct fuse_file_info *fi)
{
    XFS_INODE   *xinode;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "entered to_set=0x%x", to_set);

    if ((xinode = xfs_get(g_xfs, ino)) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "inode %ld is not valid", ino);
        fuse_reply_err(req, ENOENT);
    }
    else if (((to_set & FUSE_SET_ATTR_UID) && attr->st_uid != xinode->uid) ||
             ((to_set & FUSE_SET_ATTR_GID) && attr->st_gid != xinode->gid))
    {
        /* We don't allow any of these */
        fuse_reply_err(req, EPERM);
    }
    else if ((to_set & FUSE_SET_ATTR_MODE) &&
             (attr->st_mode & ~(0777 | S_IFDIR | S_IFREG)) != 0)
    {
        /* We only support standard mode bits and S_IFDIR / S_IFREG */
        LOG_DEVEL(LOG_LEVEL_ERROR, "Asking for invalid mode bits 0%o to be set", attr->st_mode);
        fuse_reply_err(req, EINVAL);
    }
    else
    {
        struct file_attr attrs = {0};
        tui32 change_mask = 0;

        if ((to_set & FUSE_SET_ATTR_MODE) && xinode->mode != attr->st_mode)
        {
            attrs.mode = attr->st_mode & (0777 | S_IFDIR | S_IFREG);
            change_mask |= TO_SET_MODE;
        }

        if ((to_set & FUSE_SET_ATTR_SIZE) && xinode->size != attr->st_size)
        {
            attrs.size = attr->st_size;
            change_mask |= TO_SET_SIZE;
        }

        if ((to_set & FUSE_SET_ATTR_ATIME) && xinode->atime != attr->st_atime)
        {
            attrs.atime = attr->st_atime;
            change_mask |= TO_SET_ATIME;
        }

        if ((to_set & FUSE_SET_ATTR_MTIME) && xinode->mtime != attr->st_mtime)
        {
            attrs.mtime = attr->st_mtime;
            change_mask |= TO_SET_MTIME;
        }

        if (change_mask == 0)
        {
            /* No changes have been made */
            make_fuse_attr_reply(req, xinode);
        }
        else if (!xinode->is_redirected)
        {
            /* Update the local fs */
            update_inode_file_attributes(&attrs, change_mask, xinode);
            make_fuse_attr_reply(req, xinode);
        }
        else
        {
            struct state_setattr *fip = g_new0(struct state_setattr, 1);
            char *full_path = xfs_get_full_path(g_xfs, ino);
            if (!full_path || !fip)
            {
                LOG_DEVEL(LOG_LEVEL_ERROR, "system out of memory");
                fuse_reply_err(req, ENOMEM);
                free(fip);
                free(full_path);
            }
            else
            {
                const char *cptr;
                fip->req = req;
                fip->inum = ino;
                /* Save the important stuff so we can update our node if the
                 * remote update is successful */
                fip->fattr = attrs;
                fip->change_mask = change_mask;

                /* we want path minus 'root node of the share' */
                cptr = filename_on_device(full_path);

                /*
                 * If this call succeeds, further request processing happens
                 * in xfuse_devredir_cb_setattr()
                 */
                if (devredir_setattr_for_entry(fip, xinode->device_id,
                                               cptr, &attrs, change_mask) < 0)
                {
                    fuse_reply_err(req, EIO);
                    free(fip);
                }

                free(full_path);
            }
        }
    }
}

/**
 * Get dir listing
 *
 * GOTCHA : For FUSE 2.9 at least, the 'fi' parameter is allocated on the
 *          stack by the caller, so must be copied if we're not using it
 *          to reply to FUSE immediately
 *****************************************************************************/
static void xfuse_cb_opendir(fuse_req_t req, fuse_ino_t ino,
                             struct fuse_file_info *fi)
{
    XFS_INODE      *xinode;
    XFUSE_HANDLE   *xhandle;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "inode=%ld", ino);

    if ((xinode = xfs_get(g_xfs, ino)) == NULL)
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "inode %ld is not valid", ino);
        fuse_reply_err(req, ENOENT);
    }
    else if (!xinode->is_redirected)
    {
        if ((xhandle = xfuse_handle_create()) == NULL)
        {
            fuse_reply_err(req, ENOMEM);
        }
        else
        {
            if ((xhandle->dir_handle = xfs_opendir(g_xfs, ino)) == NULL)
            {
                xfuse_handle_delete(xhandle);
                fuse_reply_err(req, ENOMEM);
            }
            else
            {
                fi->fh = xfuse_handle_to_fuse_handle(xhandle);
                fuse_reply_open(req, fi);
            }
        }
    }
    else
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "did not find entry; redirecting call to devredir");
        struct state_dirscan *fip = g_new0(struct state_dirscan, 1);
        char *full_path = xfs_get_full_path(g_xfs, ino);

        if (full_path == NULL || fip == NULL)
        {
            fuse_reply_err(req, ENOMEM);
            free(fip);
            free(full_path);
        }
        else
        {
            const char      *cptr;
            LOG_DEVEL(LOG_LEVEL_DEBUG, "dev_id=%d ino=%ld full_path=%s",
                      xinode->device_id, ino, full_path);

            fip->req = req;
            fip->pinum = ino;
            fip->fi = *fi;

            /* we want path minus 'root node of the share' */
            cptr = filename_on_device(full_path);

            /*
             * If this call succeeds, further request processing happens in:-
             * - xfuse_devredir_cb_enum_dir_add_entry()
             *       Called for every file in the directory
             * - xfuse_devredir_cb_enum_dir_done()
             *       Called at the end of the directory scan
             */
            if (devredir_get_dir_listing(fip, xinode->device_id, cptr))
            {
                LOG_DEVEL(LOG_LEVEL_ERROR, "failed to send devredir_get_dir_listing() cmd");
                fuse_reply_buf(req, NULL, 0);
                free(fip);
            }
            free(full_path);
        }
    }
}

/**
 * GOTCHA : For FUSE 2.9 at least, the 'fi' parameter is allocated on the
 *          stack by the caller, so must be copied if we're not using it
 *          to reply to FUSE immediately
 *****************************************************************************/

static void xfuse_cb_releasedir(fuse_req_t req, fuse_ino_t ino,
                                struct fuse_file_info *fi)
{
    XFUSE_HANDLE *xhandle = xfuse_handle_from_fuse_handle(fi->fh);
    xfs_closedir(g_xfs, xhandle->dir_handle);
    xhandle->dir_handle = NULL;
    xfuse_handle_delete(xhandle);
    fuse_reply_err(req, 0);
}

/******************************************************************************
 *                           miscellaneous functions
 *****************************************************************************/

static void xfs_inode_to_fuse_entry_param(const XFS_INODE *xinode,
        struct fuse_entry_param *e)
{
    memset(e, 0, sizeof(*e));
    e->ino = xinode->inum;
    e->attr_timeout = XFUSE_ATTR_TIMEOUT;
    e->entry_timeout = XFUSE_ENTRY_TIMEOUT;
    e->attr.st_ino = xinode->inum;
    e->attr.st_mode = xinode->mode & ~g_cfg->file_umask;
    e->attr.st_nlink = 1;
    e->attr.st_uid = xinode->uid;
    e->attr.st_gid = xinode->gid;
    e->attr.st_size = xinode->size;
    e->attr.st_atime = xinode->atime;
    e->attr.st_mtime = xinode->mtime;
    e->attr.st_ctime = xinode->ctime;
    e->generation =  xinode->generation;
}

static void make_fuse_entry_reply(fuse_req_t req, const XFS_INODE *xinode)
{
    struct fuse_entry_param  e;
    xfs_inode_to_fuse_entry_param(xinode, &e);
    fuse_reply_entry(req, &e);
}

static void make_fuse_attr_reply(fuse_req_t req, const XFS_INODE *xinode)
{
    struct stat st;

    memset(&st, 0, sizeof(st));
    st.st_ino = xinode->inum;
    st.st_mode = xinode->mode & ~g_cfg->file_umask;
    st.st_nlink = 1;
    st.st_uid = xinode->uid;
    st.st_gid = xinode->gid;
    st.st_size = xinode->size;
    st.st_atime = xinode->atime;
    st.st_mtime = xinode->mtime;
    st.st_ctime = xinode->ctime;

    fuse_reply_attr(req, &st, XFUSE_ATTR_TIMEOUT);
}

/*
 * Get the name of a file on the device
 *
 * For redirected devices, the routine xfs_get_full_path() returns names
 * like "/C:/Windows/System32".
 * This routine simply returns a pointer to the part of the name following
 * the device specification.
 */
static const char *filename_on_device(const char *full_path)
{
    const char *result = NULL;
    if (full_path[0] != '\0')
    {
        result = strchr(full_path + 1, '/');
    }
    return result ? result : "/";
}

/*
 * Updates attributes on the filesystem, and bumps the inode ctime
 *
 * This call is used to set attributes, either locally, or following a
 * setattr devredir call
 */
static void update_inode_file_attributes(const struct file_attr *fattr,
        tui32 change_mask, XFS_INODE *xinode)
{
    int updated = 0;

    if ((change_mask & TO_SET_MODE) != 0 && xinode->mode != fattr->mode)
    {
        xinode->mode = fattr->mode;
        updated = 1;
    }
    if ((change_mask & TO_SET_SIZE) != 0 && xinode->size != fattr->size)
    {
        xinode->size = fattr->size;
        updated = 1;
    }
    if ((change_mask & TO_SET_ATIME) != 0 && xinode->atime != fattr->atime)
    {
        xinode->atime = fattr->atime;
        updated = 1;
    }
    if ((change_mask & TO_SET_MTIME) != 0 && xinode->mtime != fattr->mtime)
    {
        xinode->mtime = fattr->mtime;
        updated = 1;
    }

    if (updated)
    {
        xinode->ctime = time(0);
    }
}

/*
 * Gets the name for a file where we know the parent inode and the
 * name for the file under that inode
 *
 * Result must be freed after use
 */
static char *get_name_for_entry_in_parent(fuse_ino_t parent, const char *name)
{
    char *result;

    if ((result = xfs_get_full_path(g_xfs, parent)) != NULL)
    {
        char *p = (char *) realloc(result,
                                   strlen(result) + 1 + strlen(name) + 1);
        if (p == NULL)
        {
            /* See cppcheck trac #9292 and #9437 */
            /* cppcheck-suppress doubleFree symbolName=result */
            free(result);
            result = NULL;
        }
        else
        {
            result = p;
            strcat(result, "/");
            strcat(result, name);
        }
    }

    return result;
}

/*
 * Scans a user-provided string substituting %u/%U/ for UID/username
 * and %d/%D fordisplaynumber/DISPLAY
 */
static unsigned int format_user_info(char *dest, unsigned int len,
                                     const char *format)
{
    char uidstr[64];
    char username[64];
    char display[64];
    char displaynum[64];
    const struct info_string_tag map[] =
    {
        {'u', uidstr},
        {'U', username},
        {'d', displaynum},
        {'D', display},
        INFO_STRING_END_OF_LIST
    };

    int uid = g_getuid();
    g_snprintf(uidstr, sizeof(uidstr), "%d", uid);
    if (g_getlogin(username, sizeof(username)) != 0)
    {
        /* Fall back to UID */
        g_strncpy(username, uidstr, sizeof(username) - 1);
    }

    if (getenv("DISPLAY") == NULL)
    {
        /* if environment variable is not set, set it to empty string */
        display[0] = '\0';
        displaynum[0] = '\0';
    }
    else
    {
        g_strncpy(display, getenv("DISPLAY"), sizeof(display) - 1);
        g_snprintf(displaynum, sizeof(displaynum), "%d",
                   g_get_display_num_from_display(display));
    }

    return g_format_info_string(dest, len, format, map);
}

#endif /* end else #ifndef XRDP_FUSE */
