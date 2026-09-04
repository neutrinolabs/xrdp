/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2026
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
 * Clipboard paste support for the xrdp login screen
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "xrdp.h"
#include "xrdp_login_clip.h"
#include "log.h"
#include "ms-rdpeclip.h"
#include "os_calls.h"
#include "scancode.h"
#include "string_calls.h"
#include "xrdp_constants.h"

/* Keysym for 'v'. Defined locally, as xrdp_bitmap.c does for XK_BackSpace */
#define XK_v 0x0076

/*****************************************************************************/
int
xrdp_login_clip_is_paste_key(const int *keys, int scan_code,
                             const struct xrdp_key_info *ki)
{
    int ctrl;
    int shift;
    int altgr;

    if (keys == NULL)
    {
        return 0;
    }

    ctrl = keys[SCANCODE_INDEX_LCTRL_KEY] || keys[SCANCODE_INDEX_RCTRL_KEY];
    shift = keys[SCANCODE_INDEX_LSHIFT_KEY] || keys[SCANCODE_INDEX_RSHIFT_KEY];
    altgr = keys[SCANCODE_INDEX_RALT_KEY];

    /* Ctrl+V. AltGr is LCTRL + RALT on Windows clients, so exclude it */
    if (ctrl && !altgr && ki != NULL && ki->sym == XK_v)
    {
        return 1;
    }

    /* Shift+Insert */
    if (shift && !ctrl && !altgr && scan_code == SCANCODE_INSERT_KEY)
    {
        return 1;
    }

    return 0;
}
