/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2009-2012
 * Copyright (C) Laxmikant Rashinkar 2012
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

#if !defined(CLIPBOARD_H)
#define CLIPBOARD_H

#include "arch.h"
#include "parse.h"

#define CB_FORMAT_LIST           2
#define CB_FORMAT_LIST_RESPONSE  3
#define CB_FORMAT_DATA_REQUEST   4
#define CB_FORMAT_DATA_RESPONSE  5

/* Clipboard Formats */

#define CB_FORMAT_RAW                   0x0000
#define CB_FORMAT_TEXT                  0x0001
#define CB_FORMAT_DIB                   0x0008
#define CB_FORMAT_UNICODETEXT           0x000D
#define CB_FORMAT_HTML                  0xD010
#define CB_FORMAT_PNG                   0xD011
#define CB_FORMAT_JPEG                  0xD012
#define CB_FORMAT_GIF                   0xD013

int APP_CC
clipboard_init(void);
int APP_CC
clipboard_deinit(void);
int APP_CC
clipboard_data_in(struct stream* s, int chan_id, int chan_flags, int length,
                  int total_length);
int APP_CC
clipboard_xevent(void* xevent);

#endif
