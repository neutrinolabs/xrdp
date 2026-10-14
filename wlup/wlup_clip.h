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
 * wlup: text clipboard over the RDP clipboard channel ([MS-RDPECLIP])
 *
 * Wayland and GNOME sessions have no chansrv clipboard, which works with
 * X11 selections, so wlup speaks to the clipboard channel itself, as the
 * VNC module does. The session side uses UTF-8 text and is provided by
 * the backend:
 *   wlroots  ext-data-control-v1
 *   Mutter   RemoteDesktop.Session clipboard methods
 */

#ifndef WLUP_CLIP_H
#define WLUP_CLIP_H

struct wlup;

void
wlup_clip_init(struct wlup *v);

void
wlup_clip_exit(struct wlup *v);

/* Opens the clipboard channel, once connected to the session.
 * return error */
int
wlup_clip_open_channel(struct wlup *v);

/* A virtual channel PDU from the RDP client. return error */
int
wlup_clip_process_channel_data(struct wlup *v, char *data, int size,
                               int total_size, int flags);

/* Reads clipboard data from fd until its writer closes it, or for at
 * most 2 seconds, and closes fd. Returns a g_malloc'd buffer (NULL if
 * empty) and its length in *len */
char *
wlup_clip_read_fd(int fd, int *len);

/* The session's clipboard now holds this UTF-8 text (len bytes, no
 * terminator needed). The RDP client is told if it changed */
void
wlup_clip_session_text(struct wlup *v, const char *text, int len);

/* Implemented by the backends: the RDP client's clipboard now holds
 * this UTF-8 text, make the session's clipboard offer it */
void
wlup_wlr_set_clipboard_text(struct wlup *v, const char *text, int len);
void
wlup_mutter_set_clipboard_text(struct wlup *v, const char *text, int len);

/* wlroots backend (wlup_wlr_clip.c) */
struct wl_registry;

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

#endif
