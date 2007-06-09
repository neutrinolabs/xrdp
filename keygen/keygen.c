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

/*
   references:

   http://www.securiteam.com/windowsntfocus/5EP010KG0G.html

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

static char g_rev_exponent[4] =
{
  0x00, 0x01, 0x00, 0x01
};

static char g_ppk_e[4] =
{
  0x5B, 0x7B, 0x88, 0xC0
};

static char g_ppk_n[72] =
{
  0x3D, 0x3A, 0x5E, 0xBD, 0x72, 0x43, 0x3E, 0xC9,
  0x4D, 0xBB, 0xC1, 0x1E, 0x4A, 0xBA, 0x5F, 0xCB,
  0x3E, 0x88, 0x20, 0x87, 0xEF, 0xF5, 0xC1, 0xE2,
  0xD7, 0xB7, 0x6B, 0x9A, 0xF2, 0x52, 0x45, 0x95,
  0xCE, 0x63, 0x65, 0x6B, 0x58, 0x3A, 0xFE, 0xEF,
  0x7C, 0xE7, 0xBF, 0xFE, 0x3D, 0xF6, 0x5C, 0x7D,
  0x6C, 0x5E, 0x06, 0x09, 0x1A, 0xF5, 0x61, 0xBB,
  0x20, 0x93, 0x09, 0x5F, 0x05, 0x6D, 0xEA, 0x87,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static char g_ppk_d[108] =
{
  0x87, 0xA7, 0x19, 0x32, 0xDA, 0x11, 0x87, 0x55,
  0x58, 0x00, 0x16, 0x16, 0x25, 0x65, 0x68, 0xF8,
  0x24, 0x3E, 0xE6, 0xFA, 0xE9, 0x67, 0x49, 0x94,
  0xCF, 0x92, 0xCC, 0x33, 0x99, 0xE8, 0x08, 0x60,
  0x17, 0x9A, 0x12, 0x9F, 0x24, 0xDD, 0xB1, 0x24,
  0x99, 0xC7, 0x3A, 0xB8, 0x0A, 0x7B, 0x0D, 0xDD,
  0x35, 0x07, 0x79, 0x17, 0x0B, 0x51, 0x9B, 0xB3,
  0xC7, 0x10, 0x01, 0x13, 0xE7, 0x3F, 0xF3, 0x5F,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};

static char g_test_e[4] =
{
  0x01, 0x00, 0x01, 0x00
};

static char g_test_n[64] =
{
  0x67, 0xab, 0x0e, 0x6a, 0x9f, 0xd6, 0x2b, 0xa3,
  0x32, 0x2f, 0x41, 0xd1, 0xce, 0xee, 0x61, 0xc3,
  0x76, 0x0b, 0x26, 0x11, 0x70, 0x48, 0x8a, 0x8d,
  0x23, 0x81, 0x95, 0xa0, 0x39, 0xf7, 0x5b, 0xaa,
  0x3e, 0xf1, 0xed, 0xb8, 0xc4, 0xee, 0xce, 0x5f,
  0x6a, 0xf5, 0x43, 0xce, 0x5f, 0x60, 0xca, 0x6c,
  0x06, 0x75, 0xae, 0xc0, 0xd6, 0xa4, 0x0c, 0x92,
  0xa4, 0xc6, 0x75, 0xea, 0x64, 0xb2, 0x50, 0x5b
};

static char g_test_sign[64] =
{
  0x6a, 0x41, 0xb1, 0x43, 0xcf, 0x47, 0x6f, 0xf1,
  0xe6, 0xcc, 0xa1, 0x72, 0x97, 0xd9, 0xe1, 0x85,
  0x15, 0xb3, 0xc2, 0x39, 0xa0, 0xa6, 0x26, 0x1a,
  0xb6, 0x49, 0x01, 0xfa, 0xa6, 0xda, 0x60, 0xd7,
  0x45, 0xf7, 0x2c, 0xee, 0xe4, 0x8e, 0x64, 0x2e,
  0x37, 0x49, 0xf0, 0x4c, 0x94, 0x6f, 0x08, 0xf5,
  0x63, 0x4c, 0x56, 0x29, 0x55, 0x5a, 0x63, 0x41,
  0x2c, 0x20, 0x65, 0x95, 0x99, 0xb1, 0x15, 0x7c
};

static char g_test_d[64] =
{
  0x41, 0x93, 0x05, 0xB1, 0xF4, 0x38, 0xFC, 0x47,
  0x88, 0xC4, 0x7F, 0x83, 0x8C, 0xEC, 0x90, 0xDA,
  0x0C, 0x8A, 0xB5, 0xAE, 0x61, 0x32, 0x72, 0xF5,
  0x2B, 0xD1, 0x7B, 0x5F, 0x44, 0xC0, 0x7C, 0xBD,
  0x8A, 0x35, 0xFA, 0xAE, 0x30, 0xF6, 0xC4, 0x6B,
  0x55, 0xA7, 0x65, 0xEF, 0xF4, 0xB2, 0xAB, 0x18,
  0x4E, 0xAA, 0xE6, 0xDC, 0x71, 0x17, 0x3B, 0x4C,
  0xC2, 0x15, 0x4C, 0xF7, 0x81, 0xBB, 0xF0, 0x03
};

static char g_testa[22 * 8] =
{
  0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x00, 0x00, 0x06, 0x00, 0x5c, 0x00,
  0x52, 0x53, 0x41, 0x31, 0x48, 0x00, 0x00, 0x00,
  0x00, 0x02, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x01, 0x00, 0x79, 0x6f, 0xb4, 0xdf,
  0xa6, 0x95, 0xb9, 0xa9, 0x61, 0xe3, 0xc4, 0x5e,
  0xff, 0x6b, 0xd8, 0x81, 0x8a, 0x12, 0x4a, 0x93,
  0x42, 0x97, 0x18, 0x93, 0xac, 0xd1, 0x3a, 0x38,
  0x3c, 0x68, 0x50, 0x19, 0x31, 0xb6, 0x84, 0x51,
  0x79, 0xfb, 0x1c, 0xe7, 0xe3, 0x99, 0x20, 0xc7,
  0x84, 0xdf, 0xd1, 0xaa, 0xb5, 0x15, 0xef, 0x47,
  0x7e, 0xfc, 0x88, 0xeb, 0x29, 0xc3, 0x27, 0x5a,
  0x35, 0xf8, 0xfd, 0xaa, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
                          0x08, 0x00, 0x48, 0x00,
  0x32, 0x3b, 0xde, 0x6f, 0x18, 0x97, 0x1e, 0xc3,
  0x6b, 0x2b, 0x2d, 0xe4, 0xfc, 0x2d, 0xa2, 0x8e,
  0x32, 0x3c, 0xf3, 0x1b, 0x24, 0x90, 0x57, 0x4d,
  0x8e, 0xe4, 0x69, 0xfc, 0x16, 0x8d, 0x41, 0x92,
  0x78, 0xc7, 0x9c, 0xb4, 0x26, 0xff, 0xe8, 0x3e,
  0xa1, 0x8a, 0xf5, 0x57, 0xc0, 0x7f, 0x3e, 0x21,
  0x17, 0x32, 0x30, 0x6f, 0x79, 0xe1, 0x36, 0xcd,
  0xb6, 0x8e, 0xbe, 0x57, 0x57, 0xd2, 0xa9, 0x36
};

/*****************************************************************************/
static void
reverse(char* p, int len)
{
  int i;
  int j;
  char temp;

  for (i = 0, j = len - 1; i < j; i++, j--)
  {
    temp = p[i];
    p[i] = p[j];
    p[j] = temp;
  }
}

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
sign_key(char* e_data, int e_len, char* n_data, int n_len,
         char* d_data, int d_len, char* sign_data, int sign_len)
{
  g_writeln("todo sign here");
  g_writeln("");
  return 0;
}

