/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2015
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
 * libvnc
 *
 * The message definitions used in this source file can be found mostly
 * in RFC6143 - "The Remote Framebuffer Protocol".
 *
 * The ExtendedDesktopSize encoding is reserved in RFC6143, but not
 * documented there. It is documented by the RFB protocol community
 * wiki currently held at https://github.com/rfbproto/rfbroto. This is
 * referred to below as the "RFB community wiki"
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <ctype.h>
#include <limits.h>
#include <X11/keysym.h>

#include "vnc.h"
#include "vnc_clip.h"
#include "rfb.h"
#include "log.h"
#include "timers.h"
#include "trans.h"
#include "ssl_calls.h"
#include "string_calls.h"
#include "xrdp_client_info.h"

/* elements in above list */
#define EDS_STATUS_MSG_COUNT \
    (sizeof(eds_status_msg) / sizeof(eds_status_msg[0]))

/* Used by enabled_encodings_mask */
enum
{
    MSK_EXTENDED_DESKTOP_SIZE = (1 << 0)
};

enum
{
    /** Time to wait for a forwarded resize to complete */
    FORWARDED_RESIZE_TIMEOUT = 1500  /* milli-seconds */
};

/******************************************************************************/
int
lib_send_copy(struct vnc *v, struct stream *s)
{
    return trans_write_copy_s(v->trans, s);
}

/******************************************************************************/
/* taken from vncauth.c */
/* performing the des3 crypt on the password so it can not be seen
   on the wire
   'bytes' in, contains 16 bytes server random
           out, random and 'passwd' conbined */
static void
rfbEncryptBytes(char *bytes, const char *passwd)
{
    char key[24];
    void *des;
    int len;

    /* key is simply password padded with nulls */
    g_memset(key, 0, sizeof(key));
    len = MIN(g_strlen(passwd), 8);
    g_mirror_memcpy(key, passwd, len);
    des = ssl_des3_encrypt_info_create(key, 0);
    ssl_des3_encrypt(des, 8, bytes, bytes);
    ssl_des3_info_delete(des);
    des = ssl_des3_encrypt_info_create(key, 0);
    ssl_des3_encrypt(des, 8, bytes + 8, bytes + 8);
    ssl_des3_info_delete(des);
}

/******************************************************************************/
/* sha1 hash 'passwd', create a string from the hash and call rfbEncryptBytes */
static void
rfbHashEncryptBytes(char *bytes, const char *passwd)
{
    char passwd_hash[20];
    char passwd_hash_text[40];
    void *sha1;
    int passwd_bytes;

    /* create password hash from password */
    passwd_bytes = g_strlen(passwd);
    sha1 = ssl_sha1_info_create();
    ssl_sha1_clear(sha1);
    ssl_sha1_transform(sha1, "xrdp_vnc", 8);
    ssl_sha1_transform(sha1, passwd, passwd_bytes);
    ssl_sha1_transform(sha1, passwd, passwd_bytes);
    ssl_sha1_complete(sha1, passwd_hash);
    ssl_sha1_info_delete(sha1);
    g_snprintf(passwd_hash_text, 39, "%2.2x%2.2x%2.2x%2.2x",
               (tui8)passwd_hash[0], (tui8)passwd_hash[1],
               (tui8)passwd_hash[2], (tui8)passwd_hash[3]);
    passwd_hash_text[39] = 0;
    rfbEncryptBytes(bytes, passwd_hash_text);
}

/**************************************************************************//**
 * Logs a debug message containing a screen layout
 *
 * @param lvl Level to log at
 * @param source Where the layout came from
 * @param layout Layout to log
 */
static void
log_screen_layout(const enum logLevels lvl, const char *source,
                  const struct vnc_screen_layout *layout)
{
    unsigned int i;
    char text[256];
    size_t pos;
    int res;

    pos = 0;
    res = g_snprintf(text, sizeof(text) - pos,
                     "Layout from %s (geom=%dx%d #screens=%u) :",
                     source, layout->total_width, layout->total_height,
                     layout->count);

    i = 0;
    while (res > 0 && (size_t)res < sizeof(text) - pos && i < layout->count)
    {
        pos += res;
        res = g_snprintf(&text[pos], sizeof(text) - pos,
                         " %d:(%dx%d+%d+%d)",
                         layout->s[i].id,
                         layout->s[i].width, layout->s[i].height,
                         layout->s[i].x, layout->s[i].y);
        ++i;
    }
    LOG(lvl, "%s", text);
}

/**************************************************************************//**
 * Compares two vnc_screen structures
 *
 * @param a First structure
 * @param b Second structure
 *
 * @return Suitable for sorting structures on (x, y, width, height)
 */
static int cmp_vnc_screen(const struct vnc_screen *a,
                          const struct vnc_screen *b)
{
    int result = 0;
    if (a->x != b->x)
    {
        result = a->x - b->x;
    }
    else if (a->y != b->y)
    {
        result = a->y - b->y;
    }
    else if (a->width != b->width)
    {
        result = a->width - b->width;
    }
    else if (a->height != b->height)
    {
        result = a->height - b->height;
    }

    return result;
}

/**************************************************************************//**
 * Compares two vnc_screen_layout structures for equality
 * @param a First layout
 * @param b First layout
 * @return != 0 if structures are equal
 */
static int vnc_screen_layouts_equal(const struct vnc_screen_layout *a,
                                    const struct vnc_screen_layout *b)
{
    unsigned int i;
    int result = (a->total_width == b->total_width &&
                  a->total_height == b->total_height &&
                  a->count == b->count);
    if (result)
    {
        for (i = 0 ; result && i < a->count ; ++i)
        {
            result = (cmp_vnc_screen(&a->s[i], &b->s[i]) == 0);
        }
    }

    return result;
}

/**************************************************************************//**
 * Reads an extended desktop size rectangle from the VNC server
 *
 * @param v VNC object
 * @param [out] layout Desired layout for server
 * @return != 0 for error
 *
 * @pre The next octet read from v->trans is the number of screens
 *
 * @post Returned structure is in increasing ID order
 * @post layout->total_width is untouched
 * @post layout->total_height is untouched
 */
static int
read_extended_desktop_size_rect(struct vnc *v,
                                struct vnc_screen_layout *layout)
{
    struct stream *s;
    int error;
    unsigned int count;

    layout->count = 0;

    make_stream(s);
    init_stream(s, 8192);

    /* Read in the current screen config */
    error = trans_force_read_s(v->trans, s, 4);
    if (error == 0)
    {
        /* Get the number of screens */
        in_uint8(s, count);
        if (count <= 0 || count > CLIENT_MONITOR_DATA_MAXIMUM_MONITORS)
        {
            LOG(LOG_LEVEL_ERROR,
                "Bad monitor count %d in ExtendedDesktopSize rectangle",
                count);
            error = 1;
        }
        else
        {
            in_uint8s(s, 3);

            error = trans_force_read_s(v->trans, s, 16 * count);
            if (error == 0)
            {
                unsigned int i;
                for (i = 0 ; i < count ; ++i)
                {
                    in_uint32_be(s, layout->s[i].id);
                    in_uint16_be(s, layout->s[i].x);
                    in_uint16_be(s, layout->s[i].y);
                    in_uint16_be(s, layout->s[i].width);
                    in_uint16_be(s, layout->s[i].height);
                    in_uint32_be(s, layout->s[i].flags);
                }

                /* sort monitors in increasing (x,y) order */
                qsort(layout->s, count, sizeof(layout->s[0]),
                      (int (*)(const void *, const void *))cmp_vnc_screen);
                layout->count = count;
            }
        }
    }

    free_stream(s);

    return error;
}

/**************************************************************************//**
 * Sends a SetDesktopSize message
 *
 * @param v VNC object
 * @param layout Desired layout for server
 * @return != 0 for error
 *
 * The SetDesktopSize message is documented in the RFB community wiki
 * "SetDesktopSize" section.
 */
static int
send_set_desktop_size(struct vnc *v, const struct vnc_screen_layout *layout)
{
    unsigned int i;
    struct stream *s;
    int error;

    make_stream(s);
    init_stream(s, 8192);
    out_uint8(s, 251);
    out_uint8(s, 0);
    out_uint16_be(s, layout->total_width);
    out_uint16_be(s, layout->total_height);

    out_uint8(s, layout->count);
    out_uint8(s, 0);
    for (i = 0 ; i < layout->count ; ++i)
    {
        out_uint32_be(s, layout->s[i].id);
        out_uint16_be(s, layout->s[i].x);
        out_uint16_be(s, layout->s[i].y);
        out_uint16_be(s, layout->s[i].width);
        out_uint16_be(s, layout->s[i].height);
        out_uint32_be(s, layout->s[i].flags);
    }
    s_mark_end(s);
    LOG(LOG_LEVEL_DEBUG, "VNC_RESIZE: Sending SetDesktopSize %dx%d",
        layout->total_width, layout->total_height);
    error = lib_send_copy(v, s);
    free_stream(s);

    return error;
}

/**************************************************************************//**
 * Initialises a vnc_screen_layout as a single screen
 * @param[in] width Screen Width
 * @param[in] height Screen Height
 * @param[out] layout Layout to initialise
 */
static void
init_single_screen_layout(int width, int height,
                          struct vnc_screen_layout *layout)
{
    layout->total_width = width;
    layout->total_height = height;
    layout->count = 1;
    layout->s[0].id = 0;
    layout->s[0].x = 0;
    layout->s[0].y = 0;
    layout->s[0].width = width;
    layout->s[0].height = height;
    layout->s[0].flags = 0;
}

/**************************************************************************//**
 * Resize the client to match the server_layout
 *
 * @param v VNC object
 * @param update_in_progress True if there's a painter update in progress
 * @return != 0 for error
 *
 * The new client layout is recorded in v->client_layout.
 */
