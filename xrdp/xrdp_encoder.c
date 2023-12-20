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

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "xrdp_encoder.h"
#include "xrdp.h"
#include "ms-rdpbcgr.h"
#include "thread_calls.h"
#include "fifo.h"

#ifdef XRDP_RFXCODEC
#include "rfxcodec_encode.h"
#endif

#define XRDP_SURCMD_PREFIX_BYTES 256

#ifdef XRDP_RFXCODEC
/* LH3 LL3, HH3 HL3, HL2 LH2, LH1 HH2, HH1 HL1 todo check this */
static const unsigned char g_rfx_quantization_values[] =
{
    0x66, 0x66, 0x77, 0x88, 0x98,
    0x76, 0x77, 0x88, 0x98, 0xA9
};
#endif

/*****************************************************************************/
static int
process_enc_jpg(struct xrdp_encoder *self, XRDP_ENC_DATA *enc);
#ifdef XRDP_RFXCODEC
static int
process_enc_rfx(struct xrdp_encoder *self, XRDP_ENC_DATA *enc);
#endif
static int
process_enc_h264(struct xrdp_encoder *self, XRDP_ENC_DATA *enc);

/*****************************************************************************/
/* Item destructor for self->fifo_to_proc */
static void
xrdp_enc_data_destructor(void *item, void *closure)
{
    XRDP_ENC_DATA *enc = (XRDP_ENC_DATA *)item;
    g_free(enc->drects);
    g_free(enc->crects);
    g_free(enc);
}

/* Item destructor for self->fifo_processed */
static void
xrdp_enc_data_done_destructor(void *item, void *closure)
{
    XRDP_ENC_DATA_DONE *enc_done = (XRDP_ENC_DATA_DONE *)item;
    g_free(enc_done->comp_pad_data);
    g_free(enc_done);
}

