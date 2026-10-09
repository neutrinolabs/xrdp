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
 * wlup: Mutter (GNOME) backend
 *
 * Setup, all over the session bus:
 *   1. RemoteDesktop.CreateSession            -> remote desktop session
 *   2. ScreenCast.CreateSession, with the remote desktop session id
 *   3. ScreenCast.Session.RecordVirtual       -> stream; Mutter adds a
 *      virtual monitor whose size is the size we ask PipeWire for
 *   4. RemoteDesktop.Session.Start            -> PipeWireStreamAdded(node)
 *   5. RemoteDesktop.Session.ConnectToEIS     -> fd for libei input
 *
 * Three file descriptors then go into xrdp's poll loop, so no extra
 * threads are needed:
 *   sd-bus connection   session Closed signal
 *   PipeWire loop fd    frames (pw_loop_iterate(loop, 0) when readable)
 *   libei fd            input devices appearing, pausing, resuming
 *
 * sd-bus was chosen for D-Bus because it exposes exactly what a poll
 * loop needs (sd_bus_get_fd/get_events/process). Mutter needs systemd
 * and logind anyway, so depending on libsystemd costs nothing here.
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <errno.h>
#include <stdarg.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include <linux/input-event-codes.h>
#include <systemd/sd-bus.h>
#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>
#include <spa/param/buffers.h>
#include <spa/buffer/meta.h>
#include <libei.h>

#include "wlup.h"
#include "wlup_mutter.h"
#include "wlup_clip.h"
#include "log.h"
#include "string_calls.h"
#include "xrdp_constants.h"
#include "scancode.h"

#define RD_NAME "org.gnome.Mutter.RemoteDesktop"
#define RD_PATH "/org/gnome/Mutter/RemoteDesktop"
#define RD_SESSION_IFACE "org.gnome.Mutter.RemoteDesktop.Session"
#define SC_NAME "org.gnome.Mutter.ScreenCast"
#define SC_PATH "/org/gnome/Mutter/ScreenCast"
#define SC_SESSION_IFACE "org.gnome.Mutter.ScreenCast.Session"
#define SC_STREAM_IFACE "org.gnome.Mutter.ScreenCast.Stream"

/* ScreenCast cursor modes */
#define CURSOR_MODE_EMBEDDED 1

/* Clipboard transfers to the session queued between two polls */
#define MAX_TRANSFERS 16

/* Damage rectangles kept per PipeWire buffer */
#define MAX_DAMAGE 16

/* Time allowed for Mutter to announce the PipeWire stream */
#define STREAM_TIMEOUT_USEC (5 * 1000 * 1000)

/* Time allowed for GNOME Shell to come up in a session sesman has just
 * started, and offer its remote desktop API */
#define MUTTER_TIMEOUT_USEC (60 * 1000 * 1000)

/* Time allowed for Mutter to resize the virtual monitor. xrdp waits for
 * the resize, so give up after this and let the picture be clipped */
#define RESIZE_TIMEOUT_USEC (5 * 1000 * 1000)

struct wlup_mutter
{
    /* D-Bus */
    sd_bus *bus;
    sd_bus_slot *stream_added_slot;
    sd_bus_slot *closed_slot;
    char *rd_session_path;
    char *sc_session_path;
    char *stream_path;
    uint32_t node_id;
    int have_node;
    int closed;

    /* PipeWire */
    struct pw_loop *loop;
    struct pw_context *context;
    struct pw_core *core;
    struct spa_hook core_listener;
    struct pw_stream *stream;
    struct spa_hook stream_listener;
    int width;
    int height;

    /* libei */
    struct ei *ei;
    struct ei_device *keyboard;
    struct ei_device *pointer; /* absolute pointer, also buttons/scroll */
    uint32_t sequence;
    int pointer_placed; /* pointer moved away from the hot corner */

    uint64_t resize_start; /* when the pending resize was requested */

    /* clipboard */
    sd_bus_slot *owner_changed_slot;
    sd_bus_slot *transfer_slot;
    int clipboard_enabled;
    const char *read_mime;  /* session selection to read, or NULL */
    uint32_t transfers[MAX_TRANSFERS]; /* SelectionTransfer serials */
    int num_transfers;
    char *client_text;      /* the RDP client's clipboard text */
    int client_text_len;
};

/* MIME types accepted and offered for text, best first */
static const char *const g_text_mime_types[] =
{
    "text/plain;charset=utf-8",
    "text/plain",
    "UTF8_STRING",
    "STRING",
    "TEXT",
    NULL
};

static int pw_initialised;