static int
resize_client_to_server(struct vnc *v, int update_in_progress)
{
    int error = 0;
    unsigned int i;
    const struct vnc_screen_layout *sl = &v->server_layout;
    struct monitor_info client_mons[CLIENT_MONITOR_DATA_MAXIMUM_MONITORS] = {0};

    if (sl->count <= 0 ||
            sl->count > CLIENT_MONITOR_DATA_MAXIMUM_MONITORS)
    {
        LOG(LOG_LEVEL_ERROR, "%s: Programming error. Bad monitors %d",
            __func__, sl->count);
        return 1;
    }

    // Convert the server monitors into client monitors
    for (i = 0; i < sl->count; ++i)
    {
        client_mons[i].left = sl->s[i].x;
        client_mons[i].top = sl->s[i].y;
        client_mons[i].right = sl->s[i].x + sl->s[i].width - 1;
        client_mons[i].bottom = sl->s[i].y + sl->s[i].height - 1;
    }

    if (update_in_progress && v->server_end_update(v) != 0)
    {
        error = 1;
    }
    else
    {
        error = v->client_monitor_resize(v, sl->total_width, sl->total_height,
                                         sl->count, client_mons);
        if (error == 0)
        {
            v->client_layout = *sl;
        }

        if (update_in_progress && v->server_begin_update(v) != 0)
        {
            error = 1;
        }
    }

    return error;
}


/**************************************************************************//**
 * Resize the server to the client layout
 *
 * @param v VNC object
 * @return != 0 for error
 *
 * The new client layout is recorded in v->client_layout.
 */
static int
resize_server_to_client_layout(struct vnc *v)
{
    int error = 0;

    /* Before checking the 'resize_supported' flag, see if this
     * is a null operation. We can get here if the server doesn't
     * support resize, and we've queued a request to resize the client
     * to the server size */
    if (vnc_screen_layouts_equal(&v->server_layout, &v->client_layout))
    {
        LOG(LOG_LEVEL_DEBUG, "Server layout is the same "
            "as the client layout");
        v->resize_status = VRS_DONE;
    }
    else if (v->resize_supported != VRSS_SUPPORTED)
    {
        LOG(LOG_LEVEL_ERROR, "%s: Asked to resize server, but not possible",
            __func__);
        error = 1;
    }
    else
    {
        /*
         * If we've only got one screen, and the other side has
         * only got one screen, we will preserve their screen ID
         * and any flags.  This may prevent us sending an unwanted
         * SetDesktopSize message if the screen dimensions are
         * a match. We can't do this with more than one screen,
         * as we have no way to map different IDs
         */
        if (v->server_layout.count == 1 && v->client_layout.count == 1)
        {
            LOG(LOG_LEVEL_DEBUG, "VNC "
                "setting screen id to %d from server",
                v->server_layout.s[0].id);

            v->client_layout.s[0].id = v->server_layout.s[0].id;
            v->client_layout.s[0].flags = v->server_layout.s[0].flags;
        }

        LOG(LOG_LEVEL_DEBUG, "Changing server layout");
        error = send_set_desktop_size(v, &v->client_layout);
        v->resize_status = VRS_WAITING_FOR_RESIZE_CONFIRM;
    }

    return error;
}

/*****************************************************************************/
/**
 * Process keysym messages
 * @param v Module
 * @param keysym Keysym of keypress
 * @param keydown boolean - is key down?
 * @return != 0 for error
 */
static int
process_keysym_msg(struct vnc *v, int keysym, int keydown)
{
    struct stream *s = NULL;
    int error = 0;

    if (keysym > 0)
    {
        make_stream(s);

        /* Break key processing [MS-RDPBCGR] 2.2.8.1.1.3.1.1.1 */
        if (v->ignore_next_numlock)
        {
            v->ignore_next_numlock = 0;
            if (keysym == XK_Num_Lock)
            {
                goto end_keysym_msg;
            }
        }

        if (keysym == XK_ISO_Level3_Shift)  /* altgr */
        {
            if (v->shift_state)
            {
                /* fix for mstsc sending left control down with altgr */
                init_stream(s, 64);
                out_uint8(s, RFB_C2S_KEY_EVENT);
                out_uint8(s, 0); /* down flag */
                out_uint8s(s, 2);
                out_uint32_be(s, XK_Control_L); /* left control */
                s_mark_end(s);
                error = lib_send_copy(v, s);
                if (error != 0)
                {
                    goto end_keysym_msg;
                }
            }
        }

        init_stream(s, 64);
        out_uint8(s, RFB_C2S_KEY_EVENT);
        out_uint8(s, keydown ? 1 : 0);
        out_uint8s(s, 2);
        out_uint32_be(s, keysym);
        s_mark_end(s);
        error = lib_send_copy(v, s);

        switch (keysym)
        {
            case XK_Control_L: /* left control */
                v->shift_state = keydown;
                break;

            case XK_Pause:
                // [MS-RDPBCGR] 2.2.8.1.1.3.1.1.1 - A pause key scancode
                // (up or down) is always immediately followed by a
                // numlock key which we should ignore
                v->ignore_next_numlock = 1;
                break;

            default:
                break;
        }
    }

end_keysym_msg:
    free_stream(s);
    return error;
}

/*****************************************************************************/
static int
lib_mod_event(struct vnc *v, int msg, long param1, long param2,
              long param3, long param4)
{
    struct stream *s;
    int error;
    int x;
    int y;
    int cx;
    int cy;
    int size;
    int total_size;
    int chanid;
    int flags;
    char *data;

    error = 0;
    make_stream(s);

    if (msg == WM_CHANNEL_DATA)
    {
        chanid = LOWORD(param1);
        flags = HIWORD(param1);
        size = (int)param2;
        data = (char *)param3;
        total_size = (int)param4;

        if ((size >= 0) && (size <= (32 * 1024)) && (data != 0))
        {
            if (chanid == v->clip_chanid)
            {
                error = vnc_clip_process_channel_data(v, data, size,
                                                      total_size, flags);
            }
            else
            {
                LOG(LOG_LEVEL_DEBUG, "lib_process_channel_data: unknown chanid: "
                    "%d :(v->clip_chanid) %d", chanid, v->clip_chanid);
            }
        }
        else
        {
            error = 1;
        }
    }
    else if ((msg >= 15) && (msg <= 16)) /* key events */
    {
        error = process_keysym_msg(v, param2, (msg == 15));
    }
    /* mouse events
     *
     * VNC supports up to 8 mouse buttons because mouse buttons are
     * represented by 7 bits bitmask
     */
    else if (msg >= WM_MOUSEMOVE && msg <= WM_BUTTON8DOWN) /* 100 to 116 */
    {
        switch (msg)
        {
            case WM_MOUSEMOVE:
                break;
            case WM_LBUTTONUP:
                v->mod_mouse_state &= ~1;
                break;
            case WM_LBUTTONDOWN:
                v->mod_mouse_state |= 1;
                break;
            case WM_RBUTTONUP:
                v->mod_mouse_state &= ~4;
                break;
            case WM_RBUTTONDOWN:
                v->mod_mouse_state |= 4;
                break;
            case WM_BUTTON3UP:
                v->mod_mouse_state &= ~2;
                break;
            case WM_BUTTON3DOWN:
                v->mod_mouse_state |= 2;
                break;
            case WM_BUTTON4UP:
                v->mod_mouse_state &= ~8;
                break;
            case WM_BUTTON4DOWN:
                v->mod_mouse_state |= 8;
                break;
            case WM_BUTTON5UP:
                v->mod_mouse_state &= ~16;
                break;
            case WM_BUTTON5DOWN:
                v->mod_mouse_state |= 16;
                break;
            case WM_BUTTON6UP:
                v->mod_mouse_state &= ~32;
                break;
            case WM_BUTTON6DOWN:
                v->mod_mouse_state |= 32;
                break;
            case WM_BUTTON7UP:
                v->mod_mouse_state &= ~64;
                break;
            case WM_BUTTON7DOWN:
                v->mod_mouse_state |= 64;
                break;
            case WM_BUTTON8UP:
                v->mod_mouse_state &= ~128;
                break;
            case WM_BUTTON8DOWN:
                v->mod_mouse_state |= 128;
                break;
        }

        init_stream(s, 8192);
        out_uint8(s, RFB_C2S_POINTER_EVENT);
        out_uint8(s, v->mod_mouse_state);
        out_uint16_be(s, param1);
        out_uint16_be(s, param2);
        s_mark_end(s);
        error = lib_send_copy(v, s);
    }
    else if (msg == 200) /* invalidate */
    {
        if (v->suppress_output == 0)
        {
            init_stream(s, 8192);
            out_uint8(s, RFB_C2S_FRAMEBUFFER_UPDATE_REQUEST);
            out_uint8(s, 0); /* incremental == 0 : Full contents */
            x = (param1 >> 16) & 0xffff;
            out_uint16_be(s, x);
            y = param1 & 0xffff;
            out_uint16_be(s, y);
            cx = (param2 >> 16) & 0xffff;
            out_uint16_be(s, cx);
            cy = param2 & 0xffff;
            out_uint16_be(s, cy);
            s_mark_end(s);
            error = lib_send_copy(v, s);
        }
    }
    free_stream(s);
    return error;
}

