/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Laxmikant Rashinkar 2012 LK.Rashinkar@gmail.com
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

#include "xrdpvr.h"
#include "xrdpvr_internal.h"

/* globals */
PLAYER_STATE_INFO g_psi;
int g_video_index = -1;
int g_audio_index = -1;

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
    if ((channel == NULL) || (stream_id <= 0) || (filename == NULL))
    {
        return -1;
    }

    /* send metadata from media file to client */
    if (xrdpvr_create_metadata_file(channel, filename))
    {
        printf("error sending metadata to client\n");
        return -1;
    }

    /* ask client to get video format from media file */
    if (xrdpvr_set_video_format(channel, 101))
    {
        printf("xrdpvr_set_video_format() failed\n");
        return -1;
    }

    /* TODO */
    sleep(3);

    /* ask client to get audio format from media file */
    if (xrdpvr_set_audio_format(channel, 101))
    {
        printf("xrdpvr_set_audio_format() failed\n");
        return 1;
    }
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
    av_free(g_psi.frame);
    avcodec_close(g_psi.p_audio_codec_ctx);
    avcodec_close(g_psi.p_video_codec_ctx);
    //avformat_close_input(&g_psi.p_format_ctx);
    av_close_input_file(g_psi.p_format_ctx);

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

    printf("$$$$$$ xrdpvr_play_media: setting audioTimeout & videoTimeout to -1\n");
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
        printf("ERRRO reading stream info\n");
        return -1;
    }

#if 0
    /* print media info to standard out */
    av_dump_format(g_psi.p_format_ctx, 0, filename, 0);
#endif

    /* find first audio / video stream */
    for (i = 0; i < g_psi.p_format_ctx->nb_streams; i++)
    {
        if (g_psi.p_format_ctx->streams[i]->codec->codec_type == CODEC_TYPE_VIDEO &&
                g_video_index < 0)
        {
            g_video_index = i;
        }

        if (g_psi.p_format_ctx->streams[i]->codec->codec_type == CODEC_TYPE_AUDIO &&
                g_audio_index < 0)
        {
            g_audio_index = i;
        }
    }

    if ((g_audio_index < 0) && (g_video_index < 0))
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
    g_psi.p_audio_codec = avcodec_find_decoder(g_psi.p_audio_codec_ctx->codec_id);

    if (g_psi.p_audio_codec == NULL)
    {
        printf("ERROR: audio codec not supported\n");
    }

    /* find decoder for video stream */
    g_psi.p_video_codec = avcodec_find_decoder(g_psi.p_video_codec_ctx->codec_id);

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

    /* open decoder for video stream */
    //if (avcodec_open2(g_psi.p_video_codec_ctx, g_psi.p_video_codec,
    //                  NULL) < 0)
    if (avcodec_open(g_psi.p_video_codec_ctx, g_psi.p_video_codec) < 0)
    {
        printf("ERROR: could not open video decoder\n");
        return -1;
    }

    return 0;
}

static int firstAudioPkt = 1;
static int firstVideoPkt = 1;

int xrdpvr_get_frame(void **av_pkt_ret, int *is_video_frame, int *delay_in_us)
{
    AVPacket *av_pkt;
    double    dts;

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
            //printf("##### first audio: dts=%f delay_in_ms=%d\n", dts, *delay_in_us);
        }
    }
    else if (av_pkt->stream_index == g_video_index)
    {
        dts = av_pkt->dts;

        //printf("$$$ video raw_dts=%f raw_pts=%f\n", (double) av_pkt->dts, (double) av_pkt->dts);

        dts *= av_q2d(g_psi.p_format_ctx->streams[g_video_index]->time_base);

        if (g_psi.videoTimeout < 0)
        {
            *delay_in_us = 1000 * 5;
            g_psi.videoTimeout = dts;
            //printf("$$$ negative: videoTimeout=%f\n", g_psi.videoTimeout);
        }
        else
        {
            //printf("$$$ positive: videoTimeout_b4 =%f\n", g_psi.videoTimeout);
            *delay_in_us = (int) ((dts - g_psi.videoTimeout) * 1000000);
            g_psi.videoTimeout = dts;
            //printf("$$$ positive: videoTimeout_aft=%f\n", g_psi.videoTimeout);
        }
        *is_video_frame = 1;

        if (firstVideoPkt)
        {
            firstVideoPkt = 0;
            //printf("$$$ first video: dts=%f delay_in_ms=%d\n", dts, *delay_in_us);
        }
    }

    *av_pkt_ret = av_pkt;
    return 0;
}

