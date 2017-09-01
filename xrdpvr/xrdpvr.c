/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Laxmikant Rashinkar 2012-2013 LK.Rashinkar@gmail.com
 * Copyright (C) Jay Sorg 2013 jay.sorg@gmail.com
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
 * a program that uses xrdpapi and ffmpeg to redirect media streams
 * to a FreeRDP client where it is decompressed and played locally
 *
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "xrdpvr.h"
#include "xrdpvr_internal.h"

/* globals */
PLAYER_STATE_INFO g_psi;
int g_video_index = -1;
int g_audio_index = -1;

/*****************************************************************************/
/* produce a hex dump */
void
hexdump(char *p, int len)
{
    unsigned char *line;
    int i;
    int thisline;
    int offset;

    line = (unsigned char *)p;
    offset = 0;

    while (offset < len)
    {
        printf("%04x ", offset);
        thisline = len - offset;

        if (thisline > 16)
        {
            thisline = 16;
        }

        for (i = 0; i < thisline; i++)
        {
            printf("%02x ", line[i]);
        }

        for (; i < 16; i++)
        {
            printf("   ");
        }

        for (i = 0; i < thisline; i++)
        {
            printf("%c", (line[i] >= 0x20 && line[i] < 0x7f) ? line[i] : '.');
        }

        printf("\n");
        offset += thisline;
        line += thisline;
    }
}

/**
 * initialize the media player
 *
 * @param  channel    opaque handle returned by WTSVirtualChannelOpenEx
 * @param  stream_id  unique identification number for this stream
 * @param  filename   media file to play
 *
 * @return  0 on success, -1 on error
 *****************************************************************************/
int
xrdpvr_init_player(void *channel, int stream_id, char *filename)
{
    printf("xrdpvr_init_player:\n");
    if ((channel == NULL) || (stream_id <= 0) || (filename == NULL))
    {
        return -1;
    }

    xrdpvr_send_init(channel);
}

/**
 * de-initialize the media player
 *
 * @param  channel    opaque handle returned by WTSVirtualChannelOpenEx
 * @param  stream_id  unique identification number for this stream
 *
 * @return  0 on success, -1 on error
 *****************************************************************************/
int
xrdpvr_deinit_player(void *channel, int stream_id)
{
    STREAM  *s;
    char    *cptr;
    int     rv;
    int     len;

    if ((channel == NULL) || (stream_id <= 0))
    {
        return -1;
    }

    /* do local clean up */
    if (g_psi.frame != 0)
    {
        av_free(g_psi.frame);
        g_psi.frame = 0;
    }
    if (g_psi.p_audio_codec_ctx != 0)
    {
        avcodec_close(g_psi.p_audio_codec_ctx);
        g_psi.p_audio_codec_ctx = 0;
    }
    if (g_psi.p_video_codec_ctx != 0)
    {
        avcodec_close(g_psi.p_video_codec_ctx);
        g_psi.p_video_codec_ctx = 0;
    }
    //avformat_close_input(&g_psi.p_format_ctx);
    if (g_psi.p_format_ctx != 0)
    {
        av_close_input_file(g_psi.p_format_ctx);
        g_psi.p_format_ctx = 0;
    }

    /* do remote cleanup */

    stream_new(s, MAX_PDU_SIZE);

    stream_ins_u32_le(s, 0); /* number of bytes to follow */
    stream_ins_u32_le(s, CMD_DEINIT_XRDPVR);
    stream_ins_u32_le(s, stream_id);

    /* insert number of bytes in stream */
    len = stream_length(s) - 4;
    cptr = s->p;
    s->p = s->data;
    stream_ins_u32_le(s, len);
    s->p = cptr;

    /* write data to virtual channel */
    rv = xrdpvr_write_to_client(channel, s);
    stream_free(s);

    return 0;
}

/**
 * play the media
 *
 * @param  channel    opaque handle returned by WTSVirtualChannelOpenEx
 * @param  stream_id  unique identification number for this stream
 * @param  filename   media file to play
 *
 * @return  0 on success, -1 on error
 *****************************************************************************/
