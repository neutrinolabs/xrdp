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
   Copyright (C) Jay Sorg 2004-2005

*/

#if !defined(SSL_CALLS_H)
#define SSL_CALLS_H

void*
g_rc4_info_create(void);
void
g_rc4_info_delete(void* rc4_info);
void
g_rc4_set_key(void* rc4_info, char* key, int len);
void
g_rc4_crypt(void* rc4_info, char* data, int len);
void*
g_sha1_info_create(void);
void
g_sha1_info_delete(void* sha1_info);
void
g_sha1_clear(void* sha1_info);
void
g_sha1_transform(void* sha1_info, char* data, int len);
void
g_sha1_complete(void* sha1_info, char* data);
void*
g_md5_info_create(void);
void
g_md5_info_delete(void* md5_info);
void
g_md5_clear(void* md5_info);
void
g_md5_transform(void* md5_info, char* data, int len);
void
g_md5_complete(void* md5_info, char* data);
int
g_mod_exp(char* out, char* in, char* mod, char* exp);

#endif