//******************************************************************************
static int
get_pixel_safe(char *data, int x, int y, int width, int height, int bpp)
{
    int start = 0;
    int shift = 0;

    if (x < 0)
    {
        return 0;
    }

    if (y < 0)
    {
        return 0;
    }

    if (x >= width)
    {
        return 0;
    }

    if (y >= height)
    {
        return 0;
    }

    if (bpp == 1)
    {
        width = (width + 7) / 8;
        start = (y * width) + x / 8;
        shift = x % 8;
        return (data[start] & (0x80 >> shift)) != 0;
    }
    else if (bpp == 4)
    {
        width = (width + 1) / 2;
        start = y * width + x / 2;
        shift = x % 2;

        if (shift == 0)
        {
            return (data[start] & 0xf0) >> 4;
        }
        else
        {
            return data[start] & 0x0f;
        }
    }
    else if (bpp == 8)
    {
        return *(((unsigned char *)data) + (y * width + x));
    }
    else if (bpp == 15 || bpp == 16)
    {
        return *(((unsigned short *)data) + (y * width + x));
    }
    else if (bpp == 24 || bpp == 32)
    {
        return *(((unsigned int *)data) + (y * width + x));
    }
    else
    {
        LOG(LOG_LEVEL_ERROR, "error in get_pixel_safe bpp %d", bpp);
    }

    return 0;
}

/******************************************************************************/
static void
set_pixel_safe(char *data, int x, int y, int width, int height, int bpp,
               int pixel)
{
    int start = 0;
    int shift = 0;

    if (x < 0)
    {
        return;
    }

    if (y < 0)
    {
        return;
    }

    if (x >= width)
    {
        return;
    }

    if (y >= height)
    {
        return;
    }

    if (bpp == 1)
    {
        width = (width + 7) / 8;
        start = (y * width) + x / 8;
        shift = x % 8;

        if (pixel & 1)
        {
            data[start] = data[start] | (0x80 >> shift);
        }
        else
        {
            data[start] = data[start] & ~(0x80 >> shift);
        }
    }
    else if (bpp == 15 || bpp == 16)
    {
        *(((unsigned short *)data) + (y * width + x)) = pixel;
    }
    else if (bpp == 24)
    {
        *(data + (3 * (y * width + x)) + 0) = pixel >> 0;
        *(data + (3 * (y * width + x)) + 1) = pixel >> 8;
        *(data + (3 * (y * width + x)) + 2) = pixel >> 16;
    }
    else
    {
        LOG(LOG_LEVEL_ERROR, "error in set_pixel_safe bpp %d", bpp);
    }
}

/******************************************************************************/
static int
split_color(int pixel, int *r, int *g, int *b, int bpp, int *palette)
{
    if (bpp == 8)
    {
        if (pixel >= 0 && pixel < 256 && palette != 0)
        {
            *r = (palette[pixel] >> 16) & 0xff;
            *g = (palette[pixel] >> 8) & 0xff;
            *b = (palette[pixel] >> 0) & 0xff;
        }
    }
    else if (bpp == 15)
    {
        *r = ((pixel >> 7) & 0xf8) | ((pixel >> 12) & 0x7);
        *g = ((pixel >> 2) & 0xf8) | ((pixel >> 8) & 0x7);
        *b = ((pixel << 3) & 0xf8) | ((pixel >> 2) & 0x7);
    }
    else if (bpp == 16)
    {
        *r = ((pixel >> 8) & 0xf8) | ((pixel >> 13) & 0x7);
        *g = ((pixel >> 3) & 0xfc) | ((pixel >> 9) & 0x3);
        *b = ((pixel << 3) & 0xf8) | ((pixel >> 2) & 0x7);
    }
    else if (bpp == 24 || bpp == 32)
    {
        *r = (pixel >> 16) & 0xff;
        *g = (pixel >> 8) & 0xff;
        *b = pixel & 0xff;
    }
    else
    {
        LOG(LOG_LEVEL_ERROR, "error in split_color bpp %d", bpp);
    }

    return 0;
}

/******************************************************************************/
static int
make_color(int r, int g, int b, int bpp)
{
    if (bpp == 24)
    {
        return (r << 16) | (g << 8) | b;
    }
    else
    {
        LOG(LOG_LEVEL_ERROR, "error in make_color bpp %d", bpp);
    }

    return 0;
}

/**
 * Converts a bits-per-pixel value to bytes-per-pixel
 */
static int
get_bytes_per_pixel(int bpp)
{
    int result = (bpp + 7) / 8;

    if (result == 3)
    {
        result = 4;
    }

    return result;
}


/**************************************************************************//**
 * Skips the specified number of bytes from the transport
 *
 * @param transport Transport to read
 * @param bytes Bytes to skip
 * @return != 0 for error
 */
int
skip_trans_bytes(struct trans *trans, unsigned int bytes)
{
    struct stream *s;
    int error = 0;

    make_stream(s);

    while (error == 0 && bytes > 0)
    {
        int chunk_size = MIN(32768, bytes);
        init_stream(s, chunk_size);
        error = trans_force_read_s(trans, s, chunk_size);
        bytes -= chunk_size;
    }

    free_stream(s);

    return error;
}

/**************************************************************************//**
 * Reads an encoding from the input stream and discards it
 *
 * @param v VNC object
 * @param x Encoding X value
 * @param y Encoding Y value
 * @param cx Encoding CX value
 * @param cy Encoding CY value
 * @param encoding Code for encoding
 * @return != 0 for error
 *
 * @pre On entry the input stream is positioned after the encoding header
 */
static int
skip_encoding(struct vnc *v, int x, int y, int cx, int cy,
              encoding_type encoding)
{
    char text[256];
    int error = 0;

    switch (encoding)
    {
        case RFB_ENC_RAW:
        {
            int need_size = cx * cy * get_bytes_per_pixel(v->server_bpp);
            LOG(LOG_LEVEL_DEBUG, "Skipping RFB_ENC_RAW encoding");
            error = skip_trans_bytes(v->trans, need_size);
        }
        break;

        case RFB_ENC_COPY_RECT:
        {
            LOG(LOG_LEVEL_DEBUG, "Skipping RFB_ENC_COPY_RECT encoding");
            error = skip_trans_bytes(v->trans, 4);
        }
        break;

        case RFB_ENC_CURSOR:
        {
            int j = cx * cy * get_bytes_per_pixel(v->server_bpp);
            int k = ((cx + 7) / 8) * cy;

            LOG(LOG_LEVEL_DEBUG, "Skipping RFB_ENC_CURSOR encoding");
            error = skip_trans_bytes(v->trans, j + k);
        }
        break;

        case RFB_ENC_DESKTOP_SIZE:
            LOG(LOG_LEVEL_DEBUG, "Skipping RFB_ENC_DESKTOP_SIZE encoding");
            break;

        case RFB_ENC_EXTENDED_DESKTOP_SIZE:
        {
            struct vnc_screen_layout layout = {0};
            LOG(LOG_LEVEL_DEBUG,
                "VNC_RESIZE: Skipping RFB_ENC_EXTENDED_DESKTOP_SIZE encoding "
                "x=%d, y=%d geom=%dx%d",
                x, y, cx, cy);
            error = read_extended_desktop_size_rect(v, &layout);
        }
        break;

        default:
            g_sprintf(text, "VNC error in skip_encoding "
                      "encoding = %8.8x", encoding);
            v->server_msg(v, text, 1);
    }

    return error;
}

/**************************************************************************//**
 * Parses an entire framebuffer update message from the wire, and returns the
 * first matching ExtendedDesktopSize encoding if found.
 *
 * Caller can check for a match by examining match_layout.count after the call
 *
 * @param v VNC object
 * @param match Function to call to check for a match
 * @param [out] match_x Matching x parameter for an encoding (if needed)
 * @param [out] match_y Matching y parameter for an encoding (if needed)
 * @param [out] match_layout Returned layout for the encoding
 * @return != 0 for error
 */
static int
find_matching_extended_rect(struct vnc *v,
                            int (*match)(int x, int y, int cx, int cy),
                            int *match_x,
                            int *match_y,
                            struct vnc_screen_layout *match_layout)
{
    int error;
    struct stream *s;
    unsigned int num_rects;
    unsigned int i;
    int x;
    int y;
    int cx;
    int cy;
    encoding_type encoding;
    int found = 0;

    make_stream(s);
    init_stream(s, 8192);
    error = trans_force_read_s(v->trans, s, 3);

    if (error == 0)
    {
        in_uint8s(s, 1);
        in_uint16_be(s, num_rects);

        for (i = 0; i < num_rects; ++i)
        {
            if (error != 0)
            {
                break;
            }

            init_stream(s, 8192);
            error = trans_force_read_s(v->trans, s, 12);

            if (error == 0)
            {
                in_uint16_be(s, x);
                in_uint16_be(s, y);
                in_uint16_be(s, cx);
                in_uint16_be(s, cy);
                in_uint32_be(s, encoding);

                if (encoding == RFB_ENC_EXTENDED_DESKTOP_SIZE &&
                        !found &&
                        match(x, y, cx, cy))
                {
                    LOG(LOG_LEVEL_DEBUG,
                        "VNC_RESIZE: VNC matched ExtendedDesktopSize rectangle "
                        "x=%d, y=%d geom=%dx%d",
                        x, y, cx, cy);
                    found = 1;
                    error = read_extended_desktop_size_rect(v, match_layout);
                    if (match_x)
                    {
                        *match_x = x;
                    }
                    if (match_y)
                    {
                        *match_y = y;
                    }
                    match_layout->total_width = cx;
                    match_layout->total_height = cy;
                }
                else
                {
                    error = skip_encoding(v, x, y, cx, cy, encoding);
                }
            }
        }
    }

    free_stream(s);

    return error;
}

/**************************************************************************//**
 * Sends a FramebufferUpdateRequest for the resize status state machine
 *
 * The state machine is used at the start of the connection to negotiate
 * a common geometry between the client and the server.
 *
 * The RFB community wiki contains the following paragraph not present
 * in RFC6143:-
 *
 *     Note that an empty area can still solicit a FramebufferUpdate
 *     even though that update will only contain pseudo-encodings
 *
 * This doesn't seem to be as widely supported as we would like at
 * present. We will always request at least a single pixel update to
 * avoid confusing the server.
 *
 * @param v VNC object
 * @return != 0 for error
 */
