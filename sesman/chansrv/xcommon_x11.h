/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2012
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
 * @file sesman/chansrv/xcommon_vars.c
 *
 * xcommon global variables
 *
 * Declares global variables defined in xcommon.h with X11-specific types
 */
#if !defined(XCOMMON_X11_H)
#define XCOMMON_X11_H

#include <X11/Xlib.h>

extern Display *g_display;
extern Screen *g_screen;
extern Window g_root_window;
extern Atom g_wm_delete_window_atom;
extern Atom g_wm_protocols_atom;
extern Atom g_utf8_string;
extern Atom g_net_wm_name;
extern Atom g_wm_state;

#endif
