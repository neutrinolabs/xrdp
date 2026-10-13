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
 */

#ifndef WLUP_GFX_H
#define WLUP_GFX_H

struct wlup;
struct xrdp_client_info;

/* Chooses between the GFX H.264 encoder and server_paint_rect(), from
 * the capture mode xrdp picked for this client */
void
wlup_gfx_init(struct wlup *v, const struct xrdp_client_info *ci);

/* Adds a rectangle of the desktop to the next frame */
void
wlup_gfx_add_damage(struct wlup *v, int x, int y, int cx, int cy);

/* Copies a rectangle of the capture buffer to the frames' copy */
void
wlup_gfx_copy_damage(struct wlup *v, int x, int y, int cx, int cy);

/* Frees the frames' copy */
void
wlup_gfx_free_shadow(struct wlup *v);

/* Sends the collected damage as one frame, unless the frame before it
 * has not been acknowledged yet. return error */
int
wlup_gfx_poll(struct wlup *v);

/* xrdp has encoded frame_id and the client is not too far behind */
void
wlup_gfx_frame_ack(struct wlup *v, int frame_id);

/* Forgets the frame waiting for its ack: xrdp drops frames while it
 * resizes, without acknowledging them */
void
wlup_gfx_reset(struct wlup *v);

/* Lowers *timeout so a lost ack is noticed */
void
wlup_gfx_get_timeout(struct wlup *v, int *timeout);

#endif
