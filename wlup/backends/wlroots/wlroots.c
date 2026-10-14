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
 * wlup backend for wlroots compositors (sway, labwc, ...): a plain
 * Wayland client of the compositor
 *   frames : ext-image-copy-capture-v1 into a wl_shm buffer
 *   input  : zwlr_virtual_pointer_v1 and zwp_virtual_keyboard_v1
 *   resize : wlr-output-management
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <errno.h>
#include <poll.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>

#include <linux/input-event-codes.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

/* cppcheck-suppress missingInclude ; generated at build time */
#include "ext-image-capture-source-v1-client-protocol.h"
/* cppcheck-suppress missingInclude ; generated at build time */
#include "ext-image-copy-capture-v1-client-protocol.h"
/* cppcheck-suppress missingInclude ; generated at build time */
#include "virtual-keyboard-unstable-v1-client-protocol.h"
/* cppcheck-suppress missingInclude ; generated at build time */
#include "wlr-output-management-unstable-v1-client-protocol.h"
/* cppcheck-suppress missingInclude ; generated at build time */
#include "wlr-virtual-pointer-unstable-v1-client-protocol.h"

#include "wlup.h"
#include "wlup_backend.h"
#include "wlup_clip.h"
#include "wlup_gfx.h"
#include "wlroots.h"
#include "log.h"
#include "string_calls.h"
#include "scancode.h"
#include "xrdp_constants.h"

/* Scroll distance for one wheel notch, matching libinput's default */
#define WLUP_WHEEL_STEP 15.0

static int start_capture(struct wlup *v);
static void continue_capture(struct wlup *v);

/******************************************************************************/
static uint32_t
now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

/******************************************************************************/
/* Shared memory                                                              */
/******************************************************************************/
static void
free_buffer(struct wlup *v)
{
    struct wlroots *w = WLR(v);
    if (w->buffer != NULL)
    {
        wl_buffer_destroy(w->buffer);
        w->buffer = NULL;
    }
    wlup_gfx_free_shadow(v);
    if (v->pixels != NULL)
    {
        munmap(v->pixels, v->pixels_size);
        v->pixels = NULL;
        v->pixels_size = 0;
    }
    v->buffer_width = 0;
    v->buffer_height = 0;
}

/******************************************************************************/
/* return error */
static int
alloc_buffer(struct wlup *v, int width, int height, uint32_t format)
{
    struct wlroots *w = WLR(v);
    struct wl_shm_pool *pool;
    size_t stride = (size_t)width * 4;
    size_t size = stride * height;
    int fd;

    free_buffer(v);

    fd = memfd_create("xrdp-wlup", MFD_CLOEXEC);
    if (fd < 0)
    {
        LOG(LOG_LEVEL_ERROR, "wlup: memfd_create failed: %s", strerror(errno));
        return 1;
    }
    if (ftruncate(fd, size) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "wlup: ftruncate failed: %s", strerror(errno));
        close(fd);
        return 1;
    }
    v->pixels = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (v->pixels == MAP_FAILED)
    {
        LOG(LOG_LEVEL_ERROR, "wlup: mmap failed: %s", strerror(errno));
        v->pixels = NULL;
        close(fd);
        return 1;
    }
    v->pixels_size = size;

    pool = wl_shm_create_pool(w->shm, fd, (int32_t)size);
    w->buffer = wl_shm_pool_create_buffer(pool, 0, width, height,
                                          (int32_t)stride, format);
    wl_shm_pool_destroy(pool);
    close(fd);

    v->buffer_width = width;
    v->buffer_height = height;
    w->buffer_format = (int)format;
    v->need_full_paint = 1;
    LOG(LOG_LEVEL_INFO, "wlup: capture buffer %dx%d format 0x%x",
        width, height, format);
    return 0;
}

/******************************************************************************/
/* Capture frame events                                                       */
/******************************************************************************/
static void
frame_transform(void *data, struct ext_image_copy_capture_frame_v1 *frame,
                uint32_t transform)
{
    if (transform != WL_OUTPUT_TRANSFORM_NORMAL)
    {
        LOG(LOG_LEVEL_WARNING, "wlup: output transform %u is not supported",
            transform);
    }
}

/******************************************************************************/
static void
frame_damage(void *data, struct ext_image_copy_capture_frame_v1 *frame,
             int32_t x, int32_t y, int32_t width, int32_t height)
{
    struct wlup *v = (struct wlup *)data;
    int i;

    if (!v->damage_overflow && v->num_damage < WLUP_MAX_DAMAGE)
    {
        i = v->num_damage++ * 4;
        v->damage[i] = x;
        v->damage[i + 1] = y;
        v->damage[i + 2] = width;
        v->damage[i + 3] = height;
        return;
    }

    if (!v->damage_overflow)
    {
        /* Collapse everything received so far into a bounding box */
        int left = v->damage[0];
        int top = v->damage[1];
        int right = left + v->damage[2];
        int bottom = top + v->damage[3];
        for (i = 1; i < v->num_damage; ++i)
        {
            left = MIN(left, v->damage[i * 4]);
            top = MIN(top, v->damage[i * 4 + 1]);
            right = MAX(right, v->damage[i * 4] + v->damage[i * 4 + 2]);
            bottom = MAX(bottom, v->damage[i * 4 + 1] + v->damage[i * 4 + 3]);
        }
        v->damage[0] = left;
        v->damage[1] = top;
        v->damage[2] = right - left;
        v->damage[3] = bottom - top;
        v->damage_overflow = 1;
    }
    {
        int right = MAX(v->damage[0] + v->damage[2], x + width);
        int bottom = MAX(v->damage[1] + v->damage[3], y + height);
        v->damage[0] = MIN(v->damage[0], x);
        v->damage[1] = MIN(v->damage[1], y);
        v->damage[2] = right - v->damage[0];
        v->damage[3] = bottom - v->damage[1];
    }
}

