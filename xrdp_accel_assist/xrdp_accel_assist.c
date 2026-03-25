/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2020-2024
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "arch.h"
#include "parse.h"
#include "trans.h"
#include "os_calls.h"
#include "string_calls.h"
#include "log.h"
#include "xup_client_info.h"

#include "xrdp_accel_assist.h"
#include "xrdp_accel_assist_x11.h"

#define ARRAYSIZE(x) (sizeof(x)/sizeof(*(x)))

#define GFX_MAP_SIZE 3145728

struct xorgxrdp_info
{
    struct trans *xorg_trans;
    struct trans *xrdp_trans;
    struct source_info si;
    int shmem_fd_ret;
    int shmem_bytes_ret;
    int idr_count;
    int pad0;
};

static int g_display_num = 0;

/*****************************************************************************/
static int
gfx_wiretosurface1(struct xorgxrdp_info *xi, struct stream *s)
{
    void *addr;
    int surface_id;
    int codec_id;
    int pixel_format;
    int flags;
    int num_rects_c;
    struct xh_rect *crects;
    int num_rects_d;
    int index;
    int left;
    int top;
    int width;
    int height;
    int cdata_bytes;
    int rv;
    int encoder_flags;
    char *flags_pointer;
    char *final_pointer;

    (void)pixel_format;
    (void)codec_id;
    (void)surface_id;
    (void)rv;

    if (xi->shmem_fd_ret != -1)
    {
        LOG(LOG_LEVEL_ERROR, "gfx_wiretosurface1: xi->shmem_fd_ret "
            "should be -1, it is %d", xi->shmem_fd_ret);
    }
    if (g_alloc_shm_map_fd(&addr, &(xi->shmem_fd_ret), GFX_MAP_SIZE) != 0)
    {
        return 1;
    }
    LOG_DEVEL(LOG_LEVEL_INFO, "gfx_wiretosurface1: addr %p fd %d",
              addr, xi->shmem_fd_ret);

    if (!s_check_rem(s, 11))
    {
        g_munmap(addr, GFX_MAP_SIZE);
        g_file_close(xi->shmem_fd_ret);
        xi->shmem_fd_ret = -1;
        return 1;
    }
    in_uint16_le(s, surface_id);
    in_uint16_le(s, codec_id);
    in_uint8(s, pixel_format);
    flags_pointer = s->p;
    in_uint32_le(s, flags);
    LOG_DEVEL(LOG_LEVEL_INFO, "gfx_wiretosurface1: surface_id %d codec_id %d "
              "pixel_format %d flags %d",
              surface_id, codec_id, pixel_format, flags);
    in_uint16_le(s, num_rects_d);
    if ((num_rects_d < 1) || (num_rects_d > 16 * 1024) ||
            (!s_check_rem(s, num_rects_d * 8)))
    {
        g_munmap(addr, GFX_MAP_SIZE);
        g_file_close(xi->shmem_fd_ret);
        xi->shmem_fd_ret = -1;
        return 1;
    }
    in_uint8s(s, num_rects_d * 8);
    if (!s_check_rem(s, 2))
    {
        g_munmap(addr, GFX_MAP_SIZE);
        g_file_close(xi->shmem_fd_ret);
        xi->shmem_fd_ret = -1;
        return 1;
    }

    in_uint16_le(s, num_rects_c);
    if ((num_rects_c < 1) || (num_rects_c > 16 * 1024) ||
            (!s_check_rem(s, num_rects_c * 8)))
    {
        g_munmap(addr, GFX_MAP_SIZE);
        g_file_close(xi->shmem_fd_ret);
        xi->shmem_fd_ret = -1;
        return 1;
    }
    crects = g_new0(struct xh_rect, num_rects_c);
    if (crects == NULL)
    {
        g_munmap(addr, GFX_MAP_SIZE);
        g_file_close(xi->shmem_fd_ret);
        xi->shmem_fd_ret = -1;
        return 1;
    }
    for (index = 0; index < num_rects_c; index++)
    {
        in_uint16_le(s, crects[index].x);
        in_uint16_le(s, crects[index].y);
        in_uint16_le(s, crects[index].w);
        in_uint16_le(s, crects[index].h);
    }
    if (!s_check_rem(s, 8))
    {
        g_munmap(addr, GFX_MAP_SIZE);
        g_file_close(xi->shmem_fd_ret);
        xi->shmem_fd_ret = -1;
        g_free(crects);
        return 1;
    }
    in_uint16_le(s, left);
    in_uint16_le(s, top);
    in_uint16_le(s, width);
    in_uint16_le(s, height);
    final_pointer = s->p;

    (void)left;
    (void)top;

    cdata_bytes = GFX_MAP_SIZE;
    encoder_flags = 0;
    if (xi->idr_count > 0)
    {
        encoder_flags = XH_ENC_FLAGS_FORCEIDR;
        xi->idr_count--;
    }
    rv = xrdp_accel_assist_x11_encode_pixmap(0, 0,
         width, height, surface_id,
         num_rects_c, crects,
         addr, &cdata_bytes,
         encoder_flags);
    LOG_DEVEL(LOG_LEVEL_INFO, "gfx_wiretosurface1: rv %d cdata_bytes %d",
              rv, cdata_bytes);

    s->p = flags_pointer;
    flags |= 1;
    out_uint32_le(s, flags); /* set already encoded bit */
    s->p = final_pointer;

    xi->shmem_bytes_ret = cdata_bytes;

    g_free(crects);
    g_munmap(addr, GFX_MAP_SIZE);
    /* do not close xi->shmem_fd_ret here, it will get closed after sent */

    return 0;
}

