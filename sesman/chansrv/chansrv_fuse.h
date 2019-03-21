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

#ifndef _CHANSRV_FUSE_H
#define _CHANSRV_FUSE_H

#include "arch.h"

/*
 * a file or dir entry in the xrdp file system (opaque type externally) */
struct xrdp_inode;

/* Used to pass file info in to xfuse_devredir_add_file_or_dir()
 *
 * The string storage the name field points to is only valid
 * for the duration of the xfuse_devredir_add_file_or_dir()
 * call
 */
struct file_attr
{
    const char     *name;              /* Name of file or directory         */
    tui32           mode;              /* File mode.                        */
    size_t          size;              /* Size of file, in bytes.           */
    time_t          atime;             /* Time of last access.              */
    time_t          mtime;             /* Time of last modification.        */
    time_t          ctime;             /* Time of last status change.       */
};

int xfuse_init(void);
int xfuse_deinit(void);
int xfuse_check_wait_objs(void);
int xfuse_get_wait_objs(tbus *objs, int *count, int *timeout);
int xfuse_create_share(tui32 share_id, const char *dirname);

int xfuse_clear_clip_dir(void);
int xfuse_file_contents_range(int stream_id, const char *data, int data_bytes);
int xfuse_file_contents_size(int stream_id, int file_size);
int xfuse_add_clip_dir_item(const char *filename, int flags, int size, int lindex);

/* functions that are invoked from devredir */
struct xrdp_inode *xfuse_devredir_add_file_or_dir(
                                 void *vp,
                                 const struct file_attr *file_info);
void xfuse_devredir_cb_enum_dir_done(void *vp, tui32 IoStatus);
void xfuse_devredir_cb_lookup_entry(void *vp, tui32 IoStatus,
                                    struct xrdp_inode *xinode);
void xfuse_devredir_cb_open_file(void *vp, tui32 IoStatus, tui32 DeviceId, tui32 FileId);
void xfuse_devredir_cb_write_file(
                                 void *vp,
                                 tui32 IoStatus,
                                 const char *buf,
                                 size_t length);
void xfuse_devredir_cb_read_file(void *vp, const char *buf, size_t length);
void xfuse_devredir_cb_rmdir_or_file(void *vp, tui32 IoStatus);
void xfuse_devredir_cb_rename_file(void *vp, tui32 IoStatus);
void xfuse_devredir_cb_file_close(void *vp);

#endif
