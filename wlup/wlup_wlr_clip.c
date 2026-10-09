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
 * wlup: session side of the text clipboard for wlroots compositors,
 * through ext-data-control-v1
 *
 * Session to client: the data device announces each new selection as an
 * offer. Its text is read through a pipe and given to wlup_clip.
 * Client to session: a data source offering the client's text becomes
 * the selection, and writes the text when a client of the compositor
 * pastes it.
 *
 * Reading the selection is deferred until the Wayland events have been
 * dispatched, and skipped while our own source is the selection: the
 * compositor would ask us to write to the pipe we are waiting on.
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <wayland-client.h>

#include "ext-data-control-v1-client-protocol.h"

#include "wlup.h"
#include "wlup_clip.h"
#include "log.h"
#include "os_calls.h"

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

/* What an offer has, from its offer events */
struct offer_info
{
    int mime_index; /* best index in g_text_mime_types, or -1 */
};

struct wlup_wlr_clip
{
    struct ext_data_control_manager_v1 *manager;
    struct ext_data_control_device_v1 *device;
    struct ext_data_control_offer_v1 *pending; /* selection to read */
    struct ext_data_control_source_v1 *source; /* ours, or NULL */
    char *text;                                /* the client's text */
    int text_len;
};

/******************************************************************************/
static void
destroy_offer(struct ext_data_control_offer_v1 *offer)
{
    if (offer != NULL)
    {
        g_free(ext_data_control_offer_v1_get_user_data(offer));
        ext_data_control_offer_v1_destroy(offer);
    }
}

/******************************************************************************/
static void
offer_offer(void *data, struct ext_data_control_offer_v1 *offer,
            const char *mime_type)
{
    struct offer_info *info = (struct offer_info *)data;
    int i;

    for (i = 0; g_text_mime_types[i] != NULL; ++i)
    {
        if (strcmp(mime_type, g_text_mime_types[i]) == 0 &&
                (info->mime_index < 0 || i < info->mime_index))
        {
            info->mime_index = i;
        }
    }
}

static const struct ext_data_control_offer_v1_listener offer_listener =
{
    .offer = offer_offer,
};

/******************************************************************************/
static void
device_data_offer(void *data, struct ext_data_control_device_v1 *device,
                  struct ext_data_control_offer_v1 *offer)
{
    struct offer_info *info;

    info = (struct offer_info *)g_malloc(sizeof(*info), 1);
    info->mime_index = -1;
    ext_data_control_offer_v1_add_listener(offer, &offer_listener, info);
}

/******************************************************************************/
static void
device_selection(void *data, struct ext_data_control_device_v1 *device,
                 struct ext_data_control_offer_v1 *offer)
{
    struct wlup *v = (struct wlup *)data;
    struct wlup_wlr_clip *c = (struct wlup_wlr_clip *)v->wlr_clip;

    destroy_offer(c->pending);
    c->pending = offer;
}

/******************************************************************************/
static void
device_finished(void *data, struct ext_data_control_device_v1 *device)
{
    struct wlup *v = (struct wlup *)data;
    struct wlup_wlr_clip *c = (struct wlup_wlr_clip *)v->wlr_clip;

    LOG(LOG_LEVEL_WARNING, "wlup: clipboard data device finished");
    ext_data_control_device_v1_destroy(c->device);
    c->device = NULL;
}

/******************************************************************************/
static void
device_primary_selection(void *data, struct ext_data_control_device_v1 *device,
                         struct ext_data_control_offer_v1 *offer)
{
    /* RDP has no primary selection */
    destroy_offer(offer);
}

static const struct ext_data_control_device_v1_listener device_listener =
{
    .data_offer = device_data_offer,
    .selection = device_selection,
    .finished = device_finished,
    .primary_selection = device_primary_selection,
};

/******************************************************************************/
static void
source_send(void *data, struct ext_data_control_source_v1 *source,
            const char *mime_type, int32_t fd)
{
    struct wlup *v = (struct wlup *)data;
    struct wlup_wlr_clip *c = (struct wlup_wlr_clip *)v->wlr_clip;
    int done = 0;
    int n;

    /* A paste in the session: write the client's text */
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) & ~O_NONBLOCK);
    while (c->text != NULL && done < c->text_len)
    {
        n = write(fd, c->text + done, c->text_len - done);
        if (n < 0 && errno == EINTR)
        {
            continue;
        }
        if (n <= 0)
        {
            break;
        }
        done += n;
    }
    close(fd);
}