static int
send_update_request_for_resize_status(struct vnc *v)
{
    int error = 0;
    struct stream *s;
    make_stream(s);
    init_stream(s, 8192);

    switch (v->resize_status)
    {
        case VRS_WAITING_FOR_FIRST_UPDATE:
        case VRS_WAITING_FOR_RESIZE_CONFIRM:
            /*
             * Ask for an immediate, minimal update.
             */
            out_uint8(s, RFB_C2S_FRAMEBUFFER_UPDATE_REQUEST);
            out_uint8(s, 0); /* incremental == 0 : Full update */
            out_uint16_be(s, 0);
            out_uint16_be(s, 0);
            out_uint16_be(s, 1);
            out_uint16_be(s, 1);
            s_mark_end(s);
            error = lib_send_copy(v, s);
            break;

        default:
            /*
             * Ask for a full update from the server
             */
            if (v->suppress_output == 0)
            {
                out_uint8(s, RFB_C2S_FRAMEBUFFER_UPDATE_REQUEST);
                out_uint8(s, 0); /* incremental == 0 : Full update */
                out_uint16_be(s, 0);
                out_uint16_be(s, 0);
                out_uint16_be(s, v->server_layout.total_width);
                out_uint16_be(s, v->server_layout.total_height);
                s_mark_end(s);
                error = lib_send_copy(v, s);
            }
            break;
    }

    free_stream(s);

    return error;
}

/**************************************************************************//**
 * Tests if extended desktop size rect is an initial geometry specification
 *
 * This should be x == 0, but the specification says to treat undefined
 * values as 0 also */
static int
rect_is_initial_geometry(int x, int y, int cx, int cy)
{
    return (x != 1 && x != 2);
}

/**************************************************************************//**
 * Tests if extended desktop size rect is a reply to a request from us
 */
static int
rect_is_reply_to_us(int x, int y, int cx, int cy)
{
    return (x == 1);
}

/**************************************************************************//**
 * Tests if extended desktop size rect is a general change.
 *
 * This happens when we are looking for a layout change that the
 * VNC server has reported as forwarded to the real desktop.
 */
static int
rect_is_general_change(int x, int y, int cx, int cy)
{
    return (x == 0);
}

/**************************************************************************//**
 * Handles the first framebuffer update from the server
 *
 * This is used to determine if the server supports resizes from
 * us. See The RFB community wiki for details.
 *
 * If the server does support resizing, we send our client geometry over.
 *
 * @param v VNC object
 * @return != 0 for error
 */
static int
lib_framebuffer_first_update(struct vnc *v)
{
    int error;
    struct vnc_screen_layout layout = {0};

    error = find_matching_extended_rect(v,
                                        rect_is_initial_geometry,
                                        NULL,
                                        NULL,
                                        &layout);
    if (error == 0)
    {
        if (layout.count > 0)
        {
            LOG(LOG_LEVEL_DEBUG, "VNC server supports resizing");
            v->resize_supported = VRSS_SUPPORTED;
            v->server_layout = layout;

            /* Force the client geometry over to the server */
            log_screen_layout(LOG_LEVEL_INFO, "ClientLayout", &v->client_layout);
            log_screen_layout(LOG_LEVEL_INFO, "OldServerLayout", &layout);

            /*
             * If we've only got one screen, and the other side has
             * only got one screen, we will preserve their screen ID
             * and any flags.  This may prevent us sending an unwanted
             * SetDesktopSize message if the screen dimensions are
             * a match. We can't do this with more than one screen,
             * as we have no way to map different IDs
             */
            if (layout.count == 1 && v->client_layout.count == 1)
            {
                LOG(LOG_LEVEL_DEBUG, "VNC "
                    "setting screen id to %d from server",
                    layout.s[0].id);

                v->client_layout.s[0].id = layout.s[0].id;
                v->client_layout.s[0].flags = layout.s[0].flags;
            }

            resize_server_to_client_layout(v);
        }
        else
        {
            LOG(LOG_LEVEL_DEBUG, "VNC server does not support resizing");
            v->resize_supported = VRSS_NOT_SUPPORTED;

            /* Force client to same size as server */
            LOG(LOG_LEVEL_DEBUG, "Resizing client to server %dx%d",
                v->server_layout.total_width, v->server_layout.total_height);
            error = resize_client_to_server(v, 0);
            v->resize_status = VRS_DONE;
        }
    }

    if (error == 0)
    {
        error = send_update_request_for_resize_status(v);
    }

    return error;
}

/**************************************************************************//**
 * Looks for a resize confirm in a framebuffer update request
 *
 * If the server supports resizes from us, this is used to find the
 * reply to our resize request. See The RFB community wiki for details.
 *
 * @param v VNC object
 * @return != 0 for error
 */
static int
lib_framebuffer_waiting_for_resize_confirm(struct vnc *v)
{
    int error;
    struct vnc_screen_layout layout = {0};
    int response_code = 0;

    error = find_matching_extended_rect(v,
                                        rect_is_reply_to_us,
                                        NULL,
                                        &response_code,
                                        &layout);
    if (error == 0)
    {
        if (layout.count > 0)
        {
            if (response_code == RFB_EDS_REQUEST_FORWARDED)
            {
                LOG(LOG_LEVEL_DEBUG, "VNC_RESIZE: VNC server resize forwarded");
                log_screen_layout(LOG_LEVEL_INFO, "ForwardedLayout", &layout);
                v->forward_timer =
                    timers_oneshot_init(FORWARDED_RESIZE_TIMEOUT);
                v->forwarded_layout = layout;
            }
            else
            {
                // The resize has either succeeded or failed
                if (response_code == RFB_EDS_NO_ERROR)
                {
                    LOG(LOG_LEVEL_DEBUG, "VNC_RESIZE:"
                        " VNC server successfully resized");
                    log_screen_layout(LOG_LEVEL_INFO, "NewLayout", &layout);
                    v->server_layout = layout;
                }
                else
                {
                    LOG(LOG_LEVEL_WARNING,
                        "VNC server resize failed - error code %d [%s]",
                        response_code,
                        rfb_get_eds_status_msg(response_code));
                    // This is awkward. The client has asked for a
                    // specific size which we can't support.
                    //
                    // Currently we handle this by queueing a resize
                    // to our supported size, and continuing with the
                    // resize state machine in xrdp_mm.c
                    LOG(LOG_LEVEL_WARNING, "Resizing client to server");
                    error = resize_client_to_server(v, 0);
                }

                v->resize_status = VRS_DONE;
                if (error == 0)
                {
                    // If this resize was requested by the client mid-session
                    // (dynamic resize), we need to tell xrdp_mm that
                    // it's OK to continue with the resize state machine.
                    error = v->server_monitor_resize_done(v);
                    if (error == 0)
                    {
                        error = send_update_request_for_resize_status(v);
                    }
                }
            }
        }
    }


    return error;
}

/**************************************************************************//**
 * Looks for the forwarded screen layout in a framebuffer update request
 *
 * Looks for an ExtendedDesktopSize rectangle following a notification
 * from the VNC server that the request has been forwarded to the real
 * desktop. See rfbproto/pfbproto#32 for more info.
 *
 * @param v VNC object
 * @return != 0 for error
 */
static int
lib_framebuffer_look_for_forwarded_layout(struct vnc *v)
{
    int error;
    struct vnc_screen_layout layout = {0};
    int x = 0;
    int y = 0;

    error = find_matching_extended_rect(v,
                                        rect_is_general_change,
                                        &x,
                                        &y,
                                        &layout);
    if (error == 0)
    {
        if (layout.count > 0)
        {
            if (vnc_screen_layouts_equal(&layout, &v->forwarded_layout))
            {
                LOG(LOG_LEVEL_DEBUG,
                    "VNC_RESIZE: VNC server forwarded resize complete");
                free(v->forward_timer);
                v->forward_timer = NULL;
                v->server_layout = layout;
                error = v->server_monitor_resize_done(v);
                v->resize_status = VRS_DONE;
            }
            else
            {
                LOG(LOG_LEVEL_DEBUG,
                    "VNC_RESIZE: Ignored ExtendedDesktopSize %dx%d x=%d y=%d",
                    layout.total_width, layout.total_height, x, y);
                // Delay for a little before we send another request for
                // the size
                g_sleep(100);
            }

            if (error != 0)
            {
                error = send_update_request_for_resize_status(v);
            }
        }
    }

    return error;
}

/******************************************************************************/
/*
 * The VNC server has not actioned a forwarded resize request
 */
static int
forward_timer_expired(struct vnc *v)
{
    LOG(LOG_LEVEL_WARNING, "VNC server forwarded resize timed out");
    LOG(LOG_LEVEL_DEBUG,
        "VNC_RESIZE: VNC server forwarded resize timed out");
    free(v->forward_timer);
    v->forward_timer = NULL;
    v->resize_status = VRS_DONE;

    int rv = v->server_monitor_resize_done(v);
    if (rv == 0)
    {
        LOG(LOG_LEVEL_WARNING, "Resizing client to server");
        rv = resize_client_to_server(v, 0);
    }

    return rv;
}

