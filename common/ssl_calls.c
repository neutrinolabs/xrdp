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
   Copyright (C) Jay Sorg 2004-2009

   ssl calls

*/

#include <stdlib.h> /* needed for openssl headers */
#include <openssl/rc4.h>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <openssl/bn.h>
#include <openssl/rsa.h>

#include "os_calls.h"
#include "arch.h"
#include "ssl_calls.h"

#if defined(OPENSSL_VERSION_NUMBER) && (OPENSSL_VERSION_NUMBER >= 0x0090800f)
#undef OLD_RSA_GEN1
#else
#define OLD_RSA_GEN1
#endif

/* rc4 stuff */

/*****************************************************************************/
void* APP_CC
ssl_rc4_info_create(void)
{
  return g_malloc(sizeof(RC4_KEY), 1);
}

/*****************************************************************************/
void APP_CC
ssl_rc4_info_delete(void* rc4_info)
{
  g_free(rc4_info);
}

/*****************************************************************************/
void APP_CC
ssl_rc4_set_key(void* rc4_info, char* key, int len)
{
  RC4_set_key((RC4_KEY*)rc4_info, len, (tui8*)key);
}

/*****************************************************************************/
void APP_CC
ssl_rc4_crypt(void* rc4_info, char* data, int len)
{
  RC4((RC4_KEY*)rc4_info, len, (tui8*)data, (tui8*)data);
}

/* sha1 stuff */

/*****************************************************************************/
void* APP_CC
ssl_sha1_info_create(void)
{
  return g_malloc(sizeof(SHA_CTX), 1);
}

/*****************************************************************************/
void APP_CC
ssl_sha1_info_delete(void* sha1_info)
{
  g_free(sha1_info);
}

/*****************************************************************************/
void APP_CC
ssl_sha1_clear(void* sha1_info)
{
  SHA1_Init((SHA_CTX*)sha1_info);
}

/*****************************************************************************/
void APP_CC
ssl_sha1_transform(void* sha1_info, char* data, int len)
{
  SHA1_Update((SHA_CTX*)sha1_info, data, len);
}

/*****************************************************************************/
void APP_CC
ssl_sha1_complete(void* sha1_info, char* data)
{
  SHA1_Final((tui8*)data, (SHA_CTX*)sha1_info);
}

/* md5 stuff */

/*****************************************************************************/
void* APP_CC
ssl_md5_info_create(void)
{
  return g_malloc(sizeof(MD5_CTX), 1);
}

/*****************************************************************************/
void APP_CC
ssl_md5_info_delete(void* md5_info)
{
  g_free(md5_info);
}

/*****************************************************************************/
void APP_CC
ssl_md5_clear(void* md5_info)
{
  MD5_Init((MD5_CTX*)md5_info);
}

/*****************************************************************************/
void APP_CC
ssl_md5_transform(void* md5_info, char* data, int len)
{
  MD5_Update((MD5_CTX*)md5_info, data, len);
}

/*****************************************************************************/
void APP_CC
ssl_md5_complete(void* md5_info, char* data)
{
  MD5_Final((tui8*)data, (MD5_CTX*)md5_info);
}

/*****************************************************************************/
static void APP_CC
ssl_reverse_it(char* p, int len)
{
  int i;
  int j;
  char temp;

  i = 0;
  j = len - 1;
  while (i < j)
  {
    temp = p[i];
    p[i] = p[j];
    p[j] = temp;
    i++;
    j--;
  }
}

/*****************************************************************************/
int APP_CC
ssl_mod_exp(char* out, int out_len, char* in, int in_len,
            char* mod, int mod_len, char* exp, int exp_len)
{
  BN_CTX* ctx;
  BIGNUM lmod;
  BIGNUM lexp;
  BIGNUM lin;
  BIGNUM lout;
  int rv;
  char* l_out;
  char* l_in;
  char* l_mod;
  char* l_exp;

  l_out = (char*)g_malloc(out_len, 1);
  l_in = (char*)g_malloc(in_len, 1);
  l_mod = (char*)g_malloc(mod_len, 1);
  l_exp = (char*)g_malloc(exp_len, 1);
  g_memcpy(l_in, in, in_len);
  g_memcpy(l_mod, mod, mod_len);
  g_memcpy(l_exp, exp, exp_len);
  ssl_reverse_it(l_in, in_len);
  ssl_reverse_it(l_mod, mod_len);
  ssl_reverse_it(l_exp, exp_len);
  ctx = BN_CTX_new();
  BN_init(&lmod);
  BN_init(&lexp);
  BN_init(&lin);
  BN_init(&lout);
  BN_bin2bn((tui8*)l_mod, mod_len, &lmod);
  BN_bin2bn((tui8*)l_exp, exp_len, &lexp);
  BN_bin2bn((tui8*)l_in, in_len, &lin);
  BN_mod_exp(&lout, &lin, &lexp, &lmod, ctx);
  rv = BN_bn2bin(&lout, (tui8*)l_out);
  if (rv <= out_len)
  {
    ssl_reverse_it(l_out, rv);
    g_memcpy(out, l_out, out_len);
  }
  else
  {
    rv = 0;
  }
  BN_free(&lin);
  BN_free(&lout);
  BN_free(&lexp);
  BN_free(&lmod);
  BN_CTX_free(ctx);
  g_free(l_out);
  g_free(l_in);
  g_free(l_mod);
  g_free(l_exp);
  return rv;
}

