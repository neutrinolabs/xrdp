/**
 * xrdp: A Remote Desktop Protocol server.
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
 * wlup: frames to xrdp's GFX H.264 encoder
 *
 * When xrdp has chosen GFX H.264 for the client (capture code
 * CC_GFX_A2), frames go the way xorgxrdp sends them, rather than
 * through server_paint_rect(), which xrdp sends as planar bitmaps:
 *
 *   - the damage collected since the last frame is converted to NV12
 *     (BT.709, full range) in a new anonymous mapping. Only the damaged
 *     rectangles, widened to even coordinates, are filled in: the
 *     encoder copies just those into its own picture
 *   - STARTFRAME, WIRETOSURFACE_1 (AVC420) and ENDFRAME go to
 *     server_egfx_cmd(), which hands the mapping to the encoder thread
 *     and unmaps it once the frame is encoded
 *   - the next frame waits for mod_frame_ack(). xrdp acknowledges a
 *     frame once it is encoded and the client has not fallen more than
 *     a few frames behind, so a slow link lowers the frame rate instead
 *     of filling up with frames: damage keeps collecting meanwhile
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <string.h>
#include <sys/mman.h>

#include "wlup.h"
#include "wlup_gfx.h"
#include "log.h"
#include "xrdp_constants.h"

/* Codec and pixel format of WIRETOSURFACE_1 (MS-RDPEGFX) */
#define GFX_CODECID_AVC420 0x000B
#define GFX_PIXEL_FORMAT_XRGB_8888 0x20

/* Commands understood by xrdp's encoder (xrdp_encoder.c) */
#define GFX_CMDID_WIRETOSURFACE_1 0x0001
#define GFX_CMDID_STARTFRAME 0x000B
#define GFX_CMDID_ENDFRAME 0x000C

/* A frame waiting longer than this for its ack is taken as lost */
#define ACK_TIMEOUT_MS 2000

/******************************************************************************/
void
wlup_gfx_init(struct wlup *v, const struct xrdp_client_info *ci)
{
    /* wlup has one output, so one GFX surface */
    v->gfx_h264 = ci->capture_code == CC_GFX_A2 &&
                  ci->capture_format == XRDP_nv12_709fr &&
                  ci->display_sizes.monitorCount <= 1;
    v->gfx_frame_id = 0;
    v->gfx_frame_acked = 0;
    v->gfx_num_damage = 0;
    v->gfx_damage_overflow = 0;
    LOG(LOG_LEVEL_INFO, "wlup: frames go to %s",
        v->gfx_h264 ? "the GFX H.264 encoder" : "server_paint_rect()");
}

/******************************************************************************/
void
wlup_gfx_add_damage(struct wlup *v, int x, int y, int cx, int cy)
{
    short *d = v->gfx_damage;
    int i;

    if (!v->gfx_damage_overflow && v->gfx_num_damage < WLUP_MAX_DAMAGE)
    {
        i = v->gfx_num_damage++ * 4;
        d[i] = x;
        d[i + 1] = y;
        d[i + 2] = cx;
        d[i + 3] = cy;
        return;
    }
    if (!v->gfx_damage_overflow)
    {
        /* Collapse everything into a bounding box, kept in d[0..3] */
        for (i = 1; i < v->gfx_num_damage; ++i)
        {
            int right = MAX(d[0] + d[2], d[i * 4] + d[i * 4 + 2]);
            int bottom = MAX(d[1] + d[3], d[i * 4 + 1] + d[i * 4 + 3]);
            d[0] = MIN(d[0], d[i * 4]);
            d[1] = MIN(d[1], d[i * 4 + 1]);
            d[2] = right - d[0];
            d[3] = bottom - d[1];
        }
        v->gfx_num_damage = 1;
        v->gfx_damage_overflow = 1;
    }
    {
        int right = MAX(d[0] + d[2], x + cx);
        int bottom = MAX(d[1] + d[3], y + cy);
        d[0] = MIN(d[0], x);
        d[1] = MIN(d[1], y);
        d[2] = right - d[0];
        d[3] = bottom - d[1];
    }
}

/******************************************************************************/
/* Converts a rectangle with even coordinates from 32-bit xRGB to NV12,
 * BT.709 full range: the format xrdp asks for with XRDP_nv12_709fr */