int
xrdpvr_play_media(void *channel, int stream_id, char *filename)
{
    int i;

    printf("$$$$$$ xrdpvr_play_media: setting audioTimeout & "
           "videoTimeout to -1\n");
    g_psi.videoTimeout = -1;
    g_psi.audioTimeout = -1;

    /* register all available fileformats and codecs */
    av_register_all();

    /* open media file - this will read just the header */
    //if (avformat_open_input(&g_psi.p_format_ctx, filename, NULL, NULL))
    if (av_open_input_file(&g_psi.p_format_ctx, filename, NULL, 0, NULL))
    {
        printf("ERROR opening %s\n", filename);
        return -1;
    }

    /* now get the real stream info */
    //if (avformat_find_stream_info(g_psi.p_format_ctx, NULL) < 0)
    if (av_find_stream_info(g_psi.p_format_ctx) < 0)
    {
        printf("ERROR reading stream info\n");
        return -1;
    }

#if 1
    /* print media info to standard out */
    av_dump_format(g_psi.p_format_ctx, 0, filename, 0);
#endif

    printf("nb_streams %d\n", g_psi.p_format_ctx->nb_streams);

    g_audio_index = -1;
    g_video_index = -1;

    /* find first audio / video stream */
    for (i = 0; i < g_psi.p_format_ctx->nb_streams; i++)
    {
        if (g_psi.p_format_ctx->streams[i]->codec->codec_type ==
                                                     CODEC_TYPE_VIDEO &&
            g_psi.p_format_ctx->streams[i]->codec->codec_id ==
                                                     CODEC_ID_H264 &&
            g_video_index < 0)
        {
            g_video_index = i;
        }

        if (g_psi.p_format_ctx->streams[i]->codec->codec_type ==
                                                     CODEC_TYPE_AUDIO &&
            g_psi.p_format_ctx->streams[i]->codec->codec_id ==
                                                     CODEC_ID_AAC &&
            g_audio_index < 0)
        {
            g_audio_index = i;
        }
    }

    if ((g_audio_index < 0) || (g_video_index < 0))
    {
        /* close file and return with error */
        printf("ERROR: no audio/video stream found in %s\n", filename);
        //avformat_close_input(&g_psi.p_format_ctx);
        av_close_input_file(g_psi.p_format_ctx);
        return -1;
    }

    g_psi.audio_stream_index = g_audio_index;
    g_psi.video_stream_index = g_video_index;

    /* get pointers to codex contexts for both streams */
    g_psi.p_audio_codec_ctx = g_psi.p_format_ctx->streams[g_audio_index]->codec;
    g_psi.p_video_codec_ctx = g_psi.p_format_ctx->streams[g_video_index]->codec;

    /* find decoder for audio stream */
    g_psi.p_audio_codec =
           avcodec_find_decoder(g_psi.p_audio_codec_ctx->codec_id);

    if (g_psi.p_audio_codec == NULL)
    {
        printf("ERROR: audio codec not supported\n");
    }

    /* find decoder for video stream */
    g_psi.p_video_codec =
             avcodec_find_decoder(g_psi.p_video_codec_ctx->codec_id);

    if (g_psi.p_video_codec == NULL)
    {
        printf("ERROR: video codec not supported\n");
    }

    /* open decoder for audio stream */
    //if (avcodec_open2(g_psi.p_audio_codec_ctx, g_psi.p_audio_codec,
    //                  NULL) < 0)
    if (avcodec_open(g_psi.p_audio_codec_ctx, g_psi.p_audio_codec) < 0)
    {
        printf("ERROR: could not open audio decoder\n");
        return -1;
    }

    printf("%d\n", g_psi.p_audio_codec_ctx->extradata_size);
    hexdump(g_psi.p_audio_codec_ctx->extradata,
            g_psi.p_audio_codec_ctx->extradata_size);
    printf("%d %d %d %d\n", g_psi.p_audio_codec_ctx->sample_rate,
           g_psi.p_audio_codec_ctx->bit_rate,
           g_psi.p_audio_codec_ctx->channels,
           g_psi.p_audio_codec_ctx->block_align);

    /* open decoder for video stream */
    //if (avcodec_open2(g_psi.p_video_codec_ctx, g_psi.p_video_codec,
    //                  NULL) < 0)
    if (avcodec_open(g_psi.p_video_codec_ctx, g_psi.p_video_codec) < 0)
    {
        printf("ERROR: could not open video decoder\n");
        return -1;
    }

    g_psi.bsfc = av_bitstream_filter_init("h264_mp4toannexb");
    printf("g_psi.bsfc %p\n", g_psi.bsfc);

    if (xrdpvr_set_video_format(channel, 101, 0,
                                g_psi.p_video_codec_ctx->width,
                                g_psi.p_video_codec_ctx->height))
    {
        printf("xrdpvr_set_video_format() failed\n");
        return -1;
    }

    printf("xrdpvr_play_media: calling xrdpvr_set_audio_format\n");
    if (xrdpvr_set_audio_format(channel, 101, 0,
                                g_psi.p_audio_codec_ctx->extradata,
                                g_psi.p_audio_codec_ctx->extradata_size,
                                g_psi.p_audio_codec_ctx->sample_rate,
                                g_psi.p_audio_codec_ctx->bit_rate,
                                g_psi.p_audio_codec_ctx->channels,
                                g_psi.p_audio_codec_ctx->block_align))
    {
        printf("xrdpvr_set_audio_format() failed\n");
        return 1;
    }

    return 0;
}

