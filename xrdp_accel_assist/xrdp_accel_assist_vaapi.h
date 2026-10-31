/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Tim Pletcher 2026
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
 */

#ifndef _XRDP_ACCEL_ASSIST_VAAPI_H
#define _XRDP_ACCEL_ASSIST_VAAPI_H

int
xrdp_accel_assist_vaapi_init(void);
int
xrdp_accel_assist_vaapi_create_encoder(int width, int height, int tex,
                                       int tex_format, struct enc_info **ei);
int
xrdp_accel_assist_vaapi_delete_encoder(struct enc_info *ei);
int
xrdp_accel_assist_vaapi_get_planes(struct enc_info *ei, int *y_tex,
                                   int *uv_tex);
enum encoder_result
xrdp_accel_assist_vaapi_encode(struct enc_info *ei, int tex,
                               void *cdata, int *cdata_bytes,
                               int flags);

#endif