int send_audio_pkt(void *channel, int stream_id, void *pkt_p)
{
    AVPacket *av_pkt = (AVPacket *) pkt_p;

    xrdpvr_send_audio_data(channel, stream_id, av_pkt->size, av_pkt->data);
    av_free_packet(av_pkt);
    free(av_pkt);
}

int send_video_pkt(void *channel, int stream_id, void *pkt_p)
{
    AVPacket *av_pkt = (AVPacket *) pkt_p;

    xrdpvr_send_video_data(channel, stream_id, av_pkt->size, av_pkt->data);
    av_free_packet(av_pkt);
    free(av_pkt);
}

int xrdpvr_play_frame(void *channel, int stream_id, int *videoTimeout, int *audioTimeout)
{
    AVPacket av_pkt;
    double   dts;
    int      delay_in_us;

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

        //printf("audio delay: %d\n", delay_in_us);
        usleep(delay_in_us);
        //usleep(1000 * 1);
    }
    else if (av_pkt.stream_index == g_video_index)
    {
        xrdpvr_send_video_data(channel, stream_id, av_pkt.size, av_pkt.data);

        dts = av_pkt.dts;
        dts *= av_q2d(g_psi.p_format_ctx->streams[g_video_index]->time_base);

        //printf("xrdpvr_play_frame:video: saved=%f dts=%f\n", g_psi.videoTimeout, dts);

        *videoTimeout = (int) ((dts - g_psi.videoTimeout) * 1000000);
        *audioTimeout = -1;

        if (g_psi.videoTimeout > dts)
        {
            g_psi.videoTimeout = dts;
            delay_in_us = 1000 * 40;
            //printf("xrdpvr_play_frame:video1: saved=%f dts=%f delay_in_us=%d\n", g_psi.videoTimeout, dts, delay_in_us);
        }
        else
        {
            delay_in_us = (int) ((dts - g_psi.videoTimeout) * 1000000);
            g_psi.videoTimeout = dts;
            //printf("xrdpvr_play_frame:video2: saved=%f dts=%f delay_in_us=%d\n", g_psi.videoTimeout, dts, delay_in_us);
        }

        //printf("video delay: %d\n", delay_in_us);
        usleep(delay_in_us);
    }

    av_free_packet(&av_pkt);
    return 0;
}

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


    if(av_seek_frame(g_psi.p_format_ctx, g_video_index, seek_target, seek_flag) < 0)
    {
        printf("media seek error\n");
        return -1;
    }
    printf("xrdpvr_seek_media: success\n");
    return 0;
}

void
xrdpvr_get_media_duration(int64_t *start_time, int64_t *duration)
{
    *start_time = g_psi.p_format_ctx->start_time / AV_TIME_BASE;
    *duration = g_psi.p_format_ctx->duration / AV_TIME_BASE;
}

int
xrdpvr_set_geometry(void *channel, int stream_id, int xpos, int ypos, int width, int height)
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
xrdpvr_set_video_format(void *channel, uint32_t stream_id)
{
    STREAM  *s;
    char    *cptr;
    int     rv;
    int     len;

    stream_new(s, MAX_PDU_SIZE);

    stream_ins_u32_le(s, 0); /* number of bytes to follow */
    stream_ins_u32_le(s, CMD_SET_VIDEO_FORMAT);
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
xrdpvr_set_audio_format(void *channel, uint32_t stream_id)
{
    STREAM  *s;
    char    *cptr;
    int     rv;
    int     len;

    stream_new(s, MAX_PDU_SIZE);

    stream_ins_u32_le(s, 0); /* number of bytes to follow */
    stream_ins_u32_le(s, CMD_SET_AUDIO_FORMAT);
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
xrdpvr_send_video_data(void *channel, uint32_t stream_id, uint32_t data_len, uint8_t *data)
{
    STREAM  *s;
    char    *cptr;
    int     rv;
    int     len;

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
xrdpvr_send_audio_data(void *channel, uint32_t stream_id, uint32_t data_len, uint8_t *data)
{
    STREAM  *s;
    char    *cptr;
    int     rv;
    int     len;

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
        rv = WTSVirtualChannelWrite(channel, &s->data[index], bytes_to_send, &bytes_written);

        if (rv < 0)
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
