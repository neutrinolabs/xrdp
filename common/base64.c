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

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "string_calls.h"

#include <openssl/bio.h>
#include <openssl/evp.h>

size_t
base64_decoded_bytes(const char *src)
{
    size_t len;
    size_t padding;

    len = g_strlen(src);
    padding = 0;

    if (src[len - 1] == '=')
    {
        padding++;

        if (src[len - 2] == '=')
        {
            padding++;
        }
    }

    return len * 3 / 4 - padding;
}

/*****************************************************************************/
char *
base64_decode(char *dst, const char *src, size_t len)
{
    BIO *b64;
    BIO *bio;
    char *b64str;
    size_t estimated_decoded_bytes;
    size_t decoded_bytes;

    b64str = g_strdup(src);
    estimated_decoded_bytes = base64_decoded_bytes(b64str);
    dst[estimated_decoded_bytes] = '\0';

    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new_mem_buf(b64str, len);
    bio = BIO_push(b64, bio);
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    decoded_bytes = BIO_read(bio, dst, len);
    BIO_free_all(bio);

    /* if input is corrupt, return empty string */
    if (estimated_decoded_bytes != decoded_bytes)
    {
        g_strncpy(dst, "", sizeof(""));
    }

    return dst;
}