static void
xrgb_to_nv12_709fr(const uint8_t *src, int src_stride,
                   uint8_t *dst_y, uint8_t *dst_uv, int dst_stride,
                   int x1, int y1, int x2, int y2)
{
    int x;
    int y;
    int i;

    for (y = y1; y < y2; y += 2)
    {
        const uint32_t *s[2];
        uint8_t *dy[2];
        uint8_t *duv = dst_uv + (size_t)(y / 2) * dst_stride + x1;

        s[0] = (const uint32_t *)(src + (size_t)y * src_stride) + x1;
        s[1] = (const uint32_t *)(src + (size_t)(y + 1) * src_stride) + x1;
        dy[0] = dst_y + (size_t)y * dst_stride + x1;
        dy[1] = dst_y + (size_t)(y + 1) * dst_stride + x1;
        for (x = x1; x < x2; x += 2)
        {
            int u_sum = 0;
            int v_sum = 0;

            /* the 2x2 block shares one U and one V */
            for (i = 0; i < 4; ++i)
            {
                uint32_t pixel = s[i >> 1][i & 1];
                int r = (pixel >> 16) & 0xff;
                int g = (pixel >> 8) & 0xff;
                int b = pixel & 0xff;

                dy[i >> 1][i & 1] = (54 * r + 183 * g + 18 * b) >> 8;
                u_sum += ((-29 * r - 99 * g + 128 * b) >> 8) + 128;
                v_sum += ((128 * r - 116 * g - 12 * b) >> 8) + 128;
            }
            duv[0] = MIN(MAX((u_sum + 2) >> 2, 0), 255);
            duv[1] = MIN(MAX((v_sum + 2) >> 2, 0), 255);
            s[0] += 2;
            s[1] += 2;
            dy[0] += 2;
            dy[1] += 2;
            duv += 2;
        }
    }
}

/******************************************************************************/
/* Writes the damage clipped to width x height as rects (x, y, cx, cy),
 * either as they are (destination rects) or widened to even
 * coordinates (copy rects, for the 2x2 chroma blocks). Returns the
 * number of rects written */
static int
out_damage_rects(struct wlup *v, struct stream *s, int width, int height,
                 int even, short *copy_rects)
{
    int count = 0;
    int i;

    for (i = 0; i < v->gfx_num_damage; ++i)
    {
        int x1 = MAX(v->gfx_damage[i * 4], 0);
        int y1 = MAX(v->gfx_damage[i * 4 + 1], 0);
        int x2 = MIN(v->gfx_damage[i * 4] + v->gfx_damage[i * 4 + 2], width);
        int y2 = MIN(v->gfx_damage[i * 4 + 1] + v->gfx_damage[i * 4 + 3],
                     height);

        if (even)
        {
            x1 &= ~1;
            y1 &= ~1;
            x2 = MIN((x2 + 1) & ~1, width & ~1);
            y2 = MIN((y2 + 1) & ~1, height & ~1);
        }
        if (x2 <= x1 || y2 <= y1)
        {
            continue;
        }
        if (s != NULL)
        {
            out_uint16_le(s, x1);
            out_uint16_le(s, y1);
            out_uint16_le(s, x2 - x1);
            out_uint16_le(s, y2 - y1);
        }
        if (copy_rects != NULL)
        {
            copy_rects[count * 4] = x1;
            copy_rects[count * 4 + 1] = y1;
            copy_rects[count * 4 + 2] = x2;
            copy_rects[count * 4 + 3] = y2;
        }
        ++count;
    }
    return count;
}