/******************************************************************************/
static int
lib_framebuffer_update(struct vnc *v)
{
    char *d1;
    char *d2;
    char cursor_data[32 * (32 * 3)];
    char cursor_mask[32 * (32 / 8)];
    char text[256];
    int num_recs;
    int i;
    int j;
    int k;
    int x;
    int y;
    int cx;
    int cy;
    int srcx;
    int srcy;
    unsigned int encoding;
    int pixel;
    int r = 0;
    int g = 0;
    int b = 0;
    int error;
    int need_size;
    struct stream *s;
    struct stream *pixel_s;
    struct vnc_screen_layout layout = { 0 };

    num_recs = 0;

    make_stream(pixel_s);

    make_stream(s);
    init_stream(s, 8192);
    error = trans_force_read_s(v->trans, s, 3);

    if (error == 0)
    {
        in_uint8s(s, 1);
        in_uint16_be(s, num_recs);
        error = v->server_begin_update(v);
    }

    for (i = 0; i < num_recs; i++)
    {
        if (error != 0)
        {
            break;
        }

        init_stream(s, 8192);
        error = trans_force_read_s(v->trans, s, 12);

        if (error == 0)
        {
            in_uint16_be(s, x);
            in_uint16_be(s, y);
            in_uint16_be(s, cx);
            in_uint16_be(s, cy);
            in_uint32_be(s, encoding);

            if (encoding == RFB_ENC_RAW)
            {
                need_size = cx * cy * get_bytes_per_pixel(v->server_bpp);
                init_stream(pixel_s, need_size);
                error = trans_force_read_s(v->trans, pixel_s, need_size);

                if (error == 0)
                {
                    error = v->server_paint_rect(v, x, y, cx, cy, pixel_s->data, cx, cy, 0, 0);
                }
            }
            else if (encoding == RFB_ENC_COPY_RECT)
            {
                init_stream(s, 8192);
                error = trans_force_read_s(v->trans, s, 4);

                if (error == 0)
                {
                    in_uint16_be(s, srcx);
                    in_uint16_be(s, srcy);
                    error = v->server_screen_blt(v, x, y, cx, cy, srcx, srcy);
                }
            }
            else if (encoding == RFB_ENC_CURSOR)
            {
                g_memset(cursor_data, 0, 32 * (32 * 3));
                g_memset(cursor_mask, 0, 32 * (32 / 8));
                j = cx * cy * get_bytes_per_pixel(v->server_bpp);
                k = ((cx + 7) / 8) * cy;
                init_stream(s, j + k);
                error = trans_force_read_s(v->trans, s, j + k);

                if (error == 0)
                {
                    in_uint8p(s, d1, j);
                    in_uint8p(s, d2, k);

                    for (j = 0; j < 32; j++)
                    {
                        for (k = 0; k < 32; k++)
                        {
                            pixel = get_pixel_safe(d2, k, 31 - j, cx, cy, 1);
                            set_pixel_safe(cursor_mask, k, j, 32, 32, 1, !pixel);

                            if (pixel)
                            {
                                pixel = get_pixel_safe(d1, k, 31 - j, cx, cy, v->server_bpp);
                                split_color(pixel, &r, &g, &b, v->server_bpp, v->palette);
                                pixel = make_color(r, g, b, 24);
                                set_pixel_safe(cursor_data, k, j, 32, 32, 24, pixel);
                            }
                        }
                    }

                    /* keep these in 32x32, vnc cursor can be a lot bigger */
                    if (x > 31)
                    {
                        x = 31;
                    }

                    if (y > 31)
                    {
                        y = 31;
                    }

                    error = v->server_set_cursor(v, x, y, cursor_data, cursor_mask);
                }
            }
            else if (encoding == RFB_ENC_DESKTOP_SIZE)
            {
                /* Server end has resized */
                init_single_screen_layout(cx, cy, &v->server_layout);
                error = resize_client_to_server(v, 1);
            }
            else if (encoding == RFB_ENC_EXTENDED_DESKTOP_SIZE)
            {
                layout.total_width = cx;
                layout.total_height = cy;
                error = read_extended_desktop_size_rect(v, &layout);
                /* If this is a reply to a request from us, x == 1 */
                LOG(LOG_LEVEL_DEBUG,
                    "VNC_RESIZE: Read ExtendedDesktopSize %dx%d x=%d y=%d",
                    layout.total_width, layout.total_height, x, y);
                if (error == 0 && x != 1)
                {
                    if (!vnc_screen_layouts_equal(&v->server_layout, &layout))
                    {
                        v->server_layout = layout;
                        log_screen_layout(LOG_LEVEL_INFO, "NewServerLayout",
                                          &v->server_layout);
                        error = resize_client_to_server(v, 1);
                    }
                }
            }
            else
            {
                g_sprintf(text, "VNC error in lib_framebuffer_update encoding = %8.8x",
                          encoding);
                v->server_msg(v, text, 1);
            }
        }
    }

    if (error == 0)
    {
        error = v->server_end_update(v);
    }

    if (error == 0)
    {
        if (v->suppress_output == 0)
        {
            init_stream(s, 8192);
            out_uint8(s, RFB_C2S_FRAMEBUFFER_UPDATE_REQUEST);
            out_uint8(s, 1); /* incremental == 1 : Changes only */
            out_uint16_be(s, 0);
            out_uint16_be(s, 0);
            out_uint16_be(s, v->server_layout.total_width);
            out_uint16_be(s, v->server_layout.total_height);
            s_mark_end(s);
            error = lib_send_copy(v, s);
        }
    }

    free_stream(s);
    free_stream(pixel_s);
    return error;
}

/******************************************************************************/
static int
lib_palette_update(struct vnc *v)
{
    struct stream *s;
    int first_color;
    int num_colors;
    int i;
    int r;
    int g;
    int b;
    int error;

    make_stream(s);
    init_stream(s, 8192);
    error = trans_force_read_s(v->trans, s, 5);

    if (error == 0)
    {
        in_uint8s(s, 1);
        in_uint16_be(s, first_color);
        in_uint16_be(s, num_colors);
        init_stream(s, 8192);
        error = trans_force_read_s(v->trans, s, num_colors * 6);
    }

    if (error == 0)
    {
        for (i = 0; i < num_colors; i++)
        {
            in_uint16_be(s, r);
            in_uint16_be(s, g);
            in_uint16_be(s, b);
            r = r >> 8;
            g = g >> 8;
            b = b >> 8;
            v->palette[first_color + i] = (r << 16) | (g << 8) | b;
        }

        error = v->server_begin_update(v);
    }

    if (error == 0)
    {
        error = v->server_palette(v, v->palette);
    }

    if (error == 0)
    {
        error = v->server_end_update(v);
    }

    free_stream(s);
    return error;
}

/******************************************************************************/
static int
lib_bell_trigger(struct vnc *v)
{
    int error;

    error = v->server_bell_trigger(v);
    return error;
}

/******************************************************************************/
static int
lib_mod_signal(struct vnc *v)
{
    return 0;
}

/******************************************************************************/
static int
lib_mod_process_message(struct vnc *v, struct stream *s)
{
    char type;
    int error;
    char text[256];

    in_uint8(s, type);

    error = 0;
    if (error == 0)
    {
        if (type == RFB_S2C_FRAMEBUFFER_UPDATE)
        {
            switch (v->resize_status)
            {
                case VRS_WAITING_FOR_FIRST_UPDATE:
                    error = lib_framebuffer_first_update(v);
                    break;

                case VRS_WAITING_FOR_RESIZE_CONFIRM:
                    if (v->forward_timer != NULL)
                    {
                        error = lib_framebuffer_look_for_forwarded_layout(v);
                    }
                    else
                    {
                        error = lib_framebuffer_waiting_for_resize_confirm(v);
                    }
                    break;

                default:
                    error = lib_framebuffer_update(v);
            }
        }
        else if (type == RFB_S2C_SET_COLOUR_MAP_ENTRIES)
        {
            error = lib_palette_update(v);
        }
        else if (type == RFB_S2C_BELL)
        {
            error = lib_bell_trigger(v);
        }
        else if (type == RFB_S2C_SERVER_CUT_TEXT) /* clipboard */
        {
            LOG(LOG_LEVEL_DEBUG, "VNC got clip data");
            error = vnc_clip_process_rfb_data(v);
        }
        else
        {
            g_sprintf(text, "VNC unknown in lib_mod_process_message %d", type);
            v->server_msg(v, text, 1);
        }
    }

    return error;
}

/******************************************************************************/
static int
lib_mod_start(struct vnc *v, int w, int h, int bpp)
{
    v->server_begin_update(v);
    v->server_set_fgcolor(v, 0);
    v->server_fill_rect(v, 0, 0, w, h);
    v->server_end_update(v);
    v->server_bpp = bpp;
    return 0;
}

/******************************************************************************/
static int
lib_data_in(struct trans *trans)
{
    struct vnc *self;
    struct stream *s;

    LOG_DEVEL(LOG_LEVEL_TRACE, "lib_data_in:");

    if (trans == 0)
    {
        return 1;
    }

    self = (struct vnc *)(trans->callback_data);
    s = trans_get_in_s(trans);

    if (s == 0)
    {
        return 1;
    }

    if (lib_mod_process_message(self, s) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "lib_data_in: lib_mod_process_message failed");
        return 1;
    }

    init_stream(s, 0);

    return 0;
}

/******************************************************************************/
/**
 * Gets a reason string from the server
 *
 * Sometimes the server sends an error, which is a 32-bit word
 * followed by a string
 */
static int
get_reason_string(struct vnc *v, char buff[], unsigned int bufflen)
{
    int rv = 1;
    struct stream *s;
    make_stream(s);
    init_stream(s, (int)(bufflen + 4));

    if (trans_force_read_s(v->trans, s, 4) == 0)
    {
        unsigned int len;
        in_uint32_be(s, len);
        if (len < bufflen)
        {
            if (trans_force_read_s(v->trans, s, len) == 0)
            {
                in_uint8a(s, buff, len);
                buff[len] = '\0';
                rv = 0;
            }
        }
    }

    free_stream(s);
    return rv;
}

