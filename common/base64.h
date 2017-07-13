/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Koichiro Iwao 2017
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
 * Base64 encoder / decoder
 */

#if !defined(SSL_CALLS_H)
#define SSL_CALLS_H

#include "arch.h"

size_t
base64_decoded_bytes(const char *src);
char *
base64_decode(char *dst, const char *src, size_t len);

#endif
