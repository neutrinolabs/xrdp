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
   Copyright (C) Jay Sorg 2007

   rsa key generator for xrdp

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/rc4.h>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <openssl/bn.h>
#include <openssl/x509v3.h>
#include <openssl/rsa.h>
#include "os_calls.h"
#include "arch.h"

#define MY_KEY_SIZE 512

static char g_rev_exponent[4] = { 0x00, 0x01, 0x00, 0x01 };

/*****************************************************************************/
static int APP_CC
out_params(void)
{
  g_writeln("");
  g_writeln("key gen utility example './keygen xrdp'");
  g_writeln("");
  return 0;
}

/*****************************************************************************/
static int APP_CC
key_gen(void)
{
  char* e_data;
  char* n_data;
  char* d_data;
  unsigned char* p;
  int len;
  int e_len;
  int n_len;
  int d_len;
  int error;
  BN_CTX* my_ctx;
  RSA* my_key;
  BIGNUM* my_e;

  e_data = 0;
  n_data = 0;
  d_data = 0;
  my_ctx = BN_CTX_new();
  my_e = BN_new();
  p = (unsigned char*)g_rev_exponent;
  len = sizeof(g_rev_exponent);
  BN_bin2bn(p, len, my_e);
  my_key = RSA_new();
  g_writeln("");
  g_writeln("Generating %d bit rsa key...", MY_KEY_SIZE);
  g_writeln("");
  /* RSA_generate_key_ex returns boolean */
  error = RSA_generate_key_ex(my_key, MY_KEY_SIZE, my_e, 0) == 0;
  if (error == 0)
  {
    g_writeln("RSA_generate_key_ex ok");
    g_writeln("");
    e_len = BN_num_bytes(my_key->e);
    if (e_len > 0)
    {
      e_data = (char*)g_malloc(e_len, 0);
      p = (unsigned char*)e_data;
      BN_bn2bin(my_key->e, p);
      g_writeln("public exponent size %d bytes", e_len);
      g_hexdump(e_data, e_len);
      g_writeln("");
    }
    n_len = BN_num_bytes(my_key->n);
    if (n_len > 0)
    {
      n_data = (char*)g_malloc(n_len, 0);
      p = (unsigned char*)n_data;
      BN_bn2bin(my_key->n, p);
      g_writeln("public modulus size %d bytes", n_len);
      g_hexdump(n_data, n_len);
      g_writeln("");
    }
    d_len = BN_num_bytes(my_key->d);
    if (d_len > 0)
    {
      d_data = (char*)g_malloc(d_len, 0);
      p = (unsigned char*)d_data;
      BN_bn2bin(my_key->d, p);
      g_writeln("private exponent size %d bytes", d_len);
      g_hexdump(d_data, d_len);
      g_writeln("");
    }
  }
  else
  {
    g_writeln("error %d in key_gen, RSA_generate_key_ex", error);
  }
  BN_free(my_e);
  RSA_free(my_key);
  BN_CTX_free(my_ctx);
  g_free(e_data);
  g_free(n_data);
  g_free(d_data);
  return error;
}

/*****************************************************************************/
int DEFAULT_CC
main(int argc, char** argv)
{
  if (argc == 2)
  {
    if (g_strcasecmp(argv[1], "xrdp") == 0)
    {
      return key_gen();
    }
  }
  out_params();
  return 0;
}
