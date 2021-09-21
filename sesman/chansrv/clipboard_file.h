/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2012
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

#if !defined(CLIPBOARD_FILE_H)
#define CLIPBOARD_FILE_H

#include "arch.h"
#include "parse.h"

int
clipboard_send_data_response_for_file(const char *data, int data_size);
int
clipboard_process_file_request(struct stream *s, int clip_msg_status,
                               int clip_msg_len);
int
clipboard_process_file_response(struct stream *s, int clip_msg_status,
                                int clip_msg_len);
/**
 * Process a CLIPRDR_FILELIST - see [MS-RDPECLIP] 2.2.5.2.3
 *
 * Files in the list are added to the xfs filesystem in the clipboard
 * directory. The filenames names are added to the 'file_list' for the user.
 * Files are prefixed with '<fprefix><clipboard_dir>/' and separated by '\n'.
 *
 * If the list is not big enough, whole filenames are omitted, and a warning
 * message is logged. This is not an error.
 *
 * @param s Input stream containing CLIPRDR_FILELIST
 * @param file_list Output buffer for filenames
 * @param file_list_size Size of buffer, including space for '\0'.
 * @param fprefix Prefix for each file in the file list (e.g. "file://")
 *
 * @return Zero for success.
 */
int
clipboard_c2s_in_files(struct stream *s, char *file_list, int file_list_size,
                       const char *fprefix);

int
clipboard_request_file_size(int stream_id, int lindex);

int
clipboard_request_file_data(int stream_id, int lindex, int offset,
                            int request_bytes);

#endif