/******************************************************************************/
static uint64_t
monotonic_usec(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

/******************************************************************************/
/* PipeWire                                                                   */
/******************************************************************************/

/* Copies one rectangle of a PipeWire buffer into our desktop copy */
static void
copy_region(struct wlup *v, const uint8_t *src, int src_stride,
            int x, int y, int cx, int cy)
{
    int row;
    int dst_stride = v->buffer_width * 4;

    x = MAX(x, 0);
    y = MAX(y, 0);
    cx = MIN(cx, v->buffer_width - x);
    cy = MIN(cy, v->buffer_height - y);
    for (row = y; row < y + cy; ++row)
    {
        memcpy(v->pixels + (size_t)row * dst_stride + x * 4,
               src + (size_t)row * src_stride + x * 4, (size_t)cx * 4);
    }
}

/******************************************************************************/
static void
on_process(void *data)
{
    struct wlup *v = (struct wlup *)data;
    struct wlup_mutter *m = (struct wlup_mutter *)v->mutter;
    struct pw_buffer *pb;
    short rects[MAX_DAMAGE * 4];
    int num_rects = 0;
    int full = v->need_full_paint;
    int i;

    /* Take every queued buffer, so no damage is lost when frames
     * arrive faster than xrdp polls */
    while ((pb = pw_stream_dequeue_buffer(m->stream)) != NULL)
    {
        struct spa_buffer *buf = pb->buffer;
        struct spa_data *d = &buf->datas[0];
        struct spa_meta_header *h;
        struct spa_meta *dm;
        const uint8_t *src;
        int stride;

        h = spa_buffer_find_meta_data(buf, SPA_META_Header, sizeof(*h));
        if ((h != NULL && (h->flags & SPA_META_HEADER_FLAG_CORRUPTED)) ||
                d->data == NULL || d->chunk->size == 0 || v->pixels == NULL)
        {
            /* No picture in this buffer (e.g. only the cursor moved) */
            pw_stream_queue_buffer(m->stream, pb);
            continue;
        }
        src = (const uint8_t *)d->data + d->chunk->offset;
        stride = d->chunk->stride != 0 ? d->chunk->stride : m->width * 4;

        dm = spa_buffer_find_meta(buf, SPA_META_VideoDamage);
        if (dm == NULL || full)
        {
            copy_region(v, src, stride, 0, 0, m->width, m->height);
            full = 1;
        }
        else
        {
            struct spa_meta_region *r;
            spa_meta_for_each(r, dm)
            {
                if (!spa_meta_region_is_valid(r))
                {
                    break;
                }
                copy_region(v, src, stride, r->region.position.x,
                            r->region.position.y, r->region.size.width,
                            r->region.size.height);
                if (num_rects < MAX_DAMAGE)
                {
                    rects[num_rects * 4] = r->region.position.x;
                    rects[num_rects * 4 + 1] = r->region.position.y;
                    rects[num_rects * 4 + 2] = r->region.size.width;
                    rects[num_rects * 4 + 3] = r->region.size.height;
                    ++num_rects;
                }
                else
                {
                    full = 1;
                }
            }
        }
        pw_stream_queue_buffer(m->stream, pb);
    }

    if (v->suppress_output || (!full && num_rects == 0))
    {
        return;
    }
    v->server_begin_update(v);
    if (full)
    {
        wlup_paint_region(v, 0, 0, m->width, m->height);
        v->need_full_paint = 0;
    }
    else
    {
        for (i = 0; i < num_rects; ++i)
        {
            wlup_paint_region(v, rects[i * 4], rects[i * 4 + 1],
                              rects[i * 4 + 2], rects[i * 4 + 3]);
        }
    }
    v->server_end_update(v);
}

/******************************************************************************/
/* Our desktop copy, an anonymous mapping so wlup's free_buffer() can
 * release it like a Wayland shm buffer. return error */
static int
alloc_pixels(struct wlup *v, int width, int height)
{
    size_t size = (size_t)width * height * 4;

    if (v->pixels != NULL)
    {
        munmap(v->pixels, v->pixels_size);
    }
    v->pixels = mmap(NULL, size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (v->pixels == MAP_FAILED)
    {
        v->pixels = NULL;
        return 1;
    }
    v->pixels_size = size;
    v->buffer_width = width;
    v->buffer_height = height;
    v->need_full_paint = 1;
    return 0;
}

/******************************************************************************/
static void
on_param_changed(void *data, uint32_t id, const struct spa_pod *param)
{
    struct wlup *v = (struct wlup *)data;
    struct wlup_mutter *m = (struct wlup_mutter *)v->mutter;
    struct spa_video_info_raw info;
    uint8_t pod_buf[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(pod_buf, sizeof(pod_buf));
    const struct spa_pod *params[3];
    int stride;

    if (param == NULL || id != SPA_PARAM_Format)
    {
        return;
    }
    if (spa_format_video_raw_parse(param, &info) < 0)
    {
        LOG(LOG_LEVEL_ERROR, "wlup/mutter: cannot parse the stream format");
        return;
    }
    m->width = info.size.width;
    m->height = info.size.height;
    stride = m->width * 4;
    LOG(LOG_LEVEL_INFO, "wlup/mutter: stream format %s %dx%d",
        info.format == SPA_VIDEO_FORMAT_BGRx ? "BGRx" :
        info.format == SPA_VIDEO_FORMAT_BGRA ? "BGRA" : "other",
        m->width, m->height);
    if (alloc_pixels(v, m->width, m->height) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "wlup/mutter: out of memory");
        return;
    }
    if (v->resize_pending && m->width == v->resize_width &&
            m->height == v->resize_height)
    {
        LOG(LOG_LEVEL_INFO, "wlup/mutter: virtual monitor resized to %dx%d",
            m->width, m->height);
        wlup_resize_finished(v);
    }

    /* Shared memory only (no dma-buf), plus damage regions */
    params[0] = spa_pod_builder_add_object(&b,
                                           SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
                                           SPA_PARAM_BUFFERS_buffers, SPA_POD_CHOICE_RANGE_Int(4, 2, 16),
                                           SPA_PARAM_BUFFERS_blocks, SPA_POD_Int(1),
                                           SPA_PARAM_BUFFERS_size, SPA_POD_Int(stride * m->height),
                                           SPA_PARAM_BUFFERS_stride, SPA_POD_Int(stride),
                                           SPA_PARAM_BUFFERS_dataType,
                                           SPA_POD_CHOICE_FLAGS_Int((1 << SPA_DATA_MemFd) |
                                                   (1 << SPA_DATA_MemPtr)));
    params[1] = spa_pod_builder_add_object(&b,
                                           SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta,
                                           SPA_PARAM_META_type, SPA_POD_Id(SPA_META_Header),
                                           SPA_PARAM_META_size,
                                           SPA_POD_Int(sizeof(struct spa_meta_header)));
    params[2] = spa_pod_builder_add_object(&b,
                                           SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta,
                                           SPA_PARAM_META_type, SPA_POD_Id(SPA_META_VideoDamage),
                                           SPA_PARAM_META_size, SPA_POD_CHOICE_RANGE_Int(
                                                   sizeof(struct spa_meta_region) * MAX_DAMAGE,
                                                   sizeof(struct spa_meta_region),
                                                   sizeof(struct spa_meta_region) * MAX_DAMAGE));
    pw_stream_update_params(m->stream, params, 3);
}

/******************************************************************************/
static void
on_state_changed(void *data, enum pw_stream_state old,
                 enum pw_stream_state state, const char *error)
{
    LOG(LOG_LEVEL_INFO, "wlup/mutter: stream %s%s%s",
        pw_stream_state_as_string(state),
        error != NULL ? ": " : "", error != NULL ? error : "");
}

static const struct pw_stream_events stream_events =
{
    PW_VERSION_STREAM_EVENTS,
    .state_changed = on_state_changed,
    .param_changed = on_param_changed,
    .process = on_process,
};

/******************************************************************************/
static void
on_core_error(void *data, uint32_t id, int seq, int res, const char *message)
{
    LOG(LOG_LEVEL_ERROR, "wlup/mutter: PipeWire error on %u: %s", id, message);
}

static const struct pw_core_events core_events =
{
    PW_VERSION_CORE_EVENTS,
    .error = on_core_error,
};

/******************************************************************************/
/* The formats we accept. For a virtual monitor, the preferred size here
 * becomes the size of the monitor. No modifier is offered, so buffers
 * are shared memory */
static const struct spa_pod *
build_enum_format(struct spa_pod_builder *b, int width, int height)
{
    struct spa_rectangle size = SPA_RECTANGLE(width, height);
    struct spa_rectangle min_size = SPA_RECTANGLE(1, 1);
    struct spa_rectangle max_size = SPA_RECTANGLE(8192, 8192);
    struct spa_fraction rate = SPA_FRACTION(0, 1);
    struct spa_fraction max_rate = SPA_FRACTION(60, 1);
    struct spa_fraction min_rate = SPA_FRACTION(1, 1);

    return spa_pod_builder_add_object(b,
                                      SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
                                      SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video),
                                      SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
                                      SPA_FORMAT_VIDEO_format,
                                      SPA_POD_CHOICE_ENUM_Id(3, SPA_VIDEO_FORMAT_BGRx,
                                              SPA_VIDEO_FORMAT_BGRx, SPA_VIDEO_FORMAT_BGRA),
                                      SPA_FORMAT_VIDEO_size,
                                      SPA_POD_CHOICE_RANGE_Rectangle(&size, &min_size, &max_size),
                                      SPA_FORMAT_VIDEO_framerate, SPA_POD_Fraction(&rate),
                                      SPA_FORMAT_VIDEO_maxFramerate,
                                      SPA_POD_CHOICE_RANGE_Fraction(&max_rate, &min_rate, &max_rate));
}

/******************************************************************************/
/* return error */
static int
connect_stream(struct wlup *v)
{
    struct wlup_mutter *m = (struct wlup_mutter *)v->mutter;
    uint8_t pod_buf[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(pod_buf, sizeof(pod_buf));
    const struct spa_pod *params[1];

    if (!pw_initialised)
    {
        pw_init(NULL, NULL);
        pw_initialised = 1;
    }
    m->loop = pw_loop_new(NULL);
    m->context = pw_context_new(m->loop, NULL, 0);
    if (m->context == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "wlup/mutter: pw_context_new failed");
        return 1;
    }
    if (v->pipewire_fd >= 0)
    {
        /* Connection to the session's PipeWire daemon from sesman, made
         * as the session user. PipeWire takes ownership of it */
        m->core = pw_context_connect_fd(m->context, v->pipewire_fd, NULL, 0);
        v->pipewire_fd = -1;
    }
    else
    {
        /* The session's PipeWire daemon, found through XDG_RUNTIME_DIR */
        m->core = pw_context_connect(m->context, NULL, 0);
    }
    if (m->core == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "wlup/mutter: cannot connect to PipeWire: %s",
            strerror(errno));
        return 1;
    }
    pw_core_add_listener(m->core, &m->core_listener, &core_events, v);

    m->stream = pw_stream_new(m->core, "xrdp",
                              pw_properties_new(PW_KEY_MEDIA_TYPE, "Video",
                                      PW_KEY_MEDIA_CATEGORY, "Capture",
                                      PW_KEY_MEDIA_ROLE, "Screen",
                                      NULL));
    pw_stream_add_listener(m->stream, &m->stream_listener, &stream_events, v);

    params[0] = build_enum_format(&b, v->server_width, v->server_height);

    if (pw_stream_connect(m->stream, PW_DIRECTION_INPUT, m->node_id,
                          PW_STREAM_FLAG_AUTOCONNECT |
                          PW_STREAM_FLAG_MAP_BUFFERS,
                          params, 1) < 0)
    {
        LOG(LOG_LEVEL_ERROR, "wlup/mutter: pw_stream_connect failed");
        return 1;
    }
    return 0;
}

