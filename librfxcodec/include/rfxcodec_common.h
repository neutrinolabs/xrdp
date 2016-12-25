/**
 * RFX codec
 *
 * Copyright 2015 Jay Sorg <jay.sorg@gmail.com>
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

#ifndef __RFXCODEC_COMMON_H
#define __RFXCODEC_COMMON_H

#define RFX_FORMAT_BGRA 0
#define RFX_FORMAT_RGBA 1
#define RFX_FORMAT_BGR  2
#define RFX_FORMAT_RGB  3
#define RFX_FORMAT_YUV  4 /* YUV444 linear tiled mode */

#define RFX_FLAGS_NONE  0 /* default RFX_FLAGS_RLGR3 and RFX_FLAGS_SAFE */

#define RFX_FLAGS_SAFE     0 /* default */
#define RFX_FLAGS_OPT1    (1 << 3)
#define RFX_FLAGS_OPT2    (1 << 4)
#define RFX_FLAGS_NOACCEL (1 << 6)

#define RFX_FLAGS_RLGR3 0 /* default */
#define RFX_FLAGS_RLGR1 1

#define RFX_FLAGS_ALPHAV1 1 /* used in flags for rfxcodec_encode */

#endif