/******************************************************************************/
/* return error */
static int
send_frame(struct wlup *v)
{
    short copy_rects[WLUP_MAX_DAMAGE * 4];
    struct stream *s;
    int width;  /* the surface: the RDP desktop */
    int height;
    int clip_width; /* the part the capture buffer covers */
    int clip_height;
    int num_d;
    int num_c;
    int start_bytes;
    int w2s_bytes;
    int end_bytes;
    int data_bytes;
    uint8_t *data;
    int i;
    int rv;

    width = v->server_width;
    height = v->server_height;
    clip_width = MIN(v->buffer_width, width);
    clip_height = MIN(v->buffer_height, height);
    num_d = out_damage_rects(v, NULL, clip_width, clip_height, 0, NULL);
    num_c = out_damage_rects(v, NULL, clip_width, clip_height, 1, copy_rects);
    if (num_d < 1 || num_c < 1)
    {
        return 0;
    }

    /* NV12 of the whole surface; untouched pages are never allocated */
    data_bytes = width * height + width * ((height + 1) / 2);
    data = (uint8_t *)mmap(NULL, data_bytes, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (data == MAP_FAILED)
    {
        LOG(LOG_LEVEL_ERROR, "wlup: no memory for a %dx%d NV12 frame",
            width, height);
        return 1;
    }
    for (i = 0; i < num_c; ++i)
    {
        xrgb_to_nv12_709fr((const uint8_t *)v->pixels, v->buffer_width * 4,
                           data, data + (size_t)width * height, width,
                           copy_rects[i * 4], copy_rects[i * 4 + 1],
                           copy_rects[i * 4 + 2], copy_rects[i * 4 + 3]);
    }

    start_bytes = 8 + 8;
    w2s_bytes = 8 + 9 + 2 + num_d * 8 + 2 + num_c * 8 + 8;
    end_bytes = 8 + 4;
    make_stream(s);
    init_stream(s, start_bytes + w2s_bytes + end_bytes);
    ++v->gfx_frame_id;

    out_uint16_le(s, GFX_CMDID_STARTFRAME);
    out_uint16_le(s, 0); /* flags */
    out_uint32_le(s, start_bytes);
    out_uint32_le(s, v->gfx_frame_id);
    out_uint32_le(s, 0); /* time stamp */

    out_uint16_le(s, GFX_CMDID_WIRETOSURFACE_1);
    out_uint16_le(s, 0); /* flags */
    out_uint32_le(s, w2s_bytes);
    out_uint16_le(s, 0); /* surface id: the first monitor */
    out_uint16_le(s, GFX_CODECID_AVC420);
    out_uint8(s, GFX_PIXEL_FORMAT_XRGB_8888);
    out_uint32_le(s, 0); /* flags: monitor 0, not compressed yet */
    out_uint16_le(s, num_d);
    out_damage_rects(v, s, clip_width, clip_height, 0, NULL);
    out_uint16_le(s, num_c);
    out_damage_rects(v, s, clip_width, clip_height, 1, NULL);
    out_uint16_le(s, 0); /* left */
    out_uint16_le(s, 0); /* top */
    out_uint16_le(s, width);
    out_uint16_le(s, height);

    out_uint16_le(s, GFX_CMDID_ENDFRAME);
    out_uint16_le(s, 0); /* flags */
    out_uint32_le(s, end_bytes);
    out_uint32_le(s, v->gfx_frame_id);
    s_mark_end(s);

    v->gfx_num_damage = 0;
    v->gfx_damage_overflow = 0;
    v->gfx_sent_time = g_get_elapsed_ms();
    /* xrdp owns the mapping from here on, and unmaps it */
    rv = v->server_egfx_cmd(v, s->data, (int)(s->end - s->data),
                            (char *)data, data_bytes);
    free_stream(s);
    if (v->gfx_frame_id == 1)
    {
        LOG(LOG_LEVEL_INFO, "wlup: first H.264 frame sent, %dx%d",
            width, height);
    }
    return rv;
}

/******************************************************************************/
int
wlup_gfx_poll(struct wlup *v)
{
    if (!v->gfx_h264 || v->gfx_num_damage == 0 || v->pixels == NULL)
    {
        return 0;
    }
    if (v->gfx_frame_acked < v->gfx_frame_id)
    {
        unsigned int waited = g_get_elapsed_ms() - v->gfx_sent_time;

        if (waited < ACK_TIMEOUT_MS)
        {
            /* The damage goes into the next frame */
            return 0;
        }
        LOG(LOG_LEVEL_INFO, "wlup: no ack for frame %d after %u ms, "
            "sending the next one anyway", v->gfx_frame_id, waited);
        v->gfx_frame_acked = v->gfx_frame_id;
    }
    return send_frame(v);
}

/******************************************************************************/
void
wlup_gfx_frame_ack(struct wlup *v, int frame_id)
{
    if (frame_id > v->gfx_frame_acked)
    {
        /* INT_MAX when xrdp has no encoder: every frame is done */
        v->gfx_frame_acked = MIN(frame_id, v->gfx_frame_id);
    }
    wlup_gfx_poll(v);
}

/******************************************************************************/
void
wlup_gfx_reset(struct wlup *v)
{
    v->gfx_frame_acked = v->gfx_frame_id;
    v->gfx_num_damage = 0;
    v->gfx_damage_overflow = 0;
}

/******************************************************************************/
void
wlup_gfx_get_timeout(struct wlup *v, int *timeout)
{
    int left;

    if (!v->gfx_h264 || v->gfx_num_damage == 0 ||
            v->gfx_frame_acked >= v->gfx_frame_id)
    {
        return;
    }
    left = ACK_TIMEOUT_MS - (int)(g_get_elapsed_ms() - v->gfx_sent_time);
    left = MAX(left, 0);
    if (*timeout < 0 || *timeout > left)
    {
        *timeout = left;
    }
}
