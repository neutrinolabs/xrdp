/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) 2021 Matt Burt
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
 * Enforce stream primitive checking
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <stdlib.h>

#include "arch.h"
#include "parse.h"
#include "log.h"

void
parser_stream_overflow_check(const struct stream *s, int n, int is_out,
                             const char *file, int line)
{
    /* Sanity checks */
    if (n < 0)
    {
        LOG(LOG_LEVEL_ALWAYS, "%s:%d "
            "stream primitive called with negative n=%d",
            file, line, n);
        abort();
    }

    if (is_out)
    {
        /* Output overflow */
        if (!s_check_rem_out(s, n))
        {
            LOG(LOG_LEVEL_ALWAYS, "%s:%d Stream output buffer overflow. "
                "Size=%d, pos=%d, requested=%d", file, line,
                s->size, (int)(s->p - s->data), n);
            abort();
        }
    }
    else
    {
        /* Input overflow */
        if (!s_check_rem(s, n))
        {
            LOG(LOG_LEVEL_ALWAYS, "%s:%d Stream input buffer overflow. "
                "Max=%d, pos=%d, requested=%d", file, line,
                (int)(s->end - s->data), (int)(s->p - s->data), n);
            abort();
        }
    }
}
