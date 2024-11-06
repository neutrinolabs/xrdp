/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * xrdp device redirection - we mainly use it for drive redirection
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

#if !defined(DEVREDIR_H)
#define DEVREDIR_H

#include "irp.h"
#include "ms-rdpefs.h"

int devredir_init(void);
int devredir_deinit(void);

int devredir_data_in(struct stream *s, int chan_id, int chan_flags,
                     int length, int total_length);

int devredir_get_wait_objs(tbus *objs, int *count, int *timeout);
int devredir_check_wait_objs(void);

/* misc stuff */
void devredir_insert_DeviceIoRequest(struct stream *s,
                                     tui32 DeviceId,
                                     tui32 FileId,
                                     tui32 CompletionId,
                                     enum IRP_MJ MajorFunction,
                                     enum IRP_MN MinorFunction);

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
struct state_close;
struct state_statfs;

/* called from FUSE module */

int devredir_get_dir_listing(struct state_dirscan *fusep, tui32 device_id,
                             const char *path);

int devredir_lookup_entry(struct state_lookup *fusep, tui32 device_id,
                          const char *path);

int devredir_setattr_for_entry(
    struct state_setattr *fusep, tui32 device_id,
    const char *filename,
    const struct file_attr *fattr,
    tui32 to_set);

int devredir_file_create(
    struct state_create *fusep, tui32 device_id,
    const char *path, int mode);

int devredir_file_open(struct state_open *fusep, tui32 device_id,
                       const char *path, int flags);

int devredir_file_close(struct state_close *fusep, tui32 device_id,
                        tui32 file_id);

void
devredir_file_read(struct state_read *fusep, tui32 device_id, tui32 FileId,
                   tui32 Length, tui64 Offset);

void
devredir_file_write(struct state_write *fusep, tui32 DeviceId, tui32 FileId,
                    const char *buf, int Length, tui64 Offset);

int devredir_file_rename(
    struct state_rename *fusep, tui32 device_id,
    const char *old_name,
    const char *new_name);

int
devredir_rmdir_or_file(struct state_remove *fusep, tui32 device_id,
                       const char *path);

int
devredir_statfs(struct state_statfs *fusep, tui32 device_id, const char *path);

#endif
