/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2019
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
 * MS-RDPEAI
 *
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "os_calls.h"
#include "chansrv.h"
#include "log.h"
#include "xrdp_constants.h"
#include "fifo.h"
#include "audin.h"
#include "chansrv_config.h"
#include "resample.h"

#if defined(XRDP_OPUS)
#include <opus/opus.h>
#endif

#define MSG_SNDIN_VERSION       1
#define MSG_SNDIN_FORMATS       2
#define MSG_SNDIN_OPEN          3
#define MSG_SNDIN_OPEN_REPLY    4
#define MSG_SNDIN_DATA_INCOMING 5
#define MSG_SNDIN_DATA          6
#define MSG_SNDIN_FORMATCHANGE  7

#define AUDIN_VERSION 0x00000001

#define AUDIN_NAME "AUDIO_INPUT"
#define AUDIN_FLAGS  1 /* WTS_CHANNEL_OPTION_DYNAMIC */

extern struct fifo *g_in_fifo; /* in sound.c */
extern int g_bytes_in_fifo; /* in sound.c */
extern struct config_chansrv *g_cfg; /* in chansrv.c */

struct xr_wave_format_ex
{
    int wFormatTag;
    int nChannels;
    int nSamplesPerSec;
    int nAvgBytesPerSec;
    int nBlockAlign;
    int wBitsPerSample;
    int cbSize;
    uint8_t *data;
};

static uint8_t g_pcm_44100_data[] = { 0 };
static struct xr_wave_format_ex g_pcm_44100 =
{
    WAVE_FORMAT_PCM, /* wFormatTag */
    2,               /* num of channels */
    44100,           /* samples per sec */
    176400,          /* avg bytes per sec */
    4,               /* block align */
    16,              /* bits per sample */
    0,               /* data size */
    g_pcm_44100_data /* data */
};

#if defined(XRDP_OPUS)
static uint8_t g_opus_48000_data[] = { 0 };
static struct xr_wave_format_ex g_opus_48000 =
{
    WAVE_FORMAT_OPUS, /* wFormatTag */
    2,                /* num of channels */
    48000,            /* samples per sec */
    8000,             /* avg bytes per sec: clients encode at 64 kbit/s */
    4,                /* block align */
    16,               /* bits per sample */
    0,                /* data size */
    g_opus_48000_data /* data */
};

/* The sound server's source (pulseaudio-module-xrdp) reads 16 bit stereo
   at 44100 Hz, like the PCM format above, so Opus is decoded at 48 kHz
   and resampled. The client sends one Opus packet per data message; ask
   for 20 ms packets, as Opus takes 2.5, 5, 10, 20, 40 or 60 ms frames. */
#define OPUS_FRAMES_PER_PACKET 960
#define OPUS_MAX_FRAMES 5760 /* 120 ms, the longest Opus packet */
#define SOURCE_RATE 44100

static OpusDecoder *g_opus_decoder = NULL;
static struct resampler *g_opus_rs = NULL; /* 48000 Hz to 44100 Hz */
#endif

#define PCM_FRAMES_PER_PACKET 2048

static struct chansrv_drdynvc_procs g_audin_info;
static int g_audin_chanid;

/* in order of preference: Opus needs about 1/20 of the PCM bandwidth */
static struct xr_wave_format_ex *g_server_formats[] =
{
#if defined(XRDP_OPUS)
    &g_opus_48000,
#endif
    &g_pcm_44100,
    NULL
};

static struct xr_wave_format_ex **g_client_formats = NULL;

static int g_num_client_formats = 0;
static int g_current_format = 0; /* index in g_client_formats */

/*****************************************************************************/

/*
 * This can be called from sound.c because it includes audin.h.
 */

const char *
audin_wave_format_tag_to_str(int tag)
{
    return
        (tag == WAVE_FORMAT_PCM)        ? "WAVE_FORMAT_PCM" :
        (tag == WAVE_FORMAT_ADPCM)      ? "WAVE_FORMAT_ADPCM" :
        (tag == WAVE_FORMAT_ALAW)       ? "WAVE_FORMAT_ALAW" :
        (tag == WAVE_FORMAT_MULAW)      ? "WAVE_FORMAT_MULAW" :
        (tag == WAVE_FORMAT_MPEGLAYER3) ? "WAVE_FORMAT_MPEGLAYER3" :
        (tag == WAVE_FORMAT_OPUS)       ? "WAVE_FORMAT_OPUS" :
        (tag == WAVE_FORMAT_AAC)        ? "WAVE_FORMAT_AAC" :
        "UNKNOWN";
}