/*****************************************************************************/
struct xrdp_encoder *
xrdp_encoder_create(struct xrdp_mm *mm)
{
    struct xrdp_encoder *self;
    struct xrdp_client_info *client_info;
    char buf[1024];
    int pid;

    client_info = mm->wm->client_info;

    /* RemoteFX 7.1 requires LAN but GFX does not */
    if (client_info->mcs_connection_type != CONNECTION_TYPE_LAN)
    {
        if ((mm->egfx_flags & (XRDP_EGFX_H264 | XRDP_EGFX_RFX_PRO)) == 0)
        {
            return 0;
        }
    }
    if (client_info->bpp < 24)
    {
        return 0;
    }

    self = g_new0(struct xrdp_encoder, 1);
    if (self == NULL)
    {
        return NULL;
    }
    self->mm = mm;

    if (client_info->jpeg_codec_id != 0)
    {
        LOG_DEVEL(LOG_LEVEL_INFO, "xrdp_encoder_create: starting jpeg codec session");
        self->codec_id = client_info->jpeg_codec_id;
        self->in_codec_mode = 1;
        self->codec_quality = client_info->jpeg_prop[0];
        client_info->capture_code = 0;
        client_info->capture_format = XRDP_a8b8g8r8;
        self->process_enc = process_enc_jpg;
    }
#ifdef XRDP_RFXCODEC
    else if (mm->egfx_flags & XRDP_EGFX_RFX_PRO)
    {
        LOG(LOG_LEVEL_INFO,
            "xrdp_encoder_create: starting gfx rfx pro codec session");
        self->in_codec_mode = 1;
        client_info->capture_code = 2;
        self->process_enc = process_enc_rfx;
        self->gfx = 1;
        self->quants = (const char *) g_rfx_quantization_values;
        self->num_quants = 2;
        self->quant_idx_y = 0;
        self->quant_idx_u = 1;
        self->quant_idx_v = 1;
        self->codec_handle = rfxcodec_encode_create(
                                 mm->wm->screen->width,
                                 mm->wm->screen->height,
                                 RFX_FORMAT_YUV,
                                 RFX_FLAGS_RLGR1 | RFX_FLAGS_PRO1);
    }
    else if (client_info->rfx_codec_id != 0)
    {
        LOG_DEVEL(LOG_LEVEL_INFO, "xrdp_encoder_create: starting rfx codec session");
        self->codec_id = client_info->rfx_codec_id;
        self->in_codec_mode = 1;
        client_info->capture_code = 2;
        self->process_enc = process_enc_rfx;
        self->codec_handle = rfxcodec_encode_create(mm->wm->screen->width,
                             mm->wm->screen->height,
                             RFX_FORMAT_YUV, 0);
    }
#endif
    else if (client_info->h264_codec_id != 0)
    {
        LOG_DEVEL(LOG_LEVEL_INFO, "xrdp_encoder_create: starting h264 codec session");
        self->codec_id = client_info->h264_codec_id;
        self->in_codec_mode = 1;
        client_info->capture_code = 3;
        client_info->capture_format = XRDP_nv12;
        self->process_enc = process_enc_h264;
    }
    else
    {
        g_free(self);
        return 0;
    }

    LOG_DEVEL(LOG_LEVEL_INFO,
              "init_xrdp_encoder: initializing encoder codec_id %d",
              self->codec_id);

    /* setup required FIFOs */
    self->fifo_to_proc = fifo_create(xrdp_enc_data_destructor);
    self->fifo_processed = fifo_create(xrdp_enc_data_done_destructor);
    self->mutex = tc_mutex_create();

    pid = g_getpid();
    /* setup wait objects for signalling */
    g_snprintf(buf, 1024, "xrdp_%8.8x_encoder_event_to_proc", pid);
    self->xrdp_encoder_event_to_proc = g_create_wait_obj(buf);
    g_snprintf(buf, 1024, "xrdp_%8.8x_encoder_event_processed", pid);
    self->xrdp_encoder_event_processed = g_create_wait_obj(buf);
    g_snprintf(buf, 1024, "xrdp_%8.8x_encoder_term", pid);
    self->xrdp_encoder_term = g_create_wait_obj(buf);
    if (client_info->gfx)
    {
        // Magic numbers... Why?
        self->frames_in_flight = 2;
        self->max_compressed_bytes = 3145728;
    }
    else
    {
        self->frames_in_flight = client_info->max_unacknowledged_frame_count;
        self->max_compressed_bytes = client_info->max_fastpath_frag_bytes & ~15;
    }
    /* make sure frames_in_flight is at least 1 */
    self->frames_in_flight = MAX(self->frames_in_flight, 1);

    /* create thread to process messages */
    tc_thread_create(proc_enc_msg, self);

    return self;
}

/*****************************************************************************/
void
xrdp_encoder_delete(struct xrdp_encoder *self)
{
    LOG_DEVEL(LOG_LEVEL_INFO, "xrdp_encoder_delete:");
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

    /* todo delete specific encoder */

    if (self->process_enc == process_enc_jpg)
    {
    }
#ifdef XRDP_RFXCODEC
    else if (self->process_enc == process_enc_rfx)
    {
        rfxcodec_encode_destroy(self->codec_handle);
    }
#endif

    /* destroy wait objects used for signalling */
    g_delete_wait_obj(self->xrdp_encoder_event_to_proc);
    g_delete_wait_obj(self->xrdp_encoder_event_processed);
    g_delete_wait_obj(self->xrdp_encoder_term);

    /* cleanup fifos */
    fifo_delete(self->fifo_to_proc, NULL);
    fifo_delete(self->fifo_processed, NULL);
    tc_mutex_delete(self->mutex);
    g_free(self);
}

