/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2009-2014
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

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <signal.h>
#include <sys/un.h>

#include "sound.h"
#include "thread_calls.h"
#include "defines.h"
#include "fifo.h"
#include "xrdp_constants.h"
#include "xrdp_sockets.h"
#include "chansrv_common.h"

#if defined(XRDP_FDK_AAC)
#include <fdk-aac/aacenc_lib.h>
static HANDLE_AACENCODER g_fdk_aac_encoder = 0;
#endif

#if defined(XRDP_OPUS)
#include <opus/opus.h>
static OpusEncoder *g_opus_encoder = 0;
#endif

#if defined(XRDP_MP3LAME)
#include <lame/lame.h>
static lame_global_flags *g_lame_encoder = 0;
#endif

extern int g_rdpsnd_chan_id;    /* in chansrv.c */
extern int g_display_num;       /* in chansrv.c */

/* audio out: sound_server -> xrdp -> NeutrinoRDP */
static struct trans *g_audio_l_trans_out = 0; /* listener */
static struct trans *g_audio_c_trans_out = 0; /* connection */

/* audio in:  sound_server <- xrdp <- NeutrinoRDP */
static struct trans *g_audio_l_trans_in = 0;  /* listener */
static struct trans *g_audio_c_trans_in = 0;  /* connection */

static int    g_training_sent_time = 0;
static int    g_cBlockNo = 0;
static int    g_bytes_in_stream = 0;
static FIFO   g_in_fifo;
static int    g_bytes_in_fifo = 0;
static int    g_unacked_frames = 0;

static struct stream *g_stream_inp = NULL;
static struct stream *g_stream_incoming_packet = NULL;

#define MAX_BBUF_SIZE (1024 * 16)
static char g_buffer[MAX_BBUF_SIZE];
static int g_buf_index = 0;
static int g_sent_time[256];
static int g_sent_flag[256];

static int g_bbuf_size = 1024 * 8; /* may change later */

struct xr_wave_format_ex
{
    int wFormatTag;
    int nChannels;
    int nSamplesPerSec;
    int nAvgBytesPerSec;
    int nBlockAlign;
    int wBitsPerSample;
    int cbSize;
    tui8 *data;
};

/* output formats */

static tui8 g_pcm_22050_data[] = { 0 };
static struct xr_wave_format_ex g_pcm_22050 =
{
    WAVE_FORMAT_PCM, /* wFormatTag */
    2,               /* num of channels */
    22050,           /* samples per sec */
    88200,           /* avg bytes per sec */
    4,               /* block align */
    16,              /* bits per sample */
    0,               /* data size */
    g_pcm_22050_data /* data */
};

static tui8 g_pcm_44100_data[] = { 0 };
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

#if defined(XRDP_FDK_AAC)
static tui8 g_fdk_aac_44100_data[] = { 0 };
static struct xr_wave_format_ex g_fdk_aac_44100 =
{
    WAVE_FORMAT_AAC,     /* wFormatTag */
    2,                   /* num of channels */
    44100,               /* samples per sec */
    12000,               /* avg bytes per sec */
    4,                   /* block align */
    16,                  /* bits per sample */
    0,                   /* data size */
    g_fdk_aac_44100_data /* data */
};
#endif

#if defined(XRDP_OPUS)
static tui8 g_opus_44100_data[] = { 0 };
static struct xr_wave_format_ex g_opus_44100 =
{
    WAVE_FORMAT_OPUS, /* wFormatTag */
    2,                /* num of channels */
    44100,            /* samples per sec */
    176400,           /* avg bytes per sec */
    4,                /* block align */
    16,               /* bits per sample */
    0,                /* data size */
    g_opus_44100_data /* data */
};
#endif

#if defined(XRDP_MP3LAME)
static tui8 g_mp3lame_44100_data[] = { 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0xb6, 0x00, 0x01, 0x00, 0x71, 0x05 };
static struct xr_wave_format_ex g_mp3lame_44100 =
{
    WAVE_FORMAT_MPEGLAYER3, /* wFormatTag */
    2,                      /* num of channels */
    44100,                  /* samples per sec */
    176400,                 /* avg bytes per sec */
    4,                      /* block align */
    0,                      /* bits per sample */
    12,                     /* data size */
    g_mp3lame_44100_data    /* data */
};
#endif

static struct xr_wave_format_ex *g_wave_outp_formats[] =
{
    &g_pcm_44100,
    &g_pcm_22050,
#if defined(XRDP_FDK_AAC)
    &g_fdk_aac_44100,
#endif
#if defined(XRDP_OPUS)
    &g_opus_44100,
#endif
#if defined(XRDP_MP3LAME)
    &g_mp3lame_44100,
#endif
    0
};

static int g_client_does_fdk_aac = 0;
static int g_client_fdk_aac_index = 0;

static int g_client_does_opus = 0;
static int g_client_opus_index = 0;

static int g_client_does_mp3lame = 0;
static int g_client_mp3lame_index = 0;

/* index into list from client */
static int g_current_client_format_index = 0;

/* index into list from server */
static int g_current_server_format_index = 0;

/* input formats */

static tui8 g_pcm_inp_22050_data[] = { 0 };
static struct xr_wave_format_ex g_pcm_inp_22050 =
{
    WAVE_FORMAT_PCM, /* wFormatTag */
    2,               /* num of channels */
    22050,           /* samples per sec */
    88200,           /* avg bytes per sec */
    4,               /* block align */
    16,              /* bits per sample */
    0,               /* data size */
    g_pcm_inp_22050_data /* data */
};

static tui8 g_pcm_inp_44100_data[] = { 0 };
static struct xr_wave_format_ex g_pcm_inp_44100 =
{
    WAVE_FORMAT_PCM, /* wFormatTag */
    2,               /* num of channels */
    44100,           /* samples per sec */
    176400,          /* avg bytes per sec */
    4,               /* block align */
    16,              /* bits per sample */
    0,               /* data size */
    g_pcm_inp_44100_data /* data */
};

static struct xr_wave_format_ex *g_wave_inp_formats[] =
{
    &g_pcm_inp_44100,
    &g_pcm_inp_22050,
    0
};

static int g_client_input_format_index = 0;
static int g_server_input_format_index = 0;

/* microphone related */
static int sound_send_server_input_formats(void);
static int sound_process_input_format(int aindex, int wFormatTag,
                           int nChannels, int nSamplesPerSec,
                           int nAvgBytesPerSec, int nBlockAlign,
                           int wBitsPerSample, int cbSize, char *data);
static int sound_process_input_formats(struct stream *s, int size);
static int sound_input_start_recording(void);
static int sound_input_stop_recording(void);
static int sound_process_input_data(struct stream *s, int bytes);
static int sound_sndsrvr_source_data_in(struct trans *trans);
static int sound_start_source_listener(void);
static int sound_start_sink_listener(void);

