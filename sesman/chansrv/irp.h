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
 *
 */

/*
 * manage I/O for redirected file system and devices
 */

#ifndef __IRP_H
#define __IRP_H

typedef struct fuse_data FUSE_DATA;
struct fuse_data
{
    void      *data_ptr;
    FUSE_DATA *next;
};

/* An I/O Resource Packet to track I/O calls */

typedef struct irp IRP;

struct irp
{
    tui32      CompletionId;        /* unique number                     */
    tui32      DeviceId;            /* identifies remote device          */
    tui32      FileId;              /* RDP client provided unique number */
    char       completion_type;     /* describes I/O type                */
    char       pathname[256];       /* absolute pathname                 */
    char       gen_buf[1024];       /* for general use                   */
    int        type;
    FUSE_DATA *fd_head;             /* point to first FUSE opaque object */
    FUSE_DATA *fd_tail;             /* point to last FUSE opaque object  */
    IRP       *next;                /* point to next IRP                 */
    IRP       *prev;                /* point to previous IRP             */
    int        scard_index;         /* used to smart card to locate dev  */

    void     (*callback)(struct stream *s, IRP *irp, tui32 DeviceId,
                         tui32 CompletionId, tui32 IoStatus);
    void      *user_data;
};

IRP * devredir_irp_new(void);
IRP * devredir_irp_clone(IRP *irp);
int   devredir_irp_delete(IRP *irp);
IRP * devredir_irp_find(tui32 completion_id);
IRP * devredir_irp_find_by_fileid(tui32 FileId);
IRP * devredir_irp_get_last(void);
void  devredir_irp_dump(void);

#endif /* end ifndef __IRP_H */
