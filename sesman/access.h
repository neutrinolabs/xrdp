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
   Copyright (C) Jay Sorg 2005-2007
*/

/**
 *
 * @file access.h
 * @brief User access control definitions
 * @author Simone Fedele
 * 
 */

#ifndef ACCESS_H
#define ACCESS_H

/**
 *
 * @brief Checks if the user is allowed to access the terminal server
 * @param user the user to check
 * @return 0 if access is denied, !=0 if allowed
 *
 */
int DEFAULT_CC
access_login_allowed(char* user);

#endif
