/**
 * FreeRDP: A Remote Desktop Protocol client.
 * RemoteFX Codec Library
 *
 * Copyright 2011 Vic Lee
 * Copyright 2015 Jay Sorg <jay.sorg@gmail.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rfxcodec_encode.h>

#include "rfxcommon.h"
#include "rfxencode.h"
#include "rfxconstants.h"
#include "rfxencode_tile.h"

#define LLOG_LEVEL 1
#define LLOGLN(_level, _args) \
    do { if (_level < LLOG_LEVEL) { printf _args ; printf("\n"); } } while (0)

/*
 * LL3, LH3, HL3, HH3, LH2, HL2, HH2, LH1, HL1, HH1
 */
static const unsigned char g_rfx_default_quantization_values[] =
{
    0x66, 0x66, 0x77, 0x88, 0x98
};

/******************************************************************************/
static int
rfx_compose_message_sync(struct rfxencode* enc, STREAM* s)
{
    if (stream_get_left(s) < 12)
    {
        return 1;
    }
    stream_write_uint16(s, WBT_SYNC); /* BlockT.blockType */
    stream_write_uint32(s, 12); /* BlockT.blockLen */
    stream_write_uint32(s, WF_MAGIC); /* magic */
    stream_write_uint16(s, WF_VERSION_1_0); /* version */
    return 0;
}

/******************************************************************************/
static int
rfx_compose_message_context(struct rfxencode* enc, STREAM* s)
{
    uint16 properties;
    int rlgr;

    if (stream_get_left(s) < 13)
    {
        return 1;
    }
    stream_write_uint16(s, WBT_CONTEXT); /* CodecChannelT.blockType */
    stream_write_uint32(s, 13); /* CodecChannelT.blockLen */
    stream_write_uint8(s, 1); /* CodecChannelT.codecId */
    stream_write_uint8(s, 0); /* CodecChannelT.channelId */
    stream_write_uint8(s, 0); /* ctxId */
    stream_write_uint16(s, CT_TILE_64x64); /* tileSize */

    /* properties */
    properties = enc->flags; /* flags */
    properties |= (COL_CONV_ICT << 3); /* cct */
    properties |= (CLW_XFORM_DWT_53_A << 5); /* xft */
    rlgr = enc->mode == RLGR1 ? CLW_ENTROPY_RLGR1 : CLW_ENTROPY_RLGR3;
    properties |= rlgr << 9; /* et */
    properties |= (SCALAR_QUANTIZATION << 13); /* qt */
    stream_write_uint16(s, properties);

    /* properties in tilesets: note that this has different format from
     * the one in TS_RFX_CONTEXT */
    properties = 1; /* lt */
    properties |= (enc->flags << 1); /* flags */
    properties |= (COL_CONV_ICT << 4); /* cct */
    properties |= (CLW_XFORM_DWT_53_A << 6); /* xft */
    properties |= rlgr << 10; /* et */
    properties |= (SCALAR_QUANTIZATION << 14); /* qt */
    enc->properties = properties;
    return 0;
}

/******************************************************************************/
static int
rfx_compose_message_codec_versions(struct rfxencode* enc, STREAM* s)
{
    if (stream_get_left(s) < 10)
    {
        return 1;
    }
    stream_write_uint16(s, WBT_CODEC_VERSIONS); /* BlockT.blockType */
    stream_write_uint32(s, 10); /* BlockT.blockLen */
    stream_write_uint8(s, 1); /* numCodecs */
    stream_write_uint8(s, 1); /* codecs.codecId */
    stream_write_uint16(s, WF_VERSION_1_0); /* codecs.version */
    return 0;
}

/******************************************************************************/
static int
rfx_compose_message_channels(struct rfxencode* enc, STREAM* s)
{
    if (stream_get_left(s) < 12)
    {
        return 1;
    }
    stream_write_uint16(s, WBT_CHANNELS); /* BlockT.blockType */
    stream_write_uint32(s, 12); /* BlockT.blockLen */
    stream_write_uint8(s, 1); /* numChannels */
    stream_write_uint8(s, 0); /* Channel.channelId */
    stream_write_uint16(s, enc->width); /* Channel.width */
    stream_write_uint16(s, enc->height); /* Channel.height */
    return 0;
}