/*****************************************************************************/
static int
xorg_process_message_62(struct xorgxrdp_info *xi, struct stream *s)
{
    int recv_bytes;
    int total_cmd_bytes;
    int total_shm_bytes;
    int cmd_bytes;
    int cmd_id;
    int fd;
    unsigned int num_fds;
    char msg[4];
    char *total_holdp;
    char *total_holdend;
    char *holdp;
    char *holdend;

    if (!s_check_rem(s, 4))
    {
        return 1;
    }
    in_uint32_le(s, total_cmd_bytes);
    LOG_DEVEL(LOG_LEVEL_INFO, "xorg_process_message_62: "
              "total_cmd_bytes %d",
              total_cmd_bytes);
    if ((total_cmd_bytes < 1) || (total_cmd_bytes > 32 * 1024) ||
            (!s_check_rem(s, total_cmd_bytes)))
    {
        return 1;
    }
    total_holdp = s->p;
    total_holdend = s->end;
    s->end = s->p + total_cmd_bytes;
    LOG_DEVEL(LOG_LEVEL_INFO, "xorg_process_message_62: "
              "rem %d",
              (int)(s->end - s->p));
    while (s_check_rem(s, 8))
    {
        holdp = s->p;
        in_uint16_le(s, cmd_id);
        in_uint8s(s, 2); /* flags */
        in_uint32_le(s, cmd_bytes);
        LOG_DEVEL(LOG_LEVEL_INFO, "xorg_process_message_62: "
                  "cmd_id %d cmd_bytes %d",
                  cmd_id, cmd_bytes);
        if ((cmd_bytes < 8) || (cmd_bytes > 32 * 1024) ||
                (!s_check_rem(s, cmd_bytes - 8)))
        {
            return 1;
        }
        holdend = s->end;
        s->end = holdp + cmd_bytes;
        switch (cmd_id)
        {
            case 0x0001: /* XR_RDPGFX_CMDID_WIRETOSURFACE_1 */
                if (gfx_wiretosurface1(xi, s) != 0)
                {
                    return 1;
                }
                break;
            case 0x000E: /* XR_RDPGFX_CMDID_RESETGRAPHICS */
                LOG(LOG_LEVEL_INFO, "xorg_process_message_62: "
                    "XR_RDPGFX_CMDID_RESETGRAPHICS detected");
                break;
        }
        /* setup for next cmd */
        s->p = holdp + cmd_bytes;
        s->end = holdend;
        LOG_DEVEL(LOG_LEVEL_INFO, "xorg_process_message_62: "
                  "rem %d",
                  (int)(s->end - s->p));
    }
    s->p = total_holdp + total_cmd_bytes;
    s->end = total_holdend;
    if (!s_check_rem(s, 4))
    {
        return 1;
    }
    in_uint32_le(s, total_shm_bytes);
    s->p -= 4;
    out_uint32_le(s, xi->shmem_bytes_ret);
    LOG_DEVEL(LOG_LEVEL_INFO, "xorg_process_message_62: "
              "total_shm_bytes %d",
              total_shm_bytes);
    if (total_shm_bytes < 1)
    {
        /* return no error */
        return 0;
    }
    num_fds = 0;
    if (g_tcp_can_recv(xi->xorg_trans->sck, 5000) == 0)
    {
        return 1;
    }
    recv_bytes = g_sck_recv_fd_set(xi->xorg_trans->sck, msg, 4,
                                   &fd, 1, &num_fds);
    LOG_DEVEL(LOG_LEVEL_INFO, "xorg_process_message_62: "
              "g_sck_recv_fd_set rv %d fd %d, num_fds %d",
              recv_bytes, fd, num_fds);
    if (recv_bytes == 4)
    {
        if (num_fds == 1)
        {
            g_file_close(fd);
            /* return no error */
            return 0;
        }
    }
    return 1;
}