/******************************************************************************/
int
wlup_mutter_resize(struct wlup *v, int width, int height)
{
    struct wlup_mutter *m = (struct wlup_mutter *)v->mutter;
    uint8_t pod_buf[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(pod_buf, sizeof(pod_buf));
    const struct spa_pod *params[1];

    if (m == NULL || m->stream == NULL)
    {
        return 0;
    }
    /* Offering a new preferred size renegotiates the stream format, and
     * Mutter resizes the virtual monitor to match */
    LOG(LOG_LEVEL_INFO, "wlup/mutter: requesting virtual monitor size %dx%d",
        width, height);
    params[0] = build_enum_format(&b, width, height);
    if (pw_stream_update_params(m->stream, params, 1) < 0)
    {
        LOG(LOG_LEVEL_ERROR, "wlup/mutter: cannot renegotiate the stream");
        return 0;
    }
    m->resize_start = monotonic_usec();
    return 1;
}

/******************************************************************************/
/* libei                                                                      */
/******************************************************************************/
static void
process_ei_events(struct wlup *v)
{
    struct wlup_mutter *m = (struct wlup_mutter *)v->mutter;
    struct ei_event *e;

    ei_dispatch(m->ei);
    while ((e = ei_get_event(m->ei)) != NULL)
    {
        struct ei_device *dev = ei_event_get_device(e);

        switch (ei_event_get_type(e))
        {
            case EI_EVENT_SEAT_ADDED:
                ei_seat_bind_capabilities(ei_event_get_seat(e),
                                          EI_DEVICE_CAP_POINTER_ABSOLUTE,
                                          EI_DEVICE_CAP_KEYBOARD,
                                          EI_DEVICE_CAP_BUTTON,
                                          EI_DEVICE_CAP_SCROLL,
                                          NULL);
                break;
            case EI_EVENT_DEVICE_ADDED:
                if (ei_device_has_capability(dev, EI_DEVICE_CAP_KEYBOARD) &&
                        m->keyboard == NULL)
                {
                    m->keyboard = ei_device_ref(dev);
                    LOG(LOG_LEVEL_INFO, "wlup/mutter: EIS keyboard '%s'",
                        ei_device_get_name(dev));
                }
                if (ei_device_has_capability(dev,
                                             EI_DEVICE_CAP_POINTER_ABSOLUTE) &&
                        m->pointer == NULL)
                {
                    m->pointer = ei_device_ref(dev);
                    LOG(LOG_LEVEL_INFO, "wlup/mutter: EIS pointer '%s'",
                        ei_device_get_name(dev));
                }
                break;
            case EI_EVENT_DEVICE_RESUMED:
                ei_device_start_emulating(dev, ++m->sequence);
                if (dev == m->pointer && !m->pointer_placed)
                {
                    /* GNOME's pointer starts at (0,0), its hot corner,
                     * where the first motion opens the Activities
                     * overview. Start in the middle of the screen */
                    struct ei_region *r = ei_device_get_region(dev, 0);
                    if (r != NULL)
                    {
                        ei_device_pointer_motion_absolute(dev,
                                                          ei_region_get_x(r) + ei_region_get_width(r) / 2.0,
                                                          ei_region_get_y(r) + ei_region_get_height(r) / 2.0);
                        ei_device_frame(dev, ei_now(m->ei));
                        m->pointer_placed = 1;
                    }
                }
                break;
            case EI_EVENT_DEVICE_REMOVED:
                if (dev == m->keyboard)
                {
                    ei_device_unref(m->keyboard);
                    m->keyboard = NULL;
                }
                if (dev == m->pointer)
                {
                    ei_device_unref(m->pointer);
                    m->pointer = NULL;
                }
                break;
            case EI_EVENT_DISCONNECT:
                LOG(LOG_LEVEL_WARNING, "wlup/mutter: EIS disconnected, "
                    "input stops");
                break;
            default:
                break;
        }
        ei_event_unref(e);
    }
}

/******************************************************************************/
void
wlup_mutter_key(struct wlup *v, int evdev_code, int down)
{
    struct wlup_mutter *m = (struct wlup_mutter *)v->mutter;

    if (m == NULL || m->keyboard == NULL)
    {
        return;
    }
    /* Mutter tracks the modifier state itself, as for a real keyboard */
    ei_device_keyboard_key(m->keyboard, evdev_code, down);
    ei_device_frame(m->keyboard, ei_now(m->ei));
}

/******************************************************************************/
void
wlup_mutter_mouse(struct wlup *v, int msg, int x, int y)
{
    struct wlup_mutter *m = (struct wlup_mutter *)v->mutter;
    struct ei_device *dev;
    struct ei_region *region;
    uint32_t button = 0;
    int scroll = 0;
    int down = 0;

    if (m == NULL || m->pointer == NULL)
    {
        return;
    }
    dev = m->pointer;

    switch (msg)
    {
        case WM_LBUTTONDOWN: down = 1; /* fall through */
        case WM_LBUTTONUP: button = BTN_LEFT; break;
        case WM_RBUTTONDOWN: down = 1; /* fall through */
        case WM_RBUTTONUP: button = BTN_RIGHT; break;
        case WM_BUTTON3DOWN: down = 1; /* fall through */
        case WM_BUTTON3UP: button = BTN_MIDDLE; break;
        case WM_BUTTON8DOWN: down = 1; /* fall through */
        case WM_BUTTON8UP: button = BTN_SIDE; break;
        case WM_BUTTON9DOWN: down = 1; /* fall through */
        case WM_BUTTON9UP: button = BTN_EXTRA; break;
        case WM_BUTTON4DOWN: scroll = -1; break;
        case WM_BUTTON5DOWN: scroll = 1; break;
        default: break;
    }

    /* Scroll goes where the pointer is; see the note in wlup.c */
    if (scroll != 0)
    {
        if (ei_device_has_capability(dev, EI_DEVICE_CAP_SCROLL))
        {
            ei_device_scroll_discrete(dev, 0, scroll * 120);
            ei_device_frame(dev, ei_now(m->ei));
        }
        return;
    }
    if (msg >= WM_BUTTON4UP && msg <= WM_BUTTON7DOWN)
    {
        return;
    }

    /* Absolute coordinates are in the region of our virtual monitor */
    region = ei_device_get_region(dev, 0);
    if (region != NULL && v->buffer_width > 0 && v->buffer_height > 0)
    {
        ei_device_pointer_motion_absolute(dev,
                                          ei_region_get_x(region) +
                                          (double)x * ei_region_get_width(region) / v->buffer_width,
                                          ei_region_get_y(region) +
                                          (double)y * ei_region_get_height(region) / v->buffer_height);
    }
    else
    {
        ei_device_pointer_motion_absolute(dev, x, y);
    }
    ei_device_frame(dev, ei_now(m->ei));

    if (button != 0 && ei_device_has_capability(dev, EI_DEVICE_CAP_BUTTON))
    {
        ei_device_button_button(dev, button, down);
        ei_device_frame(dev, ei_now(m->ei));
    }
}

/******************************************************************************/
/* D-Bus                                                                      */
/******************************************************************************/
static int
on_stream_added(sd_bus_message *msg, void *data, sd_bus_error *err)
{
    struct wlup *v = (struct wlup *)data;
    struct wlup_mutter *m = (struct wlup_mutter *)v->mutter;

    if (sd_bus_message_read(msg, "u", &m->node_id) >= 0)
    {
        m->have_node = 1;
        LOG(LOG_LEVEL_INFO, "wlup/mutter: PipeWire node %u", m->node_id);
    }
    return 0;
}

/******************************************************************************/
static int
on_closed(sd_bus_message *msg, void *data, sd_bus_error *err)
{
    struct wlup *v = (struct wlup *)data;
    struct wlup_mutter *m = (struct wlup_mutter *)v->mutter;

    LOG(LOG_LEVEL_INFO, "wlup/mutter: remote desktop session closed");
    m->closed = 1;
    return 0;
}

/******************************************************************************/
/* Calls a method returning one object path; *path receives a copy.
 * return error */
static int
call_for_path(struct wlup_mutter *m, const char *dest, const char *path,
              const char *iface, const char *method, char **out,
              sd_bus_message *args)
{
    sd_bus_error err = SD_BUS_ERROR_NULL;
    sd_bus_message *reply = NULL;
    const char *result;
    int r;

    r = sd_bus_call(m->bus, args, 0, &err, &reply);
    if (r < 0)
    {
        LOG(LOG_LEVEL_ERROR, "wlup/mutter: %s.%s failed: %s", iface, method,
            err.message != NULL ? err.message : strerror(-r));
        sd_bus_error_free(&err);
        return 1;
    }
    r = sd_bus_message_read(reply, "o", &result);
    if (r >= 0)
    {
        *out = g_strdup(result);
    }
    sd_bus_message_unref(reply);
    return r < 0;
}

/******************************************************************************/
/* return error */
static int
setup_sessions(struct wlup *v)
{
    struct wlup_mutter *m = (struct wlup_mutter *)v->mutter;
    sd_bus_error err = SD_BUS_ERROR_NULL;
    sd_bus_message *msg = NULL;
    sd_bus_message *reply = NULL;
    char *session_id = NULL;
    uint64_t start;
    int fd;
    int r;

    /* 1. remote desktop session */
    sd_bus_message_new_method_call(m->bus, &msg, RD_NAME, RD_PATH, RD_NAME,
                                   "CreateSession");
    r = call_for_path(m, RD_NAME, RD_PATH, RD_NAME, "CreateSession",
                      &m->rd_session_path, msg);
    msg = sd_bus_message_unref(msg);
    if (r != 0)
    {
        return 1;
    }
    r = sd_bus_get_property_string(m->bus, RD_NAME, m->rd_session_path,
                                   RD_SESSION_IFACE, "SessionId", &err,
                                   &session_id);
    if (r < 0)
    {
        LOG(LOG_LEVEL_ERROR, "wlup/mutter: no SessionId: %s", err.message);
        sd_bus_error_free(&err);
        return 1;
    }

    /* 2. screen cast session tied to it */
    sd_bus_message_new_method_call(m->bus, &msg, SC_NAME, SC_PATH, SC_NAME,
                                   "CreateSession");
    sd_bus_message_append(msg, "a{sv}", 1,
                          "remote-desktop-session-id", "s", session_id);
    free(session_id);
    r = call_for_path(m, SC_NAME, SC_PATH, SC_NAME, "CreateSession",
                      &m->sc_session_path, msg);
    msg = sd_bus_message_unref(msg);
    if (r != 0)
    {
        return 1;
    }

    /* 3. a virtual monitor; cursor painted into the frames for now */
    sd_bus_message_new_method_call(m->bus, &msg, SC_NAME, m->sc_session_path,
                                   SC_SESSION_IFACE, "RecordVirtual");
    sd_bus_message_append(msg, "a{sv}", 2,
                          "cursor-mode", "u", (uint32_t)CURSOR_MODE_EMBEDDED,
                          "is-platform", "b", 1);
    r = call_for_path(m, SC_NAME, m->sc_session_path, SC_SESSION_IFACE,
                      "RecordVirtual", &m->stream_path, msg);
    msg = sd_bus_message_unref(msg);
    if (r != 0)
    {
        return 1;
    }
    sd_bus_match_signal(m->bus, &m->stream_added_slot, SC_NAME,
                        m->stream_path, SC_STREAM_IFACE,
                        "PipeWireStreamAdded", on_stream_added, v);
    sd_bus_match_signal(m->bus, &m->closed_slot, RD_NAME,
                        m->rd_session_path, RD_SESSION_IFACE, "Closed",
                        on_closed, v);

    /* 4. start, then wait for the PipeWire node */
    r = sd_bus_call_method(m->bus, RD_NAME, m->rd_session_path,
                           RD_SESSION_IFACE, "Start", &err, NULL, "");
    if (r < 0)
    {
        LOG(LOG_LEVEL_ERROR, "wlup/mutter: Start failed: %s", err.message);
        sd_bus_error_free(&err);
        return 1;
    }
    start = monotonic_usec();
    while (!m->have_node)
    {
        if (sd_bus_process(m->bus, NULL) > 0)
        {
            continue;
        }
        if (monotonic_usec() - start > STREAM_TIMEOUT_USEC)
        {
            LOG(LOG_LEVEL_ERROR, "wlup/mutter: no PipeWireStreamAdded");
            return 1;
        }
        sd_bus_wait(m->bus, 100 * 1000);
    }

    /* 5. input */
    r = sd_bus_call_method(m->bus, RD_NAME, m->rd_session_path,
                           RD_SESSION_IFACE, "ConnectToEIS", &err, &reply,
                           "a{sv}", 0);
    if (r < 0 || sd_bus_message_read(reply, "h", &fd) < 0)
    {
        LOG(LOG_LEVEL_WARNING, "wlup/mutter: ConnectToEIS failed, no input: "
            "%s", err.message != NULL ? err.message : "bad reply");
        sd_bus_error_free(&err);
    }
    else
    {
        /* The reply owns the fd */
        fd = fcntl(fd, F_DUPFD_CLOEXEC, 3);
        m->ei = ei_new_sender(v);
        ei_configure_name(m->ei, "xrdp");
        if (ei_setup_backend_fd(m->ei, fd) != 0)
        {
            LOG(LOG_LEVEL_WARNING, "wlup/mutter: libei setup failed");
            m->ei = ei_unref(m->ei);
        }
    }
    sd_bus_message_unref(reply);
    return 0;
}

/******************************************************************************/
/* Clipboard                                                                  */
/******************************************************************************/
static int
on_selection_owner_changed(sd_bus_message *msg, void *data, sd_bus_error *err)
{
    struct wlup *v = (struct wlup *)data;
    struct wlup_mutter *m = (struct wlup_mutter *)v->mutter;
    const char *key;
    const char *mime;
    const char *contents;
    const char *best = NULL;
    int best_index = -1;
    int session_is_owner = 0;
    int i;

    if (sd_bus_message_enter_container(msg, 'a', "{sv}") <= 0)
    {
        return 0;
    }
    while (sd_bus_message_enter_container(msg, 'e', "sv") > 0)
    {
        if (sd_bus_message_read(msg, "s", &key) < 0)
        {
            break;
        }
        if (strcmp(key, "mime-types") == 0 &&
                sd_bus_message_peek_type(msg, NULL, &contents) > 0 &&
                (strcmp(contents, "as") == 0 || strcmp(contents, "(as)") == 0))
        {
            /* Documented as "as"; GNOME Shell 50 sends "(as)" */
            int in_struct = (contents[0] == '(');
            sd_bus_message_enter_container(msg, 'v', contents);
            if (in_struct)
            {
                sd_bus_message_enter_container(msg, 'r', "as");
            }
            sd_bus_message_enter_container(msg, 'a', "s");
            while (sd_bus_message_read(msg, "s", &mime) > 0)
            {
                for (i = 0; g_text_mime_types[i] != NULL; ++i)
                {
                    if (strcmp(mime, g_text_mime_types[i]) == 0 &&
                            (best_index < 0 || i < best_index))
                    {
                        best_index = i;
                        best = g_text_mime_types[i];
                    }
                }
            }
            sd_bus_message_exit_container(msg);
            if (in_struct)
            {
                sd_bus_message_exit_container(msg);
            }
            sd_bus_message_exit_container(msg);
        }
        else if (strcmp(key, "session-is-owner") == 0)
        {
            sd_bus_message_read(msg, "v", "b", &session_is_owner);
        }
        else
        {
            sd_bus_message_skip(msg, "v");
        }
        sd_bus_message_exit_container(msg);
    }

    LOG(LOG_LEVEL_DEBUG, "wlup/mutter: selection owner changed, text %s, "
        "session is owner %d", best != NULL ? best : "none", session_is_owner);
    /* When we are the owner, the selection holds the client's text */
    m->read_mime = session_is_owner ? NULL : best;
    return 0;
}

/******************************************************************************/
static int
on_selection_transfer(sd_bus_message *msg, void *data, sd_bus_error *err)
{
    struct wlup *v = (struct wlup *)data;
    struct wlup_mutter *m = (struct wlup_mutter *)v->mutter;
    const char *mime;
    uint32_t serial;

    if (sd_bus_message_read(msg, "su", &mime, &serial) >= 0 &&
            m->num_transfers < MAX_TRANSFERS)
    {
        m->transfers[m->num_transfers++] = serial;
    }
    return 0;
}

/******************************************************************************/
/* Calls a method returning an fd; returns our own copy or -1 */
static int
call_for_fd(struct wlup_mutter *m, const char *method, const char *types,
            ...)
{
    sd_bus_error err = SD_BUS_ERROR_NULL;
    sd_bus_message *msg = NULL;
    sd_bus_message *reply = NULL;
    int fd = -1;
    va_list ap;

    sd_bus_message_new_method_call(m->bus, &msg, RD_NAME, m->rd_session_path,
                                   RD_SESSION_IFACE, method);
    va_start(ap, types);
    sd_bus_message_appendv(msg, types, ap);
    va_end(ap);
    if (sd_bus_call(m->bus, msg, 0, &err, &reply) >= 0 &&
            sd_bus_message_read(reply, "h", &fd) >= 0)
    {
        /* The reply owns the fd */
        fd = fcntl(fd, F_DUPFD_CLOEXEC, 3);
    }
    else
    {
        LOG(LOG_LEVEL_WARNING, "wlup/mutter: %s failed: %s", method,
            err.message != NULL ? err.message : "bad reply");
        fd = -1;
    }
    sd_bus_error_free(&err);
    sd_bus_message_unref(reply);
    sd_bus_message_unref(msg);
    return fd;
}

/******************************************************************************/
/* Work queued by the clipboard signals, done outside sd-bus callbacks */
static void
process_clipboard(struct wlup *v)
{
    struct wlup_mutter *m = (struct wlup_mutter *)v->mutter;
    int fd;
    int i;
    int done;
    int n;
    int ok;
    char *text;
    int len;

    if (m->read_mime != NULL)
    {
        fd = call_for_fd(m, "SelectionRead", "s", m->read_mime);
        m->read_mime = NULL;
        if (fd >= 0)
        {
            text = wlup_clip_read_fd(fd, &len);
            wlup_clip_session_text(v, text != NULL ? text : "", len);
            g_free(text);
        }
    }

    for (i = 0; i < m->num_transfers; ++i)
    {
        ok = 0;
        fd = call_for_fd(m, "SelectionWrite", "u", m->transfers[i]);
        if (fd >= 0)
        {
            fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) & ~O_NONBLOCK);
            for (done = 0; m->client_text != NULL &&
                    done < m->client_text_len; done += n)
            {
                n = write(fd, m->client_text + done, m->client_text_len - done);
                if (n <= 0 && errno != EINTR)
                {
                    break;
                }
                n = MAX(n, 0);
            }
            ok = (m->client_text != NULL && done == m->client_text_len);
            close(fd);
        }
        sd_bus_call_method(m->bus, RD_NAME, m->rd_session_path,
                           RD_SESSION_IFACE, "SelectionWriteDone", NULL, NULL,
                           "ub", m->transfers[i], ok);
    }
    m->num_transfers = 0;
}