/*****************************************************************************/
static int APP_CC
write_out_line(int fd, char* name, char* data, int len)
{
  int max;
  int error;
  int index;
  int data_item;
  int buf_pos;
  char* buf;
  char* text;

  text = (char*)g_malloc(256, 0);
  max = len;
  max = max * 10;
  buf_pos = g_strlen(name);
  max = max + buf_pos + 16;
  buf = (char*)g_malloc(max, 0);
  g_strncpy(buf, name, max - 1);
  buf[buf_pos] = '=';
  buf_pos++;
  for (index = 0; index < len; index++)
  {
    data_item = (unsigned char)data[index];
    g_snprintf(text, 255, "0x%2.2x", data_item);
    if (index != 0)
    {
      buf[buf_pos] = ',';
      buf_pos++;
    }
    buf[buf_pos] = text[0];
    buf_pos++;
    buf[buf_pos] = text[1];
    buf_pos++;
    buf[buf_pos] = text[2];
    buf_pos++;
    buf[buf_pos] = text[3];
    buf_pos++;
  }
  buf[buf_pos] = '\n';
  buf_pos++;
  buf[buf_pos] = 0;
  error = g_file_write(fd, buf, buf_pos) == -1;
  g_free(buf);
  g_free(text);
  return error;
}

/*****************************************************************************/
static int APP_CC
save_all(char* e_data, int e_len, char* n_data, int n_len,
         char* d_data, int d_len, char* sign_data, int sign_len)
{
  int fd;

  g_writeln("saving to rsakeys.ini");
  g_writeln("");
  if (g_file_exist("rsakeys.ini"))
  {
    g_file_delete("rsakeys.ini");
  }
  fd = g_file_open("rsakeys.ini");
  if (fd > 0)
  {
    g_file_write(fd, "[keys]\n", 7);
    write_out_line(fd, "pub_exp", e_data, e_len);
    write_out_line(fd, "pub_mod", n_data, n_len);
    write_out_line(fd, "pub_sig", sign_data, sign_len);
    write_out_line(fd, "pri_exp", d_data, d_len);
  }
  g_file_close(fd);  
  return 0;
}