/*****************************************************************************/
static int
xorg_process_message_63(struct xorgxrdp_info *xi, struct stream *s)
{
    int recv_bytes;
    unsigned int num_fds;
    char msg[4];

    if (xi->shmem_fd_ret != -1)
    {
        LOG(LOG_LEVEL_ERROR, "xorg_process_message_63: xi->shmem_fd_ret "
            "should be -1, it is %d", xi->shmem_fd_ret);
    }
    num_fds = -1;
    if (g_tcp_can_recv(xi->xorg_trans->sck, 5000) == 0)
    {
        return 1;
    }
    recv_bytes = g_sck_recv_fd_set(xi->xorg_trans->sck, msg, 4,
                                   &(xi->shmem_fd_ret), 1, &num_fds);
    LOG_DEVEL(LOG_LEVEL_INFO, "xorg_process_message_63: "
              "g_sck_recv_fd_set rv %d fd %d, num_fds %d",
              recv_bytes, xi->shmem_fd_ret, num_fds);
    if (recv_bytes == 4)
    {
        if (num_fds == 1)
        {
            return 0;
        }
    }
    return 1;
}

/*****************************************************************************/
static int
xorg_process_message_64(struct xorgxrdp_info *xi, struct stream *s)
{
    int num_drects;
    int num_crects;
    int flags;
    int shmem_bytes;
    int shmem_offset;
    int frame_id;
    int left;
    int top;
    int width;
    int height;
    int twidth;
    int theight;
    int cdata_bytes;
    int index;
    int recv_bytes;
    int encoder_flags;
    enum encoder_result rv;
    struct xh_rect *crects;
    char *bmpdata;
    char msg[4];
    unsigned int num_fds;
    void *shmem_ptr;

    (void)twidth;
    (void)theight;
    (void)frame_id;

    /* dirty pixels */
    in_uint16_le(s, num_drects);
    in_uint8s(s, 8 * num_drects);
    /* copied pixels */
    in_uint16_le(s, num_crects);
    crects = g_new(struct xh_rect, num_crects);
    for (index = 0; index < num_crects; index++)
    {
        in_uint16_le(s, crects[index].x);
        in_uint16_le(s, crects[index].y);
        in_uint16_le(s, crects[index].w);
        in_uint16_le(s, crects[index].h);
    }
    char *flag_pointer = s->p;
    in_uint32_le(s, flags);
    LOG_DEVEL(LOG_LEVEL_INFO, "xorg_process_message_64: flags 0x%8.8X", flags);
    in_uint32_le(s, frame_id);
    in_uint32_le(s, shmem_bytes);
    in_uint32_le(s, shmem_offset);

    in_uint16_le(s, left);
    in_uint16_le(s, top);
    in_uint16_le(s, width);
    in_uint16_le(s, height);
    in_uint16_le(s, twidth);
    in_uint16_le(s, theight);
    char *final_pointer = s->p;

    if (xi->shmem_fd_ret != -1)
    {
        LOG(LOG_LEVEL_ERROR, "xorg_process_message_64: xi->shmem_fd_ret "
            "should be -1, it is %d", xi->shmem_fd_ret);
    }

    num_fds = -1;
    if (g_tcp_can_recv(xi->xorg_trans->sck, 5000) == 0)
    {
        g_free(crects);
        return 1;
    }
    recv_bytes = g_sck_recv_fd_set(xi->xorg_trans->sck, msg, 4,
                                   &(xi->shmem_fd_ret), 1, &num_fds);
    LOG_DEVEL(LOG_LEVEL_INFO, "xorg_process_message_64: "
              "g_sck_recv_fd_set rv %d fd %d, num_fds %d",
              recv_bytes, xi->shmem_fd_ret, num_fds);

    shmem_ptr = NULL;
    if (recv_bytes == 4)
    {
        if (num_fds == 1)
        {
            if (g_file_map(xi->shmem_fd_ret, 1, 1, shmem_bytes,
                           &shmem_ptr) == 0)
            {
                bmpdata = (char *)shmem_ptr;
                bmpdata += shmem_offset;

                if ((bmpdata != NULL) && (flags & 1))
                {
                    cdata_bytes = 16 * 1024 * 1024;
                    encoder_flags = 0;
                    if (xi->idr_count > 0)
                    {
                        encoder_flags = XH_ENC_FLAGS_FORCEIDR;
                        xi->idr_count--;
                    }
                    rv = xrdp_accel_assist_x11_encode_pixmap(left, top,
                         width, height,
                         (flags >> 28) & 0xF,
                         num_crects, crects,
                         bmpdata + 4,
                         &cdata_bytes, encoder_flags);
                    if (rv == ENCODER_ERROR)
                    {
                        LOG(LOG_LEVEL_ERROR, "error %d", rv);
                    }
                    if (rv == KEY_FRAME_ENCODED)
                    {
                        s->p = flag_pointer;
                        out_uint32_le(s, flags | 1 | 2);
                        s->p = final_pointer;
                    }

                    bmpdata[0] = cdata_bytes;
                    bmpdata[1] = cdata_bytes >> 8;
                    bmpdata[2] = cdata_bytes >> 16;
                    bmpdata[3] = cdata_bytes >> 24;
                    LOG_DEVEL(LOG_LEVEL_INFO, "cdata_bytes %d", cdata_bytes);
                }
            }
        }
        else
        {
            LOG(LOG_LEVEL_INFO,
                "xorg_process_message_64: num_fds %d", num_fds);
        }
    }
    else
    {
        LOG(LOG_LEVEL_INFO,
            "xorg_process_message_64: recv_bytes %d", recv_bytes);
    }

    if (shmem_ptr != NULL)
    {
        g_munmap(shmem_ptr, shmem_bytes);
    }
    g_free(crects);
    return 0;
}