static int firstAudioPkt = 1;
static int firstVideoPkt = 1;

/******************************************************************************/
int
xrdpvr_get_frame(void **av_pkt_ret, int *is_video_frame, int *delay_in_us)
{
    AVPacket                 *av_pkt;
    double                    dts;
    int                       error;
    AVBitStreamFilterContext *bsfc;
    AVPacket                  new_pkt;

    //printf("xrdpvr_get_frame:\n");
    /* alloc an AVPacket */
    if ((av_pkt = (AVPacket *) malloc(sizeof(AVPacket))) == NULL)
        return -1;

    /* read one frame into AVPacket */
    if (av_read_frame(g_psi.p_format_ctx, av_pkt) < 0)
    {
        free(av_pkt);
        return -1;
    }

    if (av_pkt->stream_index == g_audio_index)
    {
        /* got an audio packet */
        dts = av_pkt->dts;
        dts *= av_q2d(g_psi.p_format_ctx->streams[g_audio_index]->time_base);

        if (g_psi.audioTimeout < 0)
        {
            *delay_in_us = 1000 * 5;
            g_psi.audioTimeout = dts;
        }
        else
        {
            *delay_in_us = (int) ((dts - g_psi.audioTimeout) * 1000000);
            g_psi.audioTimeout = dts;
        }
        *is_video_frame = 0;

        if (firstAudioPkt)
        {
            firstAudioPkt = 0;
        }
    }
    else if (av_pkt->stream_index == g_video_index)
    {

        bsfc = g_psi.bsfc;
        while (bsfc != 0)
        {
            new_pkt = *av_pkt;
            error = av_bitstream_filter_filter(bsfc, g_psi.p_video_codec_ctx, 0,
                                               &new_pkt.data, &new_pkt.size,
                                               av_pkt->data, av_pkt->size,
                                               av_pkt->flags & PKT_FLAG_KEY);
            if (error > 0)
            {
                av_free_packet(av_pkt);
                new_pkt.destruct = av_destruct_packet;
            }
            else if (error < 0)
            {
                printf("bitstream filter error\n");
            }
            *av_pkt = new_pkt;
            bsfc = bsfc->next;
        }

        dts = av_pkt->dts;

        dts *= av_q2d(g_psi.p_format_ctx->streams[g_video_index]->time_base);

        if (g_psi.videoTimeout < 0)
        {
            *delay_in_us = 1000 * 5;
            g_psi.videoTimeout = dts;
        }
        else
        {
            *delay_in_us = (int) ((dts - g_psi.videoTimeout) * 1000000);
            g_psi.videoTimeout = dts;
        }
        *is_video_frame = 1;

        if (firstVideoPkt)
        {
            firstVideoPkt = 0;
        }
    }

    *av_pkt_ret = av_pkt;
    return 0;
}

/******************************************************************************/
int
send_audio_pkt(void *channel, int stream_id, void *pkt_p)
{
    AVPacket *av_pkt = (AVPacket *) pkt_p;

    xrdpvr_send_audio_data(channel, stream_id, av_pkt->size, av_pkt->data);
    av_free_packet(av_pkt);
    free(av_pkt);
}

