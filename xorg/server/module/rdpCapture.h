/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Laxmikant Rashinkar 2014
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
 * Routines to copy regions from framebuffer to shared memory
 */

Bool
rdpCapture(rdpClientCon *clientCon,
           RegionPtr in_reg, BoxPtr *out_rects, int *num_out_rects,
           void *src, int src_width, int src_height,
           int src_stride, int src_format,
           void *dst, int dst_width, int dst_height,
           int dst_stride, int dst_format, int mode);

int
a8r8g8b8_to_a8b8g8r8_box(char *s8, int src_stride,
                         char *d8, int dst_stride,
                         int width, int height);

