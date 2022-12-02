/**
 * Copyright (C) 2021 Koichiro Iwao, all xrdp contributors
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
 * @file    common/base64.h
 * @brief   Base64 encoder / decoder
 *
 * Base-64 is described in RFC4648. The following notes apply to this
 * implementation:-
 * - The only supported characters are [A-Za-z0-9+/=]. At present,
 *   embedded linefeeds and URL encodings are not supported.
 *
 */

#if !defined(BASE64_CALLS_H)
#define BASE64_CALLS_H

#include "arch.h"

/*
 * Decodes a base64 string
 *
 * @param src Pointer to null-terminated source
 * @param dst Pointer to output buffer
 * @param dst_len Length of above. No more than this is written to the
 *                output buffer
 * @param actual_len Pointer to value to receive actual length of decoded data
 * @return 0 for success, 1 for an invalid input string
 *
 * The following notes apply to this implementation:-
 * - Embedded padding is supported, provided it only occurs at the end
 *   of a quantum as described in RFC4648(4). This allows concatenated
 *   encodings to be fed into the decoder.
 * - Padding of the last quantum is assumed if not provided.
 * - Excess padding of the last quantum is ignored.
 *
 * Only dst_len bytes at most are written to the output. The length
 * returned in actual_len however represents how much buffer is needed for
 * a correct result. This may be more than dst_len, and enables the caller
 * to detect a potential buffer overflow
 */
int
base64_decode(const char *src, char *dst, size_t dst_len, size_t *actual_len);

/*
 * Encodes a buffer as base64
 *
 * @param src Pointer to source
 * @param src_len Length of above.
 * @param dst Pointer to output buffer for null-terminated string
 * @param dst_len Length of above. No more than this is written to the
 *                output buffer
 * @return Number of source characters processed
 *
 * The following notes apply to this implementation:-
 * - Padding of the last quantum is always written if the number of
 *   source bytes is not divisible by 3.
 *
 * The number of source characters processed is always returned. If
 * the destination buffer is too small for all the output plus a
 * terminator, the returned value from this procedure will be less than
 * src_len. In this case no padding characters will have been written,
 * and the remaining characters can be converted with more calls to
 * this procedure.
 */
size_t
base64_encode(const char *src, size_t src_len, char *dst, size_t dst_len);

#endif /* BASE64_CALLS_H */