/*****************************************************************************/
/* called from encoder thread */
static int
process_enc_jpg(struct xrdp_encoder *self, XRDP_ENC_DATA *enc)
{
    int index;
    int x;
    int y;
    int cx;
    int cy;
    int quality;
    int error;
    int out_data_bytes;
    int count;
    char *out_data;
    XRDP_ENC_DATA_DONE *enc_done;
    struct fifo *fifo_processed;
    tbus mutex;
    tbus event_processed;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "process_enc_jpg:");
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
            LOG_DEVEL(LOG_LEVEL_WARNING, "process_enc_jpg: error 1");
            continue;
        }

        LOG_DEVEL(LOG_LEVEL_DEBUG, "process_enc_jpg: x %d y %d cx %d cy %d", x, y, cx, cy);

        out_data_bytes = MAX((cx + 4) * cy * 4, 8192);
        if ((out_data_bytes < 1) || (out_data_bytes > 16 * 1024 * 1024))
        {
            LOG_DEVEL(LOG_LEVEL_ERROR, "process_enc_jpg: error 2");
            return 1;
        }
        out_data = (char *) g_malloc(out_data_bytes + 256 + 2, 0);
        if (out_data == 0)
        {
            LOG_DEVEL(LOG_LEVEL_ERROR, "process_enc_jpg: error 3");
            return 1;
        }

        out_data[256] = 0; /* header bytes */
        out_data[257] = 0;
        error = libxrdp_codec_jpeg_compress(self->mm->wm->session, 0, enc->data,
                                            enc->width, enc->height,
                                            enc->width * 4, x, y, cx, cy,
                                            quality,
                                            out_data + 256 + 2, &out_data_bytes);
        if (error < 0)
        {
            LOG_DEVEL(LOG_LEVEL_ERROR, "process_enc_jpg: jpeg error %d bytes %d",
                      error, out_data_bytes);
            g_free(out_data);
            return 1;
        }
        LOG_DEVEL(LOG_LEVEL_WARNING, "jpeg error %d bytes %d", error, out_data_bytes);
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
process_enc_rfx(struct xrdp_encoder *self, XRDP_ENC_DATA *enc)
{
    int index;
    int x;
    int y;
    int cx;
    int cy;
    int out_data_bytes;
    int count;
    int tiles_written;
    int all_tiles_written;
    int tiles_left;
    int finished;
    char *out_data;
    XRDP_ENC_DATA_DONE *enc_done;
    struct fifo *fifo_processed;
    tbus mutex;
    tbus event_processed;
    struct rfx_tile *tiles;
    struct rfx_rect *rfxrects;
    int alloc_bytes;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "process_enc_rfx:");
    LOG_DEVEL(LOG_LEVEL_DEBUG, "process_enc_rfx: num_crects %d num_drects %d",
              enc->num_crects, enc->num_drects);
    fifo_processed = self->fifo_processed;
    mutex = self->mutex;
    event_processed = self->xrdp_encoder_event_processed;

    all_tiles_written = 0;
    do
    {
        tiles_written = 0;
        tiles_left = enc->num_crects - all_tiles_written;
        out_data = NULL;
        out_data_bytes = 0;

        if ((tiles_left > 0) && (enc->num_drects > 0))
        {
            alloc_bytes = XRDP_SURCMD_PREFIX_BYTES;
            alloc_bytes += self->max_compressed_bytes;
            alloc_bytes += sizeof(struct rfx_tile) * tiles_left +
                           sizeof(struct rfx_rect) * enc->num_drects;
            out_data = g_new(char, alloc_bytes);
            if (out_data != NULL)
            {
                tiles = (struct rfx_tile *)
                        (out_data + XRDP_SURCMD_PREFIX_BYTES +
                         self->max_compressed_bytes);
                rfxrects = (struct rfx_rect *) (tiles + tiles_left);

                count = tiles_left;
                for (index = 0; index < count; index++)
                {
                    x = enc->crects[(index + all_tiles_written) * 4 + 0];
                    y = enc->crects[(index + all_tiles_written) * 4 + 1];
                    cx = enc->crects[(index + all_tiles_written) * 4 + 2];
                    cy = enc->crects[(index + all_tiles_written) * 4 + 3];
                    tiles[index].x = x;
                    tiles[index].y = y;
                    tiles[index].cx = cx;
                    tiles[index].cy = cy;
                    tiles[index].quant_y = self->quant_idx_y;
                    tiles[index].quant_cb = self->quant_idx_u;
                    tiles[index].quant_cr = self->quant_idx_v;
                }

                count = enc->num_drects;
                for (index = 0; index < count; index++)
                {
                    x = enc->drects[index * 4 + 0];
                    y = enc->drects[index * 4 + 1];
                    cx = enc->drects[index * 4 + 2];
                    cy = enc->drects[index * 4 + 3];
                    rfxrects[index].x = x;
                    rfxrects[index].y = y;
                    rfxrects[index].cx = cx;
                    rfxrects[index].cy = cy;
                }

                out_data_bytes = self->max_compressed_bytes;
                tiles_written = rfxcodec_encode(self->codec_handle,
                                                out_data + XRDP_SURCMD_PREFIX_BYTES,
                                                &out_data_bytes, enc->data,
                                                enc->width, enc->height,
                                                enc->width * 4,
                                                rfxrects, enc->num_drects,
                                                tiles, enc->num_crects,
                                                self->quants, self->num_quants);
            }
        }

        LOG_DEVEL(LOG_LEVEL_DEBUG,
                  "process_enc_rfx: rfxcodec_encode tiles_written %d",
                  tiles_written);
        /* only if enc_done->comp_bytes is not zero is something sent
           to the client but you must always send something back even
           on error so Xorg can get ack */
        enc_done = g_new0(XRDP_ENC_DATA_DONE, 1);
        if (enc_done == NULL)
        {
            return 1;
        }
        enc_done->comp_bytes = tiles_written > 0 ? out_data_bytes : 0;
        enc_done->pad_bytes = XRDP_SURCMD_PREFIX_BYTES;
        enc_done->comp_pad_data = out_data;
        enc_done->enc = enc;
        enc_done->cx = self->mm->wm->screen->width;
        enc_done->cy = self->mm->wm->screen->height;
        if (self->gfx)
        {
            enc_done->flags = (enum xrdp_encoder_flags)
                              ((int)enc_done->flags | GFX_PROGRESSIVE_RFX);
        }
        enc_done->continuation = all_tiles_written > 0;
        if (tiles_written > 0)
        {
            all_tiles_written += tiles_written;
        }
        finished =
            (all_tiles_written == enc->num_crects) || (tiles_written < 0);
        enc_done->last = finished;

        /* done with msg */
        /* inform main thread done */
        tc_mutex_lock(mutex);
        fifo_add_item(fifo_processed, enc_done);
        tc_mutex_unlock(mutex);
        /* signal completion for main thread */
        g_set_wait_obj(event_processed);

    }
    while (!finished);

    return 0;
}
#endif

