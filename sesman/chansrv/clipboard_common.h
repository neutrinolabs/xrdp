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

/*
 * TODO : Replace these with the definitions in xrdp_constants.h
 * where possible */
#define CB_FORMAT_RAW                   0x0000
#define CB_FORMAT_TEXT                  0x0001
#define CB_FORMAT_DIB                   0x0008
#define CB_FORMAT_UNICODETEXT           0x000D
#define CB_FORMAT_HTML                  0xD010
#define CB_FORMAT_PNG                   0xD011
#define CB_FORMAT_JPEG                  0xD012
#define CB_FORMAT_GIF                   0xD013
#define CB_FORMAT_FILE                  0xC0BC

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
    int doing_response_ss; /* doing response short circuit */
    Time clip_time;
};

struct clip_file_desc /* CLIPRDR_FILEDESCRIPTOR */
{
    tui32 flags;
    tui32 fileAttributes;
    tui32 lastWriteTimeLow;
    tui32 lastWriteTimeHigh;
    tui32 fileSizeHigh;
    tui32 fileSizeLow;
    char cFileName[256];
};

int clipboard_out_unicode(struct stream *s, const char *text,
                          int num_chars);
int clipboard_in_unicode(struct stream *s, char *text, int *num_chars);

#endif