/*****************************************************************************/
static int
cleanup_client_formats(void)
{
    int index;

    if (g_client_formats == NULL)
    {
        return 0;
    }
    index = 0;
    while (g_client_formats[index] != NULL)
    {
        g_free(g_client_formats[index]->data);
        g_free(g_client_formats[index]);
        index++;
    }
    g_free(g_client_formats);
    g_client_formats = NULL;
    g_num_client_formats = 0;
    g_current_format = 0;
    return 0;
}

/*****************************************************************************/
static int
audin_send_version(int chan_id)
{
    int error;
    int bytes;
    struct stream *s;

    LOG_DEVEL(LOG_LEVEL_INFO, "audin_send_version:");
    make_stream(s);
    init_stream(s, 32);
    out_uint8(s, MSG_SNDIN_VERSION);
    out_uint32_le(s, AUDIN_VERSION);
    s_mark_end(s);
    bytes = (int) (s->end - s->data);
    error = chansrv_drdynvc_data(chan_id, s->data, bytes);
    free_stream(s);
    return error;
}

/*****************************************************************************/
/* Opus is only offered when sesman.ini asks for it, see MicrophoneOpus */
static int
audin_server_format_enabled(const struct xr_wave_format_ex *wf)
{
#if defined(XRDP_OPUS)
    if (wf == &g_opus_48000)
    {
        return g_cfg != NULL && g_cfg->microphone_opus;
    }
#endif
    return 1;
}

/*****************************************************************************/
static int
audin_send_formats(int chan_id)
{
    int error;
    int bytes;
    int num_formats;
    int index;
    struct stream *s;
    struct xr_wave_format_ex *wf;

    LOG_DEVEL(LOG_LEVEL_INFO, "audin_send_formats:");
    num_formats = 0;
    for (index = 0; g_server_formats[index] != NULL; index++)
    {
        num_formats += audin_server_format_enabled(g_server_formats[index]);
    }
    make_stream(s);
    init_stream(s, 8192 * num_formats);
    out_uint8(s, MSG_SNDIN_FORMATS);
    out_uint32_le(s, num_formats);
    out_uint32_le(s, 0); /* cbSizeFormatsPacket */
    for (index = 0; g_server_formats[index] != NULL; index++)
    {
        wf = g_server_formats[index];
        if (!audin_server_format_enabled(wf))
        {
            continue;
        }
        LOG_DEVEL(LOG_LEVEL_INFO, "audin_send_formats: sending format wFormatTag 0x%4.4x "
                  "nChannels %d nSamplesPerSec %d",
                  wf->wFormatTag, wf->nChannels, wf->nSamplesPerSec);
        out_uint16_le(s, wf->wFormatTag);
        out_uint16_le(s, wf->nChannels);
        out_uint32_le(s, wf->nSamplesPerSec);
        out_uint32_le(s, wf->nAvgBytesPerSec);
        out_uint16_le(s, wf->nBlockAlign);
        out_uint16_le(s, wf->wBitsPerSample);
        out_uint16_le(s, wf->cbSize);
        if (wf->cbSize > 0)
        {
            out_uint8p(s, wf->data, wf->cbSize);
        }
    }
    s_mark_end(s);
    bytes = (int) (s->end - s->data);
    error = chansrv_drdynvc_data(chan_id, s->data, bytes);
    free_stream(s);
    return error;
}

/*****************************************************************************/
static int
audin_is_format(const struct xr_wave_format_ex *wf,
                const struct xr_wave_format_ex *want)
{
    return wf != NULL &&
           wf->wFormatTag == want->wFormatTag &&
           wf->nChannels == want->nChannels &&
           wf->nSamplesPerSec == want->nSamplesPerSec &&
           (want->wFormatTag != WAVE_FORMAT_PCM ||
            wf->wBitsPerSample == want->wBitsPerSample);
}

#if defined(XRDP_OPUS)
/*****************************************************************************/
static int
audin_is_opus(const struct xr_wave_format_ex *wf)
{
    return audin_is_format(wf, &g_opus_48000);
}
#endif

/*****************************************************************************/
static int
audin_frames_per_packet(const struct xr_wave_format_ex *wf)
{
#if defined(XRDP_OPUS)
    if (audin_is_opus(wf))
    {
        return OPUS_FRAMES_PER_PACKET;
    }
#endif
    return PCM_FRAMES_PER_PACKET;
}

/*****************************************************************************/
/* the client lists the server formats it supports; take the first one
   the source can be fed with, in the server's order of preference */
