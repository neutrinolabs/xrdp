/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) 2026
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
 * FFmpeg H.264 encoder
 */

#ifndef _XRDP_ENCODER_FFMPEG_H
#define _XRDP_ENCODER_FFMPEG_H

#include "arch.h"

struct xrdp_encoder_dmabuf_surface;

int
xrdp_encoder_ffmpeg_install_ok(void);
void *
xrdp_encoder_ffmpeg_create(void);
int
xrdp_encoder_ffmpeg_delete(void *handle);
int
xrdp_encoder_ffmpeg_encode(void *handle, int session, int left, int top,
                           int width, int height, int twidth, int theight,
                           int format, const char *data,
                           short *crects, int num_crects,
                           char *cdata, int *cdata_bytes,
                           int connection_type, int *flags_ptr);
int
xrdp_encoder_ffmpeg_encode_dmabuf(
    void *handle,
    const struct xrdp_encoder_dmabuf_surface *surface,
    int width, int height, int format,
    short *crects, int num_crects,
    int connection_type, int keyframe,
    char *cdata, int *cdata_bytes,
    int *flags_ptr);

#endif