/******************************************************************************/
int
send_video_pkt(void *channel, int stream_id, void *pkt_p)
{
    AVPacket *av_pkt = (AVPacket *) pkt_p;

    xrdpvr_send_video_data(channel, stream_id, av_pkt->size, av_pkt->data);
    av_free_packet(av_pkt);
    free(av_pkt);
}

/******************************************************************************/
int
xrdpvr_play_frame(void *channel, int stream_id, int *videoTimeout,
                  int *audioTimeout)
{
    AVPacket                  av_pkt;
    double                    dts;
    int                       delay_in_us;
    int                       error;
    AVBitStreamFilterContext *bsfc;
    AVPacket                  new_pkt;

    //printf("xrdpvr_play_frame:\n");

    if (av_read_frame(g_psi.p_format_ctx, &av_pkt) < 0)
    {
        printf("xrdpvr_play_frame: av_read_frame failed\n");
        return -1;
    }

    if (av_pkt.stream_index == g_audio_index)
    {
        xrdpvr_send_audio_data(channel, stream_id, av_pkt.size, av_pkt.data);

        dts = av_pkt.dts;
        dts *= av_q2d(g_psi.p_format_ctx->streams[g_audio_index]->time_base);

        *audioTimeout = (int) ((dts - g_psi.audioTimeout) * 1000000);
        *videoTimeout = -1;

        if (g_psi.audioTimeout > dts)
        {
            g_psi.audioTimeout = dts;
            delay_in_us = 1000 * 40;
        }
        else
        {
            delay_in_us = (int) ((dts - g_psi.audioTimeout) * 1000000);
            g_psi.audioTimeout = dts;
        }

        printf("audio delay: %d\n", delay_in_us);
        usleep(delay_in_us);
        //usleep(1000 * 1);
    }
    else if (av_pkt.stream_index == g_video_index)
    {
        bsfc = g_psi.bsfc;
        while (bsfc != 0)
        {
            new_pkt= av_pkt;
            error = av_bitstream_filter_filter(bsfc, g_psi.p_video_codec_ctx, 0,
                                               &new_pkt.data, &new_pkt.size,
                                               av_pkt.data, av_pkt.size,
                                               av_pkt.flags & PKT_FLAG_KEY);
            if (error > 0)
            {
                av_free_packet(&av_pkt);
                new_pkt.destruct = av_destruct_packet;
            }
            else if (error < 0)
            {
                printf("bitstream filter error\n");
            }
            av_pkt = new_pkt;
            bsfc = bsfc->next;
        }

        xrdpvr_send_video_data(channel, stream_id, av_pkt.size, av_pkt.data);

        dts = av_pkt.dts;
        dts *= av_q2d(g_psi.p_format_ctx->streams[g_video_index]->time_base);

        *videoTimeout = (int) ((dts - g_psi.videoTimeout) * 1000000);
        *audioTimeout = -1;

        if (g_psi.videoTimeout > dts)
        {
            g_psi.videoTimeout = dts;
            delay_in_us = 1000 * 40;
        }
        else
        {
            delay_in_us = (int) ((dts - g_psi.videoTimeout) * 1000000);
            g_psi.videoTimeout = dts;
        }

        printf("video delay: %d\n", delay_in_us);
        usleep(delay_in_us);
    }

    av_free_packet(&av_pkt);
    return 0;
}

/******************************************************************************/
int
xrdpvr_seek_media(int64_t pos, int backward)
{
    int64_t seek_target;
    int     seek_flag;

    printf("xrdpvr_seek_media() entered\n");

    g_psi.audioTimeout = -1;
    g_psi.videoTimeout = -1;

    seek_flag = (backward) ? AVSEEK_FLAG_BACKWARD : 0;

    seek_target = av_rescale_q(pos * AV_TIME_BASE,
                               AV_TIME_BASE_Q,
                      g_psi.p_format_ctx->streams[g_video_index]->time_base);


    if (av_seek_frame(g_psi.p_format_ctx, g_video_index, seek_target,
                     seek_flag) < 0)
    {
        printf("media seek error\n");
        return -1;
    }
    printf("xrdpvr_seek_media: success\n");
    return 0;
}