/*****************************************************************************/
static int APP_CC
key_gen(void)
{
  char* e_data;
  char* n_data;
  char* d_data;
  char* sign_data;
  unsigned char* p;
  int len;
  int e_len;
  int n_len;
  int d_len;
  int sign_len;
  int error;
  int offset;
  BN_CTX* my_ctx;
  RSA* my_key;
  BIGNUM* my_e;

  e_data = 0;
  n_data = 0;
  d_data = 0;
  sign_data = 0;
  e_len = 0;
  n_len = 0;
  d_len = 0;
  sign_len = 0;
  error = 0;
  my_ctx = BN_CTX_new();
  my_e = BN_new();
  p = (unsigned char*)g_rev_exponent;
  len = sizeof(g_rev_exponent);
  BN_bin2bn(p, len, my_e);
  my_key = RSA_new();
  g_writeln("");
  g_writeln("Generating %d bit rsa key...", MY_KEY_SIZE);
  g_writeln("");
  if (error == 0)
  {
    /* RSA_generate_key_ex returns boolean */
    error = RSA_generate_key_ex(my_key, MY_KEY_SIZE, my_e, 0) == 0;
    if (error != 0)
    {
      g_writeln("error %d in key_gen, RSA_generate_key_ex", error);
    }
  }
  if (error == 0)
  {
    g_writeln("RSA_generate_key_ex ok");
    g_writeln("");
    e_len = BN_num_bytes(my_key->e);
    if (e_len > 0)
    {
      e_data = (char*)g_malloc(e_len, 1);
      offset = (((e_len + 3) / 4) * 4) - e_len;
      e_len = e_len + offset;
      p = (unsigned char*)(e_data + offset);
      BN_bn2bin(my_key->e, p);
      reverse(e_data, e_len);
      g_writeln("public exponent size %d bytes", e_len);
      g_hexdump(e_data, e_len);
      g_writeln("");
    }
    n_len = BN_num_bytes(my_key->n);
    if (n_len > 0)
    {
      n_data = (char*)g_malloc(n_len, 1);
      offset = (((n_len + 3) / 4) * 4) - n_len;
      n_len = n_len + offset;
      p = (unsigned char*)(n_data + offset);
      BN_bn2bin(my_key->n, p);
      reverse(n_data, n_len);
      g_writeln("public modulus size %d bytes", n_len);
      g_hexdump(n_data, n_len);
      g_writeln("");
      /* signature is same size as public modulus */
      sign_data = (char*)g_malloc(n_len, 1);
      sign_len = n_len;
    }
    d_len = BN_num_bytes(my_key->d);
    if (d_len > 0)
    {
      d_data = (char*)g_malloc(d_len, 0);
      offset = (((d_len + 3) / 4) * 4) - d_len;
      d_len = d_len + offset;
      p = (unsigned char*)(d_data + offset);
      BN_bn2bin(my_key->d, p);
      reverse(d_data, d_len);
      g_writeln("private exponent size %d bytes", d_len);
      g_hexdump(d_data, d_len);
      g_writeln("");
    }
    error = sign_key(e_data, e_len, n_data, n_len, d_data, d_len,
                     sign_data, sign_len);
    if (error != 0)
    {
      g_writeln("error %d in key_gen, sign_key", error);
    }
  }
  if (error == 0)
  {
    error = save_all(e_data, e_len, n_data, n_len, d_data, d_len,
                     sign_data, sign_len);
    if (error != 0)
    {
      g_writeln("error %d in key_gen, save_all", error);
    }
  }
  BN_free(my_e);
  RSA_free(my_key);
  BN_CTX_free(my_ctx);
  g_free(e_data);
  g_free(n_data);
  g_free(d_data);
  g_free(sign_data);
  return error;
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
  reverse(l_in, in_len);
  reverse(l_mod, mod_len);
  reverse(l_exp, exp_len);
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
    reverse(l_out, rv);
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

/*****************************************************************************/
static int APP_CC
key_test(void)
{
  char md5_final[16];
  char sig[64];
  unsigned char* p;
  MD5_CTX md5;

  g_writeln("original key is:");
  g_hexdump(g_testa, 176);
  g_writeln("original exponent is:");
  g_hexdump(g_testa + 32, 4);
  g_writeln("original modulus is:");
  g_hexdump(g_testa + 36, 64);
  g_writeln("original signature is:");
  g_hexdump(g_testa + 112, 64);
  MD5_Init(&md5);
  p = (unsigned char*)g_testa;
  MD5_Update(&md5, p, 108);
  p = (unsigned char*)md5_final;
  MD5_Final(p, &md5);
  g_writeln("md5 hash of first 108 bytes of this key is:");
  g_hexdump(md5_final, 16);
  ssl_mod_exp(sig, 64, md5_final, 16, g_ppk_n, 64, g_ppk_d, 64);
  g_writeln("produced signature(fix this, it should match original signature above) is:");
  g_hexdump(sig, 64);
  return 0;
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
    else if (g_strcasecmp(argv[1], "test") == 0)
    {
      return key_test();
    }
  }
  out_params();
  return 0;
}