/******************************************************************************/
static void
frame_presentation_time(void *data,
                        struct ext_image_copy_capture_frame_v1 *frame,
                        uint32_t tv_sec_hi, uint32_t tv_sec_lo,
                        uint32_t tv_nsec)
{
}

/******************************************************************************/
static void
frame_ready(void *data, struct ext_image_copy_capture_frame_v1 *frame)
{
    struct wlup *v = (struct wlup *)data;
    struct wlroots *w = WLR(v);

    ext_image_copy_capture_frame_v1_destroy(w->frame);
    w->frame = NULL;
    /* With the H.264 encoder, the damage goes now if the last frame has
     * been acknowledged, while the buffer is still ours */
    wlup_paint_damage(v);
    continue_capture(v);
}

/******************************************************************************/
static void
frame_failed(void *data, struct ext_image_copy_capture_frame_v1 *frame,
             uint32_t reason)
{
    struct wlup *v = (struct wlup *)data;
    struct wlroots *w = WLR(v);

    ext_image_copy_capture_frame_v1_destroy(w->frame);
    w->frame = NULL;

    switch (reason)
    {
        case EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_BUFFER_CONSTRAINTS:
            /* New constraints follow, ending with a 'done' event, which
             * reallocates the buffer and restarts the capture */
            LOG(LOG_LEVEL_INFO, "wlup: buffer constraints changed");
            break;
        case EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_STOPPED:
            LOG(LOG_LEVEL_ERROR, "wlup: capture session stopped");
            break;
        default:
            LOG(LOG_LEVEL_WARNING, "wlup: frame capture failed, retrying");
            start_capture(v);
            break;
    }
}

static const struct ext_image_copy_capture_frame_v1_listener frame_listener =
{
    .transform = frame_transform,
    .damage = frame_damage,
    .presentation_time = frame_presentation_time,
    .ready = frame_ready,
    .failed = frame_failed,
};

/******************************************************************************/
/* return error */
static int
start_capture(struct wlup *v)
{
    struct wlroots *w = WLR(v);
    if (w->frame != NULL || w->buffer == NULL || w->session == NULL)
    {
        return 0;
    }
    v->num_damage = 0;
    v->damage_overflow = 0;
    w->frame = ext_image_copy_capture_session_v1_create_frame(w->session);
    ext_image_copy_capture_frame_v1_add_listener(w->frame, &frame_listener, v);
    ext_image_copy_capture_frame_v1_attach_buffer(w->frame, w->buffer);
    if (v->need_full_paint)
    {
        /* Fresh buffer: ask the compositor to fill all of it */
        ext_image_copy_capture_frame_v1_damage_buffer(w->frame, 0, 0,
                v->buffer_width, v->buffer_height);
    }
    ext_image_copy_capture_frame_v1_capture(w->frame);
    return 0;
}

/******************************************************************************/
/* Asks for the next frame, which the compositor holds until the screen
 * changes, if the core wants frames (wlup_wants_frames()). While it does
 * not, the compositor collects the damage, and the first capture after
 * that carries it: a slow client or a hidden window also stops the
 * copying, not just the sending */
static void
continue_capture(struct wlup *v)
{
    if (wlup_wants_frames(v))
    {
        start_capture(v);
    }
}

/******************************************************************************/
/* Capture session events (buffer constraints)                                */
/******************************************************************************/
static void
session_buffer_size(void *data, struct ext_image_copy_capture_session_v1 *s,
                    uint32_t width, uint32_t height)
{
    struct wlup *v = (struct wlup *)data;
    struct wlroots *w = WLR(v);
    w->constraint_width = width;
    w->constraint_height = height;
}

/******************************************************************************/
static void
session_shm_format(void *data, struct ext_image_copy_capture_session_v1 *s,
                   uint32_t format)
{
    struct wlup *v = (struct wlup *)data;
    struct wlroots *w = WLR(v);

    /* Both have the 0x00RRGGBB little-endian layout xrdp uses at 24
     * and 32 bpp. Prefer XRGB as the alpha byte is meaningless here */
    if (format == WL_SHM_FORMAT_XRGB8888)
    {
        w->constraint_format = format;
        w->constraint_xrgb8888 = 1;
    }
    else if (format == WL_SHM_FORMAT_ARGB8888 && !w->constraint_xrgb8888)
    {
        w->constraint_format = format;
        w->constraint_xrgb8888 = 1;
    }
}

