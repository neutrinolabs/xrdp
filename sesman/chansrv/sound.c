/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2009-2013
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

#include "sound.h"
#include "thread_calls.h"

extern int g_rdpsnd_chan_id;    /* in chansrv.c */
extern int g_display_num;       /* in chansrv.c */

static struct trans *g_audio_l_trans = 0; // listener
static struct trans *g_audio_c_trans = 0; // connection
static int g_training_sent_time = 0;
static int g_cBlockNo = 0;

#if defined(XRDP_SIMPLESOUND)
static void *DEFAULT_CC
read_raw_audio_data(void *arg);
#endif

/*****************************************************************************/
static int APP_CC
sound_send_server_formats(void)
{
    struct stream *s;
    int bytes;
    char *size_ptr;

    print_got_here();

    make_stream(s);
    init_stream(s, 8182);
    out_uint16_le(s, SNDC_FORMATS);
    size_ptr = s->p;
    out_uint16_le(s, 0);        /* size, set later */
    out_uint32_le(s, 0);        /* dwFlags */
    out_uint32_le(s, 0);        /* dwVolume */
    out_uint32_le(s, 0);        /* dwPitch */
    out_uint16_le(s, 0);        /* wDGramPort */
    out_uint16_le(s, 1);        /* wNumberOfFormats */
    out_uint8(s, g_cBlockNo);   /* cLastBlockConfirmed */
    out_uint16_le(s, 2);        /* wVersion */
    out_uint8(s, 0);            /* bPad */

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

    out_uint16_le(s, 1);         // wFormatTag - WAVE_FORMAT_PCM
    out_uint16_le(s, 2);         // num of channels
    out_uint32_le(s, 44100);     // samples per sec
    out_uint32_le(s, 176400);    // avg bytes per sec
    out_uint16_le(s, 4);         // block align
    out_uint16_le(s, 16);        // bits per sample
    out_uint16_le(s, 0);         // size

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

    print_got_here();

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
/*
    0000 07 02 26 00 03 00 80 00 ff ff ff ff 00 00 00 00 ..&.............
    0010 00 00 01 00 00 02 00 00 01 00 02 00 44 ac 00 00 ............D...
    0020 10 b1 02 00 04 00 10 00 00 00
*/

static int APP_CC
sound_process_formats(struct stream *s, int size)
{
    int num_formats;

    print_got_here();

    LOG(0, ("sound_process_formats:"));

    if (size < 16)
    {
        return 1;
    }

    in_uint8s(s, 14);
    in_uint16_le(s, num_formats);

    if (num_formats > 0)
    {
        sound_send_training();
    }

    return 0;
}

/*****************************************************************************/
static int
sound_send_wave_data(char *data, int data_bytes)
{
    struct stream *s;
    int bytes;
    int time;
    char *size_ptr;

    print_got_here();

    if ((data_bytes < 4) || (data_bytes > 128 * 1024))
    {
        LOG(0, ("sound_send_wave_data: bad data_bytes %d", data_bytes));
    }

    /* part one of 2 PDU wave info */

    LOG(10, ("sound_send_wave_data: sending %d bytes", data_bytes));

    make_stream(s);
    init_stream(s, data_bytes);
    out_uint16_le(s, SNDC_WAVE);
    size_ptr = s->p;
    out_uint16_le(s, 0); /* size, set later */
    time = g_time2();
    out_uint16_le(s, time);
    out_uint16_le(s, 0); /* wFormatNo */
    g_cBlockNo++;
    out_uint8(s, g_cBlockNo);

    LOG(10, ("sound_send_wave_data: sending time %d, g_cBlockNo %d",
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

    /* part two of 2 PDU wave info */
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
static int
sound_send_close(void)
{
    struct stream *s;
    int bytes;
    char *size_ptr;

    print_got_here();

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
static int APP_CC
sound_process_training(struct stream *s, int size)
{
    int time_diff;

    print_got_here();

    time_diff = g_time2() - g_training_sent_time;
    LOG(0, ("sound_process_training: round trip time %u", time_diff));
    return 0;
}

/*****************************************************************************/
static int APP_CC
sound_process_wave_confirm(struct stream *s, int size)
{
    int wTimeStamp;
    int cConfirmedBlockNo;

    print_got_here();

    in_uint16_le(s, wTimeStamp);
    in_uint8(s, cConfirmedBlockNo);

    LOG(10, ("sound_process_wave_confirm: wTimeStamp %d, cConfirmedBlockNo %d",
             wTimeStamp, cConfirmedBlockNo));

    return 0;
}

/*****************************************************************************/
static int APP_CC
process_pcm_message(int id, int size, struct stream *s)
{
    print_got_here();

    switch (id)
    {
        case 0:
            sound_send_wave_data(s->p, size);
            break;
        case 1:
            sound_send_close();
            break;
        default:
            LOG(0, ("process_pcm_message: unknown id %d", id));
            break;
    }
    return 0;
}

/*****************************************************************************/
/* data coming in from audio source, eg pulse, alsa */
static int DEFAULT_CC
sound_trans_audio_data_in(struct trans *trans)
{
    struct stream *s;
    int id;
    int size;
    int error;

    if (trans == 0)
    {
        return 0;
    }

    if (trans != g_audio_c_trans)
    {
        return 1;
    }

    s = trans_get_in_s(trans);
    in_uint32_le(s, id);
    in_uint32_le(s, size);

    if ((id & 3) || (size > 128 * 1024 + 8) || (size < 8))
    {
        LOG(0, ("sound_trans_audio_data_in: bad message id %d size %d", id, size));
        return 1;
    }
    LOG(10, ("sound_trans_audio_data_in: good message id %d size %d", id, size));

    error = trans_force_read(trans, size - 8);

    if (error == 0)
    {
        /* here, the entire message block is read in, process it */
        error = process_pcm_message(id, size - 8, s);
    }

    return error;
}

/*****************************************************************************/
static int DEFAULT_CC
sound_trans_audio_conn_in(struct trans *trans, struct trans *new_trans)
{
    LOG(0, ("sound_trans_audio_conn_in:"));
    print_got_here();

    if (trans == 0)
    {
        return 1;
    }

    if (trans != g_audio_l_trans)
    {
        return 1;
    }

    if (g_audio_c_trans != 0) /* if already set, error */
    {
        return 1;
    }

    if (new_trans == 0)
    {
        return 1;
    }

    g_audio_c_trans = new_trans;
    g_audio_c_trans->trans_data_in = sound_trans_audio_data_in;
    g_audio_c_trans->header_size = 8;
    trans_delete(g_audio_l_trans);
    g_audio_l_trans = 0;
    return 0;
}

#define CHANSRV_PORT_STR "/tmp/.xrdp/xrdp_chansrv_audio_socket_%d"

/*****************************************************************************/
int APP_CC
sound_init(void)
{
    char port[256];
    int error;

    print_got_here();
    LOG(0, ("sound_init:"));

    sound_send_server_formats();
    g_audio_l_trans = trans_create(2, 128 * 1024, 8192);
    g_snprintf(port, 255, CHANSRV_PORT_STR, g_display_num);
    g_audio_l_trans->trans_conn_in = sound_trans_audio_conn_in;
    error = trans_listen(g_audio_l_trans, port);

    if (error != 0)
    {
        LOG(0, ("sound_init: trans_listen failed"));
    }

#if defined(XRDP_SIMPLESOUND)

    /* start thread to read raw audio data from pulseaudio device */
    tc_thread_create(read_raw_audio_data, 0);

#endif

    return 0;
}

/*****************************************************************************/
int APP_CC
sound_deinit(void)
{
    print_got_here();

    if (g_audio_l_trans != 0)
    {
        trans_delete(g_audio_l_trans);
        g_audio_l_trans = 0;
    }

    if (g_audio_c_trans != 0)
    {
        trans_delete(g_audio_c_trans);
        g_audio_c_trans = 0;
    }

    return 0;
}

/*****************************************************************************/
/* data in from client ( clinet -> xrdp -> chansrv ) */
int APP_CC
sound_data_in(struct stream *s, int chan_id, int chan_flags, int length,
              int total_length)
{
    int code;
    int size;

    print_got_here();

    in_uint8(s, code);
    in_uint8s(s, 1);
    in_uint16_le(s, size);

    switch (code)
    {
        case SNDC_WAVECONFIRM:
            sound_process_wave_confirm(s, size);
            break;

        case SNDC_TRAINING:
            sound_process_training(s, size);
            break;

        case SNDC_FORMATS:
            sound_process_formats(s, size);
            break;

        default:
            LOG(0, ("sound_data_in: unknown code %d size %d", code, size));
            break;
    }

    return 0;
}

/*****************************************************************************/
int APP_CC
sound_get_wait_objs(tbus *objs, int *count, int *timeout)
{
    int lcount;

    lcount = *count;

    if (g_audio_l_trans != 0)
    {
        objs[lcount] = g_audio_l_trans->sck;
        lcount++;
    }

    if (g_audio_c_trans != 0)
    {
        objs[lcount] = g_audio_c_trans->sck;
        lcount++;
    }

    *count = lcount;
    return 0;
}

/*****************************************************************************/
int APP_CC
sound_check_wait_objs(void)
{
    if (g_audio_l_trans != 0)
    {
        trans_check_wait_objs(g_audio_l_trans);
    }

    if (g_audio_c_trans != 0)
    {
        trans_check_wait_objs(g_audio_c_trans);
    }

    return 0;
}

#if defined(XRDP_SIMPLESOUND)

static int DEFAULT_CC
sttrans_data_in(struct trans *self)
{
    LOG(0, ("sttrans_data_in:\n"));
    return 0;
}

/**
 * read raw audio data from pulseaudio device and write it
 * to a unix domain socket on which trans server is listening
 */

static void *DEFAULT_CC
read_raw_audio_data(void *arg)
{
    pa_sample_spec samp_spec;
    pa_simple *simple = NULL;
    uint32_t bytes_read;
    char *cptr;
    int i;
    int error;
    struct trans *strans;
    char path[256];
    struct stream *outs;

    strans = trans_create(TRANS_MODE_UNIX, 8192, 8192);

    if (strans == 0)
    {
        LOG(0, ("read_raw_audio_data: trans_create failed\n"));
        return 0;
    }

    strans->trans_data_in = sttrans_data_in;
    g_snprintf(path, 255, "/tmp/xrdp_chansrv_audio_socket_%d", g_display_num);

    if (trans_connect(strans, "", path, 100) != 0)
    {
        LOG(0, ("read_raw_audio_data: trans_connect failed\n"));
        trans_delete(strans);
        return 0;
    }

    /* setup audio format */
    samp_spec.format = PA_SAMPLE_S16LE;
    samp_spec.rate = 44100;
    samp_spec.channels = 2;

    /* if we are root, then for first 8 seconds connection to pulseaudo server
       fails; if we are non-root, then connection succeeds on first attempt;
       for now we have changed code to be non-root, but this may change in the
       future - so pretend we are root and try connecting to pulseaudio server
       for upto one minute */
    for (i = 0; i < 60; i++)
    {
        simple = pa_simple_new(NULL, "xrdp", PA_STREAM_RECORD, NULL,
                               "record", &samp_spec, NULL, NULL, &error);

        if (simple)
        {
            /* connected to pulseaudio server */
            LOG(0, ("read_raw_audio_data: connected to pulseaudio server\n"));
            break;
        }

        LOG(0, ("read_raw_audio_data: ERROR creating PulseAudio async interface\n"));
        LOG(0, ("read_raw_audio_data: %s\n", pa_strerror(error)));
        g_sleep(1000);
    }

    if (i == 60)
    {
        /* failed to connect to audio server */
        trans_delete(strans);
        return NULL;
    }

    /* insert header just once */
    outs = trans_get_out_s(strans, 8192);
    out_uint32_le(outs, 0);
    out_uint32_le(outs, AUDIO_BUF_SIZE + 8);
    cptr = outs->p;
    out_uint8s(outs, AUDIO_BUF_SIZE);
    s_mark_end(outs);

    while (1)
    {
        /* read a block of raw audio data... */
        g_memset(cptr, 0, 4);
        bytes_read = pa_simple_read(simple, cptr, AUDIO_BUF_SIZE, &error);

        if (bytes_read < 0)
        {
            LOG(0, ("read_raw_audio_data: ERROR reading from pulseaudio stream\n"));
            LOG(0, ("read_raw_audio_data: %s\n", pa_strerror(error)));
            break;
        }

        /* bug workaround:
           even when there is no audio data, pulseaudio is returning without
           errors but the data itself is zero; we use this zero data to
           determine that there is no audio data present */
        if (*cptr == 0 && *(cptr + 1) == 0 && *(cptr + 2) == 0 && *(cptr + 3) == 0)
        {
            g_sleep(10);
            continue;
        }

        if (trans_force_write_s(strans, outs) != 0)
        {
            LOG(0, ("read_raw_audio_data: ERROR writing audio data to server\n"));
            break;
        }
    }

    pa_simple_free(simple);
    trans_delete(strans);
    return NULL;
}

#endif