/*****************************************************************************/
static int
sound_send_server_output_formats(void)
{
    struct stream *s;
    int bytes;
    int index;
    int num_formats;
    char *size_ptr;

    num_formats = sizeof(g_wave_outp_formats) /
                  sizeof(g_wave_outp_formats[0]) - 1;
    LOG(10, ("sound_send_server_output_formats: num_formats %d", num_formats));

    make_stream(s);
    init_stream(s, 8182);
    out_uint16_le(s, SNDC_FORMATS);
    size_ptr = s->p;
    out_uint16_le(s, 0);                    /* size, set later */
    out_uint32_le(s, 0);                    /* dwFlags */
    out_uint32_le(s, 0);                    /* dwVolume */
    out_uint32_le(s, 0);                    /* dwPitch */
    out_uint16_le(s, 0);                    /* wDGramPort */
    out_uint16_le(s, num_formats);          /* wNumberOfFormats */
    out_uint8(s, g_cBlockNo);               /* cLastBlockConfirmed */
    out_uint16_le(s, 5);                    /* wVersion */
    out_uint8(s, 0);                        /* bPad */

    /* sndFormats */
    /*
        wFormatTag      2 byte offset 0
        nChannels       2 byte offset 2
        nSamplesPerSec  4 byte offset 4
        nAvgBytesPerSec 4 byte offset 8
        nBlockAlign     2 byte offset 12
        wBitsPerSample  2 byte offset 14
        cbSize          2 byte offset 16
        data            variable offset 18
    */

    /*  examples
        01 00 02 00 44 ac 00 00 10 b1 02 00 04 00 10 00 ....D...........
        00 00
        01 00 02 00 22 56 00 00 88 58 01 00 04 00 10 00 ...."V...X......
        00 00
    */

    for (index = 0; index < num_formats; index++)
    {
        out_uint16_le(s, g_wave_outp_formats[index]->wFormatTag);
        out_uint16_le(s, g_wave_outp_formats[index]->nChannels);
        out_uint32_le(s, g_wave_outp_formats[index]->nSamplesPerSec);
        out_uint32_le(s, g_wave_outp_formats[index]->nAvgBytesPerSec);
        out_uint16_le(s, g_wave_outp_formats[index]->nBlockAlign);
        out_uint16_le(s, g_wave_outp_formats[index]->wBitsPerSample);
        bytes = g_wave_outp_formats[index]->cbSize;
        out_uint16_le(s, bytes);
        if (bytes > 0)
        {
            out_uint8p(s, g_wave_outp_formats[index]->data, bytes);
        }
    }

    s_mark_end(s);
    bytes = (int)((s->end - s->data) - 4);
    size_ptr[0] = bytes;
    size_ptr[1] = bytes >> 8;
    bytes = (int)(s->end - s->data);
    send_channel_data(g_rdpsnd_chan_id, s->data, bytes);
    free_stream(s);
    return 0;
}

/*****************************************************************************/
static int
sound_send_training(void)
{
    struct stream *s;
    int bytes;
    int time;
    char *size_ptr;

    make_stream(s);
    init_stream(s, 8182);
    out_uint16_le(s, SNDC_TRAINING);
    size_ptr = s->p;
    out_uint16_le(s, 0); /* size, set later */
    time = g_time2();
    g_training_sent_time = time;
    out_uint16_le(s, time);
    out_uint16_le(s, 1024);
    out_uint8s(s, (1024 - 4));
    s_mark_end(s);
    bytes = (int)((s->end - s->data) - 4);
    size_ptr[0] = bytes;
    size_ptr[1] = bytes >> 8;
    bytes = (int)(s->end - s->data);
    send_channel_data(g_rdpsnd_chan_id, s->data, bytes);
    free_stream(s);
    return 0;
}

/*****************************************************************************/
static int
sound_process_output_format(int aindex, int wFormatTag, int nChannels,
                            int nSamplesPerSec, int nAvgBytesPerSec,
                            int nBlockAlign, int wBitsPerSample,
                            int cbSize, char *data)
{
    LOG(1, ("sound_process_output_format:"));
    LOG(1, ("      wFormatTag      %d", wFormatTag));
    LOG(1, ("      nChannels       %d", nChannels));
    LOG(1, ("      nSamplesPerSec  %d", nSamplesPerSec));
    LOG(1, ("      nAvgBytesPerSec %d", nAvgBytesPerSec));
    LOG(1, ("      nBlockAlign     %d", nBlockAlign));
    LOG(1, ("      wBitsPerSample  %d", wBitsPerSample));
    LOG(1, ("      cbSize          %d", cbSize));

    g_hexdump(data, cbSize);

    /* select CD quality audio */
    if (wFormatTag == g_pcm_44100.wFormatTag &&
        nChannels == g_pcm_44100.nChannels &&
        nSamplesPerSec == g_pcm_44100.nSamplesPerSec &&
        nAvgBytesPerSec == g_pcm_44100.nAvgBytesPerSec &&
        nBlockAlign == g_pcm_44100.nBlockAlign &&
        wBitsPerSample == g_pcm_44100.wBitsPerSample)
    {
        g_current_client_format_index = aindex;
        g_current_server_format_index = 0;
    }
#if 0
    for (lindex = 0; lindex < NUM_BUILT_IN; lindex++)
    {
        if (wFormatTag == g_wave_formats[lindex]->wFormatTag &&
            nChannels == g_wave_formats[lindex]->nChannels &&
            nSamplesPerSec == g_wave_formats[lindex]->nSamplesPerSec &&
            nAvgBytesPerSec == g_wave_formats[lindex]->nAvgBytesPerSec &&
            nBlockAlign == g_wave_formats[lindex]->nBlockAlign &&
            wBitsPerSample == g_wave_formats[lindex]->wBitsPerSample)
        {
            g_current_client_format_index = aindex;
            g_current_server_format_index = lindex;
        }
    }
#endif

    switch(wFormatTag)
    {
        case WAVE_FORMAT_AAC:
            LOG(0, ("wFormatTag, fdk aac"));
            g_client_does_fdk_aac = 1;
            g_client_fdk_aac_index = aindex;
            break;
        case WAVE_FORMAT_MPEGLAYER3:
            LOG(0, ("wFormatTag, mp3"));
            g_client_does_mp3lame = 1;
            g_client_mp3lame_index = aindex;
            break;
        case WAVE_FORMAT_OPUS:
            LOG(0, ("wFormatTag, opus"));
            g_client_does_opus = 1;
            g_client_opus_index = aindex;
            break;
    }

    return 0;
}

/*****************************************************************************/
/*
    0000 07 02 26 00 03 00 80 00 ff ff ff ff 00 00 00 00 ..&.............
    0010 00 00 01 00 00 02 00 00 01 00 02 00 44 ac 00 00 ............D...
    0020 10 b1 02 00 04 00 10 00 00 00
*/

