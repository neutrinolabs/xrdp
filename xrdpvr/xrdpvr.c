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
    avformat_close_input(&g_psi.p_format_ctx);

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
    AVDictionary *p_audio_opt_dict = NULL;
    AVDictionary *p_video_opt_dict = NULL;
    AVPacket      av_pkt;

    int video_index = -1;
    int audio_index = -1;
    int i;

    /* register all available fileformats and codecs */
    av_register_all();

    /* open media file - this will read just the header */
    if (avformat_open_input(&g_psi.p_format_ctx, filename, NULL, NULL))
    {
        printf("ERROR opening %s\n", filename);
        return -1;
    }

    /* now get the real stream info */
    if (avformat_find_stream_info(g_psi.p_format_ctx, NULL) < 0)
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
        if (g_psi.p_format_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO &&
                video_index < 0)
        {
            video_index = i;
        }

        if (g_psi.p_format_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO &&
                audio_index < 0)
        {
            audio_index = i;
        }
    }

    if ((audio_index < 0) && (video_index < 0))
    {
        /* close file and return with error */
        printf("ERROR: no audio/video stream found in %s\n", filename);
        avformat_close_input(&g_psi.p_format_ctx);
        return -1;
    }

    g_psi.audio_stream_index = audio_index;
    g_psi.video_stream_index = video_index;

    /* get pointers to codex contexts for both streams */
    g_psi.p_audio_codec_ctx = g_psi.p_format_ctx->streams[audio_index]->codec;
    g_psi.p_video_codec_ctx = g_psi.p_format_ctx->streams[video_index]->codec;

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
    if (avcodec_open2(g_psi.p_audio_codec_ctx, g_psi.p_audio_codec,
                      &p_audio_opt_dict) < 0)
    {
        printf("ERROR: could not open audio decoder\n");
        return -1;
    }

    /* open decoder for video stream */
    if (avcodec_open2(g_psi.p_video_codec_ctx, g_psi.p_video_codec,
                      &p_video_opt_dict) < 0)
    {
        printf("ERROR: could not open video decoder\n");
        return -1;
    }

    while (av_read_frame(g_psi.p_format_ctx, &av_pkt) >= 0)
    {
        if (av_pkt.stream_index == audio_index)
        {
            xrdpvr_send_audio_data(channel, stream_id, av_pkt.size, av_pkt.data);
            usleep(1000 * 1);
        }
        else if (av_pkt.stream_index == video_index)
        {
            xrdpvr_send_video_data(channel, stream_id, av_pkt.size, av_pkt.data);
            usleep(1000 * 40); // was 50
        }
    }

    av_free_packet(&av_pkt);

    return 0;
}

/******************************************************************************
 *
 * code below this is local to this file and cannot be accessed externally;
 * this code communicates with the xrdpvr plugin in NeutrinoRDP;
 * NeutrinoRDP is a fork of FreeRDP 1.0.1
 *
 *****************************************************************************/

/**
 * set video format
 *
 * @param  channel    opaque handle returned by WTSVirtualChannelOpenEx
 * @param  stream_id  unique identification number for this stream
 *
 * @return  0 on success, -1 on error
 *****************************************************************************/
static int
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
static int
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
static int
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
static int
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
static int
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
