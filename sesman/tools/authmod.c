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
 * @file authmod.c
 *
 * @brief Pull in the configured authentication module
 *
 * Configured auth module is referenced by the macro define
 * XRDP_AUTHMOD_SRC, defined by the configure system
 *
 * @author Matt Burt
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include XRDP_AUTHMOD_SRC