/*****************************************************************************/
/* called from encoder thread */
static int
process_enc_h264(struct xrdp_encoder *self, XRDP_ENC_DATA *enc)
{
    LOG_DEVEL(LOG_LEVEL_INFO, "process_enc_h264: dummy func");
    return 0;
}

/**
 * Encoder thread main loop
 *****************************************************************************/
THREAD_RV THREAD_CC
proc_enc_msg(void *arg)
{
    XRDP_ENC_DATA *enc;
    struct fifo *fifo_to_proc;
    tbus mutex;
    tbus event_to_proc;
    tbus term_obj;
    tbus lterm_obj;
    int robjs_count;
    int wobjs_count;
    int cont;
    int timeout;
    tbus robjs[32];
    tbus wobjs[32];
    struct xrdp_encoder *self;

    LOG_DEVEL(LOG_LEVEL_INFO, "proc_enc_msg: thread is running");

    self = (struct xrdp_encoder *) arg;
    if (self == 0)
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "proc_enc_msg: self nil");
        return 0;
    }

    fifo_to_proc = self->fifo_to_proc;
    mutex = self->mutex;
    event_to_proc = self->xrdp_encoder_event_to_proc;

    term_obj = g_get_term();
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
            LOG(LOG_LEVEL_DEBUG,
                "Received termination signal, stopping the encoder thread");
            break;
        }

        if (g_is_wait_obj_set(lterm_obj)) /* xrdp_mm term */
        {
            LOG_DEVEL(LOG_LEVEL_DEBUG, "proc_enc_msg: xrdp_mm term");
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
    LOG_DEVEL(LOG_LEVEL_DEBUG, "proc_enc_msg: thread exit");
    return 0;
}