/******************************************************************************/
void
wlup_mutter_set_clipboard_text(struct wlup *v, const char *text, int len)
{
    struct wlup_mutter *m = (struct wlup_mutter *)v->mutter;
    sd_bus_error err = SD_BUS_ERROR_NULL;

    if (m == NULL || !m->clipboard_enabled)
    {
        return;
    }
    g_free(m->client_text);
    m->client_text = (char *)g_malloc(len + 1, 0);
    memcpy(m->client_text, text, len);
    m->client_text_len = len;

    /* Mutter asks for the data with SelectionTransfer when it is pasted */
    if (sd_bus_call_method(m->bus, RD_NAME, m->rd_session_path,
                           RD_SESSION_IFACE, "SetSelection", &err, NULL,
                           "a{sv}", 1, "mime-types", "as", 2,
                           g_text_mime_types[0], g_text_mime_types[1]) < 0)
    {
        LOG(LOG_LEVEL_WARNING, "wlup/mutter: SetSelection failed: %s",
            err.message);
    }
    sd_bus_error_free(&err);
}

/******************************************************************************/
static void
enable_clipboard(struct wlup *v)
{
    struct wlup_mutter *m = (struct wlup_mutter *)v->mutter;
    sd_bus_error err = SD_BUS_ERROR_NULL;

    sd_bus_match_signal(m->bus, &m->owner_changed_slot, RD_NAME,
                        m->rd_session_path, RD_SESSION_IFACE,
                        "SelectionOwnerChanged", on_selection_owner_changed, v);
    sd_bus_match_signal(m->bus, &m->transfer_slot, RD_NAME,
                        m->rd_session_path, RD_SESSION_IFACE,
                        "SelectionTransfer", on_selection_transfer, v);
    if (sd_bus_call_method(m->bus, RD_NAME, m->rd_session_path,
                           RD_SESSION_IFACE, "EnableClipboard", &err, NULL,
                           "a{sv}", 0) < 0)
    {
        LOG(LOG_LEVEL_WARNING, "wlup/mutter: EnableClipboard failed: %s",
            err.message);
    }
    else
    {
        m->clipboard_enabled = 1;
        LOG(LOG_LEVEL_INFO, "wlup/mutter: clipboard enabled");
    }
    sd_bus_error_free(&err);
}