/*****************************************************************************/
/* data going from xorg to xrdp */
static int
xorg_process_message(struct xorgxrdp_info *xi, struct stream *s)
{
    int type;
    int num;
    int size;
    int index;
    char *phold;
    char *endhold;
    int width;
    int height;
    int magic;
    int con_id;
    int mon_id;
    int ret;

    xi->shmem_fd_ret = -1;
    in_uint16_le(s, type);
    in_uint16_le(s, num);
    in_uint32_le(s, size);
    if (type == XUP_MSG_ORDER_LIST)
    {
        for (index = 0; index < num; index++)
        {
            phold = s->p;
            in_uint16_le(s, type);
            in_uint16_le(s, size);
            endhold = s->end;
            s->end = phold + size;
            switch (type)
            {
                case XUP_ORDER_EGFX_SHMFD:
                    /* process_server_egfx_shmfd */
                    if (xorg_process_message_62(xi, s) != 0)
                    {
                        LOG(LOG_LEVEL_ERROR, "xorg_process_message: "
                            "xorg_process_message_62 failed");
                        return 1;
                    }
                    break;
                case XUP_ORDER_SET_POINTER_SHMFD:
                    /* process_server_set_pointer_shmfd */
                    if (xorg_process_message_63(xi, s) != 0)
                    {
                        LOG(LOG_LEVEL_ERROR, "xorg_process_message: "
                            "xorg_process_message_63 failed");
                        return 1;
                    }
                    break;
                case XUP_ORDER_PAINT_RECT_SHMFD:
                    /* process_server_paint_rect_shmfd */
                    if (xorg_process_message_64(xi, s) != 0)
                    {
                        LOG(LOG_LEVEL_ERROR, "xorg_process_message: "
                            "xorg_process_message_64 failed");
                        return 1;
                    }
                    break;
            }
            s->p = phold + size;
            s->end = endhold;
        }
    }
    else if (type == XUP_MSG_METADATA)
    {
        for (index = 0; index < num; index++)
        {
            phold = s->p;
            in_uint16_le(s, type);
            in_uint16_le(s, size);
            LOG(LOG_LEVEL_DEBUG, "100 type %d size %d", type, size);
            switch (type)
            {
                case XUP_METADATA_CLEAR_MONITORS:
                    LOG(LOG_LEVEL_DEBUG, "calling xrdp_accel_assist_x11_delete_all_pixmaps");
                    xrdp_accel_assist_x11_delete_all_pixmaps();
                    break;
                case XUP_METADATA_ADD_MONITOR:
                    in_uint16_le(s, width);
                    in_uint16_le(s, height);
                    in_uint32_le(s, magic);
                    in_uint32_le(s, con_id);
                    in_uint32_le(s, mon_id);
                    LOG(LOG_LEVEL_DEBUG, "calling xrdp_accel_assist_x11_create_pixmap");
                    xrdp_accel_assist_x11_create_pixmap(width, height, magic,
                                                        con_id, mon_id);
                    break;
            }
            s->p = phold + size;
        }
        /* this will force I frames for 10 frames */
        xi->idr_count = 10;
        LOG(LOG_LEVEL_INFO, "setting idr_count to %d", xi->idr_count);
    }
    s->p = s->data;
    if (xi->shmem_fd_ret == -1)
    {
        /* no shared memory, ok */
        ret = trans_write_copy_s(xi->xrdp_trans, s);
        return ret;
    }
    /* using posix shared memory, got to pass fd on to xrdp */
    ret = trans_force_write_s(xi->xrdp_trans, s);
    if (ret)
    {
        return ret;
    }
    ret = g_sck_send_fd_set(xi->xrdp_trans->sck, "int", 4,
                            &(xi->shmem_fd_ret), 1);
    if (ret < 0)
    {
        return 1;
    }
    g_file_close(xi->shmem_fd_ret);
    return 0;
}

