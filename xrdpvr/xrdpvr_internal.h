/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Laxmikant Rashinkar 2012-2013 LK.Rashinkar@gmail.com
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

#ifndef __XRDPVR_INTERNAL_H__
#define __XRDPVR_INTERNAL_H__

#include <stdint.h>
#include <sys/types.h>
#include <fcntl.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#if LIBAVCODEC_VERSION_MAJOR == 52 && LIBAVCODEC_VERSION_MINOR == 20
#define DISTRO_DEBIAN6
#endif

#if LIBAVCODEC_VERSION_MAJOR == 52 && LIBAVCODEC_VERSION_MINOR == 72
#define DISTRO_UBUNTU1104
#endif

#if LIBAVCODEC_VERSION_MAJOR == 53 && LIBAVCODEC_VERSION_MINOR == 35
#define DISTRO_UBUNTU1204
#endif

#if !defined(DISTRO_DEBIAN6) && !defined(DISTRO_UBUNTU1104) && !defined(DISTRO_UBUNTU1204)
#warning unsupported distro
#endif

#ifdef DISTRO_UBUNTU1204
#define CODEC_TYPE_VIDEO AVMEDIA_TYPE_VIDEO
#define CODEC_TYPE_AUDIO AVMEDIA_TYPE_AUDIO
#define PKT_FLAG_KEY AV_PKT_FLAG_KEY
#endif

#define MAX_BUFSIZE (1024 * 1024 * 8)

#define CMD_SET_VIDEO_FORMAT        1
#define CMD_SET_AUDIO_FORMAT        2
#define CMD_SEND_VIDEO_DATA         3
#define CMD_SEND_AUDIO_DATA         4
#define CMD_CREATE_META_DATA_FILE   5
#define CMD_CLOSE_META_DATA_FILE    6
#define CMD_WRITE_META_DATA         7
#define CMD_DEINIT_XRDPVR           8
#define CMD_SET_GEOMETRY            9
#define CMD_SET_VOLUME              10
#define CMD_INIT_XRDPVR             11

/* max number of bytes we can send in one pkt */
#define MAX_PDU_SIZE                1600

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;

typedef struct stream
{
    u8  *data;
    u8  *p;
    u32  size;
    int  from_buf;
} STREAM;

/**
 * create and init a new stream
 *
 * @param  _s     stream to create and init
 * @param  _len   number of bytes to store in stream
 ******************************************************************************/
#define stream_new(_s, _len)                          \
    do                                                \
    {                                                 \
        (_s) = (STREAM *) calloc(1, sizeof(STREAM));  \
        (_s)->data = (u8 *) calloc(1, (_len));        \
        (_s)->p = (_s)->data;                         \
        (_s)->size = (_len);                          \
        (_s)->from_buf = 0;                           \
    } while (0)

/**
 * create a stream from an existing buffer
 ******************************************************************************/
#define stream_from_buffer(_s, _buf, _buf_len)        \
    do                                                \
    {                                                 \
        (_s) = (STREAM *) calloc(1, sizeof(STREAM));  \
        (_s)->data = (u8 *) (_buf);                   \
        (_s)->p = (_s)->data;                         \
        (_s)->size = (_buf_len);                      \
        (_s)->from_buf = 1;                           \
    } while (0)

/**
 * release stream resources, including stream itself
 * note: release _s->data only if we allocated it
 *
 * @param  _s  the stream whose resources are to be released
 ******************************************************************************/
#define stream_free(_s)                               \
    do                                                \
    {                                                 \
        if (!(_s)->from_buf)                          \
        {                                             \
            free((_s)->data);                         \
        }                                             \
        free((_s));                                   \
        (_s) = NULL;                                  \
    } while (0)

/** return number of bytes in stream */
#define stream_length(_s) (int) ((_s)->p - (_s)->data)

/** insert a 8 bit value into stream */
#define stream_ins_u8(_s, _val)                       \
    do                                                \
    {                                                 \
        *(_s)->p++ = (unsigned char) (_val);          \
    } while(0)

/** insert a 16 bit value into stream */
#define stream_ins_u16_le(_s, _val)                   \
    do                                                \
    {                                                 \
        *(_s)->p++ = (unsigned char) ((_val) >> 0);   \
        *(_s)->p++ = (unsigned char) ((_val) >> 8);   \
    } while (0)

/** insert a 32 bit value into stream */
#define stream_ins_u32_le(_s, _val)                   \
    do                                                \
    {                                                 \
        *(_s)->p++ = (unsigned char) ((_val) >> 0);   \
        *(_s)->p++ = (unsigned char) ((_val) >> 8);   \
        *(_s)->p++ = (unsigned char) ((_val) >> 16);  \
        *(_s)->p++ = (unsigned char) ((_val) >> 24);  \
    } while (0)

/** insert a 64 bit value into stream */
#define stream_ins_u64_le(_s, _val)                   \
    do                                                \
    {                                                 \
        *(_s)->p++ = (unsigned char) ((_val) >> 0);   \
        *(_s)->p++ = (unsigned char) ((_val) >> 8);   \
        *(_s)->p++ = (unsigned char) ((_val) >> 16);  \
        *(_s)->p++ = (unsigned char) ((_val) >> 24);  \
        *(_s)->p++ = (unsigned char) ((_val) >> 32);  \
        *(_s)->p++ = (unsigned char) ((_val) >> 40);  \
        *(_s)->p++ = (unsigned char) ((_val) >> 48);  \
        *(_s)->p++ = (unsigned char) ((_val) >> 56);  \
    } while (0)

/** insert array of chars into stream */
#define stream_ins_byte_array(_s, _ba, _count)        \
    do                                                \
    {                                                 \
        memcpy((_s)->p, (_ba), (_count));             \
        (_s)->p += (_count);                          \
    } while (0)

/** extract a 8 bit value from stream */
#define stream_ext_u8(_s, _v)                         \
    do                                                \
    {                                                 \
        (_v) = (u8) *(_s)->p++;                       \
    } while (0)

/** extract a 16 bit value from stream */
#define stream_ext_u16_le(_s, _v)                     \
    do                                                \
    {                                                 \
        (_v) = (u16) ((_s)->p[1] << 8 | (_s)->p[0]);  \
        (_s)->p += 2;                                 \
    } while (0)

/** extract a 32 bit value from stream */
#define stream_ext_u32_le(_s, _v)                     \
    do                                                \
    {                                                 \
        (_v) = (u32) ((_s)->p[3] << 24 |              \
                      (_s)->p[2] << 16 |              \
                      (_s)->p[1] << 8  |              \
                      (_s)->p[0]);                    \
        (_s)->p += 4;                                 \
    } while (0)

typedef struct _player_state_info
{
    AVFormatContext *p_format_ctx;
    AVCodecContext  *p_audio_codec_ctx;
    AVCodecContext  *p_video_codec_ctx;
    AVCodec         *p_audio_codec;
    AVCodec         *p_video_codec;

    int              audio_stream_index;
    int              video_stream_index;
    double           audioTimeout;
    double           videoTimeout;

    /* LK_TODO delete this after we fix the problem */
    AVFrame         *frame;
    AVPacket        avpkt;

    AVBitStreamFilterContext *bsfc;

} PLAYER_STATE_INFO;

static int xrdpvr_read_from_client(void *channel, STREAM *s, int bytes, int timeout);
static int xrdpvr_write_to_client(void *channel, STREAM *s);

#endif /* __XRDPVR_INTERNAL_H__ */
