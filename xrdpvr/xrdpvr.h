/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Laxmikant Rashinkar 2012 LK.Rashinkar@gmail.com
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
 * a program that uses xrdpapi and ffmpeg to redirect media streams
 * to a FreeRDP client where it is decompressed and played locally
 *
 */

#ifndef __XRDPVR_H__
#define __XRDPVR_H__

#ifdef __cplusplus
extern "C" {
#endif

int xrdpvr_init_player(void *channel, int stream_id, char *filename);
int xrdpvr_deinit_player(void *channel, int stream_id);
int xrdpvr_play_media(void *channel, int stream_id, char *filename);

#ifdef __cplusplus
}
#endif

#endif /* __XRDPVR_H__ */