/******************************************************************************/
void
xrdpvr_get_media_duration(int64_t *start_time, int64_t *duration)
{
    *start_time = g_psi.p_format_ctx->start_time / AV_TIME_BASE;
    *duration = g_psi.p_format_ctx->duration / AV_TIME_BASE;
}

/******************************************************************************/
int
xrdpvr_set_geometry(void *channel, int stream_id, int xpos, int ypos,
                    int width, int height)
{
    STREAM  *s;
    char    *cptr;
    int     rv;
    int     len;

    printf("xrdpvr_set_geometry: entered; x=%d y=%d\n", xpos, ypos);

    stream_new(s, MAX_PDU_SIZE);

    stream_ins_u32_le(s, 0); /* number of bytes to follow */
    stream_ins_u32_le(s, CMD_SET_GEOMETRY);
    stream_ins_u32_le(s, stream_id);
    stream_ins_u32_le(s, xpos);
    stream_ins_u32_le(s, ypos);
    stream_ins_u32_le(s, width);
    stream_ins_u32_le(s, height);

    /* insert number of bytes in stream */
    len = stream_length(s) - 4;
    cptr = s->p;
    s->p = s->data;
    stream_ins_u32_le(s, len);
    s->p = cptr;

    /* write data to virtual channel */
    rv = xrdpvr_write_to_client(channel, s);
    stream_free(s);
    return rv;
}

/**
 * set video format
 *
 * @param  channel    opaque handle returned by WTSVirtualChannelOpenEx
 * @param  stream_id  unique identification number for this stream
 *
 * @return  0 on success, -1 on error
 *****************************************************************************/
int
xrdpvr_set_video_format(void *channel, uint32_t stream_id, int format,
                        int width, int height)
{
    STREAM  *s;
    char    *cptr;
    int     rv;
    int     len;

    width = (width + 15) & ~15;
    stream_new(s, MAX_PDU_SIZE);
    stream_ins_u32_le(s, 0); /* number of bytes to follow */
    stream_ins_u32_le(s, CMD_SET_VIDEO_FORMAT);
    stream_ins_u32_le(s, stream_id);
    stream_ins_u32_le(s, format);
    stream_ins_u32_le(s, width);
    stream_ins_u32_le(s, height);

    /* insert number of bytes in stream */
    len = stream_length(s) - 4;
    cptr = s->p;
    s->p = s->data;
    stream_ins_u32_le(s, len);
    s->p = cptr;

    /* write data to virtual channel */
    rv = xrdpvr_write_to_client(channel, s);
    stream_free(s);
    return rv;
}

/**
 * set audio format
 *
 * @param  channel    opaque handle returned by WTSVirtualChannelOpenEx
 * @param  stream_id  unique identification number for this stream
 *
 * @return  0 on success, -1 on error
 *****************************************************************************/
int
xrdpvr_set_audio_format(void *channel, uint32_t stream_id, int format,
                        char *extradata, int extradata_size, int sample_rate,
                        int bit_rate, int channels, int block_align)
{
    STREAM  *s;
    char    *cptr;
    int     rv;
    int     len;

    stream_new(s, MAX_PDU_SIZE);

    printf("extradata_size %d sample_rate %d bit_rate %d channels %d "
           "block_align %d\n", extradata_size, sample_rate, bit_rate,
           channels, block_align);

    stream_ins_u32_le(s, 0); /* number of bytes to follow */
    stream_ins_u32_le(s, CMD_SET_AUDIO_FORMAT);
    stream_ins_u32_le(s, stream_id);
    stream_ins_u32_le(s, format);
    stream_ins_u32_le(s, extradata_size);
    memcpy(s->p, extradata, extradata_size);
    s->p += extradata_size;
    stream_ins_u32_le(s, sample_rate);
    stream_ins_u32_le(s, bit_rate);
    stream_ins_u32_le(s, channels);
    stream_ins_u32_le(s, block_align);

    /* insert number of bytes in stream */
    len = stream_length(s) - 4;
    cptr = s->p;
    s->p = s->data;
    stream_ins_u32_le(s, len);
    s->p = cptr;

    /* write data to virtual channel */
    rv = xrdpvr_write_to_client(channel, s);
    stream_free(s);
    return rv;
}

