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

#ifndef _TIME_H_
#include <time.h>
#endif
#include "chansrv_fuse.h"

/* Opaque data types to us */
typedef struct xfuse_info XFUSE_INFO;

enum irp_lookup_state
{
    E_LOOKUP_GET_FH = 0,
    E_LOOKUP_CHECK_BASIC,
    E_LOOKUP_CHECK_EOF
} ;

enum irp_setattr_state
{
    E_SETATTR_GET_FH = 0,
    E_SETATTR_CHECK_BASIC,
    E_SETATTR_CHECK_EOF
} ;

struct irp_lookup
{
    enum irp_lookup_state state; /* Next state to consider */
    struct file_attr fattr;      /* Attributes to get */
};

struct irp_setattr
{
    enum irp_setattr_state state; /* Next state to consider */
    tui32            to_set;      /* Bit mask for elements in use */
    struct file_attr fattr;       /* Attributes to set */
};

struct irp_write
{
    tui64            offset;      /* Offset the write was made at */
};

struct irp_create
{
    int             creating_dir; /* We're creating a directory */
};

struct irp_rename
{
    char           *new_name;     /* New name for file */
};

/* An I/O Resource Packet to track I/O calls */

typedef struct irp IRP;

struct irp
{
    tui32      CompletionId;        /* unique number                     */
    tui32      DeviceId;            /* identifies remote device          */
    tui32      FileId;              /* RDP client provided unique number */
    char       completion_type;     /* describes I/O type                */
    char       *pathname;           /* absolute pathname
                                     * Allocate with
                                     * devredir_irp_with_pathname_new()  */
    union
    {
        struct irp_lookup  lookup;  /* Used by lookup                    */
        struct irp_setattr setattr; /* Used by setattr                   */
        struct irp_write   write;   /* Used by write                     */
        struct irp_create  create;  /* Used by create                    */
        struct irp_rename  rename;  /* Use by rename                     */
    } gen;                          /* Additional state data for some ops */
    void      *fuse_info;           /* Fuse info pointer for FUSE calls  */
    IRP       *next;                /* point to next IRP                 */
    IRP       *prev;                /* point to previous IRP             */
    int        scard_index;         /* used to smart card to locate dev  */

    void     (*callback)(struct stream *s, IRP *irp, tui32 DeviceId,
                         tui32 CompletionId, tui32 IoStatus);
    void      *user_data;
};

IRP *devredir_irp_new(void);
/* As above, but allocates sufficent space for the specified
 * pathname, and copies it in to the pathname field */
IRP *devredir_irp_with_pathname_new(const char *pathname);
/* As above, but specifies a pathname length with pathname
 * initially set to "". Use if you need to modify the pathname
 * significantly */
IRP *devredir_irp_with_pathnamelen_new(unsigned int pathnamelen);
int   devredir_irp_delete(IRP *irp);
IRP *devredir_irp_find(tui32 completion_id);
IRP *devredir_irp_find_by_fileid(tui32 FileId);
IRP *devredir_irp_get_last(void);
void  devredir_irp_dump(void);

#endif /* end ifndef __IRP_H */
