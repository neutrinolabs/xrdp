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

#include <sys/types.h>
#include <time.h>

#include "arch.h"
#include "ms-erref.h"

/* Used to pass file info back to chansrv_fuse from devredir */
struct file_attr
{
    tui32           mode;              /* File mode.                        */
    off_t           size;              /* Size of file, in bytes.           */
    time_t          atime;             /* Time of last access.              */
    time_t          mtime;             /* Time of last modification.        */
};

/* Bitmask values used to identify individual elements in
 * struct file_attr
 */
#define TO_SET_MODE  (1<<0)
#define TO_SET_SIZE  (1<<1)
#define TO_SET_ATIME (1<<2)
#define TO_SET_MTIME (1<<3)
#define TO_SET_ALL   (TO_SET_MODE | TO_SET_SIZE | TO_SET_ATIME | TO_SET_MTIME)

/* Private type passed into and back-from devredir */
typedef struct xfuse_info XFUSE_INFO;

int xfuse_init(void);
int xfuse_deinit(void);
int xfuse_check_wait_objs(void);
int xfuse_get_wait_objs(tbus *objs, int *count, int *timeout);
int xfuse_create_share(tui32 share_id, const char *dirname);
void xfuse_delete_share(tui32 share_id);

int xfuse_clear_clip_dir(void);
int xfuse_file_contents_range(int stream_id, const char *data, int data_bytes);
int xfuse_file_contents_size(int stream_id, int file_size);
int xfuse_add_clip_dir_item(const char *filename,
                            int flags, int size, int lindex);

/* State pointer types (opaque outside this module), used for
 * callback data
 */
struct state_dirscan;
struct state_lookup;
struct state_setattr;
struct state_open;
struct state_create;
struct state_read;
struct state_write;
struct state_remove;
struct state_rename;
struct state_close;


/* functions that are invoked from devredir */
void xfuse_devredir_cb_enum_dir_add_entry(
    struct state_dirscan *fip,
    const char *name,
    const struct file_attr *fattr);
void xfuse_devredir_cb_enum_dir_done(struct state_dirscan *fip,
                                     enum NTSTATUS IoStatus);

void xfuse_devredir_cb_lookup_entry(struct state_lookup *fip,
                                    enum NTSTATUS IoStatus,
                                    const struct file_attr *file_info);

void xfuse_devredir_cb_setattr(struct state_setattr *fip,
                               enum NTSTATUS IoStatus);

void xfuse_devredir_cb_create_file(struct state_create *fip,
                                   enum NTSTATUS IoStatus,
                                   tui32 DeviceId, tui32 FileId);

void xfuse_devredir_cb_open_file(struct state_open *fip,
                                 enum NTSTATUS IoStatus,
                                 tui32 DeviceId, tui32 FileId);

void xfuse_devredir_cb_read_file(struct state_read *fip,
                                 enum NTSTATUS IoStatus,
                                 const char *buf, size_t length);
void xfuse_devredir_cb_write_file(
    struct state_write *fip,
    enum NTSTATUS IoStatus,
    off_t offset,
    size_t length);

void xfuse_devredir_cb_rmdir_or_file(struct state_remove *fip,
                                     enum NTSTATUS IoStatus);

void xfuse_devredir_cb_rename_file(struct state_rename *fip,
                                   enum NTSTATUS IoStatus);

void xfuse_devredir_cb_file_close(struct state_close *fip);

#endif