/******************************************************************************/
/**
 * Negotiates the protocol version with the server
 *
 * @param v Module
 * @param[out] next_char Either security-type (version 3.3) or
 *                       number-of-security-types (versions > 3.3)
 * @return 0 for error, or protocol version > 0
 *
 * The protocol negotiation is overlapped with the security negotiation.
 * The result of the protocol negotiation is either good, in which case
 * 'next_char' contains a protocol-dependent value, or not-good, in which
 * case 'next_char' was set to zero and followed by a reason string. The
 * reason string is consumed here, and logged.
 *
 * See sections 7.1.1 and 7.1.2 of the RFB community wiki
 */
static unsigned int
negotiate_protocol_version(struct vnc *v, unsigned char *next_char)
{
    struct stream *s =  NULL;
    unsigned int major;
    unsigned int minor;
    unsigned int version;
    int sec_type_size;

    make_stream(s);
    init_stream(s, 64);
    if (trans_force_read_s(v->trans, s, 12) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "Error reading server version string");
        goto fail;
    }

    const char *p;
    in_uint8p(s, p, 12); /* Moves s->p to end of string */
    /* Expecting a string "RFB ???.???\n" where ? is a digit */
    if (*p++ != 'R' || *p++ != 'F' || *p++ != 'B' ||
            *p++ != ' ' ||
            !isdigit(*p++) || !isdigit(*p++) || !isdigit(*p++) ||
            *p++ != '.' ||
            !isdigit(*p++) || !isdigit(*p++) || !isdigit(*p++) ||
            *p++ != '\n')
    {
        LOG_HEXDUMP(LOG_LEVEL_ERROR, "Invalid RFB string :", s->data, 12);
        goto fail;
    }

    /* Parse the major/minor versions in-place */
    p = s->data;
    major = (p[4] - '0') * 100 + (p[5] - '0') * 10 + (p[6] - '0');
    minor = (p[8] - '0') * 100 + (p[9] - '0') * 10 + (p[10] - '0');
    version = MAKE_RFBPROTO_VER(major, minor);

    if (version == RFBPROTO_VER_3_3 ||
            version == RFBPROTO_VER_3_7 ||
            version == RFBPROTO_VER_3_8)
    {
        /* Versions documented in RFC6143 */
        LOG(LOG_LEVEL_INFO,
            "RFB version %d.%d is supported by VNC server",
            major, minor);
    }
    else if (major == 3)
    {
        /* RFC6143 section 6 states that unknown 3.x versions should
         * be treated as 3.3 */
        LOG(LOG_LEVEL_INFO, "RFB server reports version %d.%d.",
            major, minor);

        minor = 3;
        version = MAKE_RFBPROTO_VER(major, minor);
        LOG(LOG_LEVEL_INFO, "Proposing RFB version %d.%d to server",
            major, minor);
    }
    else if (major > 3)
    {
        /* This must be a new server version. Try to fall back to 3.8 */
        LOG(LOG_LEVEL_INFO, "RFB server reports version %d.%d.",
            major, minor);

        major = 3;
        minor = 8;
        version = MAKE_RFBPROTO_VER(major, minor);
        LOG(LOG_LEVEL_INFO, "Proposing RFB version %d.%d to server",
            major, minor);
    }
    else
    {
        LOG(LOG_LEVEL_ERROR,
            "Server reports unsupported RFB version %d.%d",
            major, minor);
        goto fail;
    }

    /* Send our proposed version back to the server */
    /* s->p should be in the right place to mark the end
     * of the string */
    g_snprintf(s->data, s->size, "RFB %03d.%03d\n", major, minor);
    s_mark_end(s);

    if (trans_force_write_s(v->trans, s) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "Error writing server version string");
        goto fail;
    }

    // Version 3.3 sends back a U32 rather than a U8 for the security
    // type, even though the values it supports fit easily in a U8
    sec_type_size = (version == RFBPROTO_VER_3_3) ? 4 : 1;
    init_stream(s, 64);
    if (trans_force_read_s(v->trans, s, sec_type_size) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "Can't read negotiation result from server");
        goto fail;
    }

    // Read the next field, being careful to parse a whole U32 for
    // version 3.3.
    if (version == RFBPROTO_VER_3_3)
    {
        in_uint8s(s, 3); // Skip the three high octets
    }
    in_uint8(s, *next_char);

    if (*next_char == 0)
    {
        char text[256];
        // Server reported a reason for failure
        if (get_reason_string(v, text, sizeof(text)) != 0)
        {
            g_snprintf(text, sizeof(text), "No reason given");
        }
        LOG(LOG_LEVEL_ERROR,
            "Version negotiation with server failed [%s]", text);
        goto fail;
    }

    free_stream(s);
    return version;

fail:
    free_stream(s);
    return 0;
}

/******************************************************************************/
/**
 * Chooses the security type from a list sent by the server
 *
 * @param v Module
 * @param max_security_type Max security type to negotiate
 * @return 0 for error, or security type ( > 0)
 *
 * See section 7.1.2 of the RFB community wiki
 *
 * This call is only made for RFB version 3.7 onwards
 */
static enum sec_type
choose_security_type(struct vnc *v, unsigned char number_of_security_types,
                     enum sec_type max_security_type)
{
    enum sec_type sec_type = SEC_TYPE_INVALID;

    struct stream *s =  NULL;
    make_stream(s);
    init_stream(s, UCHAR_MAX);

    if (trans_force_read_s(v->trans, s, number_of_security_types) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "Can't read list of security types from server");
    }
    else
    {
        while (s_rem(s) > 0)
        {
            int j;
            in_uint8(s, j);
            enum sec_type st = (enum sec_type)j;
            // Choose the highest security level that we support
            if (st > max_security_type)
            {
                // If in development, log unsupported security types
                LOG_DEVEL(LOG_LEVEL_INFO,
                          "Unsupported VNC security type %d was offered", j);
                continue;
            }

            if (st > sec_type)
            {
                sec_type = st;
            }
        }
        if (sec_type == SEC_TYPE_INVALID)
        {
            LOG(LOG_LEVEL_ERROR,
                "RFB server did not offer a compatible security type");
        }
        else
        {
            init_stream(s, 1);
            out_uint8(s, sec_type);
            s_mark_end(s);
            if (trans_force_write_s(v->trans, s) != 0)
            {
                LOG(LOG_LEVEL_ERROR, "Can't send security type to server");
                sec_type = SEC_TYPE_INVALID;
            }
        }
    }

    free_stream(s);
    return sec_type;
}

/******************************************************************************/
/**
 * Negotiates the security type with the server
 *
 * @param v Module
 * @param rfbproto_version RFB version negotiated with the server
 * @param next_char The character sent back (>0) when the protocol
 *                  version was agreed.
 * @return the negotiated security type (or SEC_TYPE_INVALID)
 */
static enum sec_type
negotiate_security_type(struct vnc *v, unsigned int rfbproto_version,
                        unsigned char next_char)
{
    char text[256];
    enum sec_type sec_type;
    // Whether the SecurityResult word is read from the server
    // (RFB Community wiki section 7.1.3)
    int check_sec_result = 1;

    struct stream *s =  NULL;
    make_stream(s);
    init_stream(s, 64);

    if (rfbproto_version == RFBPROTO_VER_3_3)
    {
        // The server has already chosen the security type
        sec_type = (enum sec_type)next_char;
    }
    else
    {
        // The client chooses the security type based on what's offered
        sec_type = choose_security_type(v, next_char, SEC_TYPE_MAX);
    }

    if (sec_type == SEC_TYPE_INVALID)
    {
        goto fail; // An error has already been logged
    }

    g_sprintf(text, "VNC security type is %s", SEC_TYPE_TO_STR(sec_type));
    v->server_msg(v, text, 0);

    switch (sec_type)
    {
        case SEC_TYPE_NONE:
            if (rfbproto_version < RFBPROTO_VER_3_8)
            {
                check_sec_result = 0;
            }
            break;

        case SEC_TYPE_VNC_AUTH:
        {
            init_stream(s, 64);
            if (trans_force_read_s(v->trans, s, 16) != 0)
            {
                LOG(LOG_LEVEL_ERROR,
                    "Can't read VNC auth challenge from server");
                goto fail;
            }
            if (guid_is_set(&v->guid))
            {
                char guid_str[GUID_STR_SIZE];
                guid_to_str(&v->guid, guid_str);
                rfbHashEncryptBytes(s->data, guid_str);
            }
            else
            {
                rfbEncryptBytes(s->data, v->password);
            }
            s->p += 16;
            s_mark_end(s);
            if (trans_force_write_s(v->trans, s) != 0)
            {
                LOG(LOG_LEVEL_ERROR, "Can't send VNC auth response to server");
                goto fail;
            }
            break;
        }
        default:
            // Shouldn't get here
            LOG(LOG_LEVEL_ERROR, "VNC unsupported security type %d", sec_type);
            goto fail;
    }

    if (check_sec_result)
    {
        /* RFB Community wiki 7.1.3 */
        init_stream(s, 4);
        if (trans_force_read_s(v->trans, s, 4) != 0)
        {
            LOG(LOG_LEVEL_ERROR, "Can't read SecurityResult from server");
            goto fail;
        }

        int i;
        in_uint32_be(s, i);

        if (i != 0)
        {
            char msg[256];
            // Versions >= 3.8 of the protocol send a reason string at
            // this point
            if (rfbproto_version < RFBPROTO_VER_3_8 ||
                    get_reason_string(v, text, sizeof(text)) != 0)
            {
                g_snprintf(text, sizeof(text), "No reason given");
            }
            g_snprintf(msg, sizeof(msg),
                       "VNC security negotiation failed [%s]", text);
            v->server_msg(v, msg, 0);
            goto fail;
        }
    }

    free_stream(s);
    return sec_type;

fail:
    free_stream(s);
    return SEC_TYPE_INVALID;
}

