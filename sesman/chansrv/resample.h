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
 * sample rate conversion of 16 bit stereo audio
 *
 */

#ifndef _RESAMPLE_H_
#define _RESAMPLE_H_

#include "arch.h"

struct resampler;

/* returns NULL if the rates are invalid or the ratio needs too many phases */
struct resampler *
resampler_create(int in_rate, int out_rate);
void
resampler_delete(struct resampler *rs);
/* forget the history, as for a new stream */
void
resampler_reset(struct resampler *rs);
/* converts in_frames stereo frames, keeps what it cannot use yet for the
   next call; returns the number of frames written to 'out' */
int
resampler_process(struct resampler *rs, const short *in, int in_frames,
                  short *out, int max_out_frames);

#endif
