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

/* a file or dir entry in the xrdp file system */
struct xrdp_inode
{
    tui32           parent_inode;  /* Parent serial number.         */
    tui32           inode;         /* File serial number.           */
    tui32           mode;          /* File mode.                    */
    tui32           nlink;         /* symbolic link count.          */
    tui32           nentries;      /* number of entries in a dir    */
    tui32           nopen;         /* number of simultaneous opens  */
    tui32           uid;           /* User ID of the file's owner.  */
    tui32           gid;           /* Group ID of the file's group. */
    size_t          size;          /* Size of file, in bytes.       */
    time_t          atime;         /* Time of last access.          */
    time_t          mtime;         /* Time of last modification.    */
    time_t          ctime;         /* Time of last status change.   */
    char            name[256];     /* Dir or filename               */
    tui32           device_id;     /* for file system redirection   */
    char            is_synced;     /* dir struct has been read from */
                                   /* remote device, done just once */
    int             lindex;        /* used in clipboard operations  */
};
typedef struct xrdp_inode XRDP_INODE; // LK_TODO use this instead of using struct xrdp_inode

int xfuse_init();
int xfuse_deinit();
int xfuse_check_wait_objs(void);
int xfuse_get_wait_objs(tbus *objs, int *count, int *timeout);
int xfuse_create_share(tui32 share_id, char *dirname);

int xfuse_clear_clip_dir(void);
int xfuse_file_contents_range(int stream_id, char *data, int data_bytes);
int xfuse_file_contents_size(int stream_id, int file_size);
int xfuse_add_clip_dir_item(char *filename, int flags, int size, int lindex);

/* functions that are invoked from devredir */
void xfuse_devredir_cb_enum_dir(void *vp, struct xrdp_inode *xinode);
void xfuse_devredir_cb_enum_dir_done(void *vp, tui32 IoStatus);
void xfuse_devredir_cb_open_file(void *vp, tui32 DeviceId, tui32 FileId);
void xfuse_devredir_cb_read_file(void *vp, char *buf, size_t length);
void xfuse_devredir_cb_rmdir_or_file(void *vp, tui32 IoStatus);
void xfuse_devredir_cb_rename_file(void *vp, tui32 IoStatus);
void xfuse_devredir_cb_file_close(void *vp);

#endif
