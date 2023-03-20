/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2013
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
 *
 * @file sesman_clip_restrict.c
 * @brief Routine for parsing clip restrict strings
 *
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <stdio.h>

#include "arch.h"
#include "sesman_clip_restrict.h"
#include "string_calls.h"

/*
  Map clipboard strings into bitmask values.
*/
static const struct bitmask_string clip_restrict_map[] =
{
    { CLIP_RESTRICT_TEXT, "text"},
    { CLIP_RESTRICT_FILE, "file"},
    { CLIP_RESTRICT_IMAGE, "image"},
    { CLIP_RESTRICT_ALL, "all"},
    { CLIP_RESTRICT_NONE, "none"},
    /* Compatibility values */
    { CLIP_RESTRICT_ALL, "true"},
    { CLIP_RESTRICT_ALL, "yes"},
    { CLIP_RESTRICT_NONE, "false"},
    BITMASK_STRING_END_OF_LIST
};

/******************************************************************************/
int
sesman_clip_restrict_string_to_bitmask(
    const char *inputstr,
    char *unrecognised,
    unsigned int unrecognised_len)
{
    return g_str_to_bitmask(inputstr, clip_restrict_map, ",",
                            unrecognised, unrecognised_len);
}

/******************************************************************************/
int
sesman_clip_restrict_mask_to_string(
    int mask,
    char output[],
    unsigned int output_len)
{
    int rv;

    if (mask == CLIP_RESTRICT_NONE)
    {
        rv = snprintf(output, output_len, "none");
    }
    else if (mask == CLIP_RESTRICT_ALL)
    {
        rv = snprintf(output, output_len, "all");
    }
    else
    {
        rv = g_bitmask_to_str(mask, clip_restrict_map, ',', output, output_len);
    }

    return rv;
}

