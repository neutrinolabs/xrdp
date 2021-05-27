/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * MS-RDPECLIP : Definitions from [MS-RDPECLIP]
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
 * References to MS-RDPECLIP are currently correct for v220210407 of that
 * document
 */

#if !defined(MS_RDPECLIP_H)
#define MS_RDPECLIP_H

/* Clipboard PDU header message codes 2.2.1 */
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

#define CB_PDUTYPE_TO_STR(pdu_type) \
    ((pdu_type) == CB_MONITOR_READY ? "CB_MONITOR_READY" : \
     (pdu_type) == CB_FORMAT_LIST ? "CB_FORMAT_LIST" : \
     (pdu_type) == CB_FORMAT_LIST_RESPONSE ? "CB_FORMAT_LIST_RESPONSE" : \
     (pdu_type) == CB_FORMAT_DATA_REQUEST ? "CB_FORMAT_DATA_REQUEST" : \
     (pdu_type) == CB_FORMAT_DATA_RESPONSE ? "CB_FORMAT_DATA_RESPONSE" : \
     (pdu_type) == CB_TEMP_DIRECTORY ? "CB_TEMP_DIRECTORY" : \
     (pdu_type) == CB_CLIP_CAPS ? "CB_CLIP_CAPS" : \
     (pdu_type) == CB_FILECONTENTS_REQUEST ? "CB_FILECONTENTS_REQUEST" : \
     (pdu_type) == CB_FILECONTENTS_RESPONSE ? "CB_FILECONTENTS_RESPONSE" : \
     (pdu_type) == CB_LOCK_CLIPDATA ? "CB_LOCK_CLIPDATA" : \
     (pdu_type) == CB_UNLOCK_CLIPDATA ? "CB_UNLOCK_CLIPDATA" : \
     "unknown" \
    )

/* Clipboard PDU header message flags 2.2.1 */
#define CB_RESPONSE_OK 0x0001
#define CB_RESPONSE_FAIL 0x0002
#define CB_ASCII_NAMES 0x0004

/* Capability set codes 2.2.2.1.1 */
#define CB_CAPSTYPE_GENERAL 1
#define CB_CAPS_VERSION_1 1
#define CB_CAPS_VERSION_2 2

/* General capability set general flags 2.2.2.1.1.1 */
#define CB_USE_LONG_FORMAT_NAMES   0x00000002
#define CB_STREAM_FILECLIP_ENABLED 0x00000004
#define CB_FILECLIP_NO_FILE_PATHS  0x00000008
#define CB_CAN_LOCK_CLIPDATA       0x00000010

/* File contents request PDU 2.2.5.3 */
/* Note that in the document these do not have a CB_ prefix */
#define CB_FILECONTENTS_SIZE 0x00000001
#define CB_FILECONTENTS_RANGE 0x00000002

/* File descriptor structure flags 2.2.5.2.3.1 */
/* Note that in the document these do not have a CB_ prefix */
#define CB_FD_ATTRIBUTES 0x00000004
#define CB_FD_FILESIZE   0x00000040
#define CB_FD_WRITESTIME 0x00000020
#define CB_FD_PROGRESSUI 0x00004000

/* File descriptor structure file attributes 2.2.5.2.3.1 */
/* Note that in the document these do not have a CB_ prefix */
#define CB_FILE_ATTRIBUTE_READONLY  0x00000001
#define CB_FILE_ATTRIBUTE_HIDDEN    0x00000002
#define CB_FILE_ATTRIBUTE_SYSTEM    0x00000004
#define CB_FILE_ATTRIBUTE_DIRECTORY 0x00000010
#define CB_FILE_ATTRIBUTE_ARCHIVE   0x00000020
#define CB_FILE_ATTRIBUTE_NORMAL    0x00000080

#endif /* MS_RDPECLIP_H */
