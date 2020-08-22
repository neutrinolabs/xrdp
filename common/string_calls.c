/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2020
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
 * generic string handling calls
 */

#include <string.h>
#include <strings.h>
#include <stdlib.h>

#include "string_calls.h"

unsigned int
g_format_info_string(char* dest, unsigned int len,
                      const char* format,
                      const struct info_string_tag map[])
{
    unsigned int result = 0;
    const char *copy_from;  /* Data to add to output */
    unsigned int copy_len;  /* Length of above */
    unsigned int skip;      /* Date to skip over in format string */
    const char *p;
    const struct info_string_tag *m;

    for ( ; *format != '\0'; format += skip)
    {
        if (*format == '%')
        {
            char ch= *(format + 1);
            if (ch== '%')
            {
                /* '%%' in format - replace with single '%' */
                copy_from = format;
                copy_len= 1;
                skip = 2;
            }
            else if (ch== '\0')
            {
                /* Percent at end of string - ignore */
                copy_from = NULL;
                copy_len= 0;
                skip = 1;
            }
            else
            {
                /* Look up the character in the map, assuming failure */
                copy_from = NULL;
                copy_len= 0;
                skip = 2;

                for (m = map ; m->ch != '\0' ; ++m)
                {
                    if (ch == m->ch)
                    {
                        copy_from = m->val;
                        copy_len = strlen(copy_from);
                        break;
                    }
                }
            }
        }
        else if ((p = strchr(format, '%')) != NULL)
        {
            /* Copy up to the next '%' */
            copy_from = format;
            copy_len = p - format;
            skip = copy_len;
        }
        else
        {
            /* Copy the rest of the format string */
            copy_from = format;
            copy_len = strlen(format);
            skip = copy_len;
        }

        /* Update the result before any truncation */
        result += copy_len;

        /* Do we have room in the output buffer for any more data? We
         * must always write a terminator if possible */
        if (len > 1)
        {
            if (copy_len > (len - 1))
            {
                copy_len = len - 1;
            }
            memcpy(dest, copy_from, copy_len);
            dest += copy_len;
            len -= copy_len;
        }
    }

    /* Room for a terminator? */
    if (len > 0)
    {
        *dest = '\0';
    }

    return result;
}

/******************************************************************************/
const char *
g_bool2text(int value)
{
    return value ? "true" : "false";
}

/*****************************************************************************/
int
g_text2bool(const char *s)
{
    if ( (atoi(s) != 0) ||
         (0 == strcasecmp(s, "true")) ||
         (0 == strcasecmp(s, "on")) ||
         (0 == strcasecmp(s, "yes")))
    {
        return 1;
    }
    return 0;
}
