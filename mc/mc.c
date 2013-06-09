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

#include "mc.h"

/*****************************************************************************/
/* return error */
int DEFAULT_CC
lib_mod_start(struct mod *mod, int w, int h, int bpp)
{
    LIB_DEBUG(mod, "in lib_mod_start");
    mod->width = w;
    mod->height = h;
    mod->bpp = bpp;
    LIB_DEBUG(mod, "out lib_mod_start");
    return 0;
}

/******************************************************************************/
/* return error */
int DEFAULT_CC
lib_mod_connect(struct mod *mod)
{
    LIB_DEBUG(mod, "in lib_mod_connect");
    LIB_DEBUG(mod, "out lib_mod_connect");
    return 0;
}

/******************************************************************************/
/* return error */
int DEFAULT_CC
lib_mod_event(struct mod *mod, int msg, long param1, long param2,
              long param3, long param4)
{
    LIB_DEBUG(mod, "in lib_mod_event");
    LIB_DEBUG(mod, "out lib_mod_event");
    return 0;
}

/******************************************************************************/
/* return error */
int DEFAULT_CC
lib_mod_signal(struct mod *mod)
{
    LIB_DEBUG(mod, "in lib_mod_signal");
    LIB_DEBUG(mod, "out lib_mod_signal");
    return 0;
}

/******************************************************************************/
/* return error */
int DEFAULT_CC
lib_mod_end(struct mod *mod)
{
    return 0;
}

/******************************************************************************/
/* return error */
int DEFAULT_CC
lib_mod_set_param(struct mod *mod, char *name, char *value)
{
    return 0;
}

/******************************************************************************/
struct mod *EXPORT_CC
mod_init(void)
{
    struct mod *mod;

    mod = (struct mod *)g_malloc(sizeof(struct mod), 1);
    mod->size = sizeof(struct mod);
    mod->version = CURRENT_MOD_VER;
    mod->handle = (long)mod;
    mod->mod_connect = lib_mod_connect;
    mod->mod_start = lib_mod_start;
    mod->mod_event = lib_mod_event;
    mod->mod_signal = lib_mod_signal;
    mod->mod_end = lib_mod_end;
    mod->mod_set_param = lib_mod_set_param;
    return mod;
}

/******************************************************************************/
int EXPORT_CC
mod_exit(struct mod *mod)
{
    if (mod == 0)
    {
        return 0;
    }

    g_free(mod);
    return 0;
}