static int
sound_process_output_formats(struct stream *s, int size)
{
    int num_formats;
    int index;
    int wFormatTag;
    int nChannels;
    int nSamplesPerSec;
    int nAvgBytesPerSec;
    int nBlockAlign;
    int wBitsPerSample;
    int cbSize;
    char *data;

    if (size < 16)
        return 1;

    in_uint8s(s, 14);
    in_uint16_le(s, num_formats);
    in_uint8s(s, 4);

    if (num_formats > 0)
    {
        for (index = 0; index < num_formats; index++)
        {
            in_uint16_le(s, wFormatTag);
            in_uint16_le(s, nChannels);
            in_uint32_le(s, nSamplesPerSec);
            in_uint32_le(s, nAvgBytesPerSec);
            in_uint16_le(s, nBlockAlign);
            in_uint16_le(s, wBitsPerSample);
            in_uint16_le(s, cbSize);
            in_uint8p(s, data, cbSize);
            sound_process_output_format(index, wFormatTag, nChannels, nSamplesPerSec,
                                        nAvgBytesPerSec, nBlockAlign, wBitsPerSample,
                                        cbSize, data);
        }
        sound_send_training();
    }

    return 0;
}

#if defined(XRDP_FDK_AAC)

/*****************************************************************************/
static int
sound_wave_compress_fdk_aac(char *data, int data_bytes, int *format_index)
{
    int rv;
    int cdata_bytes;
    char *cdata;

    AACENC_ERROR error;
    int aot;
    int sample_rate;
    int mode;
    int bitrate;
    int afterburner;
    int channel_order;
    AACENC_InfoStruct info;
    AACENC_BufDesc in_buf;
    AACENC_BufDesc out_buf;
    AACENC_InArgs in_args;
    AACENC_OutArgs out_args;
    void *in_buffer;
    int in_identifier;
    int in_size;
    int in_elem_size;
    void *out_buffer;
    int out_identifier;
    int out_size;
    int out_elem_size;

    rv = data_bytes;

    if (g_client_does_fdk_aac == 0)
    {
        return rv;
    }

    if (g_fdk_aac_encoder == 0)
    {
        /* init fdk aac encoder */
        LOG(0, ("sound_wave_compress_fdk_aac: using fdk aac"));

        error = aacEncOpen(&g_fdk_aac_encoder, 0, 2);
        if (error != AACENC_OK)
        {
            LOG(0, ("sound_wave_compress_fdk_aac: aacEncOpen() failed"));
            return rv;
        }

        aot = 2; /* MPEG-4 AAC Low Complexity. */
        error = aacEncoder_SetParam(g_fdk_aac_encoder, AACENC_AOT, aot);
        if (error != AACENC_OK)
        {
            LOG(0, ("sound_wave_compress_fdk_aac: aacEncoder_SetParam() "
                    "AACENC_AOT failed"));
        }

        sample_rate = g_fdk_aac_44100.nSamplesPerSec;
        error = aacEncoder_SetParam(g_fdk_aac_encoder, AACENC_SAMPLERATE,
                                    sample_rate);
        if (error != AACENC_OK)
        {
            LOG(0, ("sound_wave_compress_fdk_aac: aacEncoder_SetParam() "
                    "AACENC_SAMPLERATE failed"));
        }

        mode = MODE_2;
        error = aacEncoder_SetParam(g_fdk_aac_encoder,
                                    AACENC_CHANNELMODE, mode);
        if (error != AACENC_OK)
        {
            LOG(0, ("sound_wave_compress_fdk_aac: aacEncoder_SetParam() "
                    "AACENC_CHANNELMODE failed"));
        }

        channel_order = 1; /* WAVE file format channel ordering */
        error = aacEncoder_SetParam(g_fdk_aac_encoder, AACENC_CHANNELORDER,
                                    channel_order);
        if (error != AACENC_OK)
        {
            LOG(0, ("sound_wave_compress_fdk_aac: aacEncoder_SetParam() "
                    "AACENC_CHANNELORDER failed"));
        }

        /* bytes rate to bit rate */
        bitrate = g_fdk_aac_44100.nAvgBytesPerSec * 8;
        error = aacEncoder_SetParam(g_fdk_aac_encoder, AACENC_BITRATE,
                                    bitrate);
        if (error != AACENC_OK)
        {
            LOG(0, ("sound_wave_compress_fdk_aac: aacEncoder_SetParam() "
                    "AACENC_BITRATE failed"));
        }

        error = aacEncoder_SetParam(g_fdk_aac_encoder, AACENC_TRANSMUX, 0);
        if (error != AACENC_OK)
        {
            LOG(0, ("sound_wave_compress_fdk_aac: aacEncoder_SetParam() "
                    "AACENC_TRANSMUX failed"));
        }

        afterburner = 1;
        error = aacEncoder_SetParam(g_fdk_aac_encoder, AACENC_AFTERBURNER,
                                    afterburner);
        if (error != AACENC_OK)
        {
            LOG(0, ("sound_wave_compress_fdk_aac: aacEncoder_SetParam() "
                    "AACENC_AFTERBURNER failed"));
        }

        error = aacEncEncode(g_fdk_aac_encoder, NULL, NULL, NULL, NULL);
        if (error != AACENC_OK)
        {
            LOG(0, ("sound_wave_compress_fdk_aac: Unable to initialize "
                    "the encoder"));
        }

        g_memset(&info, 0, sizeof(info));
        error = aacEncInfo(g_fdk_aac_encoder, &info);
        if (error != AACENC_OK)
        {
            LOG(0, ("sound_wave_compress_fdk_aac: aacEncInfo failed"));
        }

        LOG(0, ("sound_wave_compress_fdk_aac:"));
        LOG(0, ("  AACENC_InfoStruct"));
        LOG(0, ("    maxOutBufBytes %d", info.maxOutBufBytes));
        LOG(0, ("    maxAncBytes %d", info.maxAncBytes));
        LOG(0, ("    inBufFillLevel %d", info.inBufFillLevel));
        LOG(0, ("    inputChannels %d", info.inputChannels));
        LOG(0, ("    frameLength %d", info.frameLength));
        LOG(0, ("    encoderDelay %d", info.encoderDelay));
        LOG(0, ("    confBuf"));
        LOG(0, ("    confSize %d", info.confSize));
    }

    rv = data_bytes;
    cdata_bytes = data_bytes;
    cdata = (char *) g_malloc(cdata_bytes, 0);
    if (data_bytes < g_bbuf_size)
    {
        g_memset(data + data_bytes, 0, g_bbuf_size - data_bytes);
        data_bytes = g_bbuf_size;
    }

    in_buffer = data;
    in_identifier = IN_AUDIO_DATA;
    in_size = data_bytes;
    in_elem_size = 2;

    g_memset(&in_args, 0, sizeof(in_args));
    in_args.numInSamples = data_bytes / 2;
    g_memset(&in_buf, 0, sizeof(in_buf));
    in_buf.numBufs = 1;
    in_buf.bufs = &in_buffer;
    in_buf.bufferIdentifiers = &in_identifier;
    in_buf.bufSizes = &in_size;
    in_buf.bufElSizes = &in_elem_size;

    out_buffer = cdata;
    out_identifier = OUT_BITSTREAM_DATA;
    out_size = cdata_bytes;
    out_elem_size = 1;

    g_memset(&out_buf, 0, sizeof(out_buf));
    out_buf.numBufs = 1;
    out_buf.bufs = &out_buffer;
    out_buf.bufferIdentifiers = &out_identifier;
    out_buf.bufSizes = &out_size;
    out_buf.bufElSizes = &out_elem_size;

    g_memset(&out_args, 0, sizeof(out_args));
    error = aacEncEncode(g_fdk_aac_encoder, &in_buf, &out_buf,
                         &in_args, &out_args);
    if (error == AACENC_OK)
    {
        cdata_bytes = out_args.numOutBytes;
        LOG(10, ("sound_wave_compress_fdk_aac: aacEncEncode ok "
                 "cdata_bytes %d", cdata_bytes));
        *format_index = g_client_fdk_aac_index;
        g_memcpy(data, cdata, cdata_bytes);
        rv = cdata_bytes;
    }
    else
    {
        LOG(0, ("sound_wave_compress_fdk_aac: aacEncEncode failed"));
    }
    g_free(cdata);

    return rv;
}

