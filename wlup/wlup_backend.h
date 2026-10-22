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
 * wlup: the interface between the module core and a compositor backend
 *
 * The core (wlup.c) is what xrdp sees: the module entry points, painting
 * and the GFX H.264 frames, the clipboard channel, keyboard scancodes,
 * and when frames are wanted at all (suppressed output, a full queue to
 * the client). A backend is what one compositor family needs: how to
 * connect, where frames come from, how input goes in and how the output
 * is resized.
 *
 *   wlroots (backends/wlroots): Wayland protocols, ext-image-copy-capture
 *   Mutter  (backends/mutter):  D-Bus, PipeWire, libei
 *
 * Frames go one way: a backend copies new pixels into the desktop image
 * (wlup.pixels, buffer_width x buffer_height, XRGB8888) and tells the core
 * which parts changed, with wlup_paint_region() or wlup_paint_damage().
 * The core never calls into a backend to get pixels.
 */

#ifndef WLUP_BACKEND_H
#define WLUP_BACKEND_H

#include "arch.h"

struct wlup;

struct wlup_backend_ops
{
    const char *name;

    /* fd: a connection to the session made by sesman (the Wayland socket
     * for wlroots, the session bus for Mutter), or -1 to connect from
     * the settings. Reports a failure with server_msg().
     * return error */
    int (*connect)(struct wlup *v, int fd);
    /* Releases everything connect() made; also after a failed connect */
    void (*disconnect)(struct wlup *v);

    /* Adds the backend's file descriptors, and may lower *timeout */
    void (*get_wait_objs)(struct wlup *v, tbus *read_objs, int *rcount,
                          tbus *write_objs, int *wcount, int *timeout);
    /* Handles what is ready: new frames reach the core from here.
     * return error, which ends the session */
    int (*check_wait_objs)(struct wlup *v);

    /* evdev_code: Linux input event code */
    void (*key)(struct wlup *v, int evdev_code, int down);
    /* msg: WM_MOUSEMOVE .. WM_BUTTON9DOWN, x and y on the desktop */
    void (*mouse)(struct wlup *v, int msg, int x, int y);

    /* Asks the compositor for an output of this size. return 1 if it is
     * under way: the backend then calls wlup_resize_finished() once the
     * desktop image has the new size; 0 if it cannot resize */
    int (*resize)(struct wlup *v, int width, int height);

    /* Optional, NULL if not needed. ---------------------------------- */

    /* The core's answer to wlup_wants_frames() may have changed: after a
     * frame ack, a resumed output, or output queued for the client being
     * sent. A backend that asks for each frame asks for the next one */
    void (*frames_wanted)(struct wlup *v);
    /* Whether the compositor may be writing to the desktop image right
     * now (a wl_shm buffer attached to a capture). The core then does not
     * read it, and uses the copy it keeps for the GFX encoder */
    int (*pixels_busy)(struct wlup *v);
    /* Sends what is buffered for the compositor after input */
    void (*flush)(struct wlup *v);
    /* The RDP client's clipboard now holds this UTF-8 text: make the
     * session's clipboard offer it */
    void (*set_clipboard_text)(struct wlup *v, const char *text, int len);
};

/* The backends that are built, NULL for one that is not */
extern const struct wlup_backend_ops wlup_backend_wlroots;
const struct wlup_backend_ops *
wlup_backend_mutter(void);

/* --- What the core offers backends ------------------------------------- */

/* Paints a rectangle of the desktop image to the RDP client, or adds it
 * to the next GFX H.264 frame. Call between server_begin_update() and
 * server_end_update(). return error */
int
wlup_paint_region(struct wlup *v, int x, int y, int cx, int cy);

/* Paints the damage collected in wlup.damage / num_damage /
 * damage_overflow (or all of it after need_full_paint), and sends a
 * GFX H.264 frame if the last one has been acknowledged. return error */
int
wlup_paint_damage(struct wlup *v);

/* Whether frames are wanted now: not while output is suppressed, and
 * not while xrdp still holds much of what wlup painted for the client */
int
wlup_wants_frames(struct wlup *v);

/* Tells xrdp the resize it is waiting for is complete */
void
wlup_resize_finished(struct wlup *v);

#endif
