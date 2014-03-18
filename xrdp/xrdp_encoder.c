/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Laxmikant Rashinkar 2004-2014
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
 * Encoder
 */

#include "xrdp_encoder.h"
#include "xrdp.h"
#include "thread_calls.h"
#include "fifo.h"

#define LLOG_LEVEL 1
#define LLOGLN(_level, _args) \
  do \
  { \
    if (_level < LLOG_LEVEL) \
    { \
        g_write("xrdp:xrdp_encoder [%10.10u]: ", g_time3()); \
        g_writeln _args ; \
    } \
  } \
  while (0)

/**
 * Init encoder
 *
 * @return 0 on success, -1 on failure
 *****************************************************************************/

int APP_CC
init_xrdp_encoder(struct xrdp_mm *self)
{
    char buf[1024];

    LLOGLN(0, ("init_xrdp_encoder: initing encoder"));

    if (self == 0)
    {
        return -1;
    }

    /* setup required FIFOs */
    self->fifo_to_proc = fifo_create();
    self->fifo_processed = fifo_create();
    self->mutex = tc_mutex_create();

    /* setup wait objects for signalling */
    g_snprintf(buf, 1024, "xrdp_encoder_%8.8x", g_getpid());
    self->xrdp_encoder_event_to_proc = g_create_wait_obj(buf);
    self->xrdp_encoder_event_processed = g_create_wait_obj(buf);

    /* create thread to process messages */
    tc_thread_create(proc_enc_msg, self);

    return 0;
}

/**
 * Deinit xrdp encoder
 *****************************************************************************/

void APP_CC
deinit_xrdp_encoder(struct xrdp_mm *self)
{
    XRDP_ENC_DATA *enc;
    FIFO          *fifo;

    LLOGLN(0, ("deinit_xrdp_encoder: deiniting encoder"));

    if (self == 0)
    {
        return;
    }

    /* destroy wait objects used for signalling */
    g_delete_wait_obj(self->xrdp_encoder_event_to_proc);
    g_delete_wait_obj(self->xrdp_encoder_event_processed);

    /* cleanup fifo_to_proc */
    fifo = self->fifo_to_proc;
    if (fifo)
    {
        while (!fifo_is_empty(fifo))
        {
            enc = fifo_remove_item(fifo);
            if (!enc)
                continue;

            g_free(enc->drects);
            g_free(enc->crects);
            g_free(enc);
        }

        fifo_delete(fifo);
    }

    /* cleanup fifo_processed */
    fifo = self->fifo_processed;
    if (fifo)
    {
        while (!fifo_is_empty(fifo))
        {
            enc = fifo_remove_item(fifo);
            if (!enc)
            {
                continue;
            }
            g_free(enc->drects);
            g_free(enc->crects);
            g_free(enc);
        }
        fifo_delete(fifo);
    }
}

/*****************************************************************************/
static int
process_enc(struct xrdp_mm *self, XRDP_ENC_DATA *enc)
{
    int                  index;
    int                  x;
    int                  y;
    int                  cx;
    int                  cy;
    int                  quality;
    int                  error;
    int                  out_data_bytes;
    char                *out_data;
    XRDP_ENC_DATA_DONE  *enc_done;
    FIFO                *fifo_processed;
    tbus                 mutex;
    tbus                 event_processed;

    LLOGLN(10, ("process_enc:"));
    quality = 75;
    fifo_processed = self->fifo_processed;
    mutex = self->mutex;
    event_processed = self->xrdp_encoder_event_processed;
    for (index = 0; index < enc->num_crects; index++)
    {
        x = enc->crects[index * 4 + 0];
        y = enc->crects[index * 4 + 1];
        cx = enc->crects[index * 4 + 2];
        cy = enc->crects[index * 4 + 3];
        out_data_bytes = MAX((cx + 4) * cy * 4, 8192);
        if ((out_data_bytes < 1) || (out_data_bytes > 16 * 1024 * 1024))
        {
            LLOGLN(0, ("process_enc: error"));
            return 1;
        }
        out_data = (char *) g_malloc(out_data_bytes + 256, 0);
        if (out_data == 0)
        {
            LLOGLN(0, ("process_enc: error"));
            return 1;
        }
        error = libxrdp_codec_jpeg_compress(self->wm->session, 0, enc->data,
                                            enc->width, enc->height,
                                            enc->width * 4, x, y, cx, cy,
                                            quality,
                                            out_data + 256, &out_data_bytes);
        if (error < 0)
        {
            LLOGLN(0, ("process_enc: jpeg error %d bytes %d",
                   error, out_data_bytes));
            g_free(out_data);
            return 1;
        }
        LLOGLN(10, ("jpeg error %d bytes %d", error, out_data_bytes));
        enc_done = (XRDP_ENC_DATA_DONE *)
                   g_malloc(sizeof(XRDP_ENC_DATA_DONE), 1);
        enc_done->comp_bytes = out_data_bytes;
        enc_done->pad_bytes = 256;
        enc_done->comp_pad_data = out_data;
        enc_done->enc = enc;
        enc_done->last = index == (enc->num_crects - 1);
        enc_done->index = index;
        /* done with msg */
        /* inform main thread done */
        tc_mutex_lock(mutex);
        fifo_add_item(fifo_processed, enc_done);
        tc_mutex_unlock(mutex);
        /* signal completion for main thread */
        g_set_wait_obj(event_processed);
    }
    return 0;
}

/**
 * Init encoder
 *
 * @return 0 on success, -1 on failure
 *****************************************************************************/
THREAD_RV THREAD_CC
proc_enc_msg(void *arg)
{
    XRDP_ENC_DATA  *enc;
    FIFO           *fifo_to_proc;
    tbus            mutex;
    tbus            event_to_proc;
    tbus            term_obj;
    int             robjs_count;
    int             wobjs_count;
    int             cont;
    int             timeout;
    tbus            robjs[32];
    tbus            wobjs[32];
    struct xrdp_mm *self;

    LLOGLN(0, ("proc_enc_msg: thread is running"));

    self = (struct xrdp_mm *) arg;
    if (self == 0)
    {
        LLOGLN(0, ("proc_enc_msg: self nil"));
        return 0;
    }

    fifo_to_proc = self->fifo_to_proc;
    mutex = self->mutex;
    event_to_proc = self->xrdp_encoder_event_to_proc;

    term_obj = g_get_term_event();

    cont = 1;
    while (cont)
    {
        timeout = -1;
        robjs_count = 0;
        wobjs_count = 0;
        robjs[robjs_count++] = term_obj;
        robjs[robjs_count++] = event_to_proc;

        if (g_obj_wait(robjs, robjs_count, wobjs, wobjs_count, timeout) != 0)
        {
            /* error, should not get here */
            g_sleep(100);
        }

        if (g_is_wait_obj_set(term_obj)) /* term */
        {
            break;
        }

        if (g_is_wait_obj_set(event_to_proc))
        {
            /* clear it right away */
            g_reset_wait_obj(event_to_proc);
            /* get first msg */
            tc_mutex_lock(mutex);
            enc = (XRDP_ENC_DATA *) fifo_remove_item(fifo_to_proc);
            tc_mutex_unlock(mutex);
            while (enc != 0)
            {
                /* do work */
                process_enc(self, enc);
                /* get next msg */
                tc_mutex_lock(mutex);
                enc = (XRDP_ENC_DATA *) fifo_remove_item(fifo_to_proc);
                tc_mutex_unlock(mutex);
            }
        }

    } /* end while (cont) */
    LLOGLN(0, ("proc_enc_msg: thread exit"));
    return 0;
}