/*****************************************************************************/
static int
xorg_data_in(struct trans *trans)
{
    struct stream *s;
    int len;
    struct xorgxrdp_info *xi;

    xi = (struct xorgxrdp_info *) (trans->callback_data);
    s = trans_get_in_s(trans);
    switch (trans->extra_flags)
    {
        case 1:
            s->p = s->data;
            in_uint8s(s, 4);
            in_uint32_le(s, len);
            if ((len < 0) || (len > 128 * 1024))
            {
                LOG(LOG_LEVEL_ERROR, "bad size %d", len);
                return 1;
            }
            if (len > 0)
            {
                trans->header_size = len + 8;
                trans->extra_flags = 2;
                break;
            }
        /* fall through */
        case 2:
            s->p = s->data;
            if (xorg_process_message(xi, s) != 0)
            {
                LOG(LOG_LEVEL_ERROR, "xorg_process_message failed");
                return 1;
            }
            init_stream(s, 0);
            trans->header_size = 8;
            trans->extra_flags = 1;
            break;
    }
    return 0;
}

/*****************************************************************************/
/* data going from xrdp to xorg */
static int
xrdp_process_message(struct xorgxrdp_info *xi, struct stream *s)
{
    int len;
    int msg_type1;
    int msg_type2;

    in_uint32_le(s, len);
    in_uint16_le(s, msg_type1);
    if (msg_type1 == XUP_MSG_CLIENT_DATA)
    {
        in_uint32_le(s, msg_type2);
        if (msg_type2 == XUP_CLIENT_DATA_INVALIDATE)
        {
            LOG(LOG_LEVEL_DEBUG, "Invalidate found (len: %d, msg1: %d, msg2: %d)", len, msg_type1, msg_type2);
        }
        else if (msg_type2 == XUP_CLIENT_DATA_DESKTOP_RESIZE)
        {
            LOG(LOG_LEVEL_DEBUG, "Resize 300 found (len: %d, msg1: %d, msg2: %d)", len, msg_type1, msg_type2);
        }
        else if (msg_type2 == XUP_CLIENT_DATA_MONITOR_UPDATE)
        {
            LOG(LOG_LEVEL_DEBUG, "Resize 302 found (len: %d, msg1: %d, msg2: %d)", len, msg_type1, msg_type2);
        }
    }
    /* Reset read pointer */
    s->p = s->data;
    return trans_write_copy_s(xi->xorg_trans, s);
}