/**
 * send video data to client
 *
 * @param  channel    opaque handle returned by WTSVirtualChannelOpenEx
 * @param  stream_id  unique identification number for this stream
 * @param  data_len   number of bytes to send
 * @param  data       video data to send
 *
 * @return  0 on success, -1 on error
 *****************************************************************************/
int
xrdpvr_send_video_data(void *channel, uint32_t stream_id,
                       uint32_t data_len, uint8_t *data)
{
    STREAM  *s;
    char    *cptr;
    int     rv;
    int     len;

    //printf("xrdpvr_send_video_data:\n");
    stream_new(s, MAX_PDU_SIZE + data_len);

    stream_ins_u32_le(s, 0); /* number of bytes to follow */
    stream_ins_u32_le(s, CMD_SEND_VIDEO_DATA);
    stream_ins_u32_le(s, stream_id);
    stream_ins_u32_le(s, data_len);
    stream_ins_byte_array(s, data, data_len);

    /* insert number of bytes in stream */
    len = stream_length(s) - 4;
    cptr = s->p;
    s->p = s->data;
    stream_ins_u32_le(s, len);
    s->p = cptr;

    /* write data to virtual channel */
    rv = xrdpvr_write_to_client(channel, s);
    stream_free(s);

#ifdef DEBUG_FRAGMENTS
    printf("### sent %d + 4 bytes video data to client\n", len);
#endif

    return rv;
}

/**
 * send audio data to client
 *
 * @param  channel    opaque handle returned by WTSVirtualChannelOpenEx
 * @param  stream_id  unique identification number for this stream
 * @param  data_len   number of bytes to send
 * @param  data       audio data to send
 *
 * @return  0 on success, -1 on error
 *****************************************************************************/
int
xrdpvr_send_audio_data(void *channel, uint32_t stream_id, uint32_t data_len,
                       uint8_t *data)
{
    STREAM  *s;
    char    *cptr;
    int     rv;
    int     len;

    //printf("xrdpvr_send_audio_data:\n");
    stream_new(s, MAX_PDU_SIZE + data_len);

    stream_ins_u32_le(s, 0); /* number of bytes to follow */
    stream_ins_u32_le(s, CMD_SEND_AUDIO_DATA);
    stream_ins_u32_le(s, stream_id);
    stream_ins_u32_le(s, data_len);
    stream_ins_byte_array(s, data, data_len);

    /* insert number of bytes in stream */
    len = stream_length(s) - 4;
    cptr = s->p;
    s->p = s->data;
    stream_ins_u32_le(s, len);
    s->p = cptr;

    /* write data to virtual channel */
    rv = xrdpvr_write_to_client(channel, s);
    stream_free(s);

    return rv;
}

/**
 * send media meta-data to client
 *
 * @param  channel    opaque handle returned by WTSVirtualChannelOpenEx
 * @param  filename   media file
 *
 * @return  0 on success, -1 on error
 *****************************************************************************/
int
xrdpvr_create_metadata_file(void *channel, char *filename)
{
    STREAM  *s;
    char    *cptr;
    int     rv;
    int     len;
    int     fd;

    if ((fd = open(filename , O_RDONLY)) < 0)
    {
        return -1;
    }

    stream_new(s, MAX_PDU_SIZE + 1048576);

    /* send CMD_CREATE_META_DATA_FILE */
    stream_ins_u32_le(s, 4); /* number of bytes to follow */
    stream_ins_u32_le(s, CMD_CREATE_META_DATA_FILE);

    if (xrdpvr_write_to_client(channel, s))
    {
        close(fd);
        return -1;
    }

    /* read first 1MB of file and send to client */
    s->p = s->data;
    stream_ins_u32_le(s, 0); /* number of bytes to follow */
    stream_ins_u32_le(s, CMD_WRITE_META_DATA);
    stream_ins_u32_le(s, 0); /* number of bytes to follow */

    if ((rv = read(fd, s->p, 1048576)) <= 0)
    {
        close(fd);
        return -1;
    }

    s->p += rv;

    /* insert number of bytes in stream */
    len = stream_length(s) - 4;
    cptr = s->p;
    s->p = s->data;
    stream_ins_u32_le(s, len); /* number of bytes in this cmd */
    s->p += 4;
    stream_ins_u32_le(s, rv); /* number of metadata bytes */
    s->p = cptr;

    /* write data to virtual channel */
    rv = xrdpvr_write_to_client(channel, s);

    /* send CMD_CLOSE_META_DATA_FILE */
    s->p = s->data;
    stream_ins_u32_le(s, 4); /* number of bytes to follow */
    stream_ins_u32_le(s, CMD_CLOSE_META_DATA_FILE);

    if (xrdpvr_write_to_client(channel, s))
    {
        close(fd);
        return -1;
    }

    stream_free(s);
    return rv;
}

