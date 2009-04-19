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
   Copyright (C) Jay Sorg 2009
*/

#include "arch.h"
#include "parse.h"
#include "os_calls.h"

/*****************************************************************************/
int APP_CC
sound_init(void)
{
  return 0;
}

/*****************************************************************************/
int APP_CC
sound_data_in(struct stream* s, int chan_id, int chan_flags, int length,
              int total_length)
{
  return 0;
}

/*****************************************************************************/
int APP_CC
sound_get_wait_objs(tbus* objs, int* count, int* timeout)
{
  return 0;
}

/*****************************************************************************/
int APP_CC
sound_check_wait_objs(void)
{
  return 0;
}
