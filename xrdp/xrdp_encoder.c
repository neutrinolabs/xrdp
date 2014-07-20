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

#ifdef XRDP_RFXCODEC
#include "rfxcodec_encode.h"
#endif

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

#define JPG_CODEC 0
#define RFX_CODEC 1

/*****************************************************************************/
static int
process_enc_jpg(struct xrdp_mm *self, XRDP_ENC_DATA *enc);
static int
process_enc_rfx(struct xrdp_mm *self, XRDP_ENC_DATA *enc);

/**
 * Init encoder
 *
 * @return 0 on success, -1 on failure
 *****************************************************************************/
/* called from main thread */
int APP_CC
init_xrdp_encoder(struct xrdp_mm *self)
{
    char buf[1024];
    int pid;

    if (self == 0)
    {
        return -1;
    }

    LLOGLN(0, ("init_xrdp_encoder: initing encoder codec_id %d", self->codec_id));

    /* setup required FIFOs */
    self->fifo_to_proc = fifo_create();
    self->fifo_processed = fifo_create();
    self->mutex = tc_mutex_create();

    pid = g_getpid();
    /* setup wait objects for signalling */
    g_snprintf(buf, 1024, "xrdp_%8.8x_encoder_event_to_proc", pid);
    self->xrdp_encoder_event_to_proc = g_create_wait_obj(buf);
    g_snprintf(buf, 1024, "xrdp_%8.8x_encoder_event_processed", pid);
    self->xrdp_encoder_event_processed = g_create_wait_obj(buf);
    g_snprintf(buf, 1024, "xrdp_%8.8x_encoder_term", pid);
    self->xrdp_encoder_term = g_create_wait_obj(buf);

    switch (self->codec_id)
    {
        case 2:
            self->process_enc = process_enc_jpg;
            break;
        case 3:
            self->process_enc = process_enc_rfx;
#ifdef XRDP_RFXCODEC
            self->codec_handle =
                    rfxcodec_encode_create(self->wm->screen->width,
                                           self->wm->screen->height,
                                           RFX_FORMAT_YUV, 0);
                                           //RFX_FORMAT_BGRA, 0);
#endif
            break;
        default:
            LLOGLN(0, ("init_xrdp_encoder: unknown codec_id %d",
                   self->codec_id));
            break;

    }

    /* create thread to process messages */
    tc_thread_create(proc_enc_msg, self);

    return 0;
}

/**
 * Deinit xrdp encoder
 *****************************************************************************/
/* called from main thread */
void APP_CC
deinit_xrdp_encoder(struct xrdp_mm *self)
{
    XRDP_ENC_DATA *enc;
    XRDP_ENC_DATA_DONE *enc_done;
    FIFO          *fifo;

    LLOGLN(0, ("deinit_xrdp_encoder: deiniting encoder"));

    if (self == 0)
    {
        return;
    }

    if (self->in_codec_mode == 0)
    {
        return;
    }
    /* tell worker thread to shut down */
    g_set_wait_obj(self->xrdp_encoder_term);
    g_sleep(1000);

    if (self->codec_id == 3)
    {
#ifdef XRDP_RFXCODEC
        rfxcodec_encode_destroy(self->codec_handle);
#endif
    }

    /* destroy wait objects used for signalling */
    g_delete_wait_obj(self->xrdp_encoder_event_to_proc);
    g_delete_wait_obj(self->xrdp_encoder_event_processed);
    g_delete_wait_obj(self->xrdp_encoder_term);

    /* cleanup fifo_to_proc */
    fifo = self->fifo_to_proc;
    if (fifo)
    {
        while (!fifo_is_empty(fifo))
        {
            enc = fifo_remove_item(fifo);
            if (enc == 0)
            {
                continue;
            }
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
            enc_done = fifo_remove_item(fifo);
            if (enc == 0)
            {
                continue;
            }
            g_free(enc_done->comp_pad_data);
            g_free(enc_done);
        }
        fifo_delete(fifo);
    }
}