/******************************************************************************/
int
rfx_compose_message_header(struct rfxencode* enc, STREAM* s)
{
    if (rfx_compose_message_sync(enc, s) != 0)
    {
        return 1;
    }
    if (rfx_compose_message_context(enc, s) != 0)
    {
        return 1;
    }
    if (rfx_compose_message_codec_versions(enc, s) != 0)
    {
        return 1;
    }
    if (rfx_compose_message_channels(enc, s) != 0)
    {
        return 1;
    }
    enc->header_processed = 1;
    return 0;
}

/******************************************************************************/
static int
rfx_compose_message_frame_begin(struct rfxencode* enc, STREAM* s)
{
    if (stream_get_left(s) < 14)
    {
        return 1;
    }
    stream_write_uint16(s, WBT_FRAME_BEGIN); /* CodecChannelT.blockType */
    stream_write_uint32(s, 14); /* CodecChannelT.blockLen */
    stream_write_uint8(s, 1); /* CodecChannelT.codecId */
    stream_write_uint8(s, 0); /* CodecChannelT.channelId */
    stream_write_uint32(s, enc->frame_idx); /* frameIdx */
    stream_write_uint16(s, 1); /* numRegions */
    enc->frame_idx++;
    return 0;
}

/******************************************************************************/
static int
rfx_compose_message_region(struct rfxencode* enc, STREAM* s,
                           struct rfx_rect *regions, int num_regions)
{
    int size;
    int i;

    size = 15 + num_regions * 8;
    if (stream_get_left(s) < size)
    {
        return 1;
    }
    stream_write_uint16(s, WBT_REGION); /* CodecChannelT.blockType */
    stream_write_uint32(s, size); /* set CodecChannelT.blockLen later */
    stream_write_uint8(s, 1); /* CodecChannelT.codecId */
    stream_write_uint8(s, 0); /* CodecChannelT.channelId */
    stream_write_uint8(s, 1); /* regionFlags */
    stream_write_uint16(s, num_regions); /* numRects */
    for (i = 0; i < num_regions; i++)
    {
        stream_write_uint16(s, regions[i].x);
        stream_write_uint16(s, regions[i].y);
        stream_write_uint16(s, regions[i].cx);
        stream_write_uint16(s, regions[i].cy);
    }
    stream_write_uint16(s, CBT_REGION); /* regionType */
    stream_write_uint16(s, 1); /* numTilesets */
    return 0;
}

/******************************************************************************/
static int
rfx_compose_message_tile_yuv(struct rfxencode *enc, STREAM *s,
                             char *tile_data, int tile_width, int tile_height,
                             int stride_bytes, const char *quantVals,
                             int quantIdxY, int quantIdxCb, int quantIdxCr,
                             int xIdx, int yIdx)
{
    int YLen = 0;
    int CbLen = 0;
    int CrLen = 0;
    int start_pos;
    int end_pos;

    start_pos = stream_get_pos(s);
    stream_write_uint16(s, CBT_TILE); /* BlockT.blockType */
    stream_seek_uint32(s); /* set BlockT.blockLen later */
    stream_write_uint8(s, quantIdxY);
    stream_write_uint8(s, quantIdxCb);
    stream_write_uint8(s, quantIdxCr);
    stream_write_uint16(s, xIdx);
    stream_write_uint16(s, yIdx);
    stream_seek(s, 6); /* YLen, CbLen, CrLen */
    if (rfx_encode_yuv(enc, tile_data, tile_width, tile_height,
                       stride_bytes,
                       quantVals + quantIdxY * 5,
                       quantVals + quantIdxCb * 5,
                       quantVals + quantIdxCr * 5,
                       s, &YLen, &CbLen, &CrLen) != 0)
    {
        return 1;
    }
    end_pos = stream_get_pos(s);
    stream_set_pos(s, start_pos + 2);
    stream_write_uint32(s, 19 + YLen + CbLen + CrLen); /* BlockT.blockLen */
    stream_set_pos(s, start_pos + 13);
    stream_write_uint16(s, YLen);
    stream_write_uint16(s, CbLen);
    stream_write_uint16(s, CrLen);
    stream_set_pos(s, end_pos);
    return 0;
}

