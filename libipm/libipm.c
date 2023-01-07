/**
 * Copyright (C) 2022 Matt Burt, all xrdp contributors
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

/**
 * @file    libipm/libipm.c
 * @brief   Inter-Process Messaging building and parsing definitions
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "libipm.h"
#include "libipm_private.h"
#include "libipm_facilities.h"
#include "trans.h"
#include "log.h"
#include "os_calls.h"

const char *libipm_valid_type_chars = "ybnqiuxtsdhogB";

/**************************************************************************//**
 * Send function for a struct trans initialised with libipm_init_trans()
 *
 * @param trans Transport to send on
 * @param data pointer to data to send
 * @param len Length of data to send
 * @return As for write(2)
 */
static int
libipm_trans_send_proc(struct trans *self, const char *data, int len)
{
    int rv;
    struct libipm_priv *priv = (struct libipm_priv *)self->extra_data;
    if (priv != NULL && data == self->out_s->data)
    {
        /* We're sending the message header. Send any file descriptors
         * as ancillary data */
        rv = g_sck_send_fd_set(self->sck, data, len,
                               priv->out_fds, priv->out_fd_count);
    }
    else
    {
        rv = g_sck_send(self->sck, data, len, 0);
    }

    return rv;
}


/**************************************************************************//**
 * Destructor for a struct trans initialised with libipm_init_trans()
 *
 * @param trans Transport to destroy
 */
static void
libipm_trans_destructor(struct trans *trans)
{
    struct libipm_priv *priv = (struct libipm_priv *)trans->extra_data;

    if (priv != NULL)
    {
        if ((priv->flags & LIBIPM_E_MSG_IN_ERASE_AFTER_USE) != 0 &&
                trans->in_s->data != NULL)
        {
            g_memset(trans->in_s->data, '\0',
                     trans->in_s->end - trans->in_s->data);
        }

        g_free(priv);
        trans->extra_data = NULL;
        trans->extra_destructor = NULL;
    }
}

/*****************************************************************************/

enum libipm_status
libipm_init_trans(struct trans *trans,
                  enum libipm_facility facility,
                  const char *(*msgno_to_str)(unsigned short msgno))
{
    struct libipm_priv *priv;
    enum libipm_status rv;

    if (trans->extra_data != NULL || trans->extra_destructor != NULL)
    {
        LOG(LOG_LEVEL_ERROR, "%s() called with sub-classed transport",
            __func__);
        rv = E_LI_PROGRAM_ERROR;
    }
    else if ((priv = g_new0(struct libipm_priv, 1)) == 0)
    {
        LOG(LOG_LEVEL_ERROR, "%s() out of memory", __func__);
        rv = E_LI_NO_MEMORY;
    }
    else
    {
        priv->facility = facility;
        priv->msgno_to_str = msgno_to_str;

        trans->trans_send = libipm_trans_send_proc;
        trans->extra_data = priv;
        trans->extra_destructor = libipm_trans_destructor;

        g_sck_set_non_blocking(trans->sck);

        libipm_msg_in_reset(trans);

        rv = E_LI_SUCCESS;
    }

    return rv;
}

/*****************************************************************************/

void
libipm_set_flags(struct trans *trans, unsigned int flags)
{
    struct libipm_priv *priv = (struct libipm_priv *)trans->extra_data;

    if (priv != NULL)
    {
        priv->flags |= flags;
    }
}

/*****************************************************************************/

void
libipm_clear_flags(struct trans *trans, unsigned int flags)
{
    struct libipm_priv *priv = (struct libipm_priv *)trans->extra_data;

    if (priv != NULL)
    {
        priv->flags &= ~flags;
    }
}