/*****************************************************************************/
/* called from encoder thread */
static int
process_enc_jpg(struct xrdp_mm *self, XRDP_ENC_DATA *enc)
{
    int                  index;
    int                  x;
    int                  y;
    int                  cx;
    int                  cy;
    int                  quality;
    int                  error;
    int                  out_data_bytes;
    int                  count;
    char                *out_data;
    XRDP_ENC_DATA_DONE  *enc_done;
    FIFO                *fifo_processed;
    tbus                 mutex;
    tbus                 event_processed;

    LLOGLN(10, ("process_enc_jpg:"));
    quality = self->codec_quality;
    fifo_processed = self->fifo_processed;
    mutex = self->mutex;
    event_processed = self->xrdp_encoder_event_processed;
    count = enc->num_crects;
    for (index = 0; index < count; index++)
    {
        x = enc->crects[index * 4 + 0];
        y = enc->crects[index * 4 + 1];
        cx = enc->crects[index * 4 + 2];
        cy = enc->crects[index * 4 + 3];
        if (cx < 1 || cy < 1)
        {
            LLOGLN(0, ("process_enc_jpg: error 1"));
            continue;
        }

        LLOGLN(10, ("process_enc_jpg: x %d y %d cx %d cy %d", x, y, cx, cy));

        out_data_bytes = MAX((cx + 4) * cy * 4, 8192);
        if ((out_data_bytes < 1) || (out_data_bytes > 16 * 1024 * 1024))
        {
            LLOGLN(0, ("process_enc_jpg: error 2"));
            return 1;
        }
        out_data = (char *) g_malloc(out_data_bytes + 256 + 2, 0);
        if (out_data == 0)
        {
            LLOGLN(0, ("process_enc_jpg: error 3"));
            return 1;
        }
        out_data[256] = 0; /* header bytes */
        out_data[257] = 0;
        error = libxrdp_codec_jpeg_compress(self->wm->session, 0, enc->data,
                                            enc->width, enc->height,
                                            enc->width * 4, x, y, cx, cy,
                                            quality,
                                            out_data + 256 + 2, &out_data_bytes);
        if (error < 0)
        {
            LLOGLN(0, ("process_enc_jpg: jpeg error %d bytes %d",
                   error, out_data_bytes));
            g_free(out_data);
            return 1;
        }
        LLOGLN(10, ("jpeg error %d bytes %d", error, out_data_bytes));
        enc_done = (XRDP_ENC_DATA_DONE *)
                   g_malloc(sizeof(XRDP_ENC_DATA_DONE), 1);
        enc_done->comp_bytes = out_data_bytes + 2;
        enc_done->pad_bytes = 256;
        enc_done->comp_pad_data = out_data;
        enc_done->enc = enc;
        enc_done->last = index == (enc->num_crects - 1);
        enc_done->x = x;
        enc_done->y = y;
        enc_done->cx = cx;
        enc_done->cy = cy;
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

#ifdef XRDP_RFXCODEC

/*****************************************************************************/
/* called from encoder thread */
static int
process_enc_rfx(struct xrdp_mm *self, XRDP_ENC_DATA *enc)
{
    int index;
    int x;
    int y;
    int cx;
    int cy;
    int out_data_bytes;
    int count;
    int error;
    char *out_data;
    XRDP_ENC_DATA_DONE *enc_done;
    FIFO *fifo_processed;
    tbus mutex;
    tbus event_processed;
    struct rfx_tile *tiles;
    struct rfx_rect *rfxrects;

    LLOGLN(10, ("process_enc_rfx:"));
    LLOGLN(10, ("process_enc_rfx: num_crects %d num_drects %d",
           enc->num_crects, enc->num_drects));
    fifo_processed = self->fifo_processed;
    mutex = self->mutex;
    event_processed = self->xrdp_encoder_event_processed;

    if ((enc->num_crects > 512) || (enc->num_drects > 512))
    {
        return 0;
    }

    out_data_bytes = 16 * 1024 * 1024;
    index = 256 + sizeof(struct rfx_tile) * 512 +
                  sizeof(struct rfx_rect) * 512;
    out_data = (char *) g_malloc(out_data_bytes + index, 0);
    if (out_data == 0)
    {
        return 0;
    }
    tiles = (struct rfx_tile *) (out_data + out_data_bytes + 256);
    rfxrects = (struct rfx_rect *) (tiles + 512);

    count = enc->num_crects;
    for (index = 0; index < count; index++)
    {
        x = enc->crects[index * 4 + 0];
        y = enc->crects[index * 4 + 1];
        cx = enc->crects[index * 4 + 2];
        cy = enc->crects[index * 4 + 3];
        LLOGLN(10, ("process_enc_rfx:"));
        tiles[index].x = x;
        tiles[index].y = y;
        tiles[index].cx = cx;
        tiles[index].cy = cy;
        LLOGLN(10, ("x %d y %d cx %d cy %d", x, y, cx, cy));
        tiles[index].quant_y = 0;
        tiles[index].quant_cb = 0;
        tiles[index].quant_cr = 0;
    }

    count = enc->num_drects;
    for (index = 0; index < count; index++)
    {
        x = enc->drects[index * 4 + 0];
        y = enc->drects[index * 4 + 1];
        cx = enc->drects[index * 4 + 2];
        cy = enc->drects[index * 4 + 3];
        LLOGLN(10, ("process_enc_rfx:"));
        rfxrects[index].x = x;
        rfxrects[index].y = y;
        rfxrects[index].cx = cx;
        rfxrects[index].cy = cy;
    }

    error = rfxcodec_encode(self->codec_handle, out_data + 256, &out_data_bytes,
                            enc->data, enc->width, enc->height, enc->width * 4,
                            rfxrects, enc->num_drects,
                            tiles, enc->num_crects, 0, 0);
    LLOGLN(10, ("process_enc_rfx: rfxcodec_encode rv %d", error));

    enc_done = (XRDP_ENC_DATA_DONE *)
               g_malloc(sizeof(XRDP_ENC_DATA_DONE), 1);
    enc_done->comp_bytes = out_data_bytes;
    enc_done->pad_bytes = 256;
    enc_done->comp_pad_data = out_data;
    enc_done->enc = enc;
    enc_done->last = 1;
    enc_done->cx = self->wm->screen->width;
    enc_done->cy = self->wm->screen->height;

    /* done with msg */
    /* inform main thread done */
    tc_mutex_lock(mutex);
    fifo_add_item(fifo_processed, enc_done);
    tc_mutex_unlock(mutex);
    /* signal completion for main thread */
    g_set_wait_obj(event_processed);

    return 0;
}

#else

/*****************************************************************************/
/* called from encoder thread */
static int
process_enc_rfx(struct xrdp_mm *self, XRDP_ENC_DATA *enc)
{
    return 0;
}

#endif

/**
 * Encoder thread main loop
 *****************************************************************************/
THREAD_RV THREAD_CC
proc_enc_msg(void *arg)
{
    XRDP_ENC_DATA  *enc;
    FIFO           *fifo_to_proc;
    tbus            mutex;
    tbus            event_to_proc;
    tbus            term_obj;
    tbus            lterm_obj;
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
    lterm_obj = self->xrdp_encoder_term;

    cont = 1;
    while (cont)
    {
        timeout = -1;
        robjs_count = 0;
        wobjs_count = 0;
        robjs[robjs_count++] = term_obj;
        robjs[robjs_count++] = lterm_obj;
        robjs[robjs_count++] = event_to_proc;

        if (g_obj_wait(robjs, robjs_count, wobjs, wobjs_count, timeout) != 0)
        {
            /* error, should not get here */
            g_sleep(100);
        }

        if (g_is_wait_obj_set(term_obj)) /* global term */
        {
            LLOGLN(0, ("proc_enc_msg: global term"));
            break;
        }

        if (g_is_wait_obj_set(lterm_obj)) /* xrdp_mm term */
        {
            LLOGLN(0, ("proc_enc_msg: xrdp_mm term"));
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
                self->process_enc(self, enc);
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