/*****************************************************************************/
static int
xrdp_data_in(struct trans *trans)
{
    struct stream *s;
    int len;
    struct xorgxrdp_info *xi;

    xi = (struct xorgxrdp_info *) (trans->callback_data);
    s = trans_get_in_s(trans);
    switch (trans->extra_flags)
    {
        case 1:
            s->p = s->data;
            in_uint32_le(s, len);
            if ((len < 0) || (len > 128 * 1024))
            {
                LOG(LOG_LEVEL_ERROR, "bad size %d", len);
                return 1;
            }
            if (len > 0)
            {
                trans->header_size = len;
                trans->extra_flags = 2;
                break;
            }
        /* fall through */
        case 2:
            s->p = s->data;
            if (xrdp_process_message(xi, s) != 0)
            {
                LOG(LOG_LEVEL_ERROR, "xrdp_process_message failed");
                return 1;
            }
            init_stream(s, 0);
            trans->header_size = 4;
            trans->extra_flags = 1;
            break;
    }
    return 0;
}

/*****************************************************************************/
static void
sigpipe_func(int sig)
{
    (void) sig;
}

/*****************************************************************************/
static int
get_log_path(char *path, int bytes)
{
    char *log_path;
    int rv;

    rv = 1;
    log_path = g_getenv("XRDP_ACCEL_ASSIST_LOG_PATH");
    if (log_path == 0)
    {
        log_path = g_getenv("XDG_DATA_HOME");
        if (log_path != 0)
        {
            g_snprintf(path, bytes, "%s%s", log_path, "/xrdp");
            if (g_directory_exist(path) || (g_mkdir(path) == 0))
            {
                rv = 0;
            }
        }
    }
    else
    {
        g_snprintf(path, bytes, "%s", log_path);
        if (g_directory_exist(path) || (g_mkdir(path) == 0))
        {
            rv = 0;
        }
    }
    if (rv != 0)
    {
        log_path = g_getenv("HOME");
        if (log_path != 0)
        {
            g_snprintf(path, bytes, "%s%s", log_path, "/.local");
            if (g_directory_exist(path) || (g_mkdir(path) == 0))
            {
                g_snprintf(path, bytes, "%s%s", log_path, "/.local/share");
                if (g_directory_exist(path) || (g_mkdir(path) == 0))
                {
                    g_snprintf(path, bytes, "%s%s", log_path, "/.local/share/xrdp");
                    if (g_directory_exist(path) || (g_mkdir(path) == 0))
                    {
                        rv = 0;
                    }
                }
            }
        }
    }
    return rv;
}