/******************************************************************************/
/**
 * Sends the client init to the server (RFC6143 7.3.1)
 *
 * @param v Module
 * @param share_flag Share flag value to send to the server
 * @return 0 for success
 */
static int
send_client_init(struct vnc *v, int share_flag)
{
    int rv;
    struct stream *s;
    make_stream(s);
    init_stream(s, 64);
    out_uint8(s, (share_flag) ? 1 : 0);
    s_mark_end(s);
    rv = trans_force_write_s(v->trans, s);
    free_stream(s);
    return rv;
}

/******************************************************************************/
/**
 * Receives the server init from the server (RFC6143 7.3.2)
 *
 * @param v Module
 * @return 0 for success
 */
static int
receive_server_init(struct vnc *v)
{
    int rv;
    struct stream *s;
    int width;
    int height;
    int name_len;
    make_stream(s);
    init_stream(s, 256);
    rv = trans_force_read_s(v->trans, s, 2 + 2 + 16 + 4);
    if (rv == 0)
    {
        in_uint16_be(s, width);
        in_uint16_be(s, height);
        in_uint8s(s, 16); // skip server pixel format
        in_uint32_be(s, name_len);
        init_stream(s, 256); // Reset stream to read name

        if (name_len > 255 || name_len < 0)
        {
            LOG(LOG_LEVEL_ERROR, "Unexpected name length %d received",
                name_len);
            rv = 1;
        }
        else if (trans_force_read_s(v->trans, s, name_len) != 0)
        {
            LOG(LOG_LEVEL_ERROR, "Error receiving desktop name");
            rv = 1;
        }
        else
        {
            g_memcpy(v->mod_name, s->data, name_len);
            v->mod_name[name_len] = 0;

            init_single_screen_layout(width, height, &v->server_layout);
        }
    }
    free_stream(s);
    return rv;
}

/******************************************************************************/
/**
 * Sets the pixel format (RFC6143 7.5.1)
 *
 * @param v Module
 * @return 0 for success
 */
static int
set_pixel_format(struct vnc *v)
{
    struct
    {
        unsigned char bits_per_pixel;
        unsigned char depth;
        unsigned char true_color;
        unsigned short red_max;
        unsigned short green_max;
        unsigned short blue_max;
        unsigned char red_shift;
        unsigned char green_shift;
        unsigned char blue_shift;
    } pixel_format = {0};

    int rv;
    struct stream *s;

    if (v->server_bpp == 8)
    {
        pixel_format.bits_per_pixel = 8;
        pixel_format.depth = 8;
    }
    else if (v->server_bpp == 15)
    {
        pixel_format.bits_per_pixel = 16;
        pixel_format.depth = 15;
        pixel_format.true_color = 1;
        pixel_format.red_max = 31;
        pixel_format.green_max = 31;
        pixel_format.blue_max = 31;
        pixel_format.red_shift = 10;
        pixel_format.green_shift = 5;
        pixel_format.blue_shift = 0;
    }
    else if (v->server_bpp == 16)
    {
        pixel_format.bits_per_pixel = 16;
        pixel_format.depth = 16;
        pixel_format.true_color = 1;
        pixel_format.red_max = 31;
        pixel_format.green_max = 63;
        pixel_format.blue_max = 31;
        pixel_format.red_shift = 11;
        pixel_format.green_shift = 5;
        pixel_format.blue_shift = 0;
    }
    else if (v->server_bpp == 24 || v->server_bpp == 32)
    {
        pixel_format.bits_per_pixel = 32;
        pixel_format.depth = 24;
        pixel_format.true_color = 1;
        pixel_format.red_max = 255;
        pixel_format.green_max = 255;
        pixel_format.blue_max = 255;
        pixel_format.red_shift = 16;
        pixel_format.green_shift = 8;
        pixel_format.blue_shift = 0;
    }

    make_stream(s);
    init_stream(s, 64);
    out_uint8(s, RFB_C2S_SET_PIXEL_FORMAT);
    out_uint8s(s, 3); /* pad */
    /* Now send the pixel data block */
    out_uint8(s, pixel_format.bits_per_pixel);
    out_uint8(s, pixel_format.depth);
#if defined(B_ENDIAN)
    out_uint8(s, 1); /* big endian */
#else
    out_uint8(s, 0); /* big endian */
#endif
    out_uint8(s, pixel_format.true_color);
    out_uint16_be(s, pixel_format.red_max);
    out_uint16_be(s, pixel_format.green_max);
    out_uint16_be(s, pixel_format.blue_max);
    out_uint8(s, pixel_format.red_shift);
    out_uint8(s, pixel_format.green_shift);
    out_uint8(s, pixel_format.blue_shift);
    out_uint8s(s, 3); /* pad */
    s_mark_end(s);
    rv = trans_force_write_s(v->trans, s);
    free_stream(s);
    return rv;
}

/******************************************************************************/
/**
 * Sets the encodings (RFC6143 7.5.2)
 *
 * @param v Module
 * @return 0 for success
 */
static int
set_encodings(struct vnc *v)
{
    encoding_type e[10];
    unsigned int n = 0;
    unsigned int i;

    int rv;
    struct stream *s;

    /* These encodings are always supported */
    e[n++] = RFB_ENC_RAW;
    e[n++] = RFB_ENC_COPY_RECT;
    e[n++] = RFB_ENC_CURSOR;
    e[n++] = RFB_ENC_DESKTOP_SIZE;
    if (v->enabled_encodings_mask & MSK_EXTENDED_DESKTOP_SIZE)
    {
        e[n++] = RFB_ENC_EXTENDED_DESKTOP_SIZE;
    }
    else
    {
        LOG(LOG_LEVEL_INFO,
            "VNC User disabled EXTENDED_DESKTOP_SIZE");
    }

    make_stream(s);
    init_stream(s, (int)(4 + sizeof(e)));
    out_uint8(s, RFB_C2S_SET_ENCODINGS);
    out_uint8(s, 0);
    out_uint16_be(s, n); /* Number of encodings following */
    for (i = 0 ; i < n; ++i)
    {
        out_uint32_be(s, e[i]);
    }
    s_mark_end(s);
    rv = trans_force_write_s(v->trans, s);
    free_stream(s);

    return rv;
}

/******************************************************************************/
/*
  return error
*/
static int
lib_mod_connect(struct vnc *v, int fd)
{
    char cursor_data[32 * (32 * 3)];
    char cursor_mask[32 * (32 / 8)];
    char con_port[256];
    char text[256];
    unsigned int rfbproto_version;
    int error;
    int socket_mode;

    g_snprintf(con_port, sizeof(con_port), "%s", v->port);

    /* check if bpp is supported for rdp connection */
    switch (v->server_bpp)
    {
        case 8:
        case 15:
        case 16:
        case 24:
        case 32:
            break;
        default:
            v->server_msg(v, "VNC error - only supporting 8, 15, 16, 24 and 32 "
                          "bpp rdp connections", 0);
            return 1;
    }

    /* Assume a TCP-port based connection (i.e. not a UDS connection)
     * if the port is not an absolute path */
    if (con_port[0] == '/')
    {
        socket_mode = TRANS_MODE_UNIX;
    }
    else
    {
        socket_mode = TRANS_MODE_TCP;
        if (g_strcmp(v->ip, "") == 0)
        {
            v->server_msg(v, "VNC error - no IP set for TCP connection", 0);
            goto fail;
        }
    }

    v->trans = trans_create(socket_mode, 8 * 8192, 8192);
    if (v->trans == 0)
    {
        v->server_msg(v, "VNC error: trans_create() failed", 0);
        goto fail;
    }

    v->sck_closed = 0;
    if (v->delay_ms > 0)
    {
        g_sprintf(text, "Waiting %d ms for VNC to start...", v->delay_ms);
        v->server_msg(v, text, 0);
        g_sleep(v->delay_ms);
    }

    v->trans->si = v->si;
    v->trans->my_source = XRDP_SOURCE_MOD;

    if (fd >= 0)
    {
        v->trans->sck = fd;
        v->trans->status = TRANS_STATUS_UP; /* ok */
        v->trans->type1 = TRANS_TYPE_CLIENT; /* client */
        error = 0;
    }
    else
    {
        error = trans_connect(v->trans, v->ip, con_port, 3000);
    }

    if (error != 0)
    {
        g_snprintf(text, sizeof(text), "Error connecting to VNC server [%s]",
                   g_get_strerror());
        v->server_msg(v, text, 0);
        goto fail;
    }

    if (socket_mode == TRANS_MODE_TCP)
    {
        g_sprintf(text, "VNC connected to TCP %s %s", v->ip, con_port);
    }
    else
    {
        g_sprintf(text, "VNC connected to local socket %s", con_port);
    }
    v->server_msg(v, text, 0);

    /* protocol version */
    unsigned char next_char;
    if ((rfbproto_version = negotiate_protocol_version(v, &next_char)) == 0)
    {
        v->server_msg(v, "Error negotiating VNC version", 0);
        goto fail;
    }

    if (negotiate_security_type(v, rfbproto_version, next_char) ==
            SEC_TYPE_INVALID)
    {
        // An error has been logged
        v->server_msg(v, "Error negotiating security type", 0);
        goto fail;
    }

    if (send_client_init(v, 1) != 0)
    {
        v->server_msg(v, "Error sending client init", 0);
        goto fail;
    }

    if (receive_server_init(v) != 0)
    {
        v->server_msg(v, "Error receiving server init", 0);
        goto fail;
    }

    if (set_pixel_format(v) != 0)
    {
        v->server_msg(v, "Error setting pixel format", 0);
        goto fail;
    }

    if (set_encodings(v) != 0)
    {
        v->server_msg(v, "Error setting encodings", 0);
        goto fail;
    }

    v->resize_supported = VRSS_UNKNOWN;
    v->resize_status = VRS_WAITING_FOR_FIRST_UPDATE;
    if (send_update_request_for_resize_status(v) != 0)
    {
        v->server_msg(v, "Error sending resize support request", 0);
        goto fail;
    }

    /* set almost null cursor, this is the little dot cursor */
    g_memset(cursor_data, 0, 32 * (32 * 3));
    g_memset(cursor_data + (32 * (32 * 3) - 1 * 32 * 3), 0xff, 9);
    g_memset(cursor_data + (32 * (32 * 3) - 2 * 32 * 3), 0xff, 9);
    g_memset(cursor_data + (32 * (32 * 3) - 3 * 32 * 3), 0xff, 9);
    g_memset(cursor_mask, 0xff, 32 * (32 / 8));
    if (v->server_set_cursor(v, 3, 3, cursor_data, cursor_mask) != 0)
    {
        v->server_msg(v, "Error sending cursor", 0);
        goto fail;
    }

    v->server_msg(v, "VNC connection complete, connected ok", 0);
    vnc_clip_open_clip_channel(v);

    v->trans->trans_data_in = lib_data_in;
    v->trans->header_size = 1;
    v->trans->callback_data = v;

    return 0;

fail:
    trans_delete(v->trans);
    v->trans = NULL;
    v->server_msg(v, "VNC error - problem connecting", 0);

    return 1;
}