#else

/*****************************************************************************/
static int
sound_wave_compress_fdk_aac(char *data, int data_bytes, int *format_index)
{
    return data_bytes;
}

#endif

#if defined(XRDP_OPUS)

/*****************************************************************************/
static int
sound_wave_compress_opus(char *data, int data_bytes, int *format_index)
{
    unsigned char *cdata;
    int cdata_bytes;
    int rv;
    int error;
    int data_bytes_org;
    opus_int16 *os16;

    if (g_client_does_opus == 0)
    {
        return data_bytes;
    }
    if (g_opus_encoder == 0)
    {
        /* NB (narrowband)       8 kHz
           MB (medium-band)     12 kHz
           WB (wideband)        16 kHz
           SWB (super-wideband) 24 kHz
           FB (fullband)        48 kHz */
        g_opus_encoder = opus_encoder_create(48000, 2,
                                             OPUS_APPLICATION_AUDIO,
                                             &error);
        if (g_opus_encoder == 0)
        {
            LOG(0, ("sound_wave_compress_opus: opus_encoder_create failed"));
            return data_bytes;
        }
    }
    data_bytes_org = data_bytes;
    rv = data_bytes;
    cdata_bytes = data_bytes;
    cdata = (unsigned char *) g_malloc(cdata_bytes, 0);
    os16 = (opus_int16 *) data;
    /* at 48000 we have
       2.5 ms   480
       5   ms   960
       10  ms  1920
       20  ms  3840
       40  ms  7680
       60  ms 11520 */
    if (data_bytes < g_bbuf_size)
    {
        g_memset(data + data_bytes, 0, g_bbuf_size - data_bytes);
        data_bytes = g_bbuf_size;
    }
    cdata_bytes = opus_encode(g_opus_encoder, os16, data_bytes / 4,
                              cdata, cdata_bytes);
    if ((cdata_bytes > 0) && (cdata_bytes < data_bytes_org))
    {
        *format_index = g_client_opus_index;
        g_memcpy(data, cdata, cdata_bytes);
        rv = cdata_bytes;
    }
    g_free(cdata);
    return rv;
}

#else

/*****************************************************************************/
static int
sound_wave_compress_opus(char *data, int data_bytes, int *format_index)
{
    return data_bytes;
}

#endif

#if defined(XRDP_MP3LAME)

/*****************************************************************************/
static int
sound_wave_compress_mp3lame(char *data, int data_bytes, int *format_index)
{
    int rv;
    int cdata_bytes;
    int odata_bytes;
    unsigned char *cdata;

    cdata = NULL;
    rv = data_bytes;

    if (g_client_does_mp3lame == 0)
    {
        return rv;
    }

    if (g_lame_encoder == 0)
    {
        /* init mp3 lame encoder */
        LOG(0, ("sound_wave_compress_mp3lame: using mp3lame"));

        g_lame_encoder = lame_init();
        if (g_lame_encoder == 0)
        {
            LOG(0, ("sound_wave_compress_mp3lame: lame_init() failed"));
            return rv;
        }
        lame_set_num_channels(g_lame_encoder, g_mp3lame_44100.nChannels);
        lame_set_in_samplerate(g_lame_encoder, g_mp3lame_44100.nSamplesPerSec);
        if (lame_init_params(g_lame_encoder) == -1)
        {
            LOG(0, ("sound_wave_compress_mp3lame: lame_init_params() failed"));
            return rv;
        }

        LOG(0, ("sound_wave_compress_mp3lame: lame config:"));
        LOG(0, ("                             brate            : %d", lame_get_brate(g_lame_encoder)));
        LOG(0, ("                             compression ratio: %f", lame_get_compression_ratio(g_lame_encoder)));
        LOG(0, ("                             encoder delay    : %d", lame_get_encoder_delay(g_lame_encoder)));
        LOG(0, ("                             frame size       : %d", lame_get_framesize(g_lame_encoder)));
        LOG(0, ("                             encoder padding  : %d", lame_get_encoder_padding(g_lame_encoder)));
        LOG(0, ("                             mode             : %d", lame_get_mode(g_lame_encoder)));
    }

    odata_bytes = data_bytes;
    cdata_bytes = data_bytes;
    cdata = (unsigned char *) g_malloc(cdata_bytes, 0);
    if (data_bytes < g_bbuf_size)
    {
        g_memset(data + data_bytes, 0, g_bbuf_size - data_bytes);
        data_bytes = g_bbuf_size;
    }
    cdata_bytes = lame_encode_buffer_interleaved(g_lame_encoder,
                                                 (short int *) data,
                                                 data_bytes / 4,
                                                 cdata,
                                                 cdata_bytes);
    if (cdata_bytes < 0)
    {
        LOG(0, ("sound_wave_compress: lame_encode_buffer_interleaved() "
                "failed, error %d", cdata_bytes));
        return rv;
    }
    if ((cdata_bytes > 0) && (cdata_bytes < odata_bytes))
    {
        *format_index = g_client_mp3lame_index;
        g_memcpy(data, cdata, cdata_bytes);
        rv = cdata_bytes;
    }

    g_free(cdata);
    return rv;
}

#else

/*****************************************************************************/
static int
sound_wave_compress_mp3lame(char *data, int data_bytes, int *format_index)
{
    return data_bytes;
}

#endif

/*****************************************************************************/
static int
sound_wave_compress(char *data, int data_bytes, int *format_index)
{
    if (g_client_does_fdk_aac)
    {
        g_bbuf_size = 4096;
        return sound_wave_compress_fdk_aac(data, data_bytes, format_index);
    }
    else if (g_client_does_opus)
    {
        g_bbuf_size = 11520;
        return sound_wave_compress_opus(data, data_bytes, format_index);
    }
    else if (g_client_does_mp3lame)
    {
        g_bbuf_size = 11520;
        return sound_wave_compress_mp3lame(data, data_bytes, format_index);
    }
    return data_bytes;
}