/******************************************************************************/
static void
session_dmabuf_device(void *data, struct ext_image_copy_capture_session_v1 *s,
                      struct wl_array *device)
{
}

/******************************************************************************/
static void
session_dmabuf_format(void *data, struct ext_image_copy_capture_session_v1 *s,
                      uint32_t format, struct wl_array *modifiers)
{
}

/******************************************************************************/
static void
session_done(void *data, struct ext_image_copy_capture_session_v1 *s)
{
    struct wlup *v = (struct wlup *)data;
    struct wlroots *w = WLR(v);

    if (!w->constraint_xrgb8888)
    {
        LOG(LOG_LEVEL_ERROR, "wlup: compositor offers no XRGB8888/ARGB8888 "
            "shm format for capture");
        return;
    }
    if (w->buffer == NULL ||
            v->buffer_width != (int)w->constraint_width ||
            v->buffer_height != (int)w->constraint_height ||
            w->buffer_format != (int)w->constraint_format)
    {
        if (w->constraint_width != (uint32_t)v->server_width ||
                w->constraint_height != (uint32_t)v->server_height)
        {
            LOG(LOG_LEVEL_WARNING, "wlup: compositor output is %ux%u but the "
                "RDP desktop is %dx%d, the picture will be clipped or padded",
                w->constraint_width, w->constraint_height,
                v->server_width, v->server_height);
        }
        if (alloc_buffer(v, w->constraint_width, w->constraint_height,
                         w->constraint_format) != 0)
        {
            return;
        }
    }
    w->constraints_done = 1;
    /* The constraints are re-sent from scratch next time */
    w->constraint_xrgb8888 = 0;
    if (v->resize_pending &&
            v->buffer_width == v->resize_width &&
            v->buffer_height == v->resize_height)
    {
        wlup_resize_finished(v);
    }
    start_capture(v);
}

/******************************************************************************/
static void
session_stopped(void *data, struct ext_image_copy_capture_session_v1 *s)
{
    struct wlup *v = (struct wlup *)data;
    struct wlroots *w = WLR(v);

    LOG(LOG_LEVEL_ERROR, "wlup: capture session stopped by the compositor");
    ext_image_copy_capture_session_v1_destroy(w->session);
    w->session = NULL;
}

static const struct ext_image_copy_capture_session_v1_listener
    session_listener =
{
    .buffer_size = session_buffer_size,
    .shm_format = session_shm_format,
    .dmabuf_device = session_dmabuf_device,
    .dmabuf_format = session_dmabuf_format,
    .done = session_done,
    .stopped = session_stopped,
};

/******************************************************************************/
/* wl_output: only its name is needed, to find the matching head              */
/******************************************************************************/
static void
output_geometry(void *data, struct wl_output *output, int32_t x, int32_t y,
                int32_t physical_width, int32_t physical_height,
                int32_t subpixel, const char *make, const char *model,
                int32_t transform)
{
}

static void
output_mode(void *data, struct wl_output *output, uint32_t flags,
            int32_t width, int32_t height, int32_t refresh)
{
}

static void
output_done(void *data, struct wl_output *output)
{
}

static void
output_scale(void *data, struct wl_output *output, int32_t factor)
{
}

static void
output_name(void *data, struct wl_output *output, const char *name)
{
    struct wlup *v = (struct wlup *)data;
    struct wlroots *w = WLR(v);
    g_strncpy(w->output_name, name, sizeof(w->output_name) - 1);
}

static void
output_description(void *data, struct wl_output *output,
                   const char *description)
{
}

static const struct wl_output_listener output_listener =
{
    .geometry = output_geometry,
    .mode = output_mode,
    .done = output_done,
    .scale = output_scale,
    .name = output_name,
    .description = output_description,
};

/******************************************************************************/
/* wlr-output-management (version 1)                                          */
/******************************************************************************/
static struct wlup_head *
find_head(struct wlup *v, struct zwlr_output_head_v1 *head)
{
    struct wlroots *w = WLR(v);
    int i;
    for (i = 0; i < w->num_heads; ++i)
    {
        if (w->heads[i].head == head)
        {
            return &w->heads[i];
        }
    }
    return NULL;
}

static void
head_name(void *data, struct zwlr_output_head_v1 *head, const char *name)
{
    struct wlup_head *h = find_head((struct wlup *)data, head);
    if (h != NULL)
    {
        g_strncpy(h->name, name, sizeof(h->name) - 1);
    }
}

static void
head_description(void *data, struct zwlr_output_head_v1 *head,
                 const char *description)
{
}

static void
head_physical_size(void *data, struct zwlr_output_head_v1 *head,
                   int32_t width, int32_t height)
{
}

static void
mode_size(void *data, struct zwlr_output_mode_v1 *mode,
          int32_t width, int32_t height)
{
}

static void
mode_refresh(void *data, struct zwlr_output_mode_v1 *mode, int32_t refresh)
{
}

static void
mode_preferred(void *data, struct zwlr_output_mode_v1 *mode)
{
}