static int
audin_choose_format(int num_formats)
{
    int server;
    int client;

    for (server = 0; g_server_formats[server] != NULL; server++)
    {
        if (!audin_server_format_enabled(g_server_formats[server]))
        {
            continue;
        }
        for (client = 0; client < num_formats; client++)
        {
            if (audin_is_format(g_client_formats[client],
                                g_server_formats[server]))
            {
                return client;
            }
        }
    }
    LOG(LOG_LEVEL_WARNING, "audin_choose_format: no known format, "
        "using the client's first one");
    return 0;
}

/*****************************************************************************/
static void
audin_reset_decoder(void)
{
#if defined(XRDP_OPUS)
    if (g_opus_decoder != NULL)
    {
        opus_decoder_ctl(g_opus_decoder, OPUS_RESET_STATE);
    }
    if (g_opus_rs != NULL)
    {
        resampler_reset(g_opus_rs);
    }
#endif
}

/*****************************************************************************/
static int
audin_send_open(int chan_id)
{
    int error;
    int bytes;
    struct stream *s;
    struct xr_wave_format_ex *wf = g_client_formats[g_current_format];

    LOG_DEVEL(LOG_LEVEL_INFO, "audin_send_open:");
    make_stream(s);
    /* wf->cbSize was checked when the format was received */
    init_stream(s, wf->cbSize + 64);

    out_uint8(s, MSG_SNDIN_OPEN);
    out_uint32_le(s, audin_frames_per_packet(wf)); /* FramesPerPacket */
    out_uint32_le(s, g_current_format); /* initialFormat */
    out_uint16_le(s, wf->wFormatTag);
    out_uint16_le(s, wf->nChannels);
    out_uint32_le(s, wf->nSamplesPerSec);
    out_uint32_le(s, wf->nAvgBytesPerSec);
    out_uint16_le(s, wf->nBlockAlign);
    out_uint16_le(s, wf->wBitsPerSample);
    bytes = wf->cbSize;
    out_uint16_le(s, bytes);
    if (bytes > 0)
    {
        out_uint8p(s, wf->data, bytes);
    }
    s_mark_end(s);
    bytes = (int) (s->end - s->data);
    error = chansrv_drdynvc_data(chan_id, s->data, bytes);
    free_stream(s);
    return error;
}

/*****************************************************************************/
static int
audin_process_version(int chan_id, struct stream *s)
{
    int version;

    LOG_DEVEL(LOG_LEVEL_INFO, "audin_process_version:");
    if (!s_check_rem(s, 4))
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "audin_process_version: parse error");
        return 1;
    }
    in_uint32_le(s, version);
    LOG(LOG_LEVEL_INFO, "audin_process_version: version %d", version);
    return audin_send_formats(chan_id);
}

/*****************************************************************************/
static int
audin_process_formats(int chan_id, struct stream *s)
{
    int index;
    int num_formats;
    struct xr_wave_format_ex *wf;

    LOG_DEVEL(LOG_LEVEL_INFO, "audin_process_formats:");
    cleanup_client_formats();
    if (!s_check_rem(s, 8))
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "audin_process_formats: parse error");
        return 1;
    }
    in_uint32_le(s, num_formats);
    in_uint8s(s, 4); /* cbSizeFormatsPacket */
    if (num_formats < 0 || num_formats > 1024)
    {
        LOG(LOG_LEVEL_ERROR, "audin_process_formats: bad number of formats %d",
            num_formats);
        return 1;
    }
    g_client_formats = g_new0(struct xr_wave_format_ex *, num_formats + 1);
    g_num_client_formats = num_formats;
    for (index = 0; index < num_formats; index++)
    {
        if (!s_check_rem(s, 18))
        {
            LOG_DEVEL(LOG_LEVEL_ERROR, "audin_process_formats: parse error");
            return 1;
        }
        wf = g_new0(struct xr_wave_format_ex, 1);
        g_client_formats[index] = wf;
        in_uint16_le(s, wf->wFormatTag);
        in_uint16_le(s, wf->nChannels);
        in_uint32_le(s, wf->nSamplesPerSec);
        in_uint32_le(s, wf->nAvgBytesPerSec);
        in_uint16_le(s, wf->nBlockAlign);
        in_uint16_le(s, wf->wBitsPerSample);
        in_uint16_le(s, wf->cbSize);

        LOG(LOG_LEVEL_INFO, "audin_process_formats:");
        LOG(LOG_LEVEL_INFO, "      wFormatNo       %d", index);
        LOG(LOG_LEVEL_INFO, "      wFormatTag      %s", audin_wave_format_tag_to_str(wf->wFormatTag));
        LOG(LOG_LEVEL_INFO, "      nChannels       %d", wf->nChannels);
        LOG(LOG_LEVEL_INFO, "      nSamplesPerSec  %d", wf->nSamplesPerSec);
        LOG(LOG_LEVEL_INFO, "      nAvgBytesPerSec %d", wf->nAvgBytesPerSec);
        LOG(LOG_LEVEL_INFO, "      nBlockAlign     %d", wf->nBlockAlign);
        LOG(LOG_LEVEL_INFO, "      wBitsPerSample  %d", wf->wBitsPerSample);
        LOG(LOG_LEVEL_INFO, "      cbSize          %d", wf->cbSize);

        if (wf->cbSize > 0)
        {
            if (!s_check_rem(s, wf->cbSize))
            {
                LOG_DEVEL(LOG_LEVEL_ERROR, "audin_process_formats: parse error");
                return 1;
            }
            wf->data = g_new0(uint8_t, wf->cbSize);
            in_uint8a(s, wf->data, wf->cbSize);
        }
    }
    if (num_formats == 0)
    {
        LOG(LOG_LEVEL_WARNING, "audin_process_formats: the client supports "
            "none of the formats");
        return 0;
    }
    g_current_format = audin_choose_format(num_formats);
    wf = g_client_formats[g_current_format];
    LOG(LOG_LEVEL_INFO, "audin_process_formats: recording with %s, %d Hz, "
        "%d channels", audin_wave_format_tag_to_str(wf->wFormatTag),
        wf->nSamplesPerSec, wf->nChannels);
    audin_reset_decoder();
    audin_send_open(chan_id);
    return 0;
}