/*****************************************************************************/
/* send wave message to client */
static int
sound_send_wave_data_chunk(char *data, int data_bytes)
{
    struct stream *s;
    int bytes;
    int time;
    int format_index;
    char *size_ptr;

    LOG(10, ("sound_send_wave_data_chunk: data_bytes %d", data_bytes));

    if ((data_bytes < 4) || (data_bytes > 128 * 1024))
    {
        LOG(0, ("sound_send_wave_data_chunk: bad data_bytes %d", data_bytes));
        return 1;
    }

    LOG(20, ("sound_send_wave_data_chunk: g_sent_flag[%d] = %d",
            g_cBlockNo + 1, g_sent_flag[(g_cBlockNo + 1) & 0xff]));
    if (g_sent_flag[(g_cBlockNo + 1) & 0xff] & 1)
    {
        LOG(10, ("sound_send_wave_data_chunk: no room"));
        return 2;
    }
    else
    {
        LOG(10, ("sound_send_wave_data_chunk: got room"));
    }

    /* compress, if available */
    format_index = g_current_client_format_index;
    data_bytes = sound_wave_compress(data, data_bytes, &format_index);

    /* part one of 2 PDU wave info */

    LOG(10, ("sound_send_wave_data_chunk: sending %d bytes", data_bytes));

    make_stream(s);
    init_stream(s, 16 + data_bytes); /* some extra space */
    out_uint16_le(s, SNDC_WAVE);
    size_ptr = s->p;
    out_uint16_le(s, 0); /* size, set later */
    time = g_time2();
    out_uint16_le(s, time);
    out_uint16_le(s, format_index); /* wFormatNo */
    g_cBlockNo++;
    g_unacked_frames++;
    out_uint8(s, g_cBlockNo);
    g_sent_time[g_cBlockNo & 0xff] = time;
    g_sent_flag[g_cBlockNo & 0xff] = 1;

    LOG(10, ("sound_send_wave_data_chunk: sending time %d, g_cBlockNo %d",
             time & 0xffff, g_cBlockNo & 0xff));

    out_uint8s(s, 3);
    out_uint8a(s, data, 4);
    s_mark_end(s);
    bytes = (int)((s->end - s->data) - 4);
    bytes += data_bytes;
    bytes -= 4;
    size_ptr[0] = bytes;
    size_ptr[1] = bytes >> 8;
    bytes = (int)(s->end - s->data);
    send_channel_data(g_rdpsnd_chan_id, s->data, bytes);

    /* part two of 2 PDU wave info
       even is zero, we have to send this */
    init_stream(s, data_bytes);
    out_uint32_le(s, 0);
    out_uint8a(s, data + 4, data_bytes - 4);
    s_mark_end(s);
    bytes = (int)(s->end - s->data);
    send_channel_data(g_rdpsnd_chan_id, s->data, bytes);

    free_stream(s);
    return 0;
}

/*****************************************************************************/
/* send wave message to client, buffer first */
static int
sound_send_wave_data(char *data, int data_bytes)
{
    int space_left;
    int chunk_bytes;
    int data_index;
    int error;
    int res;

    LOG(10, ("sound_send_wave_data: sending %d bytes", data_bytes));
    data_index = 0;
    error = 0;
    while (data_bytes > 0)
    {
        space_left = g_bbuf_size - g_buf_index;
        chunk_bytes = MIN(space_left, data_bytes);
        if (chunk_bytes < 1)
        {
            LOG(10, ("sound_send_wave_data: error"));
            error = 1;
            break;
        }
        g_memcpy(g_buffer + g_buf_index, data + data_index, chunk_bytes);
        g_buf_index += chunk_bytes;
        if (g_buf_index >= g_bbuf_size)
        {
            g_buf_index = 0;
            res = sound_send_wave_data_chunk(g_buffer, g_bbuf_size);
            if (res == 2)
            {
                /* don't need to error on this */
                LOG(0, ("sound_send_wave_data: dropped, no room"));
                break;
            }
            else if (res != 0)
            {
                LOG(10, ("sound_send_wave_data: error"));
                error = 1;
                break;
            }
        }
        data_bytes -= chunk_bytes;
        data_index += chunk_bytes;
    }

    return error;
}

/*****************************************************************************/
/* send close message to client */
static int
sound_send_close(void)
{
    struct stream *s;
    int bytes;
    char *size_ptr;

    LOG(10, ("sound_send_close:"));

    /* send any left over data */
    if (g_buf_index)
    {
        if (sound_send_wave_data_chunk(g_buffer, g_buf_index) != 0)
        {
            LOG(10, ("sound_send_close: sound_send_wave_data_chunk failed"));
            return 1;
        }
    }
    g_buf_index = 0;
    g_memset(g_sent_flag, 0, sizeof(g_sent_flag));

    /* send close msg */
    make_stream(s);
    init_stream(s, 8182);
    out_uint16_le(s, SNDC_CLOSE);
    size_ptr = s->p;
    out_uint16_le(s, 0); /* size, set later */
    s_mark_end(s);
    bytes = (int)((s->end - s->data) - 4);
    size_ptr[0] = bytes;
    size_ptr[1] = bytes >> 8;
    bytes = (int)(s->end - s->data);
    send_channel_data(g_rdpsnd_chan_id, s->data, bytes);
    free_stream(s);
    return 0;
}

/*****************************************************************************/
/* from client */
static int
sound_process_training(struct stream *s, int size)
{
    int time_diff;

    time_diff = g_time2() - g_training_sent_time;
    LOG(0, ("sound_process_training: round trip time %u", time_diff));
    return 0;
}

/*****************************************************************************/
/* from client */
static int
sound_process_wave_confirm(struct stream *s, int size)
{
    int wTimeStamp;
    int cConfirmedBlockNo;
    int cleared_count;
    int time;
    int time_diff;
    int block_no;
    int block_no_clamped;
    int found;
    int index;

    time = g_time2();
    in_uint16_le(s, wTimeStamp);
    in_uint8(s, cConfirmedBlockNo);
    time_diff = time - g_sent_time[cConfirmedBlockNo & 0xff];
    cleared_count = 0;
    found = 0;
    block_no = g_cBlockNo;
    for (index = 0; index < g_unacked_frames; index++)
    {
        block_no_clamped = block_no & 0xff;
        if ((cConfirmedBlockNo == block_no_clamped) || found)
        {
            found = 1;
            if (g_sent_flag[block_no_clamped] & 1)
            {
                LOG(10, ("sound_process_wave_confirm: clearing %d",
                    block_no_clamped));
                g_sent_flag[block_no_clamped] &= ~1;
                cleared_count++;
            }
        }
        block_no--;
    }
    LOG(10, ("sound_process_wave_confirm: wTimeStamp %d, "
        "cConfirmedBlockNo %d time diff %d cleared_count %d "
        "g_unacked_frames %d", wTimeStamp, cConfirmedBlockNo, time_diff,
        cleared_count, g_unacked_frames));
    g_unacked_frames -= cleared_count;
    return 0;
}