static void
mode_finished(void *data, struct zwlr_output_mode_v1 *mode)
{
    zwlr_output_mode_v1_destroy(mode);
}

static const struct zwlr_output_mode_v1_listener mode_listener =
{
    .size = mode_size,
    .refresh = mode_refresh,
    .preferred = mode_preferred,
    .finished = mode_finished,
};

static void
head_mode(void *data, struct zwlr_output_head_v1 *head,
          struct zwlr_output_mode_v1 *mode)
{
    /* Modes are not used: resizing always sets a custom mode */
    zwlr_output_mode_v1_add_listener(mode, &mode_listener, data);
}

static void
head_enabled(void *data, struct zwlr_output_head_v1 *head, int32_t enabled)
{
    struct wlup_head *h = find_head((struct wlup *)data, head);
    if (h != NULL)
    {
        h->enabled = enabled;
    }
}

static void
head_current_mode(void *data, struct zwlr_output_head_v1 *head,
                  struct zwlr_output_mode_v1 *mode)
{
}

static void
head_position(void *data, struct zwlr_output_head_v1 *head,
              int32_t x, int32_t y)
{
}

static void
head_transform(void *data, struct zwlr_output_head_v1 *head,
               int32_t transform)
{
}

static void
head_scale(void *data, struct zwlr_output_head_v1 *head, wl_fixed_t scale)
{
}

static void
head_finished(void *data, struct zwlr_output_head_v1 *head)
{
    struct wlup *v = (struct wlup *)data;
    struct wlroots *w = WLR(v);
    struct wlup_head *h = find_head(v, head);

    zwlr_output_head_v1_destroy(head);
    if (h != NULL)
    {
        *h = w->heads[--w->num_heads];
    }
}

static const struct zwlr_output_head_v1_listener head_listener =
{
    .name = head_name,
    .description = head_description,
    .physical_size = head_physical_size,
    .mode = head_mode,
    .enabled = head_enabled,
    .current_mode = head_current_mode,
    .position = head_position,
    .transform = head_transform,
    .scale = head_scale,
    .finished = head_finished,
};

static void
manager_head(void *data, struct zwlr_output_manager_v1 *manager,
             struct zwlr_output_head_v1 *head)
{
    struct wlup *v = (struct wlup *)data;
    struct wlroots *w = WLR(v);

    if (w->num_heads >= WLUP_MAX_HEADS)
    {
        LOG(LOG_LEVEL_WARNING, "wlup: too many outputs, ignoring one");
        zwlr_output_head_v1_destroy(head);
        return;
    }
    g_memset(&w->heads[w->num_heads], 0, sizeof(w->heads[0]));
    w->heads[w->num_heads++].head = head;
    zwlr_output_head_v1_add_listener(head, &head_listener, v);
}

static void
manager_done(void *data, struct zwlr_output_manager_v1 *manager,
             uint32_t serial)
{
    struct wlup *v = (struct wlup *)data;
    struct wlroots *w = WLR(v);
    w->output_manager_serial = serial;
}

static void
manager_finished(void *data, struct zwlr_output_manager_v1 *manager)
{
    struct wlup *v = (struct wlup *)data;
    struct wlroots *w = WLR(v);
    zwlr_output_manager_v1_destroy(manager);
    w->output_manager = NULL;
}

static const struct zwlr_output_manager_v1_listener manager_listener =
{
    .head = manager_head,
    .done = manager_done,
    .finished = manager_finished,
};

/******************************************************************************/
static void
config_succeeded(void *data, struct zwlr_output_configuration_v1 *config)
{
    struct wlup *v = (struct wlup *)data;

    LOG(LOG_LEVEL_INFO, "wlup: output resized to %dx%d",
        v->resize_width, v->resize_height);
    zwlr_output_configuration_v1_destroy(config);
    /* The resize completes in session_done(), when the capture buffer
     * has the new size */
}

static void
config_failed(void *data, struct zwlr_output_configuration_v1 *config)
{
    struct wlup *v = (struct wlup *)data;

    LOG(LOG_LEVEL_WARNING, "wlup: compositor refused output size %dx%d, "
        "the picture will be clipped or padded",
        v->resize_width, v->resize_height);
    zwlr_output_configuration_v1_destroy(config);
    wlup_resize_finished(v);
}

static void
config_cancelled(void *data, struct zwlr_output_configuration_v1 *config)
{
    /* The output layout changed while the request was in flight */
    config_failed(data, config);
}

static const struct zwlr_output_configuration_v1_listener config_listener =
{
    .succeeded = config_succeeded,
    .failed = config_failed,
    .cancelled = config_cancelled,
};

/******************************************************************************/
/* Asks the compositor to give the captured output a new size.
 * return 1 if a request was sent, 0 if not */