/******************************************************************************/
/* Module side                                                                */
/******************************************************************************/
/* Waits until GNOME Shell offers its remote desktop API.
 * return error */
static int
wait_for_mutter(struct wlup_mutter *m)
{
    uint64_t start = monotonic_usec();
    int logged = 0;
    int has_owner = 0;
    sd_bus_error err = SD_BUS_ERROR_NULL;
    sd_bus_message *reply = NULL;

    for (;;)
    {
        if (sd_bus_call_method(m->bus, "org.freedesktop.DBus",
                               "/org/freedesktop/DBus", "org.freedesktop.DBus",
                               "NameHasOwner", &err, &reply, "s",
                               RD_NAME) >= 0 &&
                sd_bus_message_read(reply, "b", &has_owner) >= 0 &&
                has_owner)
        {
            sd_bus_message_unref(reply);
            return 0;
        }
        reply = sd_bus_message_unref(reply);
        sd_bus_error_free(&err);
        if (monotonic_usec() - start > MUTTER_TIMEOUT_USEC)
        {
            LOG(LOG_LEVEL_ERROR, "wlup/mutter: %s did not appear on the "
                "session bus", RD_NAME);
            return 1;
        }
        if (!logged)
        {
            LOG(LOG_LEVEL_INFO, "wlup/mutter: waiting for GNOME Shell");
            logged = 1;
        }
        usleep(200 * 1000);
    }
}