/*****************************************************************************/
/* process message in from the audio source, eg pulse, alsa
   on it's way to the client. returns error */
static int
process_pcm_message(int id, int size, struct stream *s)
{
    switch (id)
    {
        case 0:
            return sound_send_wave_data(s->p, size);
            break;
        case 1:
            return sound_send_close();
            break;
        default:
            LOG(10, ("process_pcm_message: unknown id %d", id));
            break;
    }
    return 1;
}

/*****************************************************************************/

/* data in from sound_server_sink */

static int
sound_sndsrvr_sink_data_in(struct trans *trans)
{
    struct stream *s;
    int id;
    int size;
    int error;

    if (trans == 0)
        return 0;

    if (trans != g_audio_c_trans_out)
        return 1;

    s = trans_get_in_s(trans);
    in_uint32_le(s, id);
    in_uint32_le(s, size);

    if ((id & ~3) || (size > 128 * 1024 + 8) || (size < 8))
    {
        LOG(0, ("sound_sndsrvr_sink_data_in: bad message id %d size %d", id, size));
        return 1;
    }

    LOG(10, ("sound_sndsrvr_sink_data_in: good message id %d size %d", id, size));

    error = trans_force_read(trans, size - 8);

    if (error == 0)
    {
        /* here, the entire message block is read in, process it */
        error = process_pcm_message(id, size - 8, s);
    }

    return error;
}

/*****************************************************************************/

/* incoming connection on unix domain socket - sound_server_sink -> xrdp */

static int
sound_sndsrvr_sink_conn_in(struct trans *trans, struct trans *new_trans)
{
    LOG(0, ("sound_sndsrvr_sink_conn_in:"));

    if (trans == 0)
        return 1;

    if (trans != g_audio_l_trans_out)
        return 1;

    if (g_audio_c_trans_out != 0) /* if already set, error */
        return 1;

    if (new_trans == 0)
        return 1;

    g_audio_c_trans_out = new_trans;
    g_audio_c_trans_out->trans_data_in = sound_sndsrvr_sink_data_in;
    g_audio_c_trans_out->header_size = 8;
    trans_delete(g_audio_l_trans_out);
    g_audio_l_trans_out = 0;

    return 0;
}

/*****************************************************************************/

/* incoming connection on unix domain socket - sound_server_source -> xrdp */

static int
sound_sndsrvr_source_conn_in(struct trans *trans, struct trans *new_trans)
{
    LOG(0, ("sound_sndsrvr_source_conn_in: client connected"));

    if (trans == 0)
        return 1;

    if (trans != g_audio_l_trans_in)
        return 1;

    if (g_audio_c_trans_in != 0) /* if already set, error */
        return 1;

    if (new_trans == 0)
        return 1;

    g_audio_c_trans_in = new_trans;
    g_audio_c_trans_in->trans_data_in = sound_sndsrvr_source_data_in;
    g_audio_c_trans_in->header_size = 8;
    trans_delete(g_audio_l_trans_in);
    g_audio_l_trans_in = 0;

    return 0;
}

/*****************************************************************************/
int
sound_init(void)
{
    LOG(0, ("sound_init:"));

    g_memset(g_sent_flag, 0, sizeof(g_sent_flag));
    g_stream_incoming_packet = NULL;

    /* init sound output */
    sound_send_server_output_formats();
    sound_start_sink_listener();

    /* init sound input */
    sound_send_server_input_formats();
    sound_start_source_listener();

    /* save data from sound_server_source */
    fifo_init(&g_in_fifo, 100);

    g_client_does_fdk_aac = 0;
    g_client_fdk_aac_index = 0;

    g_client_does_opus = 0;
    g_client_opus_index = 0;

    g_client_does_mp3lame = 0;
    g_client_mp3lame_index = 0;

    return 0;
}

/*****************************************************************************/
int
sound_deinit(void)
{
    LOG(10, ("sound_deinit:"));
    if (g_audio_l_trans_out != 0)
    {
        trans_delete(g_audio_l_trans_out);
        g_audio_l_trans_out = 0;
    }

    if (g_audio_c_trans_out != 0)
    {
        trans_delete(g_audio_c_trans_out);
        g_audio_c_trans_out = 0;
    }

    if (g_audio_l_trans_in != 0)
    {
        trans_delete(g_audio_l_trans_in);
        g_audio_l_trans_in = 0;
    }

    if (g_audio_c_trans_in != 0)
    {
        trans_delete(g_audio_c_trans_in);
        g_audio_c_trans_in = 0;
    }

#if defined(XRDP_MP3LAME)
    if (g_lame_encoder)
    {
        lame_close(g_lame_encoder);
        g_lame_encoder = 0;
        g_client_does_mp3lame = 0;
    }
#endif

    fifo_deinit(&g_in_fifo);

    return 0;
}

/*****************************************************************************/

/* data in from client ( client -> xrdp -> chansrv ) */

int
sound_data_in(struct stream *s, int chan_id, int chan_flags, int length,
              int total_length)
{
    int code;
    int size;
    int ok_to_free = 1;

    if (!read_entire_packet(s, &g_stream_incoming_packet, chan_flags,
                            length, total_length))
    {
        return 0;
    }

    in_uint8(g_stream_incoming_packet, code);
    in_uint8s(g_stream_incoming_packet, 1);
    in_uint16_le(g_stream_incoming_packet, size);

    switch (code)
    {
        case SNDC_WAVECONFIRM:
            sound_process_wave_confirm(g_stream_incoming_packet, size);
            break;

        case SNDC_TRAINING:
            sound_process_training(g_stream_incoming_packet, size);
            break;

        case SNDC_FORMATS:
            sound_process_output_formats(g_stream_incoming_packet, size);
            break;

        case SNDC_REC_NEGOTIATE:
            sound_process_input_formats(g_stream_incoming_packet, size);
            break;

        case SNDC_REC_DATA:
            sound_process_input_data(g_stream_incoming_packet, size);
            ok_to_free = 0;
            break;

        default:
            LOG(10, ("sound_data_in: unknown code %d size %d", code, size));
            break;
    }

    if (ok_to_free && g_stream_incoming_packet)
    {
        xstream_free(g_stream_incoming_packet);
        g_stream_incoming_packet = NULL;
    }

    return 0;
}

/*****************************************************************************/
int
sound_get_wait_objs(tbus *objs, int *count, int *timeout)
{
    int lcount;

    lcount = *count;

    if (g_audio_l_trans_out != 0)
    {
        objs[lcount] = g_audio_l_trans_out->sck;
        lcount++;
    }

    if (g_audio_c_trans_out != 0)
    {
        objs[lcount] = g_audio_c_trans_out->sck;
        lcount++;
    }

    if (g_audio_l_trans_in != 0)
    {
        objs[lcount] = g_audio_l_trans_in->sck;
        lcount++;
    }

    if (g_audio_c_trans_in != 0)
    {
        objs[lcount] = g_audio_c_trans_in->sck;
        lcount++;
    }

    *count = lcount;
    return 0;
}