/**
 ******************************************************************************/
static int
xrdpvr_read_from_client(void *channel, STREAM *s, int bytes, int timeout)
{
    unsigned int bytes_read;
    int total_read;
    int ok;

    //printf("xrdpvr_read_from_client:\n");
    total_read = 0;
    while (total_read < bytes)
    {
        //printf("xrdpvr_read_from_client: loop\n");
        bytes_read = bytes - total_read;
        ok = WTSVirtualChannelRead(channel, timeout, s->p, bytes_read,
                                   &bytes_read);
        //printf("xrdpvr_read_from_client: loop ok %d\n", ok);
        if (ok)
        {
            //printf("xrdpvr_read_from_client: bytes_read %d\n", bytes_read);
            total_read += bytes_read;
            s->p += bytes_read;
        }
    }
    return 0;
}

/**
 * write data to a xrdpvr client
 *
 * @param  channel  opaque handle returned by WTSVirtualChannelOpenEx
 * @param  s        structure containing data to write
 *
 * @return 0 on success, -1 on failure
 ******************************************************************************/
static int
xrdpvr_write_to_client(void *channel, STREAM *s)
{
    int bytes_to_send;
    int bytes_written;
    int index = 0;
    int rv;

    if ((channel == NULL) || (s == NULL))
    {
        return -1;
    }

    bytes_to_send = stream_length(s);

    while (1)
    {
        rv = WTSVirtualChannelWrite(channel, &s->data[index], bytes_to_send,
                                    &bytes_written);

        if (rv == 0)
        {
            return -1;
        }

        index += bytes_written;
        bytes_to_send -= bytes_written;

        if ((rv == 0) && (bytes_to_send == 0))
        {
            return 0;
        }

        usleep(1000 * 3);
    }
}

/**
 * write set volume to a xrdpvr client
 *
 * @param  channel  opaque handle returned by WTSVirtualChannelOpenEx
 * @param  volume   volume 0x0000 to 0xffff
 *
 * @return 0 on success, -1 on failure
 ******************************************************************************/
int
xrdpvr_set_volume(void *channel, int volume)
{
    STREAM  *s;
    char    *cptr;
    int     rv;
    int     len;

    stream_new(s, MAX_BUFSIZE);

    stream_ins_u32_le(s, 0); /* number of bytes to follow */
    stream_ins_u32_le(s, CMD_SET_VOLUME);
    stream_ins_u32_le(s, volume);

    /* insert number of bytes in stream */
    len = stream_length(s) - 4;
    cptr = s->p;
    s->p = s->data;
    stream_ins_u32_le(s, len);
    s->p = cptr;

    /* write data to virtual channel */
    rv = xrdpvr_write_to_client(channel, s);
    stream_free(s);
    return rv;
}

int
xrdpvr_send_init(void *channel)
{
    STREAM  *s;
    char    *cptr;
    int     rv;
    int     len;

    printf("xrdpvr_send_init:\n");
    stream_new(s, MAX_BUFSIZE);

    stream_ins_u32_le(s, 0); /* number of bytes to follow */
    stream_ins_u32_le(s, CMD_INIT_XRDPVR);

    /* insert number of bytes in stream */
    len = stream_length(s) - 4;
    cptr = s->p;
    s->p = s->data;
    stream_ins_u32_le(s, len);
    s->p = cptr;

    /* write data to virtual channel */
    rv = xrdpvr_write_to_client(channel, s);
    stream_free(s);
    return rv;
}

int
xrdpvr_read_ack(void *channel, int *frame)
{
    STREAM  *s;

    stream_new(s, MAX_PDU_SIZE);
    xrdpvr_read_from_client(channel, s, 4, 1000);
    s->p = s->data;
    stream_ext_u32_le(s, *frame);
    stream_free(s);
    return 0;
}