/******************************************************************************/
static int
lib_mod_end(struct vnc *v)
{
    if (v->vnc_desktop != 0)
    {
    }

    return 0;
}

/**************************************************************************//**
 * Initialises the client layout from the Windows monitor definition.
 *
 * @param v VNC module
 * @param [in] width session width
 * @param [in] height session height
 * @param [in] num_monitors (can be 0, meaning one monitor)
 * @param [in] monitors Monitor definitions for num_monitors > 0
 * @param [in] multimon_configured Whether multimon is configured
 */
static void
init_client_layout(struct vnc *v,
                   int width, int height,
                   int num_monitors,
                   const struct monitor_info *monitors)
{
    struct vnc_screen_layout *layout = &v->client_layout;
    if (!v->multimon_configured || num_monitors < 1)
    {
        init_single_screen_layout(width, height, layout);
    }
    else
    {
        layout->total_width = width;
        layout->total_height = height;
        layout->count = num_monitors;

        unsigned int i;
        for (i = 0 ; i < layout->count; ++i)
        {
            layout->s[i].id = i;
            layout->s[i].x = monitors[i].left;
            layout->s[i].y = monitors[i].top;
            layout->s[i].width = monitors[i].right - monitors[i].left + 1;
            layout->s[i].height = monitors[i].bottom - monitors[i].top + 1;
            layout->s[i].flags = 0;
        }
    }
}

/******************************************************************************/
static int
lib_mod_set_param(struct vnc *v, const char *name, const char *value)
{
    if (g_strcasecmp(name, "username") == 0)
    {
        g_strncpy(v->username, value, 255);
    }
    else if (g_strcasecmp(name, "password") == 0)
    {
        g_strncpy(v->password, value, 255);
    }
    else if (g_strcasecmp(name, "ip") == 0)
    {
        g_strncpy(v->ip, value, 255);
    }
    else if (g_strcasecmp(name, "port") == 0)
    {
        g_strncpy(v->port, value, 255);
    }
    else if (g_strcasecmp(name, "keylayout") == 0)
    {
        v->keylayout = g_atoi(value);
    }
    else if (g_strcasecmp(name, "delay_ms") == 0)
    {
        v->delay_ms = g_atoi(value);
    }
    else if (g_strcasecmp(name, "guid") == 0)
    {
        v->guid = *(struct guid *)value;
    }
    else if (g_strcasecmp(name, "disabled_encodings_mask") == 0)
    {
        v->enabled_encodings_mask = (unsigned int)~g_atoi(value);
    }
    else if (g_strcasecmp(name, "client_info") == 0)
    {
        const struct xrdp_client_info *client_info =
            (const struct xrdp_client_info *) value;

        v->multimon_configured = client_info->multimon;

        /* Save monitor information from the client
         * Use minfo_wm, as this is normalised for a top-left of (0,0)
         * as required by RFC6143 */
        init_client_layout(v,
                           client_info->display_sizes.session_width,
                           client_info->display_sizes.session_height,
                           client_info->display_sizes.monitorCount,
                           client_info->display_sizes.minfo_wm);
        log_screen_layout(LOG_LEVEL_DEBUG, "client_info", &v->client_layout);
    }


    return 0;
}

/******************************************************************************/
/* return error */
static int
lib_mod_get_wait_objs(struct vnc *v, tbus *read_objs, int *rcount,
                      tbus *write_objs, int *wcount, int *timeout)
{
    LOG_DEVEL(LOG_LEVEL_TRACE, "lib_mod_get_wait_objs:");

    if (v != 0)
    {
        if (v->trans != 0)
        {
            trans_get_wait_objs_rw(v->trans, read_objs, rcount,
                                   write_objs, wcount, timeout);
        }

        // Update timeout with any active timers
        unsigned int now = g_get_elapsed_ms();
        timers_oneshot_update_poll(v->forward_timer, now, timeout);
    }

    return 0;
}

/******************************************************************************/
/* return error */
static int
lib_mod_check_wait_objs(struct vnc *v)
{
    int rv;

    rv = 0;
    if (v != 0)
    {
        if (v->trans != 0)
        {
            if ((rv = trans_check_wait_objs(v->trans)) != 0)
            {
                LOG(LOG_LEVEL_ERROR, "VNC server closed connection");
            }
            else
            {
                // Check timers
                unsigned int now = g_get_elapsed_ms();
                if (timers_oneshot_get_remaining(v->forward_timer, now) == 0)
                {
                    rv = forward_timer_expired(v);
                }
            }
        }
    }

    return rv;
}

/******************************************************************************/
/* return error */
static int
lib_mod_frame_ack(struct vnc *v, int flags, int frame_id)
{
    return 0;
}

/******************************************************************************/
/* return error */
static int
lib_mod_suppress_output(struct vnc *v, int suppress,
                        int left, int top, int right, int bottom)
{
    int error;
    struct stream *s;

    error = 0;
    v->suppress_output = suppress;
    if (suppress == 0)
    {
        make_stream(s);
        init_stream(s, 8192);
        out_uint8(s, RFB_C2S_FRAMEBUFFER_UPDATE_REQUEST);
        out_uint8(s, 0); /* incremental == 0 : Full contents */
        out_uint16_be(s, 0);
        out_uint16_be(s, 0);
        out_uint16_be(s, v->server_layout.total_width);
        out_uint16_be(s, v->server_layout.total_height);
        s_mark_end(s);
        error = lib_send_copy(v, s);
        free_stream(s);
    }
    return error;
}

/******************************************************************************/
/* return error */
static int
lib_mod_server_version_message(struct vnc *v)
{
    return 0;
}

/******************************************************************************/
/* return error */
static int
lib_mod_server_monitor_resize(struct vnc *v, int width, int height,
                              int num_monitors,
                              const struct monitor_info *monitors,
                              int *in_progress)
{
    int error;
    *in_progress = 0;
    init_client_layout(v, width, height, num_monitors, monitors);

    if ((error = resize_server_to_client_layout(v)) == 0)
    {
        // If we're waiting for a confirmation, send an update request.
        // According to the spec this should not be needed, but
        // it works around a buggy VNC server not sending an
        // ExtendedDesktopSize rectangle if the desktop change is
        // small (eg. same dimensions, but 2 monitors -> 1 monitor)
        if (v->resize_status == VRS_WAITING_FOR_RESIZE_CONFIRM &&
                (error = send_update_request_for_resize_status(v)) == 0)
        {
            *in_progress = 1;
        }
    }

    return error;
}

/******************************************************************************/
/* return error */
static int
lib_mod_server_monitor_full_invalidate(struct vnc *v, int param1, int param2)
{
    return 0;
}

/******************************************************************************/
tintptr EXPORT_CC
mod_init(void)
{
    struct vnc *v;

    v = (struct vnc *)g_malloc(sizeof(struct vnc), 1);
    /* set client functions */
    v->size = sizeof(struct vnc);
    v->version = CURRENT_MOD_VER;
    v->handle = (tintptr) v;
    v->mod_connect = lib_mod_connect;
    v->mod_start = lib_mod_start;
    v->mod_event = lib_mod_event;
    v->mod_signal = lib_mod_signal;
    v->mod_end = lib_mod_end;
    v->mod_set_param = lib_mod_set_param;
    v->mod_get_wait_objs = lib_mod_get_wait_objs;
    v->mod_check_wait_objs = lib_mod_check_wait_objs;
    v->mod_frame_ack = lib_mod_frame_ack;
    v->mod_suppress_output = lib_mod_suppress_output;
    v->mod_server_monitor_resize = lib_mod_server_monitor_resize;
    v->mod_server_monitor_full_invalidate = lib_mod_server_monitor_full_invalidate;
    v->mod_server_version_message = lib_mod_server_version_message;

    /* Member variables */
    v->enabled_encodings_mask = -1;
    vnc_clip_init(v);

    return (tintptr) v;
}

/******************************************************************************/
int EXPORT_CC
mod_exit(tintptr handle)
{
    struct vnc *v = (struct vnc *) handle;
    LOG(LOG_LEVEL_TRACE, "VNC mod_exit");

    if (v == 0)
    {
        return 0;
    }
    trans_delete(v->trans);
    vnc_clip_exit(v);
    g_free(v);
    return 0;
}