static int
request_output_size(struct wlup *v, int width, int height)
{
    struct wlroots *w = WLR(v);
    struct zwlr_output_configuration_v1 *config;
    struct zwlr_output_configuration_head_v1 *config_head;
    struct wlup_head *target = NULL;
    int i;

    if (w->output_manager == NULL)
    {
        return 0;
    }
    for (i = 0; i < w->num_heads; ++i)
    {
        if (strcmp(w->heads[i].name, w->output_name) == 0)
        {
            target = &w->heads[i];
        }
    }
    if (target == NULL && w->num_heads == 1)
    {
        target = &w->heads[0];
    }
    if (target == NULL)
    {
        LOG(LOG_LEVEL_WARNING, "wlup: cannot find output '%s' to resize",
            w->output_name);
        return 0;
    }

    LOG(LOG_LEVEL_INFO, "wlup: requesting output %s size %dx%d",
        target->name, width, height);
    v->resize_width = width;
    v->resize_height = height;

    /* Every head must appear in a configuration, the other ones are
     * passed through unchanged */
    config = zwlr_output_manager_v1_create_configuration(
                 w->output_manager, w->output_manager_serial);
    zwlr_output_configuration_v1_add_listener(config, &config_listener, v);
    for (i = 0; i < w->num_heads; ++i)
    {
        if (&w->heads[i] == target)
        {
            config_head = zwlr_output_configuration_v1_enable_head(
                              config, target->head);
            zwlr_output_configuration_head_v1_set_custom_mode(
                config_head, width, height, 0);
        }
        else if (w->heads[i].enabled)
        {
            zwlr_output_configuration_v1_enable_head(config, w->heads[i].head);
        }
        else
        {
            zwlr_output_configuration_v1_disable_head(config,
                    w->heads[i].head);
        }
    }
    zwlr_output_configuration_v1_apply(config);
    return 1;
}

/******************************************************************************/
/* Registry                                                                   */
/******************************************************************************/
static void
registry_global(void *data, struct wl_registry *registry, uint32_t name,
                const char *interface, uint32_t version)
{
    struct wlup *v = (struct wlup *)data;
    struct wlroots *w = WLR(v);

    if (strcmp(interface, wl_shm_interface.name) == 0)
    {
        w->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    }
    else if (strcmp(interface, wl_seat_interface.name) == 0 && w->seat == NULL)
    {
        w->seat = wl_registry_bind(registry, name, &wl_seat_interface, 1);
    }
    else if (strcmp(interface, wl_output_interface.name) == 0 &&
             w->output == NULL)
    {
        /* Prototype: the first output is the RDP desktop. Version 4 has
         * the output name, used to find it in wlr-output-management */
        w->output = wl_registry_bind(registry, name, &wl_output_interface,
                                     MIN(version, 4));
        wl_output_add_listener(w->output, &output_listener, v);
    }
    else if (strcmp(interface,
                    ext_output_image_capture_source_manager_v1_interface.name)
             == 0)
    {
        w->source_manager = wl_registry_bind(registry, name,
                                             &ext_output_image_capture_source_manager_v1_interface, 1);
    }
    else if (strcmp(interface,
                    ext_image_copy_capture_manager_v1_interface.name) == 0)
    {
        w->copy_manager = wl_registry_bind(registry, name,
                                           &ext_image_copy_capture_manager_v1_interface, 1);
    }
    else if (strcmp(interface,
                    zwlr_virtual_pointer_manager_v1_interface.name) == 0)
    {
        w->pointer_manager_version = MIN(version, 2);
        w->pointer_manager = wl_registry_bind(registry, name,
                                              &zwlr_virtual_pointer_manager_v1_interface,
                                              w->pointer_manager_version);
    }
    else if (strcmp(interface, zwlr_output_manager_v1_interface.name) == 0)
    {
        w->output_manager = wl_registry_bind(registry, name,
                                             &zwlr_output_manager_v1_interface, 1);
        zwlr_output_manager_v1_add_listener(w->output_manager,
                                            &manager_listener, v);
    }
    else if (strcmp(interface,
                    zwp_virtual_keyboard_manager_v1_interface.name) == 0)
    {
        w->keyboard_manager = wl_registry_bind(registry, name,
                                               &zwp_virtual_keyboard_manager_v1_interface, 1);
    }
    else if (w->wlr_clip == NULL)
    {
        wlup_wlr_clip_bind(v, registry, name, interface);
    }
}

/******************************************************************************/
static void
registry_global_remove(void *data, struct wl_registry *registry,
                       uint32_t name)
{
}

static const struct wl_registry_listener registry_listener =
{
    .global = registry_global,
    .global_remove = registry_global_remove,
};

/******************************************************************************/
/* Keyboard                                                                   */
/******************************************************************************/

/* Compiles an XKB keymap and hands it to the virtual keyboard.
 * return error */