/*****************************************************************************/
static int
audin_process_open_reply(int chan_id, struct stream *s)
{
    int result;

    if (!s_check_rem(s, 4))
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "audin_process_open_reply: parse error");
        return 1;
    }
    in_uint32_le(s, result);
    LOG(LOG_LEVEL_INFO, "audin_process_open_reply: result 0x%8.8x", result);
    return 0;
}

/*****************************************************************************/
static int
audin_process_incoming_data(int chan_id, struct stream *s)
{
    LOG_DEVEL(LOG_LEVEL_DEBUG, "audin_process_incoming_data:");
    return 0;
}

/*****************************************************************************/
static void
audin_queue(const char *data, int data_bytes)
{
    struct stream *ls;

    xstream_new(ls, data_bytes);
    g_memcpy(ls->data, data, data_bytes);
    ls->p += data_bytes;
    s_mark_end(ls);
    fifo_add_item(g_in_fifo, (void *) ls);
    g_bytes_in_fifo += data_bytes;
}

#if defined(XRDP_OPUS)
/*****************************************************************************/
static int
audin_process_opus(const char *data, int data_bytes)
{
    static opus_int16 pcm[OPUS_MAX_FRAMES * 2];
    static short out[(OPUS_MAX_FRAMES * SOURCE_RATE / 48000 + 2) * 2];
    int error;
    int frames;

    if (g_opus_decoder == NULL)
    {
        g_opus_decoder = opus_decoder_create(g_opus_48000.nSamplesPerSec,
                                             g_opus_48000.nChannels, &error);
        if (g_opus_decoder == NULL)
        {
            LOG(LOG_LEVEL_ERROR, "audin_process_opus: opus_decoder_create "
                "failed: %s", opus_strerror(error));
            return 1;
        }
    }
    if (g_opus_rs == NULL)
    {
        g_opus_rs = resampler_create(g_opus_48000.nSamplesPerSec, SOURCE_RATE);
        if (g_opus_rs == NULL)
        {
            LOG(LOG_LEVEL_ERROR, "audin_process_opus: resampler_create failed");
            return 1;
        }
    }
    frames = opus_decode(g_opus_decoder, (const unsigned char *) data,
                         data_bytes, pcm, OPUS_MAX_FRAMES, 0);
    if (frames < 0)
    {
        LOG(LOG_LEVEL_WARNING, "audin_process_opus: opus_decode failed: %s",
            opus_strerror(frames));
        return 0; /* drop this packet, keep the channel */
    }
    frames = resampler_process(g_opus_rs, pcm, frames, out,
                               OPUS_MAX_FRAMES * SOURCE_RATE / 48000 + 2);
    if (frames > 0)
    {
        audin_queue((const char *) out, frames * 4);
    }
    return 0;
}
#endif

