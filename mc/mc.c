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
 *
 * media center
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "mc.h"
#include "log.h"

/*****************************************************************************/
/* return error */
int
lib_mod_start(struct mod *mod, int w, int h, int bpp)
{
    LOG_DEVEL(LOG_LEVEL_TRACE, "in lib_mod_start");
    mod->width = w;
    mod->height = h;
    mod->bpp = bpp;
    LOG_DEVEL(LOG_LEVEL_TRACE, "out lib_mod_start");
    return 0;
}

/******************************************************************************/
/* return error */
int
lib_mod_connect(struct mod *mod)
{
    LOG_DEVEL(LOG_LEVEL_TRACE, "in lib_mod_connect");
    LOG_DEVEL(LOG_LEVEL_TRACE, "out lib_mod_connect");
    return 0;
}

/******************************************************************************/
/* return error */
int
lib_mod_event(struct mod *mod, int msg, long param1, long param2,
              long param3, long param4)
{
    LOG_DEVEL(LOG_LEVEL_TRACE, "in lib_mod_event");
    LOG_DEVEL(LOG_LEVEL_TRACE, "out lib_mod_event");
    return 0;
}

/******************************************************************************/
/* return error */
int
lib_mod_signal(struct mod *mod)
{
    LOG_DEVEL(LOG_LEVEL_TRACE, "in lib_mod_signal");
    LOG_DEVEL(LOG_LEVEL_TRACE, "out lib_mod_signal");
    return 0;
}

/******************************************************************************/
/* return error */
int
lib_mod_end(struct mod *mod)
{
    return 0;
}

/******************************************************************************/
/* return error */
int
lib_mod_set_param(struct mod *mod, const char *name, const char *value)
{
    return 0;
}

/******************************************************************************/
tintptr EXPORT_CC
mod_init(void)
{
    struct mod *mod;

    mod = (struct mod *)g_malloc(sizeof(struct mod), 1);
    mod->size = sizeof(struct mod);
    mod->version = CURRENT_MOD_VER;
    mod->handle = (tintptr) mod;
    mod->mod_connect = lib_mod_connect;
    mod->mod_start = lib_mod_start;
    mod->mod_event = lib_mod_event;
    mod->mod_signal = lib_mod_signal;
    mod->mod_end = lib_mod_end;
    mod->mod_set_param = lib_mod_set_param;
    return (tintptr) mod;
}

/******************************************************************************/
int EXPORT_CC
mod_exit(tintptr handle)
{
    struct mod *mod = (struct mod *) handle;

    if (mod == 0)
    {
        return 0;
    }

    g_free(mod);
    return 0;
}
