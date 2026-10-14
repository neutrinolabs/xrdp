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
 * wlup wlroots backend: state shared by its files, not seen by the core
 */

#ifndef WLUP_WLROOTS_H
#define WLUP_WLROOTS_H

#include <stdint.h>

struct wl_display;
struct wl_registry;
struct wl_shm;
struct wl_seat;
struct wl_output;
struct wl_buffer;
struct ext_output_image_capture_source_manager_v1;
struct ext_image_copy_capture_manager_v1;
struct ext_image_capture_source_v1;
struct ext_image_copy_capture_session_v1;
struct ext_image_copy_capture_frame_v1;
struct zwlr_virtual_pointer_manager_v1;
struct zwlr_virtual_pointer_v1;
struct zwp_virtual_keyboard_manager_v1;
struct zwp_virtual_keyboard_v1;
struct xkb_keymap;
struct zwlr_output_manager_v1;
struct zwlr_output_head_v1;
struct xkb_state;

/* Largest number of compositor outputs (heads) tracked for resizing */
#define WLUP_MAX_HEADS 8

/* An output as seen through wlr-output-management */
struct wlup_head
{
    struct zwlr_output_head_v1 *head;
    char name[64];
    int enabled;
};


struct wlroots
{
    void *wlr_clip;         /* session clipboard (clip.c) */
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_shm *shm;
    struct wl_seat *seat;
    struct wl_output *output;
    struct ext_output_image_capture_source_manager_v1 *source_manager;
    struct ext_image_copy_capture_manager_v1 *copy_manager;
    struct zwlr_virtual_pointer_manager_v1 *pointer_manager;
    uint32_t pointer_manager_version;
    struct zwp_virtual_keyboard_manager_v1 *keyboard_manager;
    char output_name[64]; /* wl_output name of the captured output */
    struct zwlr_output_manager_v1 *output_manager;
    uint32_t output_manager_serial;
    struct wlup_head heads[WLUP_MAX_HEADS];
    int num_heads;
    struct ext_image_capture_source_v1 *source;
    struct ext_image_copy_capture_session_v1 *session;
    struct ext_image_copy_capture_frame_v1 *frame;
    uint32_t constraint_width; /* buffer constraints being received */
    uint32_t constraint_height;
    int constraint_xrgb8888; /* compositor offers XRGB8888 or ARGB8888 */
    uint32_t constraint_format;
    int constraints_done;
    struct wl_buffer *buffer;
    int buffer_format;
    struct zwlr_virtual_pointer_v1 *pointer;
    struct zwp_virtual_keyboard_v1 *keyboard;
    struct xkb_keymap *xkb_keymap; /* the keymap the compositor was given */
    struct xkb_state *xkb_state;   /* to compute the modifier state */
};

#define WLR(v) ((struct wlroots *)(v)->backend_data)

struct wlup;

/* Session clipboard (clip.c), through ext-data-control-v1 -------------- */

/* Binds ext-data-control-v1 if interface is it. return 1 if bound */
int
wlup_wlr_clip_bind(struct wlup *v, struct wl_registry *registry,
                   unsigned int name, const char *interface);

/* Starts following the selection, once the seat is known */
void
wlup_wlr_clip_start(struct wlup *v);

/* Reads a new session selection; call after dispatching events */
void
wlup_wlr_clip_process(struct wlup *v);

void
wlup_wlr_clip_destroy(struct wlup *v);

/* The RDP client's clipboard now holds this UTF-8 text: make the
 * session's clipboard offer it */
void
wlup_wlr_set_clipboard_text(struct wlup *v, const char *text, int len);

#endif
