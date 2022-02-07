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
 * libvnc - defines related to the RFB protocol
 */

#ifndef RFB_H
#define RFB_H

#include "arch.h"

/* Client-to-server messages */
enum c2s
{
    RFB_C2S_SET_PIXEL_FORMAT = 0,
    RFB_C2S_SET_ENCODINGS = 2,
    RFB_C2S_FRAMEBUFFER_UPDATE_REQUEST = 3,
    RFB_C2S_KEY_EVENT = 4,
    RFB_C2S_POINTER_EVENT = 5,
    RFB_C2S_CLIENT_CUT_TEXT = 6,
};

/* Server to client messages */
enum s2c
{
    RFB_S2C_FRAMEBUFFER_UPDATE = 0,
    RFB_S2C_SET_COLOUR_MAP_ENTRIES = 1,
    RFB_S2C_BELL = 2,
    RFB_S2C_SERVER_CUT_TEXT = 3
};

/* Encodings and pseudo-encodings
 *
 * The RFC uses a signed type for these. We use an unsigned type as the
 * binary representation for the negative values is standardised in C
 * (which it wouldn't be for an enum value)
 */

typedef uint32_t encoding_type;

#define RFB_ENC_RAW                   (encoding_type)0
#define RFB_ENC_COPY_RECT             (encoding_type)1
#define RFB_ENC_CURSOR                (encoding_type)-239
#define RFB_ENC_DESKTOP_SIZE          (encoding_type)-223
#define RFB_ENC_EXTENDED_DESKTOP_SIZE (encoding_type)-308

/**
 * Returns an error string for an ExtendedDesktopSize status code
 */
const char *
rfb_get_eds_status_msg(unsigned int response_code);

#endif /* RFB_H */
