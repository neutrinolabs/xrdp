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
   Copyright (C) Jay Sorg 2007-2008

   rsa key generator for xrdp

*/

/*
   references:

   http://www.securiteam.com/windowsntfocus/5EP010KG0G.html

*/

#include "os_calls.h"
#include "ssl_calls.h"
#include "arch.h"

#define MY_KEY_SIZE 512

static char g_exponent[4] =
{
  0x01, 0x00, 0x01, 0x00
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

static char g_testkey[176] =
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
static int APP_CC
out_params(void)
{
  g_writeln("");
  g_writeln("key gen utility examples");
  g_writeln("  './keygen xrdp'");
  g_writeln("  './keygen test'");
  g_writeln("");
  return 0;
}

/*****************************************************************************/
static int APP_CC
sign_key(char* e_data, int e_len, char* n_data, int n_len,
         char* d_data, int d_len, char* sign_data, int sign_len)
{
  char* key;
  char* md5_final;
  void* md5;

  if ((e_len != 4) || (n_len != 64) || (sign_len != 64))
  {
    return 1;
  }
  key = (char*)g_malloc(176, 0);
  md5_final = (char*)g_malloc(64, 0);
  md5 = ssl_md5_info_create();
  g_memcpy(key, g_testkey, 176);
  g_memcpy(key + 32, e_data, 4);
  g_memcpy(key + 36, n_data, 64);
  ssl_md5_clear(md5);
  ssl_md5_transform(md5, key, 108);
  g_memset(md5_final, 0xff, 64);
  ssl_md5_complete(md5, md5_final);
  md5_final[16] = 0;
  md5_final[62] = 1;
  md5_final[63] = 0;
  ssl_mod_exp(sign_data, 64, md5_final, 64, g_ppk_n, 64, g_ppk_d, 64);
  ssl_md5_info_delete(md5);
  g_free(key);
  g_free(md5_final);
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
  int e_len;
  int n_len;
  int d_len;
  int sign_len;
  int error;

  e_data = g_exponent;
  n_data = (char*)g_malloc(64, 0);
  d_data = (char*)g_malloc(64, 0);
  sign_data = (char*)g_malloc(64, 0);
  e_len = 4;
  n_len = 64;
  d_len = 64;
  sign_len = 64;
  error = 0;
  g_writeln("");
  g_writeln("Generating %d bit rsa key...", MY_KEY_SIZE);
  g_writeln("");
  if (error == 0)
  {
    error = ssl_gen_key_xrdp1(MY_KEY_SIZE, e_data, e_len, n_data, n_len,
                              d_data, d_len);
    if (error != 0)
    {
      g_writeln("error %d in key_gen, ssl_gen_key_xrdp1", error);
    }
  }
  if (error == 0)
  {
    g_writeln("RSA_generate_key_ex ok");
    g_writeln("");
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
  g_free(n_data);
  g_free(d_data);
  g_free(sign_data);
  return error;
}

/*****************************************************************************/
static int APP_CC
key_test(void)
{
  char* md5_final;
  char* sig;
  void* md5;

  md5_final = (char*)g_malloc(64, 0);
  sig = (char*)g_malloc(64, 0);
  md5 = ssl_md5_info_create();
  g_writeln("original key is:");
  g_hexdump(g_testkey, 176);
  g_writeln("original exponent is:");
  g_hexdump(g_testkey + 32, 4);
  g_writeln("original modulus is:");
  g_hexdump(g_testkey + 36, 64);
  g_writeln("original signature is:");
  g_hexdump(g_testkey + 112, 64);
  ssl_md5_clear(md5);
  ssl_md5_transform(md5, g_testkey, 108);
  g_memset(md5_final, 0xff, 64);
  ssl_md5_complete(md5, md5_final);
  g_writeln("md5 hash of first 108 bytes of this key is:");
  g_hexdump(md5_final, 16);
  md5_final[16] = 0;
  md5_final[62] = 1;
  md5_final[63] = 0;
  ssl_mod_exp(sig, 64, md5_final, 64, g_ppk_n, 64, g_ppk_d, 64);
  g_writeln("produced signature(this should match original \
signature above) is:");
  g_hexdump(sig, 64);
  g_memset(md5_final, 0, 64);
  ssl_mod_exp(md5_final, 64, g_testkey + 112, 64, g_ppk_n, 64, g_ppk_e, 4);
  g_writeln("decrypted hash of first 108 bytes of this key is:");
  g_hexdump(md5_final, 64);
  ssl_md5_info_delete(md5);
  g_free(md5_final);
  g_free(sig);
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