/******************************************************************************/
static int
rfx_compose_message_tile_yuva(struct rfxencode *enc, STREAM *s,
                              char *tile_data, int tile_width, int tile_height,
                              int stride_bytes, const char *quantVals,
                              int quantIdxY, int quantIdxCb, int quantIdxCr,
                              int xIdx, int yIdx)
{
    int YLen = 0;
    int CbLen = 0;
    int CrLen = 0;
    int ALen = 0;
    int start_pos;
    int end_pos;

    start_pos = stream_get_pos(s);
    stream_write_uint16(s, CBT_TILE); /* BlockT.blockType */
    stream_seek_uint32(s); /* set BlockT.blockLen later */
    stream_write_uint8(s, quantIdxY);
    stream_write_uint8(s, quantIdxCb);
    stream_write_uint8(s, quantIdxCr);
    stream_write_uint16(s, xIdx);
    stream_write_uint16(s, yIdx);
    stream_seek(s, 8); /* YLen, CbLen, CrLen, ALen */
    if (rfx_encode_yuva(enc, tile_data, tile_width, tile_height,
                        stride_bytes,
                        quantVals + quantIdxY * 5,
                        quantVals + quantIdxCb * 5,
                        quantVals + quantIdxCr * 5,
                        s, &YLen, &CbLen, &CrLen, &ALen) != 0)
    {
        return 1;
    }
    end_pos = stream_get_pos(s);
    stream_set_pos(s, start_pos + 2);
    stream_write_uint32(s, 19 + YLen + CbLen + CrLen + ALen); /* BlockT.blockLen */
    stream_set_pos(s, start_pos + 13);
    stream_write_uint16(s, YLen);
    stream_write_uint16(s, CbLen);
    stream_write_uint16(s, CrLen);
    stream_write_uint16(s, ALen);
    stream_set_pos(s, end_pos);
    return 0;
}

/******************************************************************************/
static int
rfx_compose_message_tile_rgb(struct rfxencode *enc, STREAM *s,
                             char *tile_data, int tile_width, int tile_height,
                             int stride_bytes, const char *quantVals,
                             int quantIdxY, int quantIdxCb, int quantIdxCr,
                             int xIdx, int yIdx)
{
    int YLen = 0;
    int CbLen = 0;
    int CrLen = 0;
    int start_pos;
    int end_pos;

    start_pos = stream_get_pos(s);
    stream_write_uint16(s, CBT_TILE); /* BlockT.blockType */
    stream_seek_uint32(s); /* set BlockT.blockLen later */
    stream_write_uint8(s, quantIdxY);
    stream_write_uint8(s, quantIdxCb);
    stream_write_uint8(s, quantIdxCr);
    stream_write_uint16(s, xIdx);
    stream_write_uint16(s, yIdx);
    stream_seek(s, 6); /* YLen, CbLen, CrLen */
    if (rfx_encode_rgb(enc, tile_data, tile_width, tile_height,
                       stride_bytes,
                       quantVals + quantIdxY * 5,
                       quantVals + quantIdxCb * 5,
                       quantVals + quantIdxCr * 5,
                       s, &YLen, &CbLen, &CrLen) != 0)
    {
        return 1;
    }
    end_pos = stream_get_pos(s);
    stream_set_pos(s, start_pos + 2);
    stream_write_uint32(s, 19 + YLen + CbLen + CrLen); /* BlockT.blockLen */
    stream_set_pos(s, start_pos + 13);
    stream_write_uint16(s, YLen);
    stream_write_uint16(s, CbLen);
    stream_write_uint16(s, CrLen);
    stream_set_pos(s, end_pos);
    return 0;
}

