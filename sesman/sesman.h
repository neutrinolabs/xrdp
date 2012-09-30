/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   xrdp: A Remote Desktop Protocol server.
   Copyright (C) Jay Sorg 2005-2008
*/

/**
 *
 * @file sesman.h
 * @brief Main include file
 * @author Jay Sorg
 *
 */

#ifndef SESMAN_H
#define SESMAN_H

#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif
#include "d3des.h"
#include "arch.h"
#include "parse.h"
#include "os_calls.h"
#include "log.h"
#include "file_loc.h"
#include "env.h"
#include "auth.h"
#include "config.h"
//#include "tcp.h"
#include "sig.h"
#include "session.h"
#include "access.h"
#include "scp.h"
#include "thread.h"
#include "lock.h"
#include "thread_calls.h"

#include "libscp.h"

#include <unistd.h>

#endif
