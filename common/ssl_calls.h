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
   Copyright (C) Jay Sorg 2004-2007

*/

#if !defined(SSL_CALLS_H)
#define SSL_CALLS_H

#include "arch.h"

void* APP_CC
ssl_rc4_info_create(void);
void APP_CC
ssl_rc4_info_delete(void* rc4_info);
void APP_CC
ssl_rc4_set_key(void* rc4_info, char* key, int len);
void APP_CC
ssl_rc4_crypt(void* rc4_info, char* data, int len);
void* APP_CC
ssl_sha1_info_create(void);
void APP_CC
ssl_sha1_info_delete(void* sha1_info);
void APP_CC
ssl_sha1_clear(void* sha1_info);
void APP_CC
ssl_sha1_transform(void* sha1_info, char* data, int len);
void APP_CC
ssl_sha1_complete(void* sha1_info, char* data);
void* APP_CC
ssl_md5_info_create(void);
void APP_CC
ssl_md5_info_delete(void* md5_info);
void APP_CC
ssl_md5_clear(void* md5_info);
void APP_CC
ssl_md5_transform(void* md5_info, char* data, int len);
void APP_CC
ssl_md5_complete(void* md5_info, char* data);
int APP_CC
ssl_mod_exp(char* out, int out_len, char* in, int in_len,
            char* mod, int mod_len, char* exp, int exp_len);

#endif
