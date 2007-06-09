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
