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
 * libvnc - functions used by the VNC clipboard feature
 */

#ifndef VNC_CLIP_H
#define VNC_CLIP_H

struct vnc;
struct stream;

/**
 * Init the clip module private data structures
 */
void
vnc_clip_init(struct vnc *v);

/**
 * Deallocate the clip module private data structures
 */
void
vnc_clip_exit(struct vnc *v);

/**
 * Process incoming data from the RDP clip channel
 * @param v VNC Object
 * @param s Stream object containing data
 *
 * @return Non-zero if error occurs
 */
int
vnc_clip_process_channel_data(struct vnc *v, char *data, int size,
                              int total_size, int flags);

/**
 * Process incoming RFB protocol clipboard data
 * @param v VNC Object
 *
 * @return Non-zero if error occurs
 */
int
vnc_clip_process_rfb_data(struct vnc *v);

/**
 * Open the RDP clipboard channel
 *
 * The clip channel ID is written to the VNC object
 * *
 * @param v VNC Object
 * @return Non-zero if error occurs
 */
int
vnc_clip_open_clip_channel(struct vnc *v);

#endif /* VNC_CLIP_H */