static int
send_keymap(struct wlup *v)
{
    struct wlroots *w = WLR(v);
    struct xkb_context *ctx;
    struct xkb_rule_names names = {0};
    char *text;
    size_t size;
    int fd;
    int rv = 1;

    ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (ctx == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "wlup: xkb_context_new failed");
        return 1;
    }
    names.rules = "evdev";
    names.layout = v->xkb_layout[0] != '\0' ? v->xkb_layout : "us";
    names.variant = v->xkb_variant[0] != '\0' ? v->xkb_variant : NULL;
    w->xkb_keymap = xkb_keymap_new_from_names(ctx, &names,
                    XKB_KEYMAP_COMPILE_NO_FLAGS);
    xkb_context_unref(ctx);
    if (w->xkb_keymap == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "wlup: cannot compile XKB keymap layout=%s "
            "variant=%s", names.layout,
            names.variant != NULL ? names.variant : "");
        return 1;
    }
    w->xkb_state = xkb_state_new(w->xkb_keymap);

    text = xkb_keymap_get_as_string(w->xkb_keymap, XKB_KEYMAP_FORMAT_TEXT_V1);
    size = strlen(text) + 1;
    fd = memfd_create("xrdp-wlup-keymap", MFD_CLOEXEC);
    if (fd >= 0 && write(fd, text, size) == (ssize_t)size)
    {
        zwp_virtual_keyboard_v1_keymap(w->keyboard,
                                       WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1,
                                       fd, (uint32_t)size);
        rv = 0;
        LOG(LOG_LEVEL_INFO, "wlup: keyboard layout %s", names.layout);
    }
    else
    {
        LOG(LOG_LEVEL_ERROR, "wlup: cannot write keymap: %s", strerror(errno));
    }
    if (fd >= 0)
    {
        close(fd);
    }
    free(text);
    return rv;
}

/******************************************************************************/
static void
wlroots_key(struct wlup *v, int evdev_code, int down)
{
    struct wlroots *w = WLR(v);
    /* X11 keycodes are evdev codes plus 8 */
    int x11_keycode = evdev_code + 8;

    if (w->keyboard == NULL || w->xkb_state == NULL)
    {
        return;
    }
    zwp_virtual_keyboard_v1_key(w->keyboard, now_ms(), evdev_code,
                                down ? WL_KEYBOARD_KEY_STATE_PRESSED
                                : WL_KEYBOARD_KEY_STATE_RELEASED);

    /* The compositor does not derive modifiers from the key events of a
     * virtual keyboard, the client has to send them, as a real
     * wl_keyboard does after a modifier key changes */
    if (xkb_state_update_key(w->xkb_state, x11_keycode,
                             down ? XKB_KEY_DOWN : XKB_KEY_UP) != 0)
    {
        zwp_virtual_keyboard_v1_modifiers(w->keyboard,
                                          xkb_state_serialize_mods(w->xkb_state, XKB_STATE_MODS_DEPRESSED),
                                          xkb_state_serialize_mods(w->xkb_state, XKB_STATE_MODS_LATCHED),
                                          xkb_state_serialize_mods(w->xkb_state, XKB_STATE_MODS_LOCKED),
                                          xkb_state_serialize_layout(w->xkb_state,
                                                  XKB_STATE_LAYOUT_EFFECTIVE));
    }
}

/******************************************************************************/
/* Pointer                                                                    */
/******************************************************************************/
static void
pointer_button(struct wlup *v, uint32_t button, int down)
{
    struct wlroots *w = WLR(v);
    zwlr_virtual_pointer_v1_button(w->pointer, now_ms(), button,
                                   down ? WL_POINTER_BUTTON_STATE_PRESSED
                                   : WL_POINTER_BUTTON_STATE_RELEASED);
    zwlr_virtual_pointer_v1_frame(w->pointer);
}

/******************************************************************************/
static void
pointer_wheel(struct wlup *v, uint32_t axis, int steps)
{
    struct wlroots *w = WLR(v);
    uint32_t t = now_ms();
    zwlr_virtual_pointer_v1_axis_source(w->pointer,
                                        WL_POINTER_AXIS_SOURCE_WHEEL);
    zwlr_virtual_pointer_v1_axis_discrete(w->pointer, t, axis,
                                          wl_fixed_from_double(steps * WLUP_WHEEL_STEP),
                                          steps);
    zwlr_virtual_pointer_v1_frame(w->pointer);
}

/******************************************************************************/
static int
process_mouse(struct wlup *v, int msg, int x, int y)
{
    struct wlroots *w = WLR(v);
    int width = v->buffer_width > 0 ? v->buffer_width : v->server_width;
    int height = v->buffer_height > 0 ? v->buffer_height : v->server_height;

    if (w->pointer == NULL)
    {
        return 0;
    }
    /* Scroll goes to the surface under the pointer, so wheel events must
     * not move it. xrdp passes its last stored position with them, which
     * is only updated by move events, not by clicks, and can be stale */
    if (msg < WM_BUTTON4UP || msg > WM_BUTTON7DOWN)
    {
        x = MAX(0, MIN(x, width - 1));
        y = MAX(0, MIN(y, height - 1));
        zwlr_virtual_pointer_v1_motion_absolute(w->pointer, now_ms(), x, y,
                                                width, height);
        zwlr_virtual_pointer_v1_frame(w->pointer);
    }

    switch (msg)
    {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
            pointer_button(v, BTN_LEFT, msg == WM_LBUTTONDOWN);
            break;
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
            pointer_button(v, BTN_RIGHT, msg == WM_RBUTTONDOWN);
            break;
        case WM_BUTTON3DOWN:
        case WM_BUTTON3UP:
            pointer_button(v, BTN_MIDDLE, msg == WM_BUTTON3DOWN);
            break;
        case WM_BUTTON8DOWN:
        case WM_BUTTON8UP:
            pointer_button(v, BTN_SIDE, msg == WM_BUTTON8DOWN);
            break;
        case WM_BUTTON9DOWN:
        case WM_BUTTON9UP:
            pointer_button(v, BTN_EXTRA, msg == WM_BUTTON9DOWN);
            break;
        /* Wheel notches arrive as X11-style button 4-7 down/up pairs.
         * Act on the 'down' half only */
        case WM_BUTTON4DOWN:
            pointer_wheel(v, WL_POINTER_AXIS_VERTICAL_SCROLL, -1);
            break;
        case WM_BUTTON5DOWN:
            pointer_wheel(v, WL_POINTER_AXIS_VERTICAL_SCROLL, 1);
            break;
        case WM_BUTTON6DOWN:
            pointer_wheel(v, WL_POINTER_AXIS_HORIZONTAL_SCROLL, -1);
            break;
        case WM_BUTTON7DOWN:
            pointer_wheel(v, WL_POINTER_AXIS_HORIZONTAL_SCROLL, 1);
            break;
        default:
            break;
    }
    return 0;
}