/*****************************************************************************/
int
sound_check_wait_objs(void)
{
    if (g_audio_l_trans_out != 0)
    {
        if (trans_check_wait_objs(g_audio_l_trans_out) != 0)
        {
            LOG(10, ("sound_check_wait_objs: g_audio_l_trans_out returned non-zero"));
            trans_delete(g_audio_l_trans_out);
            g_audio_l_trans_out = 0;
        }
    }

    if (g_audio_c_trans_out != 0)
    {
        if (trans_check_wait_objs(g_audio_c_trans_out) != 0)
        {
            LOG(10, ("sound_check_wait_objs: g_audio_c_trans_out returned non-zero"));
            trans_delete(g_audio_c_trans_out);
            g_audio_c_trans_out = 0;
            sound_start_sink_listener();
        }
    }

    if (g_audio_l_trans_in != 0)
    {
        if (trans_check_wait_objs(g_audio_l_trans_in) != 0)
        {
            LOG(10, ("sound_check_wait_objs: g_audio_l_trans_in returned non-zero"));
            trans_delete(g_audio_l_trans_in);
            g_audio_l_trans_in = 0;
        }
    }

    if (g_audio_c_trans_in != 0)
    {
        if (trans_check_wait_objs(g_audio_c_trans_in) != 0)
        {
            LOG(10, ("sound_check_wait_objs: g_audio_c_trans_in returned non-zero"));
            trans_delete(g_audio_c_trans_in);
            g_audio_c_trans_in = 0;
            sound_start_source_listener();
        }
    }

    return 0;
}

/******************************************************************************
 **                                                                          **
 **                       Microphone related code                            **
 **                                                                          **
 ******************************************************************************/

/**
 *
 *****************************************************************************/

static int
sound_send_server_input_formats(void)
{
    struct stream *s;
    int bytes;
    int index;
    int num_formats;
    char *size_ptr;

    num_formats = sizeof(g_wave_inp_formats) /
                  sizeof(g_wave_inp_formats[0]) - 1;
    LOG(10, ("sound_send_server_input_formats: num_formats %d", num_formats));

    make_stream(s);
    init_stream(s, 8182);
    out_uint16_le(s, SNDC_REC_NEGOTIATE);
    size_ptr = s->p;
    out_uint16_le(s, 0);                   /* size, set later */
    out_uint32_le(s, 0);                   /* unused */
    out_uint32_le(s, 0);                   /* unused */
    out_uint16_le(s, num_formats);         /* wNumberOfFormats */
    out_uint16_le(s, 5);                   /* wVersion */

    /*
        wFormatTag      2 byte offset 0
        nChannels       2 byte offset 2
        nSamplesPerSec  4 byte offset 4
        nAvgBytesPerSec 4 byte offset 8
        nBlockAlign     2 byte offset 12
        wBitsPerSample  2 byte offset 14
        cbSize          2 byte offset 16
        data            variable offset 18
    */

    for (index = 0; index < num_formats; index++)
    {
        out_uint16_le(s, g_wave_inp_formats[index]->wFormatTag);
        out_uint16_le(s, g_wave_inp_formats[index]->nChannels);
        out_uint32_le(s, g_wave_inp_formats[index]->nSamplesPerSec);
        out_uint32_le(s, g_wave_inp_formats[index]->nAvgBytesPerSec);
        out_uint16_le(s, g_wave_inp_formats[index]->nBlockAlign);
        out_uint16_le(s, g_wave_inp_formats[index]->wBitsPerSample);
        bytes = g_wave_inp_formats[index]->cbSize;
        out_uint16_le(s, bytes);
        if (bytes > 0)
        {
            out_uint8p(s, g_wave_inp_formats[index]->data, bytes);
        }
    }

    s_mark_end(s);
    bytes = (int)((s->end - s->data) - 4);
    size_ptr[0] = bytes;
    size_ptr[1] = bytes >> 8;
    bytes = (int)(s->end - s->data);
    send_channel_data(g_rdpsnd_chan_id, s->data, bytes);
    free_stream(s);
    return 0;
}

/**
 *
 *****************************************************************************/

static int
sound_process_input_format(int aindex, int wFormatTag, int nChannels,
                           int nSamplesPerSec, int nAvgBytesPerSec,
                           int nBlockAlign, int wBitsPerSample,
                           int cbSize, char *data)
{
    LOG(10, ("sound_process_input_format:"));
    LOG(10, ("      wFormatTag      %d", wFormatTag));
    LOG(10, ("      nChannels       %d", nChannels));
    LOG(10, ("      nSamplesPerSec  %d", nSamplesPerSec));
    LOG(10, ("      nAvgBytesPerSec %d", nAvgBytesPerSec));
    LOG(10, ("      nBlockAlign     %d", nBlockAlign));
    LOG(10, ("      wBitsPerSample  %d", wBitsPerSample));
    LOG(10, ("      cbSize          %d", cbSize));

#if 1
    /* select CD quality audio */
    if (wFormatTag == g_pcm_inp_44100.wFormatTag &&
        nChannels == g_pcm_inp_44100.nChannels &&
        nSamplesPerSec == g_pcm_inp_44100.nSamplesPerSec &&
        nAvgBytesPerSec == g_pcm_inp_44100.nAvgBytesPerSec &&
        nBlockAlign == g_pcm_inp_44100.nBlockAlign &&
        wBitsPerSample == g_pcm_inp_44100.wBitsPerSample)
    {
        g_client_input_format_index = aindex;
        g_server_input_format_index = 0;
    }
#else
    /* select half of CD quality audio */
    if (wFormatTag == g_pcm_inp_22050.wFormatTag &&
        nChannels == g_pcm_inp_22050.nChannels &&
        nSamplesPerSec == g_pcm_inp_22050.nSamplesPerSec &&
        nAvgBytesPerSec == g_pcm_inp_22050.nAvgBytesPerSec &&
        nBlockAlign == g_pcm_inp_22050.nBlockAlign &&
        wBitsPerSample == g_pcm_inp_22050.wBitsPerSample)
    {
        g_client_input_format_index = aindex;
        g_server_input_format_index = 0;
    }
#endif

    return 0;
}

/**
 *
 *****************************************************************************/