/*****************************************************************************/
static int
audin_process_data(int chan_id, struct stream *s)
{
    int data_bytes;

    data_bytes = (int) (s->end - s->p);
    LOG_DEVEL(LOG_LEVEL_DEBUG, "audin_process_data: data_bytes %d", data_bytes);

#if defined(XRDP_OPUS)
    if (g_current_format < g_num_client_formats &&
            audin_is_opus(g_client_formats[g_current_format]))
    {
        return audin_process_opus(s->p, data_bytes);
    }
#endif
    audin_queue(s->p, data_bytes);
    return 0;
}

/*****************************************************************************/
static int
audin_process_format_change(int chan_id, struct stream *s)
{
    int index;

    LOG_DEVEL(LOG_LEVEL_INFO, "audin_process_format_change:");
    if (!s_check_rem(s, 4))
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "audin_process_format_change: parse error");
        return 1;
    }
    in_uint32_le(s, index);
    if (index < 0 || index >= g_num_client_formats)
    {
        LOG(LOG_LEVEL_ERROR, "audin_process_format_change: invalid format %d",
            index);
        return 1;
    }
    g_current_format = index;
    LOG(LOG_LEVEL_INFO, "audin_process_format_change: format %d, %s",
        index, audin_wave_format_tag_to_str(
            g_client_formats[index]->wFormatTag));
    audin_reset_decoder();
    return 0;
}

/*****************************************************************************/
static int
audin_process_msg(int chan_id, struct stream *s)
{
    int code;

    LOG_DEVEL(LOG_LEVEL_DEBUG, "audin_process_msg:");
    if (!s_check_rem(s, 1))
    {
        LOG_DEVEL(LOG_LEVEL_ERROR, "audin_process_msg: parse error");
        return 1;
    }
    in_uint8(s, code);
    LOG_DEVEL(LOG_LEVEL_DEBUG, "audin_process_msg: code %d", code);
    switch (code)
    {
        case MSG_SNDIN_VERSION:
            return audin_process_version(chan_id, s);
        case MSG_SNDIN_FORMATS:
            return audin_process_formats(chan_id, s);
        case MSG_SNDIN_OPEN_REPLY:
            return audin_process_open_reply(chan_id, s);
        case MSG_SNDIN_DATA_INCOMING:
            return audin_process_incoming_data(chan_id, s);
        case MSG_SNDIN_DATA:
            return audin_process_data(chan_id, s);
        case MSG_SNDIN_FORMATCHANGE:
            return audin_process_format_change(chan_id, s);
        default:
            LOG_DEVEL(LOG_LEVEL_ERROR, "audin_process_msg: unprocessed code %d", code);
            break;
    }
    return 0;
}

/*****************************************************************************/
static int
audin_open_response(int chan_id, int creation_status)
{
    LOG_DEVEL(LOG_LEVEL_INFO, "audin_open_response: creation_status 0x%8.8x", creation_status);
    if (creation_status == 0)
    {
        return audin_send_version(chan_id);
    }
    return 0;
}

/*****************************************************************************/
static int
audin_close_response(int chan_id)
{
    LOG_DEVEL(LOG_LEVEL_INFO, "audin_close_response:");
    g_audin_chanid = 0;
    cleanup_client_formats();
    return 0;
}

/*****************************************************************************/
int
audin_init(void)
{
    LOG_DEVEL(LOG_LEVEL_INFO, "audin_init:");
    g_memset(&g_audin_info, 0, sizeof(g_audin_info));
    g_audin_info.open_response = audin_open_response;
    g_audin_info.close_response = audin_close_response;
    g_audin_info.data_first = NULL;
    g_audin_info.data = audin_process_msg;
    g_audin_chanid = 0;
    return 0;
}

/*****************************************************************************/
int
audin_deinit(void)
{
    LOG_DEVEL(LOG_LEVEL_INFO, "audin_deinit:");
    return 0;
}

/*****************************************************************************/
int
audin_start(void)
{
    int error;

    LOG_DEVEL(LOG_LEVEL_INFO, "audin_start:");
    if (g_audin_chanid != 0 || g_in_fifo == NULL)
    {
        return 1;
    }

    /* if there is any data in FIFO, discard it */
    fifo_clear(g_in_fifo, NULL);
    g_bytes_in_fifo = 0;

    error = chansrv_drdynvc_open(AUDIN_NAME, AUDIN_FLAGS,
                                 &g_audin_info, /* callback functions */
                                 &g_audin_chanid); /* chansrv chan_id */
    LOG_DEVEL(LOG_LEVEL_ERROR, "audin_start: error %d g_audin_chanid %d", error, g_audin_chanid);
    return error;
}

/*****************************************************************************/
int
audin_stop(void)
{
    LOG_DEVEL(LOG_LEVEL_INFO, "audin_stop:");
    chansrv_drdynvc_close(g_audin_chanid);
    return 0;
}
