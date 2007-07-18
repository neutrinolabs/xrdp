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
  RC4_set_key((RC4_KEY*)rc4_info, len, (unsigned char*)key);
}

/*****************************************************************************/
void APP_CC
ssl_rc4_crypt(void* rc4_info, char* data, int len)
{
  RC4((RC4_KEY*)rc4_info, len, (unsigned char*)data, (unsigned char*)data);
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
  SHA1_Final((unsigned char*)data, (SHA_CTX*)sha1_info);
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
  MD5_Final((unsigned char*)data, (MD5_CTX*)md5_info);
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
  BN_bin2bn((unsigned char*)l_mod, mod_len, &lmod);
  BN_bin2bn((unsigned char*)l_exp, exp_len, &lexp);
  BN_bin2bn((unsigned char*)l_in, in_len, &lin);
  BN_mod_exp(&lout, &lin, &lexp, &lmod, ctx);
  rv = BN_bn2bin(&lout, (unsigned char*)l_out);
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
/* stolen from openssl-0.9.8d, rsa_gen.c, rsa_builtin_keygen
   RSAerr function calls just commented out
   returns boolean */
static int
rsa_builtin_keygen1(RSA* rsa, int bits, BIGNUM* e_value, BN_GENCB* cb)
{
  BIGNUM* r0 = NULL;
  BIGNUM* r1 = NULL;
  BIGNUM* r2 = NULL;
  BIGNUM* r3 = NULL;
  BIGNUM* tmp;
  int bitsp;
  int bitsq;
  int ok = -1;
  int n = 0;
  BN_CTX* ctx = NULL;
  unsigned int degenerate = 0;

  ctx = BN_CTX_new();
  if (ctx == NULL)
  {
    goto err;
  }
  BN_CTX_start(ctx);
  r0 = BN_CTX_get(ctx);
  r1 = BN_CTX_get(ctx);
  r2 = BN_CTX_get(ctx);
  r3 = BN_CTX_get(ctx);
  if (r3 == NULL)
  {
    goto err;
  }
  bitsp = (bits + 1) / 2;
  bitsq = bits - bitsp;
  /* We need the RSA components non-NULL */
  if (!rsa->n && ((rsa->n = BN_new()) == NULL))
  {
    goto err;
  }
  if (!rsa->d && ((rsa->d = BN_new()) == NULL))
  {
    goto err;
  }
  if (!rsa->e && ((rsa->e = BN_new()) == NULL))
  {
    goto err;
  }
  if (!rsa->p && ((rsa->p = BN_new()) == NULL))
  {
    goto err;
  }
  if (!rsa->q && ((rsa->q = BN_new()) == NULL))
  {
    goto err;
  }
  if (!rsa->dmp1 && ((rsa->dmp1 = BN_new()) == NULL))
  {
    goto err;
  }
  if (!rsa->dmq1 && ((rsa->dmq1 = BN_new()) == NULL))
  {
    goto err;
  }
  if (!rsa->iqmp && ((rsa->iqmp = BN_new()) == NULL))
  {
    goto err;
  }
  BN_copy(rsa->e, e_value);
  /* generate p and q */
  for (;;)
  {
    if (!BN_generate_prime_ex(rsa->p, bitsp, 0, NULL, NULL, cb))
    {
      goto err;
    }
    if (!BN_sub(r2, rsa->p, BN_value_one()))
    {
      goto err;
    }
    if (!BN_gcd(r1, r2, rsa->e, ctx))
    {
      goto err;
    }
    if (BN_is_one(r1))
    {
      break;
    }
    if (!BN_GENCB_call(cb, 2, n++))
    {
      goto err;
    }
  }
  if (!BN_GENCB_call(cb, 3, 0))
  {
    goto err;
  }
  for (;;)
  {
    /* When generating ridiculously small keys, we can get stuck
     * continually regenerating the same prime values. Check for
     * this and bail if it happens 3 times. */
    do
    {
      if (!BN_generate_prime_ex(rsa->q, bitsq, 0, NULL, NULL, cb))
      {
        goto err;
      }
    } while ((BN_cmp(rsa->p, rsa->q) == 0) && (++degenerate < 3));
    if (degenerate == 3)
    {
      ok = 0; /* we set our own err */
      /*RSAerr(RSA_F_RSA_BUILTIN_KEYGEN,RSA_R_KEY_SIZE_TOO_SMALL);*/
      goto err;
    }
    if (!BN_sub(r2, rsa->q, BN_value_one()))
    {
      goto err;
    }
    if (!BN_gcd(r1, r2, rsa->e, ctx))
    {
      goto err;
    }
    if (BN_is_one(r1))
    {
      break;
    }
    if (!BN_GENCB_call(cb, 2, n++))
    {
      goto err;
    }
  }
  if (!BN_GENCB_call(cb, 3, 1))
  {
    goto err;
  }
  if (BN_cmp(rsa->p, rsa->q) < 0)
  {
    tmp = rsa->p;
    rsa->p = rsa->q;
    rsa->q = tmp;
  }
  /* calculate n */
  if (!BN_mul(rsa->n, rsa->p, rsa->q, ctx))
  {
    goto err;
  }
  /* calculate d */
  if (!BN_sub(r1, rsa->p, BN_value_one()))
  {
    goto err; /* p-1 */
  }
  if (!BN_sub(r2, rsa->q, BN_value_one()))
  {
    goto err; /* q-1 */
  }
  if (!BN_mul(r0, r1, r2, ctx))
  {
    goto err; /* (p-1)(q-1) */
  }
  if (!BN_mod_inverse(rsa->d, rsa->e, r0, ctx))
  {
    goto err; /* d */
  }
  /* calculate d mod (p-1) */
  if (!BN_mod(rsa->dmp1, rsa->d, r1, ctx))
  {
    goto err;
  }
  /* calculate d mod (q-1) */
  if (!BN_mod(rsa->dmq1, rsa->d, r2, ctx))
  {
    goto err;
  }
  /* calculate inverse of q mod p */
  if (!BN_mod_inverse(rsa->iqmp, rsa->q, rsa->p, ctx))
  {
    goto err;
  }
  ok = 1;
err:
  if (ok == -1)
  {
    /*RSAerr(RSA_F_RSA_BUILTIN_KEYGEN,ERR_LIB_BN);*/
    ok = 0;
  }
  if (ctx != NULL)
  {
    BN_CTX_end(ctx);
    BN_CTX_free(ctx);
  }
  return ok;
}
#endif

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
  BN_bin2bn((unsigned char*)lexp, exp_len, my_e);
  my_key = RSA_new();
#if defined(OLD_RSA_GEN1)
  error = rsa_builtin_keygen1(my_key, key_size_in_bits, my_e, 0) == 0;
#else
  error = RSA_generate_key_ex(my_key, key_size_in_bits, my_e, 0) == 0;
#endif
  if (error == 0)
  {
    len = BN_num_bytes(my_key->n);
    error = len != mod_len;
  }
  if (error == 0)
  {
    BN_bn2bin(my_key->n, (unsigned char*)lmod);
    ssl_reverse_it(lmod, mod_len);
  }
  if (error == 0)
  {
    len = BN_num_bytes(my_key->d);
    error = len != pri_len;
  }
  if (error == 0)
  {
    BN_bn2bin(my_key->d, (unsigned char*)lpri);
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