static int
sound_process_input_formats(struct stream *s, int size)
{
    int num_formats;
    int index;
    int wFormatTag;
    int nChannels;
    int nSamplesPerSec;
    int nAvgBytesPerSec;
    int nBlockAlign;
    int wBitsPerSample;
    int cbSize;
    char *data;

    LOG(10, ("sound_process_input_formats: size=%d", size));

    in_uint8s(s, 8); /* skip 8 bytes */
    in_uint16_le(s, num_formats);
    in_uint8s(s, 2); /* skip version */

    if (num_formats > 0)
    {
        for (index = 0; index < num_formats; index++)
        {
            in_uint16_le(s, wFormatTag);
            in_uint16_le(s, nChannels);
            in_uint32_le(s, nSamplesPerSec);
            in_uint32_le(s, nAvgBytesPerSec);
            in_uint16_le(s, nBlockAlign);
            in_uint16_le(s, wBitsPerSample);
            in_uint16_le(s, cbSize);
            in_uint8p(s, data, cbSize);
            sound_process_input_format(index, wFormatTag, nChannels, nSamplesPerSec,
                                       nAvgBytesPerSec, nBlockAlign, wBitsPerSample,
                                       cbSize, data);
        }
    }

    return 0;
}

/**
 *
 *****************************************************************************/

static int
sound_input_start_recording(void)
{
    struct stream* s;

    LOG(10, ("sound_input_start_recording:"));

    /* if there is any data in FIFO, discard it */
    while ((s = (struct stream *) fifo_remove(&g_in_fifo)) != NULL)
    {
        xstream_free(s);
    }
    g_bytes_in_fifo = 0;

    xstream_new(s, 1024);

    /*
     * command format
     *
     * 02 bytes command SNDC_REC_START
     * 02 bytes length
     * 02 bytes data format received earlier
     */

    out_uint16_le(s, SNDC_REC_START);
    out_uint16_le(s, 2);
    out_uint16_le(s, g_client_input_format_index);

    s_mark_end(s);
    send_channel_data(g_rdpsnd_chan_id, s->data, 6);
    xstream_free(s);

    return 0;
}

/**
 *
 *****************************************************************************/

static int
sound_input_stop_recording(void)
{
    struct stream* s;

    LOG(10, ("sound_input_stop_recording:"));

    xstream_new(s, 1024);

    /*
     * command format
     *
     * 02 bytes command SNDC_REC_STOP
     * 02 bytes length (zero)
     */

    out_uint16_le(s, SNDC_REC_STOP);
    out_uint16_le(s, 0);

    s_mark_end(s);
    send_channel_data(g_rdpsnd_chan_id, s->data, 4);
    xstream_free(s);

    return 0;
}

/**
 * Process data: xrdp <- client
 *****************************************************************************/

static int
sound_process_input_data(struct stream *s, int bytes)
{
    struct stream *ls;

    LOG(10, ("sound_process_input_data: bytes %d g_bytes_in_fifo %d",
        bytes, g_bytes_in_fifo));
#if 0 /* no need to cap anymore */
    /* cap data in fifo */
    if (g_bytes_in_fifo > 8 * 1024)
    {
        return 0;
    }
#endif
    xstream_new(ls, bytes);
    g_memcpy(ls->data, s->p, bytes);
    ls->p += bytes;
    s_mark_end(ls);
    fifo_insert(&g_in_fifo, (void *) ls);
    g_bytes_in_fifo += bytes;

    return 0;
}

/**
 * Got a command from sound_server_source
 *****************************************************************************/

static int
sound_sndsrvr_source_data_in(struct trans *trans)
{
    struct stream *ts = NULL;
    struct stream *s  = NULL;

    tui16    bytes_req   = 0;
    int      bytes_read  = 0;
    int      cmd;
    int      i;

    if (trans == 0)
        return 0;

    if (trans != g_audio_c_trans_in)
        return 1;

    ts = trans_get_in_s(trans);
    if (trans_force_read(trans, 3))
        log_message(LOG_LEVEL_ERROR, "sound.c: error reading from transport");

    ts->p = ts->data + 8;
    in_uint8(ts, cmd);
    in_uint16_le(ts, bytes_req);
    LOG(10, ("sound_sndsrvr_source_data_in: bytes_req %d", bytes_req));

    xstream_new(s, bytes_req + 2);

    if (cmd == PA_CMD_SEND_DATA)
    {
        /* set real len later */
        out_uint16_le(s, 0);

        while (bytes_read < bytes_req)
        {
            if (g_stream_inp == NULL)
            {
                g_stream_inp = (struct stream *) fifo_remove(&g_in_fifo);
                if (g_stream_inp != NULL)
                {
                    g_bytes_in_fifo -= g_stream_inp->size;
                    LOG(10, ("  g_bytes_in_fifo %d", g_bytes_in_fifo));
                }
            }

            if (g_stream_inp == NULL)
            {
                /* no more data, send what we have */
                break;
            }
            else
            {
                if (g_bytes_in_stream == 0)
                    g_bytes_in_stream = g_stream_inp->size;

                i = bytes_req - bytes_read;

                if (i < g_bytes_in_stream)
                {
                    xstream_copyin(s, &g_stream_inp->data[g_stream_inp->size - g_bytes_in_stream], i);
                    bytes_read += i;
                    g_bytes_in_stream -= i;
                }
                else
                {
                    xstream_copyin(s, &g_stream_inp->data[g_stream_inp->size - g_bytes_in_stream], g_bytes_in_stream);
                    bytes_read += g_bytes_in_stream;
                    g_bytes_in_stream = 0;
                    xstream_free(g_stream_inp);
                    g_stream_inp = NULL;
                }
            }
        }

        if (bytes_read)
        {
            s->data[0] = (char) (bytes_read & 0xff);
            s->data[1] = (char) ((bytes_read >> 8) & 0xff);
        }

        s_mark_end(s);

        trans_force_write_s(trans, s);
    }
    else if (cmd == PA_CMD_START_REC)
    {
        sound_input_start_recording();
    }
    else if (cmd == PA_CMD_STOP_REC)
    {
        sound_input_stop_recording();
    }

    xstream_free(s);

    return 0;
}

/**
 * Start a listener for microphone redirection connections
 *****************************************************************************/
static int
sound_start_source_listener(void)
{
    char port[1024];

    g_audio_l_trans_in = trans_create(TRANS_MODE_UNIX, 128 * 1024, 8192);
    g_audio_l_trans_in->is_term = g_is_term;
    g_snprintf(port, 255, CHANSRV_PORT_IN_STR, g_display_num);
    g_audio_l_trans_in->trans_conn_in = sound_sndsrvr_source_conn_in;
    if (trans_listen(g_audio_l_trans_in, port) != 0)
        LOG(0, ("trans_listen failed"));
    return 0;
}

/**
 * Start a listener for speaker redirection connections
 *****************************************************************************/
static int
sound_start_sink_listener(void)
{
    char port[1024];

    g_audio_l_trans_out = trans_create(TRANS_MODE_UNIX, 128 * 1024, 8192);
    g_audio_l_trans_out->is_term = g_is_term;
    g_snprintf(port, 255, CHANSRV_PORT_OUT_STR, g_display_num);
    g_audio_l_trans_out->trans_conn_in = sound_sndsrvr_sink_conn_in;
    if (trans_listen(g_audio_l_trans_out, port) != 0)
        LOG(0, ("trans_listen failed"));
    return 0;
}