/******************************************************************************/
static int
rfx_compose_message_tile_argb(struct rfxencode *enc, STREAM *s,
                              char *tile_data, int tile_width, int tile_height,
                              int stride_bytes, const char *quantVals,
                              int quantIdxY, int quantIdxCb, int quantIdxCr,
                              int xIdx, int yIdx)
{
    int YLen = 0;
    int CbLen = 0;
    int CrLen = 0;
    int ALen = 0;
    int start_pos;
    int end_pos;

    LLOGLN(10, ("rfx_compose_message_tile_argb:"));
    start_pos = stream_get_pos(s);
    stream_write_uint16(s, CBT_TILE); /* BlockT.blockType */
    stream_seek_uint32(s); /* set BlockT.blockLen later */
    stream_write_uint8(s, quantIdxY);
    stream_write_uint8(s, quantIdxCb);
    stream_write_uint8(s, quantIdxCr);
    stream_write_uint16(s, xIdx);
    stream_write_uint16(s, yIdx);
    stream_seek(s, 8); /* YLen, CbLen, CrLen, ALen */
    if (rfx_encode_argb(enc, tile_data, tile_width, tile_height,
                        stride_bytes,
                        quantVals + quantIdxY * 5,
                        quantVals + quantIdxCb * 5,
                        quantVals + quantIdxCr * 5,
                        s, &YLen, &CbLen, &CrLen, &ALen) != 0)
    {
        LLOGLN(10, ("rfx_compose_message_tile_argb: rfx_encode_argb failed"));
        return 1;
    }
    end_pos = stream_get_pos(s);
    stream_set_pos(s, start_pos + 2);
    stream_write_uint32(s, 19 + YLen + CbLen + CrLen + ALen); /* BlockT.blockLen */
    stream_set_pos(s, start_pos + 13);
    stream_write_uint16(s, YLen);
    stream_write_uint16(s, CbLen);
    stream_write_uint16(s, CrLen);
    stream_write_uint16(s, ALen);
    stream_set_pos(s, end_pos);
    return 0;
}

/******************************************************************************/
static int
rfx_compose_message_tileset(struct rfxencode* enc, STREAM* s,
                            char* buf, int width, int height,
                            int stride_bytes,
                            struct rfx_tile *tiles, int num_tiles,
                            const char *quants, int num_quants,
                            int flags)
{
    int size;
    int start_pos;
    int end_pos;
    int index;
    int numQuants;
    const char *quantVals;
    int quantIdxY;
    int quantIdxCb;
    int quantIdxCr;
    int numTiles;
    int tilesDataSize;
    int x;
    int y;
    int cx;
    int cy;
    char *tile_data;

    LLOGLN(10, ("rfx_compose_message_tileset:"));
    if (quants == 0)
    {
        numQuants = 1;
        quantVals = (const char *) g_rfx_default_quantization_values;
    }
    else
    {
        numQuants = num_quants;
        quantVals = quants;
    }
    numTiles = num_tiles;
    size = 22 + numQuants * 5;
    start_pos = stream_get_pos(s);
    if (flags & RFX_FLAGS_ALPHAV1)
    {
        LLOGLN(10, ("rfx_compose_message_tileset: RFX_FLAGS_ALPHAV1 set"));
        stream_write_uint16(s, WBT_EXTENSION_PLUS); /* CodecChannelT.blockType */
    }
    else
    {
        stream_write_uint16(s, WBT_EXTENSION); /* CodecChannelT.blockType */
    }
    stream_seek_uint32(s); /* set CodecChannelT.blockLen later */
    stream_write_uint8(s, 1); /* CodecChannelT.codecId */
    stream_write_uint8(s, 0); /* CodecChannelT.channelId */
    stream_write_uint16(s, CBT_TILESET); /* subtype */
    stream_write_uint16(s, 0); /* idx */
    stream_write_uint16(s, enc->properties); /* properties */
    stream_write_uint8(s, numQuants); /* numQuants */
    stream_write_uint8(s, 0x40); /* tileSize */
    stream_write_uint16(s, numTiles); /* numTiles */
    stream_seek_uint32(s); /* set tilesDataSize later */
    memcpy(s->p, quantVals, numQuants * 5);
    s->p += numQuants * 5;
    end_pos = stream_get_pos(s);
    if (enc->format == RFX_FORMAT_YUV)
    {
        if (flags & RFX_FLAGS_ALPHAV1)
        {
            for (index = 0; index < numTiles; index++)
            {
                x = tiles[index].x;
                y = tiles[index].y;
                cx = tiles[index].cx;
                cy = tiles[index].cy;
                quantIdxY = tiles[index].quant_y;
                quantIdxCb = tiles[index].quant_cb;
                quantIdxCr = tiles[index].quant_cr;
                tile_data = buf + (y << 8) * (stride_bytes >> 8) + (x << 8);
                if (rfx_compose_message_tile_yuva(enc, s,
                                                  tile_data, cx, cy, stride_bytes,
                                                  quantVals,
                                                  quantIdxY, quantIdxCb, quantIdxCr,
                                                  x / 64, y / 64) != 0)
                {
                    return 1;
                }
            }
        }
        else
        {
            for (index = 0; index < numTiles; index++)
            {
                x = tiles[index].x;
                y = tiles[index].y;
                cx = tiles[index].cx;
                cy = tiles[index].cy;
                quantIdxY = tiles[index].quant_y;
                quantIdxCb = tiles[index].quant_cb;
                quantIdxCr = tiles[index].quant_cr;
                tile_data = buf + (y << 8) * (stride_bytes >> 8) + (x << 8);
                if (rfx_compose_message_tile_yuv(enc, s,
                                                 tile_data, cx, cy, stride_bytes,
                                                 quantVals,
                                                 quantIdxY, quantIdxCb, quantIdxCr,
                                                 x / 64, y / 64) != 0)
                {
                    return 1;
                }
            }
        }
    }
    else
    {
        if (flags & RFX_FLAGS_ALPHAV1)
        {
            for (index = 0; index < numTiles; index++)
            {
                x = tiles[index].x;
                y = tiles[index].y;
                cx = tiles[index].cx;
                cy = tiles[index].cy;
                quantIdxY = tiles[index].quant_y;
                quantIdxCb = tiles[index].quant_cb;
                quantIdxCr = tiles[index].quant_cr;
                tile_data = buf + y * stride_bytes + x * (enc->bits_per_pixel / 8);
                if (rfx_compose_message_tile_argb(enc, s,
                                                  tile_data, cx, cy, stride_bytes,
                                                  quantVals,
                                                  quantIdxY, quantIdxCb, quantIdxCr,
                                                  x / 64, y / 64) != 0)
                {
                    return 1;
                }
            }
        }
        else
        {
            for (index = 0; index < numTiles; index++)
            {
                x = tiles[index].x;
                y = tiles[index].y;
                cx = tiles[index].cx;
                cy = tiles[index].cy;
                quantIdxY = tiles[index].quant_y;
                quantIdxCb = tiles[index].quant_cb;
                quantIdxCr = tiles[index].quant_cr;
                tile_data = buf + y * stride_bytes + x * (enc->bits_per_pixel / 8);
                if (rfx_compose_message_tile_rgb(enc, s,
                                                 tile_data, cx, cy, stride_bytes,
                                                 quantVals,
                                                 quantIdxY, quantIdxCb, quantIdxCr,
                                                 x / 64, y / 64) != 0)
                {
                    return 1;
                }
            }
        }
    }
    tilesDataSize = stream_get_pos(s) - end_pos;
    size += tilesDataSize;
    end_pos = stream_get_pos(s);
    stream_set_pos(s, start_pos + 2);
    stream_write_uint32(s, size); /* CodecChannelT.blockLen */
    stream_set_pos(s, start_pos + 18);
    stream_write_uint32(s, tilesDataSize);
    stream_set_pos(s, end_pos);
    return 0;
}

