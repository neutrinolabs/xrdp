/**
 * Copyright (C) 2022 Matt Burt, all xrdp contributors
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

/**
 * @file    libipm/libipm_private.h
 * @brief   Inter-Process Messaging building and parsing - private declarations
 */

#if !defined(LIBIPM_PRIVATE_H)
#define LIBIPM_PRIVATE_H

#include "libipm_facilities.h"

enum
{
    /**
     * Max size of a libipm message
     */
    LIBIPM_MAX_MSG_SIZE = 8192,

    /**
     * Version of libipm wire protocol
     */
    LIBIPM_VERSION = 2,

    /**
     * Size of libipm header
     */
    HEADER_SIZE = 12,

    /**
     * Max number of file descriptors in a message
     */
    MAX_FD_PER_MSG = 8
};

/**
 * Private class data stored in the trans 'extra_data' field
 */
struct libipm_priv
{
    enum libipm_facility facility;
    unsigned int flags;
    const char *(*msgno_to_str)(unsigned short msgno);
    unsigned short out_msgno;
    unsigned short out_param_count;
    unsigned short out_fd_count;
    int out_fds[MAX_FD_PER_MSG];
    unsigned short in_msgno;
    unsigned short in_param_count;
    /** Pointer to next fd to be consumed by the app */
    unsigned short in_fd_index;
    /** Number of fds in the incoming message */
    unsigned short in_fd_count;
    int in_fds[MAX_FD_PER_MSG];
};

/**
 * Valid type-system characters
 *
 * Contains a list of all the types accepted by libipm_msg_out_append() and
 * libipm_msg_in_parse()
 */
extern const char *libipm_valid_type_chars;

/**
 * Close input file descriptors for an input message
 *
 * If file descriptors are read from the other end, but not passed to the
 * application, they must be closed to prevent file descriptor leaks
 *
 * @param self Transport to close file descriptors for
 */
void
libipm_msg_in_close_file_descriptors(struct trans *self);

#endif /* LIBIPM__PRIVATE_H */