/******************************************************************************/
/* Connection                                                                 */
/******************************************************************************/
/* return error */
static int
wlroots_connect(struct wlup *v, int fd)
{
    struct wlroots *w;
    char text[512];
    const char *name = v->display_name[0] != '\0' ? v->display_name : NULL;

    w = g_new0(struct wlroots, 1);
    if (w == NULL)
    {
        return 1;
    }
    v->backend_data = w;
    if (fd >= 0)
    {
        /* sesman started the compositor and connected to it for us */
        name = "(connection from sesman)";
        w->display = wl_display_connect_to_fd(fd);
    }
    else
    {
        w->display = wl_display_connect(name);
    }
    if (w->display == NULL)
    {
        g_snprintf(text, sizeof(text), "wlup error - cannot connect to the "
                   "Wayland compositor at '%s'",
                   name != NULL ? name : "$WAYLAND_DISPLAY");
        v->server_msg(v, text, 0);
        return 1;
    }

    w->registry = wl_display_get_registry(w->display);
    wl_registry_add_listener(w->registry, &registry_listener, v);
    wl_display_roundtrip(w->display);
    /* Events of the globals just bound: output name, output heads */
    wl_display_roundtrip(w->display);

    if (w->shm == NULL || w->output == NULL || w->source_manager == NULL ||
            w->copy_manager == NULL)
    {
        v->server_msg(v, "wlup error - the compositor does not offer "
                      "ext-image-copy-capture-v1 for an output", 0);
        return 1;
    }

    /* Input is optional, the desktop is shown read-only without it */
    if (w->pointer_manager != NULL)
    {
        if (w->pointer_manager_version >= 2)
        {
            w->pointer = zwlr_virtual_pointer_manager_v1_create_virtual_pointer_with_output(
                             w->pointer_manager, w->seat, w->output);
        }
        else
        {
            w->pointer = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(
                             w->pointer_manager, w->seat);
        }
    }
    else
    {
        LOG(LOG_LEVEL_WARNING, "wlup: no zwlr_virtual_pointer_manager_v1, "
            "the mouse will not work");
    }
    if (w->keyboard_manager != NULL && w->seat != NULL)
    {
        w->keyboard = zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(
                          w->keyboard_manager, w->seat);
        scancode_set_keycode_set("evdev");
        send_keymap(v);
    }
    else
    {
        LOG(LOG_LEVEL_WARNING, "wlup: no zwp_virtual_keyboard_manager_v1, "
            "the keyboard will not work");
    }

    /* Cursors are painted into the frames for now. A separate cursor
     * session would let the RDP client draw the cursor locally */
    w->source = ext_output_image_capture_source_manager_v1_create_source(
                    w->source_manager, w->output);
    w->session = ext_image_copy_capture_manager_v1_create_session(
                     w->copy_manager, w->source,
                     EXT_IMAGE_COPY_CAPTURE_MANAGER_V1_OPTIONS_PAINT_CURSORS);
    ext_image_copy_capture_session_v1_add_listener(w->session,
            &session_listener, v);

    /* Receive the buffer constraints. session_done() starts the capture */
    wl_display_roundtrip(w->display);
    if (!w->constraints_done)
    {
        v->server_msg(v, "wlup error - no usable capture buffer format", 0);
        return 1;
    }

    /* Match the output to the size the RDP client asked for */
    if (v->buffer_width != v->server_width ||
            v->buffer_height != v->server_height)
    {
        if (!request_output_size(v, v->server_width, v->server_height))
        {
            LOG(LOG_LEVEL_WARNING, "wlup: the compositor cannot resize its "
                "output (no wlr-output-management)");
        }
    }

    /* Text clipboard, session side. The core opens the RDP channel */
    wlup_wlr_clip_start(v);

    g_snprintf(text, sizeof(text), "wlup: connected to Wayland display %s",
               name != NULL ? name : "$WAYLAND_DISPLAY");
    v->server_msg(v, text, 0);
    LOG(LOG_LEVEL_INFO, "%s", text);
    return 0;
}