/******************************************************************************/
static int
rfx_compose_message_frame_end(struct rfxencode* enc, STREAM* s)
{
    if (stream_get_left(s) < 8)
    {
        return 1;
    }
    stream_write_uint16(s, WBT_FRAME_END); /* CodecChannelT.blockType */
    stream_write_uint32(s, 8); /* CodecChannelT.blockLen */
    stream_write_uint8(s, 1); /* CodecChannelT.codecId */
    stream_write_uint8(s, 0); /* CodecChannelT.channelId */
    return 0;
}

/******************************************************************************/
int
rfx_compose_message_data(struct rfxencode* enc, STREAM* s,
                         struct rfx_rect *regions, int num_regions,
                         char *buf, int width, int height, int stride_bytes,
                         struct rfx_tile *tiles, int num_tiles,
                         const char *quants, int num_quants, int flags)
{
    if (rfx_compose_message_frame_begin(enc, s) != 0)
    {
        return 1;
    }
    if (rfx_compose_message_region(enc, s, regions, num_regions) != 0)
    {
        return 1;
    }
    if (rfx_compose_message_tileset(enc, s, buf, width, height, stride_bytes,
                                    tiles, num_tiles, quants, num_quants,
                                    flags) != 0)
    {
        return 1;
    }
    if (rfx_compose_message_frame_end(enc, s) != 0)
    {
        return 1;
    }
    return 0;
}