/*****************************************************************************/
static enum logLevels
get_log_level(const char *level_str, enum logLevels default_level)
{
    static const char *levels[] = {
        "LOG_LEVEL_ALWAYS",
        "LOG_LEVEL_ERROR",
        "LOG_LEVEL_WARNING",
        "LOG_LEVEL_INFO",
        "LOG_LEVEL_DEBUG",
        "LOG_LEVEL_TRACE"
    };
    unsigned int i;

    if (level_str == NULL || level_str[0] == 0)
    {
        return default_level;
    }
    for (i = 0; i < ARRAYSIZE(levels); ++i)
    {
        if (g_strcasecmp(levels[i], level_str) == 0)
        {
            return (enum logLevels) i;
        }
    }
    return default_level;
}

/*****************************************************************************/
static int
get_display_num_from_display(char *display_text)
{
    int index;
    int mode;
    int host_index;
    int disp_index;
    int scre_index;
    char host[256];
    char disp[256];
    char scre[256];

    g_memset(host, 0, 256);
    g_memset(disp, 0, 256);
    g_memset(scre, 0, 256);

    index = 0;
    host_index = 0;
    disp_index = 0;
    scre_index = 0;
    mode = 0;

    while (display_text[index] != 0)
    {
        if (display_text[index] == ':')
        {
            mode = 1;
        }
        else if (display_text[index] == '.')
        {
            mode = 2;
        }
        else if (mode == 0)
        {
            host[host_index] = display_text[index];
            host_index++;
        }
        else if (mode == 1)
        {
            disp[disp_index] = display_text[index];
            disp_index++;
        }
        else if (mode == 2)
        {
            scre[scre_index] = display_text[index];
            scre_index++;
        }

        index++;
    }

    host[host_index] = 0;
    disp[disp_index] = 0;
    scre[scre_index] = 0;
    g_display_num = g_atoi(disp);
    return 0;
}

/*****************************************************************************/
static int
xrdp_accel_assist_setup_log(void)
{
    struct log_config logconfig;
    enum logLevels log_level;
    char log_path[256];
    char log_file[256];
    char *display_text;
    int error;

    if (get_log_path(log_path, 255) != 0)
    {
        g_writeln("error reading XRDP_ACCEL_ASSIST_LOG_PATH and HOME "
                  "environment variable");
        g_deinit();
        return 1;
    }
    display_text = g_getenv("DISPLAY");
    if (display_text != NULL)
    {
        get_display_num_from_display(display_text);
    }
    g_snprintf(log_file, 255, "%s/xrdp-accel-assist.%d.log", log_path,
               g_display_num);
    g_writeln("xrdp-accel-assist::xrdp_accel_assist_setup_log: using "
              "log file [%s]", log_file);
    if (g_file_exist(log_file))
    {
        g_file_delete(log_file);
    }
    log_level = get_log_level(g_getenv("XRDP_ACCEL_ASSIST_LOG_LEVEL"),
                              LOG_LEVEL_INFO);
    logconfig.log_file = log_file;
    logconfig.fd = -1;
    logconfig.log_level = log_level;
    logconfig.enable_syslog = 0;
    logconfig.syslog_level = LOG_LEVEL_ALWAYS;
    logconfig.program_name = "xrdp_accel_assist";
    logconfig.enable_console = 0;
    logconfig.enable_pid = 1;
#ifdef LOG_PER_LOGGER_LEVEL
    logconfig.per_logger_level = NULL;
#endif
    error = log_start_from_param(&logconfig);

    return error;
}

