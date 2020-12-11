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

/* FIFO implementation to store a pointer to a user struct */

/* module based logging */
#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#define MODULE_NAME "FIFO      "
#define LOCAL_DEBUG

#include "chansrv.h"
#include "fifo.h"
#include "os_calls.h"

/**
 * Initialize a FIFO that grows as required
 *
 * @param  fp           pointer to a FIFO
 * @param  num_entries  initial size
 *
 * @return 0 on success, -1 on failure
 *****************************************************************************/
int
fifo_init(FIFO *fp, int num_entries)
{
    LOG_DEVEL(LOG_LEVEL_TRACE, "entered");

    /* validate params */
    if (!fp)
    {
        LOG_DEVEL(LOG_LEVEL_TRACE, "invalid parameters");
        return -1;
    }

    if (num_entries < 1)
    {
        num_entries = 10;
    }

    fp->rd_ptr = 0;
    fp->wr_ptr = 0;
    fp->user_data = (long *) g_malloc(sizeof(long) * num_entries, 1);

    if (fp->user_data)
    {
        fp->entries = num_entries;
        LOG_DEVEL(LOG_LEVEL_DEBUG, "FIFO created; rd_ptr=%d wr_ptr=%d entries=%d",
                  fp->rd_ptr, fp->wr_ptr, fp->entries);
        return 0;
    }
    else
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "FIFO create error; system out of memory");
        fp->entries = 0;
        return -1;
    }
}

/**
 * Deinit FIFO and release resources
 *
 * @param  fp  FIFO to deinit
 *
 * @return 0 on success, -1 on error
 *****************************************************************************/
int
fifo_deinit(FIFO *fp)
{
    LOG_DEVEL(LOG_LEVEL_TRACE, "entered");

    if (!fp)
    {
        LOG_DEVEL(LOG_LEVEL_TRACE, "FIFO is null");
        return -1;
    }

    if (fp->user_data)
    {
        g_free(fp->user_data);
        fp->user_data = 0;
    }

    fp->rd_ptr = 0;
    fp->wr_ptr = 0;
    fp->entries = 0;
    return 0;
}

/**
 * Check if FIFO is empty
 *
 * @param  fp  FIFO
 *
 * @return 1 if FIFO is empty, 0 otherwise
 *****************************************************************************/
int
fifo_is_empty(FIFO *fp)
{
    LOG_DEVEL(LOG_LEVEL_TRACE, "entered");

    if (!fp)
    {
        LOG_DEVEL(LOG_LEVEL_TRACE, "FIFO is null");
        return 0;
    }

    return (fp->rd_ptr == fp->wr_ptr) ? 1 : 0;
}

/**
 * Insert an item at the end
 *
 * @param  fp     FIFO
 * @param  data  data to insert into FIFO
 *
 * @param 0 on success, -1 on error
 *****************************************************************************/

int
fifo_insert(FIFO *fp, void *data)
{
    long *lp;
    int   next_val; /* next value for wr_ptr */
    int   i;

    LOG_DEVEL(LOG_LEVEL_TRACE, "entered");

    if (!fp)
    {
        LOG_DEVEL(LOG_LEVEL_TRACE, "FIFO is null");
        return -1;
    }

    next_val = fp->wr_ptr + 1;
    if (next_val >= fp->entries)
    {
        next_val = 0;
    }

    if (next_val == fp->rd_ptr)
    {
        /* FIFO is full, expand it by 10 entries */
        lp = (long *) g_malloc(sizeof(long) * (fp->entries + 10), 1);
        if (!lp)
        {
            LOG_DEVEL(LOG_LEVEL_DEBUG, "FIFO full; cannot expand, no memory");
            return -1;
        }

        LOG_DEVEL(LOG_LEVEL_DEBUG, "FIFO full, expanding by 10 entries");

        /* copy old data new location */
        for (i = 0; i < (fp->entries - 1); i++)
        {
            lp[i] = fp->user_data[fp->rd_ptr++];
            if (fp->rd_ptr >= fp->entries)
            {
                fp->rd_ptr = 0;
            }
        }

        /* update pointers */
        fp->rd_ptr = 0;
        fp->wr_ptr = fp->entries - 1;
        next_val = fp->entries;
        fp->entries += 10;

        /* free old data */
        g_free(fp->user_data);
        fp->user_data = lp;
    }

    LOG_DEVEL(LOG_LEVEL_DEBUG, "inserting data at index %d", fp->wr_ptr);

    fp->user_data[fp->wr_ptr] = (long) data;
    fp->wr_ptr = next_val;

    return 0;
}

/**
 * Remove an item from the head
 *
 * @param  fp     FIFO
 *
 * @param data on success, NULL on error
 *****************************************************************************/
void *
fifo_remove(FIFO *fp)
{
    long data;

    LOG_DEVEL(LOG_LEVEL_TRACE, "entered");

    if (!fp)
    {
        LOG_DEVEL(LOG_LEVEL_TRACE, "FIFO is null");
        return 0;
    }

    if (fp->rd_ptr == fp->wr_ptr)
    {
        LOG_DEVEL(LOG_LEVEL_TRACE, "FIFO is empty");
        return 0;
    }

    LOG_DEVEL(LOG_LEVEL_DEBUG, "removing data at index  %d", fp->rd_ptr);

    data = fp->user_data[fp->rd_ptr++];

    if (fp->rd_ptr >= fp->entries)
    {
        LOG_DEVEL(LOG_LEVEL_TRACE, "FIFO rd_ptr wrapped around");
        fp->rd_ptr = 0;
    }

    return (void *) data;
}

/**
 * Return item from head, but do not remove it
 *
 * @param  fp     FIFO
 *
 * @param data on success, NULL on error
 *****************************************************************************/
void *
fifo_peek(FIFO *fp)
{
    LOG_DEVEL(LOG_LEVEL_TRACE, "entered");

    if (!fp)
    {
        LOG_DEVEL(LOG_LEVEL_TRACE, "FIFO is null");
        return 0;
    }

    if (fp->rd_ptr == fp->wr_ptr)
    {
        LOG_DEVEL(LOG_LEVEL_TRACE, "FIFO is empty");
        return 0;
    }

    LOG_DEVEL(LOG_LEVEL_DEBUG, "peeking data at index %d", fp->rd_ptr);

    return (void *) fp->user_data[fp->rd_ptr];
}
