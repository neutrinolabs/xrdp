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
 * GNOME does not offer the wlroots capture and virtual input protocols
 * to Wayland clients. The same functions are reached through Mutter's
 * D-Bus APIs on the session bus instead:
 *   org.gnome.Mutter.RemoteDesktop  session, input through libei (EIS)
 *   org.gnome.Mutter.ScreenCast     a virtual monitor, frames through
 *                                   a PipeWire stream
 */

#ifndef WLUP_MUTTER_H
#define WLUP_MUTTER_H

#include "arch.h"

struct wlup;

/* bus_fd: connection to the session bus made for us by sesman, or -1
 * to connect ourselves. return error */
int
wlup_mutter_connect(struct wlup *v, int bus_fd);

void
wlup_mutter_disconnect(struct wlup *v);

void
wlup_mutter_get_wait_objs(struct wlup *v, tbus *read_objs, int *rcount,
                          tbus *write_objs, int *wcount, int *timeout);

/* return error */
int
wlup_mutter_check_wait_objs(struct wlup *v);

/* Asks Mutter to resize the virtual monitor. On success, the resize is
 * finished when the new stream format arrives.
 * return 1 if the request was made, 0 if not */
int
wlup_mutter_resize(struct wlup *v, int width, int height);

/* evdev_code: Linux input event code of the key */
void
wlup_mutter_key(struct wlup *v, int evdev_code, int down);

/* msg: WM_MOUSEMOVE .. WM_BUTTON9DOWN */
void
wlup_mutter_mouse(struct wlup *v, int msg, int x, int y);

#endif