#if defined(OLD_RSA_GEN1)
/*****************************************************************************/
/* returns error
   generates a new rsa key
   exp is passed in and mod and pri are passed out */
int APP_CC
ssl_gen_key_xrdp1(int key_size_in_bits, char* exp, int exp_len,
                  char* mod, int mod_len, char* pri, int pri_len)
{
  int my_e;
  RSA* my_key;
  char* lmod;
  char* lpri;
  tui8* lexp;
  int error;
  int len;

  if ((exp_len != 4) || (mod_len != 64) || (pri_len != 64))
  {
    return 1;
  }
  lmod = (char*)g_malloc(mod_len, 0);
  lpri = (char*)g_malloc(pri_len, 0);
  lexp = (tui8*)exp;
  my_e = lexp[0];
  my_e |= lexp[1] << 8;
  my_e |= lexp[2] << 16;
  my_e |= lexp[3] << 24;
  /* srand is in stdlib.h */
  srand(g_time1());
  my_key = RSA_generate_key(key_size_in_bits, my_e, 0, 0);
  error = my_key == 0;
  if (error == 0)
  {
    len = BN_num_bytes(my_key->n);
    error = len != mod_len;
  }
  if (error == 0)
  {
    BN_bn2bin(my_key->n, (tui8*)lmod);
    ssl_reverse_it(lmod, mod_len);
  }
  if (error == 0)
  {
    len = BN_num_bytes(my_key->d);
    error = len != pri_len;
  }
  if (error == 0)
  {
    BN_bn2bin(my_key->d, (tui8*)lpri);
    ssl_reverse_it(lpri, pri_len);
  }
  if (error == 0)
  {
    g_memcpy(mod, lmod, mod_len);
    g_memcpy(pri, lpri, pri_len);
  }
  RSA_free(my_key);
  g_free(lmod);
  g_free(lpri);
  return error;
}
#else
/*****************************************************************************/
/* returns error
   generates a new rsa key
   exp is passed in and mod and pri are passed out */
int APP_CC
ssl_gen_key_xrdp1(int key_size_in_bits, char* exp, int exp_len,
                  char* mod, int mod_len, char* pri, int pri_len)
{
  BIGNUM* my_e;
  RSA* my_key;
  char* lexp;
  char* lmod;
  char* lpri;
  int error;
  int len;

  if ((exp_len != 4) || (mod_len != 64) || (pri_len != 64))
  {
    return 1;
  }
  lexp = (char*)g_malloc(exp_len, 0);
  lmod = (char*)g_malloc(mod_len, 0);
  lpri = (char*)g_malloc(pri_len, 0);
  g_memcpy(lexp, exp, exp_len);
  ssl_reverse_it(lexp, exp_len);
  my_e = BN_new();
  BN_bin2bn((tui8*)lexp, exp_len, my_e);
  my_key = RSA_new();
  error = RSA_generate_key_ex(my_key, key_size_in_bits, my_e, 0) == 0;
  if (error == 0)
  {
    len = BN_num_bytes(my_key->n);
    error = len != mod_len;
  }
  if (error == 0)
  {
    BN_bn2bin(my_key->n, (tui8*)lmod);
    ssl_reverse_it(lmod, mod_len);
  }
  if (error == 0)
  {
    len = BN_num_bytes(my_key->d);
    error = len != pri_len;
  }
  if (error == 0)
  {
    BN_bn2bin(my_key->d, (tui8*)lpri);
    ssl_reverse_it(lpri, pri_len);
  }
  if (error == 0)
  {
    g_memcpy(mod, lmod, mod_len);
    g_memcpy(pri, lpri, pri_len);
  }
  BN_free(my_e);
  RSA_free(my_key);
  g_free(lexp);
  g_free(lmod);
  g_free(lpri);
  return error;
}
#endif
