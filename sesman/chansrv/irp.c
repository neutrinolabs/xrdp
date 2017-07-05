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

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "chansrv.h"
#include "parse.h"
#include "os_calls.h"
#include "irp.h"

IRP *g_irp_head = NULL;

/**
 * Create a new IRP and append to linked list
 *
 * @return new IRP or NULL on error
 *****************************************************************************/

IRP * devredir_irp_new(void)
{
    IRP *irp;
    IRP *irp_last;

    LOGM((LOG_LEVEL_DEBUG, "entered"));

    /* create new IRP */
    irp = g_new0(IRP, 1);
    if (irp == NULL)
    {
        LOGM((LOG_LEVEL_ERROR, "system out of memory!"));
        return NULL;
    }

    /* insert at end of linked list */
    if ((irp_last = devredir_irp_get_last()) == NULL)
    {
        /* list is empty, this is the first entry */
        g_irp_head = irp;
    }
    else
    {
        irp_last->next = irp;
        irp->prev = irp_last;
    }

    LOGM((LOG_LEVEL_DEBUG, "new IRP=%p", irp));
    return irp;
}

/**
 * Clone specified IRP
 *
 * @return new IRP or NULL on error
 *****************************************************************************/

IRP * devredir_irp_clone(IRP *irp)
{
    IRP *new_irp;
    IRP *prev;
    IRP *next;

    if ((new_irp = devredir_irp_new()) == NULL)
        return NULL;

    /* save link pointers */
    prev = new_irp->prev;
    next = new_irp->next;

    /* copy all members */
    g_memcpy(new_irp, irp, sizeof(IRP));

    /* restore link pointers */
    new_irp->prev = prev;
    new_irp->next = next;

    return new_irp;
}

/**
 * Delete specified IRP from linked list
 *
 * @return 0 on success, -1 on failure
 *****************************************************************************/

int devredir_irp_delete(IRP *irp)
{
    IRP *lirp = g_irp_head;

    if ((irp == NULL) || (lirp == NULL))
        return -1;

    LOGM((LOG_LEVEL_DEBUG, "irp=%p completion_id=%d type=%d",
              irp, irp->CompletionId, irp->completion_type));

    devredir_irp_dump(); // LK_TODO

    while (lirp)
    {
        if (lirp == irp)
            break;

        lirp = lirp->next;
    }

    if (lirp == NULL)
        return -1; /* did not find specified irp */

    if (lirp->prev == NULL)
    {
        /* we are at head of linked list */
        if (lirp->next == NULL)
        {
            /* only one element in list */
            g_free(lirp);
            g_irp_head = NULL;
            devredir_irp_dump(); // LK_TODO
            return 0;
        }

        lirp->next->prev = NULL;
        g_irp_head = lirp->next;
        g_free(lirp);
    }
    else if (lirp->next == NULL)
    {
        /* we are at tail of linked list */
        lirp->prev->next = NULL;
        g_free(lirp);
    }
    else
    {
        /* we are in between */
        lirp->prev->next = lirp->next;
        lirp->next->prev = lirp->prev;
        g_free(lirp);
    }

    devredir_irp_dump(); // LK_TODO

    return 0;
}

/**
 * Return IRP containing specified completion_id
 *****************************************************************************/

IRP *devredir_irp_find(tui32 completion_id)
{
    IRP *irp = g_irp_head;

    while (irp)
    {
        if (irp->CompletionId == completion_id)
        {
            LOGM((LOG_LEVEL_DEBUG, "returning irp=%p", irp));
            return irp;
        }

        irp = irp->next;
    }

    LOGM((LOG_LEVEL_DEBUG, "returning irp=NULL"));
    return NULL;
}

IRP * devredir_irp_find_by_fileid(tui32 FileId)
{
    IRP *irp = g_irp_head;

    while (irp)
    {
        if (irp->FileId == FileId)
        {
            LOGM((LOG_LEVEL_DEBUG, "returning irp=%p", irp));
            return irp;
        }

        irp = irp->next;
    }

    LOGM((LOG_LEVEL_DEBUG, "returning irp=NULL"));
    return NULL;
}

/**
 * Return last IRP in linked list
 *****************************************************************************/

IRP * devredir_irp_get_last(void)
{
    IRP *irp = g_irp_head;

    while (irp)
    {
        if (irp->next == NULL)
            break;

        irp = irp->next;
    }

    LOGM((LOG_LEVEL_DEBUG, "returning irp=%p", irp));
    return irp;
}

void devredir_irp_dump(void)
{
    IRP *irp = g_irp_head;

    LOGM((LOG_LEVEL_DEBUG, "------- dumping IRPs --------"));
    while (irp)
    {
        LOGM((LOG_LEVEL_DEBUG, "        completion_id=%d\tcompletion_type=%d\tFileId=%d",
                  irp->CompletionId, irp->completion_type, irp->FileId));

        irp = irp->next;
    }
    LOGM((LOG_LEVEL_DEBUG, "------- dumping IRPs done ---"));
}