/******************************************************************************/
static void
wlroots_disconnect(struct wlup *v)
{
    struct wlroots *w = WLR(v);

    if (w == NULL)
    {
        return;
    }
    wlup_wlr_clip_destroy(v);
    if (w->frame != NULL)
    {
        ext_image_copy_capture_frame_v1_destroy(w->frame);
        w->frame = NULL;
    }
    if (w->session != NULL)
    {
        ext_image_copy_capture_session_v1_destroy(w->session);
        w->session = NULL;
    }
    if (w->source != NULL)
    {
        ext_image_capture_source_v1_destroy(w->source);
        w->source = NULL;
    }
    free_buffer(v);
    if (w->output_manager != NULL)
    {
        int i;
        for (i = 0; i < w->num_heads; ++i)
        {
            zwlr_output_head_v1_destroy(w->heads[i].head);
        }
        w->num_heads = 0;
        zwlr_output_manager_v1_destroy(w->output_manager);
        w->output_manager = NULL;
    }
    if (w->pointer != NULL)
    {
        zwlr_virtual_pointer_v1_destroy(w->pointer);
        w->pointer = NULL;
    }
    if (w->keyboard != NULL)
    {
        zwp_virtual_keyboard_v1_destroy(w->keyboard);
        w->keyboard = NULL;
    }
    if (w->xkb_state != NULL)
    {
        xkb_state_unref(w->xkb_state);
        w->xkb_state = NULL;
    }
    if (w->xkb_keymap != NULL)
    {
        xkb_keymap_unref(w->xkb_keymap);
        w->xkb_keymap = NULL;
    }
    if (w->display != NULL)
    {
        /* Destroying the connection releases all remaining proxies on the
         * compositor side */
        wl_display_disconnect(w->display);
        w->display = NULL;
    }
    g_free(w);
    v->backend_data = NULL;
}

/******************************************************************************/
/* return error */
static int
wlroots_check_wait_objs(struct wlup *v)
{
    struct wlroots *w = WLR(v);
    struct pollfd pfd;

    if (v == NULL || w->display == NULL)
    {
        return 0;
    }

    /* Standard libwayland pattern for an external event loop */
    while (wl_display_prepare_read(w->display) != 0)
    {
        if (wl_display_dispatch_pending(w->display) < 0)
        {
            break;
        }
    }
    wl_display_flush(w->display);

    pfd.fd = wl_display_get_fd(w->display);
    pfd.events = POLLIN;
    pfd.revents = 0;
    if (poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLIN))
    {
        wl_display_read_events(w->display);
    }
    else
    {
        wl_display_cancel_read(w->display);
    }
    wl_display_dispatch_pending(w->display);
    /* A new selection is read once all events are dispatched */
    wlup_wlr_clip_process(v);
    wl_display_flush(w->display);

    if (wl_display_get_error(w->display) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "wlup: Wayland connection error %d",
            wl_display_get_error(w->display));
        return 1;
    }
    if (w->session == NULL)
    {
        /* The compositor stopped the capture, e.g. the output went away */
        return 1;
    }
    return 0;
}

/******************************************************************************/
static void
wlroots_get_wait_objs(struct wlup *v, tbus *read_objs, int *rcount,
                      tbus *write_objs, int *wcount, int *timeout)
{
    struct wlroots *w = WLR(v);
    if (w->display != NULL)
    {
        wl_display_flush(w->display);
        read_objs[(*rcount)++] = wl_display_get_fd(w->display);
    }
}

/******************************************************************************/
static void
wlroots_mouse(struct wlup *v, int msg, int x, int y)
{
    process_mouse(v, msg, x, y);
}

/******************************************************************************/
/* return 1 if the resize is under way */
static int
wlroots_resize(struct wlup *v, int width, int height)
{
    struct wlroots *w = WLR(v);
    if (!request_output_size(v, width, height))
    {
        return 0;
    }
    wl_display_flush(w->display);
    return 1;
}

/******************************************************************************/
/* The compositor owns the buffer from the capture request to ready */
static int
wlroots_pixels_busy(struct wlup *v)
{
    struct wlroots *w = WLR(v);
    return w->frame != NULL;
}

/******************************************************************************/
static void
wlroots_flush(struct wlup *v)
{
    struct wlroots *w = WLR(v);
    if (w->display != NULL)
    {
        wl_display_flush(w->display);
    }
}

/******************************************************************************/
const struct wlup_backend_ops wlup_backend_wlroots =
{
    .name = "wlroots",
    .connect = wlroots_connect,
    .disconnect = wlroots_disconnect,
    .get_wait_objs = wlroots_get_wait_objs,
    .check_wait_objs = wlroots_check_wait_objs,
    .key = wlroots_key,
    .mouse = wlroots_mouse,
    .resize = wlroots_resize,
    .frames_wanted = continue_capture,
    .pixels_busy = wlroots_pixels_busy,
    .flush = wlroots_flush
};
