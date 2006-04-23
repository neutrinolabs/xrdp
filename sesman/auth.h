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
   Copyright (C) Jay Sorg 2005-2006

   session manager - main header
*/

#ifndef AUTH_H
#define AUTH_H

long DEFAULT_CC
auth_userpass(char* user, char* pass);
int DEFAULT_CC
auth_start_session(long in_val, int in_display);
int DEFAULT_CC
auth_end(long in_val);
int DEFAULT_CC
auth_set_env(long in_val);

#endif