/******************************************************************************/
int
wlup_mutter_connect(struct wlup *v, int bus_fd)
{
    struct wlup_mutter *m;
    char text[512];
    int r;

    m = (struct wlup_mutter *)g_malloc(sizeof(struct wlup_mutter), 1);
    v->mutter = m;

    if (bus_fd >= 0)
    {
        /* sesman connected to the session bus as the user. sd-bus
         * authenticates with EXTERNAL without claiming a uid, so the bus
         * takes the identity of the process that connected. sd-bus gets
         * a copy: xrdp closes bus_fd itself if we fail */
        int fd = fcntl(bus_fd, F_DUPFD_CLOEXEC, 3);
        r = sd_bus_new(&m->bus);
        if (r >= 0)
        {
            r = sd_bus_set_fd(m->bus, fd, fd);
        }
        if (r < 0)
        {
            close(fd);
        }
        if (r >= 0)
        {
            sd_bus_set_bus_client(m->bus, 1);
            r = sd_bus_start(m->bus);
        }
    }
    else if (v->dbus_address[0] != '\0')
    {
        sd_bus_new(&m->bus);
        sd_bus_set_address(m->bus, v->dbus_address);
        sd_bus_set_bus_client(m->bus, 1);
        r = sd_bus_start(m->bus);
    }
    else
    {
        r = sd_bus_open_user(&m->bus);
    }
    if (r < 0)
    {
        g_snprintf(text, sizeof(text), "wlup error - cannot connect to the "
                   "session bus: %s", strerror(-r));
        v->server_msg(v, text, 0);
        return 1;
    }

    if (wait_for_mutter(m) != 0)
    {
        v->server_msg(v, "wlup error - GNOME Shell does not offer its "
                      "remote desktop API", 0);
        return 1;
    }
    if (setup_sessions(v) != 0)
    {
        v->server_msg(v, "wlup error - cannot start a Mutter remote desktop "
                      "session", 0);
        return 1;
    }
    if (connect_stream(v) != 0)
    {
        v->server_msg(v, "wlup error - cannot receive the PipeWire stream",
                      0);
        return 1;
    }
    enable_clipboard(v);

    /* Success: the connection from sesman is ours to close */
    if (bus_fd >= 0)
    {
        close(bus_fd);
    }
    scancode_set_keycode_set("evdev");
    g_snprintf(text, sizeof(text), "wlup: connected to Mutter, PipeWire "
               "node %u%s", m->node_id, m->ei != NULL ? ", EIS input" : "");
    v->server_msg(v, text, 0);
    LOG(LOG_LEVEL_INFO, "%s", text);
    return 0;
}

