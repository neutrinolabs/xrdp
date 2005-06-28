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

   ssl calls

*/

#include "os_calls.h"

#include <openssl/rc4.h>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <openssl/bn.h>

/* rc4 stuff */

/*****************************************************************************/
void*
g_rc4_info_create(void)
{
  return g_malloc(sizeof(RC4_KEY), 1);;
}

/*****************************************************************************/
void
g_rc4_info_delete(void* rc4_info)
{
  g_free(rc4_info);
}

/*****************************************************************************/
void
g_rc4_set_key(void* rc4_info, char* key, int len)
{
  RC4_set_key((RC4_KEY*)rc4_info, len, (unsigned char*)key);
}

/*****************************************************************************/
void
g_rc4_crypt(void* rc4_info, char* data, int len)
{
  RC4((RC4_KEY*)rc4_info, len, (unsigned char*)data, (unsigned char*)data);
}

/* sha1 stuff */

/*****************************************************************************/
void*
g_sha1_info_create(void)
{
  return g_malloc(sizeof(SHA_CTX), 1);
}

/*****************************************************************************/
void
g_sha1_info_delete(void* sha1_info)
{
  g_free(sha1_info);
}

/*****************************************************************************/
void
g_sha1_clear(void* sha1_info)
{
  SHA1_Init((SHA_CTX*)sha1_info);
}

/*****************************************************************************/
void
g_sha1_transform(void* sha1_info, char* data, int len)
{
  SHA1_Update((SHA_CTX*)sha1_info, data, len);
}

/*****************************************************************************/
void
g_sha1_complete(void* sha1_info, char* data)
{
  SHA1_Final((unsigned char*)data, (SHA_CTX*)sha1_info);
}

/* md5 stuff */

/*****************************************************************************/
void*
g_md5_info_create(void)
{
  return g_malloc(sizeof(MD5_CTX), 1);
}

/*****************************************************************************/
void
g_md5_info_delete(void* md5_info)
{
  g_free(md5_info);
}

/*****************************************************************************/
void
g_md5_clear(void* md5_info)
{
  MD5_Init((MD5_CTX*)md5_info);
}

/*****************************************************************************/
void
g_md5_transform(void* md5_info, char* data, int len)
{
  MD5_Update((MD5_CTX*)md5_info, data, len);
}

/*****************************************************************************/
void
g_md5_complete(void* md5_info, char* data)
{
  MD5_Final((unsigned char*)data, (MD5_CTX*)md5_info);
}

/*****************************************************************************/
int
g_mod_exp(char* out, char* in, char* mod, char* exp)
{
  BN_CTX* ctx;
  BIGNUM lmod;
  BIGNUM lexp;
  BIGNUM lin;
  BIGNUM lout;
  int rv;

  ctx = BN_CTX_new();
  BN_init(&lmod);
  BN_init(&lexp);
  BN_init(&lin);
  BN_init(&lout);
  BN_bin2bn((unsigned char*)mod, 64, &lmod);
  BN_bin2bn((unsigned char*)exp, 64, &lexp);
  BN_bin2bn((unsigned char*)in, 64, &lin);
  BN_mod_exp(&lout, &lin, &lexp, &lmod, ctx);
  rv = BN_bn2bin(&lout, (unsigned char*)out);
  BN_free(&lin);
  BN_free(&lout);
  BN_free(&lexp);
  BN_free(&lmod);
  BN_CTX_free(ctx);
  return rv;
}