/*****************************************************************************/
int
main(int argc, char **argv)
{
    int xorg_fd;
    int xrdp_fd;
    int error;
    intptr_t robjs[16];
    int robj_count;
    intptr_t wobjs[16];
    int wobj_count;
    int timeout;
    struct xorgxrdp_info xi;

    if (argc < 2)
    {
        g_writeln("need to pass -d");
        return 0;
    }
    if (strcmp(argv[1], "-d") != 0)
    {
        g_writeln("need to pass -d");
        return 0;
    }
    g_init("xrdp_accel_assist");

    if (xrdp_accel_assist_setup_log() != 0)
    {
        return 1;
    }
    LOG(LOG_LEVEL_INFO, "startup");
    g_memset(&xi, 0, sizeof(xi));
    g_signal_pipe(sigpipe_func);
    if (xrdp_accel_assist_x11_init() != 0)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_accel_assist_x11_init failed");
        return 1;
    }
    xorg_fd = g_atoi(g_getenv("XORGXRDP_XORG_FD"));
    LOG(LOG_LEVEL_INFO, "xorg_fd: %d", xorg_fd);
    xrdp_fd = g_atoi(g_getenv("XORGXRDP_XRDP_FD"));
    LOG(LOG_LEVEL_INFO, "xrdp_fd: %d", xrdp_fd);

    xi.xorg_trans = trans_create(TRANS_MODE_UNIX, 128 * 1024, 128 * 1024);
    xi.xorg_trans->sck = xorg_fd;
    xi.xorg_trans->status = TRANS_STATUS_UP;
    xi.xorg_trans->trans_data_in = xorg_data_in;
    xi.xorg_trans->header_size = 8;
    xi.xorg_trans->no_stream_init_on_data_in = 1;
    xi.xorg_trans->extra_flags = 1;
    xi.xorg_trans->callback_data = &xi;
    xi.xorg_trans->si = &(xi.si);
    xi.xorg_trans->my_source = XORGXRDP_SOURCE_XORG;

    xi.xrdp_trans = trans_create(TRANS_MODE_UNIX, 128 * 1024, 128 * 1024);
    xi.xrdp_trans->sck = xrdp_fd;
    xi.xrdp_trans->status = TRANS_STATUS_UP;
    xi.xrdp_trans->trans_data_in = xrdp_data_in;
    xi.xrdp_trans->no_stream_init_on_data_in = 1;
    xi.xrdp_trans->header_size = 4;
    xi.xrdp_trans->extra_flags = 1;
    xi.xrdp_trans->callback_data = &xi;
    xi.xrdp_trans->si = &(xi.si);
    xi.xrdp_trans->my_source = XORGXRDP_SOURCE_XRDP;

    for (;;)
    {
        robj_count = 0;
        wobj_count = 0;
        timeout = -1;
        error = trans_get_wait_objs_rw(xi.xorg_trans, robjs, &robj_count,
                                       wobjs, &wobj_count, &timeout);
        if (error != 0)
        {
            LOG(LOG_LEVEL_INFO, "trans_get_wait_objs_rw failed");
            break;
        }
        error = trans_get_wait_objs_rw(xi.xrdp_trans, robjs, &robj_count,
                                       wobjs, &wobj_count, &timeout);
        if (error != 0)
        {
            LOG(LOG_LEVEL_INFO, "trans_get_wait_objs_rw failed");
            break;
        }
        error = xrdp_accel_assist_x11_get_wait_objs(robjs, &robj_count);
        if (error != 0)
        {
            LOG(LOG_LEVEL_INFO, "xrdp_accel_assist_x11_get_wait_objs failed");
            break;
        }
        error = g_obj_wait(robjs, robj_count, wobjs, wobj_count, timeout);
        if (error != 0)
        {
            LOG(LOG_LEVEL_INFO, "g_obj_wait failed");
            break;
        }
        error = trans_check_wait_objs(xi.xorg_trans);
        if (error != 0)
        {
            LOG(LOG_LEVEL_INFO, "xorg_trans trans_check_wait_objs failed");
            break;
        }
        error = trans_check_wait_objs(xi.xrdp_trans);
        if (error != 0 && error != 10)
        {
            LOG(LOG_LEVEL_ERROR, "xrdp_trans trans_check_wait_objs failed");
            break;
        }
        error = xrdp_accel_assist_x11_check_wait_objs();
        if (error != 0)
        {
            LOG(LOG_LEVEL_ERROR, "xrdp_accel_assist_x11_check_wait_objs failed");
            break;
        }
    }
    LOG(LOG_LEVEL_INFO, "exit");
    return 0;
}