/******************************************************************************/
void
wlup_mutter_disconnect(struct wlup *v)
{
    struct wlup_mutter *m = (struct wlup_mutter *)v->mutter;

    if (m == NULL)
    {
        return;
    }
    if (m->stream != NULL)
    {
        pw_stream_destroy(m->stream);
    }
    if (m->core != NULL)
    {
        pw_core_disconnect(m->core);
    }
    if (m->context != NULL)
    {
        pw_context_destroy(m->context);
    }
    if (m->loop != NULL)
    {
        pw_loop_destroy(m->loop);
    }
    if (m->keyboard != NULL)
    {
        ei_device_unref(m->keyboard);
    }
    if (m->pointer != NULL)
    {
        ei_device_unref(m->pointer);
    }
    if (m->ei != NULL)
    {
        ei_unref(m->ei);
    }
    if (m->bus != NULL)
    {
        if (m->rd_session_path != NULL && !m->closed)
        {
            sd_bus_call_method(m->bus, RD_NAME, m->rd_session_path,
                               RD_SESSION_IFACE, "Stop", NULL, NULL, "");
        }
        sd_bus_slot_unref(m->stream_added_slot);
        sd_bus_slot_unref(m->closed_slot);
        sd_bus_slot_unref(m->owner_changed_slot);
        sd_bus_slot_unref(m->transfer_slot);
        sd_bus_flush_close_unref(m->bus);
    }
    g_free(m->client_text);
    g_free(m->rd_session_path);
    g_free(m->sc_session_path);
    g_free(m->stream_path);
    g_free(m);
    v->mutter = NULL;
}