/******************************************************************************/
static void
source_cancelled(void *data, struct ext_data_control_source_v1 *source)
{
    struct wlup *v = (struct wlup *)data;
    struct wlup_wlr_clip *c = (struct wlup_wlr_clip *)v->wlr_clip;

    /* Something else in the session took the selection */
    ext_data_control_source_v1_destroy(source);
    if (c->source == source)
    {
        c->source = NULL;
    }
}

static const struct ext_data_control_source_v1_listener source_listener =
{
    .send = source_send,
    .cancelled = source_cancelled,
};

/******************************************************************************/
void
wlup_wlr_set_clipboard_text(struct wlup *v, const char *text, int len)
{
    struct wlup_wlr_clip *c = (struct wlup_wlr_clip *)v->wlr_clip;
    int i;

    if (c == NULL || c->device == NULL)
    {
        return;
    }
    g_free(c->text);
    c->text = (char *)g_malloc(len + 1, 0);
    memcpy(c->text, text, len);
    c->text_len = len;

    if (c->source != NULL)
    {
        ext_data_control_source_v1_destroy(c->source);
    }
    c->source = ext_data_control_manager_v1_create_data_source(c->manager);
    ext_data_control_source_v1_add_listener(c->source, &source_listener, v);
    for (i = 0; g_text_mime_types[i] != NULL; ++i)
    {
        ext_data_control_source_v1_offer(c->source, g_text_mime_types[i]);
    }
    ext_data_control_device_v1_set_selection(c->device, c->source);
    wl_display_flush(v->display);
}

/******************************************************************************/
/* Reads the text of an offer. Returns a g_malloc'd buffer or NULL */
static char *
read_offer(struct wlup *v, struct ext_data_control_offer_v1 *offer,
           const char *mime_type, int *len)
{
    int fds[2];

    *len = 0;
    if (pipe2(fds, O_CLOEXEC) != 0)
    {
        return NULL;
    }
    ext_data_control_offer_v1_receive(offer, mime_type, fds[1]);
    close(fds[1]);
    wl_display_flush(v->display);
    return wlup_clip_read_fd(fds[0], len);
}

/******************************************************************************/
void
wlup_wlr_clip_process(struct wlup *v)
{
    struct wlup_wlr_clip *c = (struct wlup_wlr_clip *)v->wlr_clip;
    struct ext_data_control_offer_v1 *offer;
    struct offer_info *info;
    char *text;
    int len = 0;

    if (c == NULL || c->pending == NULL)
    {
        return;
    }
    offer = c->pending;
    c->pending = NULL;
    info = (struct offer_info *)ext_data_control_offer_v1_get_user_data(offer);

    /* While our own source is the selection, the offer is our own text */
    if (c->source == NULL && info != NULL && info->mime_index >= 0)
    {
        text = read_offer(v, offer, g_text_mime_types[info->mime_index], &len);
        if (text != NULL || len == 0)
        {
            wlup_clip_session_text(v, text != NULL ? text : "", len);
        }
        g_free(text);
    }
    destroy_offer(offer);
}

/******************************************************************************/
int
wlup_wlr_clip_bind(struct wlup *v, struct wl_registry *registry,
                   unsigned int name, const char *interface)
{
    struct wlup_wlr_clip *c;

    if (strcmp(interface, ext_data_control_manager_v1_interface.name) != 0)
    {
        return 0;
    }
    c = (struct wlup_wlr_clip *)g_malloc(sizeof(*c), 1);
    c->manager = wl_registry_bind(registry, name,
                                  &ext_data_control_manager_v1_interface, 1);
    v->wlr_clip = c;
    return 1;
}

/******************************************************************************/
void
wlup_wlr_clip_start(struct wlup *v)
{
    struct wlup_wlr_clip *c = (struct wlup_wlr_clip *)v->wlr_clip;

    if (c == NULL || v->seat == NULL)
    {
        LOG(LOG_LEVEL_INFO, "wlup: no ext-data-control-v1, no clipboard");
        return;
    }
    c->device = ext_data_control_manager_v1_get_data_device(c->manager,
                v->seat);
    ext_data_control_device_v1_add_listener(c->device, &device_listener, v);
}

/******************************************************************************/
void
wlup_wlr_clip_destroy(struct wlup *v)
{
    struct wlup_wlr_clip *c = (struct wlup_wlr_clip *)v->wlr_clip;

    if (c == NULL)
    {
        return;
    }
    destroy_offer(c->pending);
    if (c->source != NULL)
    {
        ext_data_control_source_v1_destroy(c->source);
    }
    if (c->device != NULL)
    {
        ext_data_control_device_v1_destroy(c->device);
    }
    if (c->manager != NULL)
    {
        ext_data_control_manager_v1_destroy(c->manager);
    }
    g_free(c->text);
    g_free(c);
    v->wlr_clip = NULL;
}
