/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2012-2013
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

#if !defined(CLIPBOARD_COMMON_H)
#define CLIPBOARD_COMMON_H

#include "arch.h"
#include "parse.h"

/* these are the supported general types */
#define XRDP_CB_TEXT   1
#define XRDP_CB_BITMAP 2
#define XRDP_CB_FILE   3

struct clip_s2c /* server to client, pasting from linux app to mstsc */
{
    int incr_in_progress;
    int total_bytes;
    char *data;
    Atom type; /* UTF8_STRING, image/bmp, ... */
    Atom property; /* XRDP_CLIP_PROPERTY_ATOM, _QT_SELECTION, ... */
    int xrdp_clip_type; /* XRDP_CB_TEXT, XRDP_CB_BITMAP, XRDP_CB_FILE, ... */
    int converted;
    Time clip_time;
};

struct clip_c2s /* client to server, pasting from mstsc to linux app */
{
    int incr_in_progress;
    int incr_bytes_done;
    int read_bytes_done;
    int total_bytes;
    char *data;
    Atom type; /* UTF8_STRING, image/bmp, ... */
    Atom property; /* XRDP_CLIP_PROPERTY_ATOM, _QT_SELECTION, ... */
    Window window; /* Window used in INCR transfer */
    int xrdp_clip_type; /* XRDP_CB_TEXT, XRDP_CB_BITMAP, XRDP_CB_FILE, ... */
    int converted;
    int in_request; /* a data request has been sent to client */
};

struct clip_file_desc /* CLIPRDR_FILEDESCRIPTOR */
{
    tui32 flags;
    tui32 fileAttributes;
    tui32 lastWriteTimeLow;
    tui32 lastWriteTimeHigh;
    tui32 fileSizeHigh;
    tui32 fileSizeLow;
    char cFileName[260 * 4]; /* Allow each UCS-16 char to become 32 bits */
};

/**
 * Input a terminated UTF-16 string from a stream as UTF-8.
 * @param s stream
 * @param text UTF-8 String buffer
 * @param text_len Length of above
 * @return number of bytes copied from stream
 */
unsigned int
clipboard_in_utf16_le_as_utf8(struct stream *s, char *text,
                              unsigned int num_chars);

#endif