/******************************************************************************/
void
wlup_mutter_get_wait_objs(struct wlup *v, tbus *read_objs, int *rcount,
                          tbus *write_objs, int *wcount, int *timeout)
{
    struct wlup_mutter *m = (struct wlup_mutter *)v->mutter;

    if (m == NULL)
    {
        return;
    }
    if (m->bus != NULL)
    {
        read_objs[(*rcount)++] = sd_bus_get_fd(m->bus);
        if (sd_bus_get_events(m->bus) & POLLOUT)
        {
            write_objs[(*wcount)++] = sd_bus_get_fd(m->bus);
        }
    }
    if (m->loop != NULL)
    {
        read_objs[(*rcount)++] = pw_loop_get_fd(m->loop);
    }
    if (m->ei != NULL)
    {
        read_objs[(*rcount)++] = ei_get_fd(m->ei);
    }
    if (v->resize_pending && (*timeout < 0 || *timeout > 200))
    {
        /* Wake up to check the resize timeout */
        *timeout = 200;
    }
}

/******************************************************************************/
int
wlup_mutter_check_wait_objs(struct wlup *v)
{
    struct wlup_mutter *m = (struct wlup_mutter *)v->mutter;

    if (m == NULL)
    {
        return 0;
    }
    if (m->bus != NULL)
    {
        while (sd_bus_process(m->bus, NULL) > 0)
        {
        }
        process_clipboard(v);
    }
    if (m->loop != NULL)
    {
        pw_loop_enter(m->loop);
        pw_loop_iterate(m->loop, 0);
        pw_loop_leave(m->loop);
    }
    if (m->ei != NULL)
    {
        process_ei_events(v);
    }
    if (v->resize_pending &&
            monotonic_usec() - m->resize_start > RESIZE_TIMEOUT_USEC)
    {
        LOG(LOG_LEVEL_WARNING, "wlup/mutter: the virtual monitor was not "
            "resized, the picture will be clipped or padded");
        wlup_resize_finished(v);
    }
    return m->closed;
}
