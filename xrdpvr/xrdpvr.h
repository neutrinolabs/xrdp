/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Laxmikant Rashinkar 2012-2013 LK.Rashinkar@gmail.com
 * Copyright (C) Jay Sorg 2013 jay.sorg@gmail.com
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

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int  xrdpvr_init_player(void *channel, int stream_id, char *filename);
int  xrdpvr_deinit_player(void *channel, int stream_id);
int  xrdpvr_play_media(void *channel, int stream_id, char *filename);
int  xrdpvr_set_geometry(void *channel, int stream_id, int xpos, int ypos,
                         int width, int height);
int  xrdpvr_set_video_format(void *channel, uint32_t stream_id, int format,
                             int width, int height);
int  xrdpvr_set_audio_format(void *channel, uint32_t stream_id, int format,
                             char *extradata, int extradata_size,
                             int sample_rate, int bit_rate,
                             int channels, int block_align);
int  xrdpvr_send_video_data(void *channel, uint32_t stream_id,
                            uint32_t data_len, uint8_t *data);
int  xrdpvr_send_audio_data(void *channel, uint32_t stream_id,
                            uint32_t data_len, uint8_t *data);
int  xrdpvr_create_metadata_file(void *channel, char *filename);
int  xrdpvr_play_frame(void *channel, int stream_id, int *vdoTimeout,
                       int *audioTimeout);
void xrdpvr_get_media_duration(int64_t *start_time, int64_t *duration);
int  xrdpvr_seek_media(int64_t pos, int backward);
int  xrdpvr_get_frame(void **av_pkt_ret, int *is_video_frame,
                      int *delay_in_us);
int  send_audio_pkt(void *channel, int stream_id, void *pkt_p);
int  send_video_pkt(void *channel, int stream_id, void *pkt_p);
int  xrdpvr_set_volume(void *channel, int volume);
int  xrdpvr_send_init(void *channel);
int  xrdpvr_read_ack(void *channel, int *frame);

#ifdef __cplusplus
}
#endif

#endif /* __XRDPVR_H__ */
