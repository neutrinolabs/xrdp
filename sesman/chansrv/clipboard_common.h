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

#define CB_CAPSTYPE_GENERAL 1

#define CB_MONITOR_READY         1
#define CB_FORMAT_LIST           2
#define CB_FORMAT_LIST_RESPONSE  3
#define CB_FORMAT_DATA_REQUEST   4
#define CB_FORMAT_DATA_RESPONSE  5
#define CB_TEMP_DIRECTORY        6
#define CB_CLIP_CAPS             7
#define CB_FILECONTENTS_REQUEST  8
#define CB_FILECONTENTS_RESPONSE 9
#define CB_LOCK_CLIPDATA         10
#define CB_UNLOCK_CLIPDATA       11

#define CB_USE_LONG_FORMAT_NAMES   0x00000002
#define CB_STREAM_FILECLIP_ENABLED 0x00000004
#define CB_FILECLIP_NO_FILE_PATHS  0x00000008
#define CB_CAN_LOCK_CLIPDATA       0x00000010

#define CB_FILECONTENTS_SIZE 0x00000001
#define CB_FILECONTENTS_RANGE 0x00000002

#define CB_FORMAT_RAW                   0x0000
#define CB_FORMAT_TEXT                  0x0001
#define CB_FORMAT_DIB                   0x0008
#define CB_FORMAT_UNICODETEXT           0x000D
#define CB_FORMAT_HTML                  0xD010
#define CB_FORMAT_PNG                   0xD011
#define CB_FORMAT_JPEG                  0xD012
#define CB_FORMAT_GIF                   0xD013
#define CB_FORMAT_FILE                  0xC0BC

/* Used by the Format List Response PDU, Format Data Response PDU, and File
   Contents Response PDU to indicate that the associated request Format List
   PDU, Format Data Request PDU, and File Contents Request PDU were processed
   successfully. */
#define CB_RESPONSE_OK 0x0001

/* Used by the Format List Response PDU, Format Data Response PDU, and File
   Contents Response PDU to indicate that the associated Format List PDU,
   Format Data Request PDU, and File Contents Request PDU were not processed
   successfully. */
#define CB_RESPONSE_FAIL 0x0002

/* Used by the Short Format Name variant of the Format List Response PDU to
   indicate the format names are in ASCII 8. */
#define CB_ASCII_NAMES 0x0004

/* these are the supported general types */
#define XRDP_CB_TEXT   1
#define XRDP_CB_BITMAP 2
#define XRDP_CB_FILE   3

#define CB_FD_ATTRIBUTES 0x00000004
#define CB_FD_FILESIZE   0x00000040
#define CB_FD_WRITESTIME 0x00000020
#define CB_FD_PROGRESSUI 0x00004000

#define CB_FILE_ATTRIBUTE_READONLY  0x00000001
#define CB_FILE_ATTRIBUTE_HIDDEN    0x00000002
#define CB_FILE_ATTRIBUTE_SYSTEM    0x00000004
#define CB_FILE_ATTRIBUTE_DIRECTORY 0x00000010
#define CB_FILE_ATTRIBUTE_ARCHIVE   0x00000020
#define CB_FILE_ATTRIBUTE_NORMAL    0x00000080

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
