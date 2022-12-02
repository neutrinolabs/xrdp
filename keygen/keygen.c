/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2014
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * rsa key generator for xrdp
 */

/*
   references:

   http://www.securiteam.com/windowsntfocus/5EP010KG0G.html

*/

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "os_calls.h"
#include "string_calls.h"
#include "ssl_calls.h"
#include "arch.h"
#include "list.h"
#include "file.h"

/* this is the signature size in bytes */
#define TSSK_KEY_LENGTH 64

/* default to 2048 bit key size, can set changed, set */
static int g_key_size_bits = 2048;

static tui8 g_exponent[4] =
{
    0x01, 0x00, 0x01, 0x00
};

/* 4 bytes public exponent */
static tui8 g_ppk_e[4] =
{
    0x5B, 0x7B, 0x88, 0xC0
};

/* 64 byte modulus */
static tui8 g_ppk_n[72] = /* 64 bytes + 8 bytes pad */
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

/* 64 bytes private exponent */
static tui8 g_ppk_d[108] = /* 64 bytes + 44 bytes pad */
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

/* 512 bit proprietary certificate
  dwVersion            0   4  bytes always 0x00000001
  dwSigAlgId           4   4  bytes always 0x00000001
  dwKeyAlgId           8   4  bytes always 0x00000001
  wPublicKeyBlobType  12   2  bytes always 0x0006
  wPublicKeyBlobLen   14   2  bytes        0x005C      92  bytes
    magic             16   4  bytes always 0x31415352
    keylen            20   4  bytes        0x0048      72  bytes
    bitlen            24   4  bytes        0x0200     512  bits
    datalen           28   4  bytes        0x003F      63  bytes
    pubExp            32   4  bytes        0x00010001
    modulus           36  72  bytes
  wSignatureBlobType 108   2  bytes always 0x0008
  wSignatureBlobLen  110   2  bytes        0x0048      72 bytes
    SignatureBlob    112  72  bytes */

static tui8 g_testkey512[184] = /* 512 bit test key */
{
    0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, /* 0 */
    0x01, 0x00, 0x00, 0x00, 0x06, 0x00, 0x5c, 0x00,
    0x52, 0x53, 0x41, 0x31, 0x48, 0x00, 0x00, 0x00,
    0x00, 0x02, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x01, 0x00, 0x79, 0x6f, 0xb4, 0xdf, /* 32 */
    0xa6, 0x95, 0xb9, 0xa9, 0x61, 0xe3, 0xc4, 0x5e,
    0xff, 0x6b, 0xd8, 0x81, 0x8a, 0x12, 0x4a, 0x93,
    0x42, 0x97, 0x18, 0x93, 0xac, 0xd1, 0x3a, 0x38,
    0x3c, 0x68, 0x50, 0x19, 0x31, 0xb6, 0x84, 0x51, /* 64 */
    0x79, 0xfb, 0x1c, 0xe7, 0xe3, 0x99, 0x20, 0xc7,
    0x84, 0xdf, 0xd1, 0xaa, 0xb5, 0x15, 0xef, 0x47,
    0x7e, 0xfc, 0x88, 0xeb, 0x29, 0xc3, 0x27, 0x5a,
    0x35, 0xf8, 0xfd, 0xaa, 0x00, 0x00, 0x00, 0x00, /* 96 */
    0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x48, 0x00,
    0x32, 0x3b, 0xde, 0x6f, 0x18, 0x97, 0x1e, 0xc3,
    0x6b, 0x2b, 0x2d, 0xe4, 0xfc, 0x2d, 0xa2, 0x8e,
    0x32, 0x3c, 0xf3, 0x1b, 0x24, 0x90, 0x57, 0x4d, /* 128 */
    0x8e, 0xe4, 0x69, 0xfc, 0x16, 0x8d, 0x41, 0x92,
    0x78, 0xc7, 0x9c, 0xb4, 0x26, 0xff, 0xe8, 0x3e,
    0xa1, 0x8a, 0xf5, 0x57, 0xc0, 0x7f, 0x3e, 0x21,
    0x17, 0x32, 0x30, 0x6f, 0x79, 0xe1, 0x36, 0xcd, /* 160 */
    0xb6, 0x8e, 0xbe, 0x57, 0x57, 0xd2, 0xa9, 0x36,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* 2048 bit proprietary certificate
  dwVersion            0   4  bytes always 0x00000001
  dwSigAlgId           4   4  bytes always 0x00000001
  dwKeyAlgId           8   4  bytes always 0x00000001
  wPublicKeyBlobType  12   2  bytes always 0x0006
  wPublicKeyBlobLen   14   2  bytes        0x011C     284  bytes
    magic             16   4  bytes always 0x31415352
    keylen            20   4  bytes        0x0108     264  bytes
    bitlen            24   4  bytes        0x0800    2048  bits
    datalen           28   4  bytes        0x00FF     255  bytes
    pubExp            32   4  bytes        0x00010001
    modulus           36 264  bytes
  wSignatureBlobType 300   2  bytes always 0x0008
  wSignatureBlobLen  302   2  bytes        0x0048      72 bytes
    SignatureBlob    304  72  bytes */

static tui8 g_testkey2048[376] = /* 2048 bit test key */
{
    0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, /* 0 */
    0x01, 0x00, 0x00, 0x00, 0x06, 0x00, 0x1c, 0x01,
    0x52, 0x53, 0x41, 0x31, 0x08, 0x01, 0x00, 0x00,
    0x00, 0x08, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x01, 0x00, 0x13, 0x77, 0x6d, 0xd8, /* 32 */
    0x7b, 0x6e, 0x6f, 0xb4, 0x27, 0x6d, 0x70, 0x3a,
    0x97, 0x5f, 0xcb, 0x50, 0x9b, 0x13, 0x6b, 0xc7,
    0xba, 0xdf, 0x73, 0x54, 0x17, 0x35, 0xf0, 0x09,
    0x9e, 0x9d, 0x0b, 0x6a, 0x2c, 0x9f, 0xd1, 0x0c, /* 64 */
    0xc6, 0x47, 0x83, 0xde, 0xca, 0x90, 0x20, 0xac,
    0x70, 0x63, 0x9b, 0xb7, 0x49, 0x07, 0x0b, 0xf5,
    0xf2, 0x38, 0x2a, 0x40, 0xff, 0xf1, 0xba, 0x97,
    0x79, 0x3e, 0xd4, 0xd1, 0xf3, 0x41, 0x0f, 0x91, /* 96 */
    0xfe, 0x1a, 0x86, 0xf4, 0x1b, 0xef, 0xcc, 0x29,
    0xa3, 0x35, 0x6f, 0x60, 0xfa, 0x98, 0x53, 0x51,
    0x80, 0x57, 0x15, 0x2f, 0xe1, 0x8b, 0xd7, 0x86,
    0x15, 0x2d, 0xb5, 0xec, 0x7a, 0xdd, 0xc5, 0x1d, /* 128 */
    0x1b, 0x88, 0x53, 0x67, 0x86, 0xe1, 0x6e, 0xcd,
    0x4e, 0x2e, 0xfd, 0xd2, 0x49, 0x04, 0xfb, 0x1d,
    0x95, 0xf0, 0xe9, 0x7f, 0x97, 0xa3, 0x1b, 0xb2,
    0x92, 0x2e, 0x62, 0x2a, 0x96, 0xd4, 0xea, 0x18, /* 160 */
    0x8e, 0x99, 0x41, 0xea, 0x83, 0xb5, 0xf1, 0x0e,
    0xea, 0x53, 0x70, 0x99, 0xd7, 0x9e, 0x0c, 0x65,
    0x2b, 0xf4, 0xdc, 0x0e, 0xe7, 0x9e, 0xce, 0x04,
    0x25, 0x01, 0x88, 0xc4, 0xc1, 0xd2, 0xa4, 0x18, /* 192 */
    0xc2, 0x8a, 0x52, 0x0f, 0x01, 0xb2, 0x71, 0x30,
    0x44, 0x3f, 0x5b, 0x11, 0x2e, 0xe7, 0x53, 0xa0,
    0xc8, 0x1f, 0x77, 0xaf, 0xb5, 0xbb, 0xaf, 0x12,
    0xe8, 0x19, 0x0c, 0xf6, 0x1f, 0xa8, 0x3e, 0x11, /* 224 */
    0x34, 0xe4, 0xac, 0x1c, 0x1c, 0x00, 0xbb, 0xb9,
    0x2f, 0xbb, 0x81, 0x76, 0x4e, 0x46, 0xda, 0x73,
    0x00, 0x82, 0x71, 0xa4, 0x62, 0xc3, 0xd4, 0x3f,
    0xda, 0x34, 0x54, 0x27, 0xe3, 0xd0, 0x3a, 0x73, /* 256 */
    0x2f, 0x99, 0xc4, 0x91, 0x56, 0x12, 0x98, 0xa8,
    0x3b, 0x8d, 0x61, 0x83, 0x62, 0xb7, 0x20, 0x61,
    0x4d, 0xc9, 0x41, 0xd1, 0x40, 0x02, 0x11, 0x4b,
    0x63, 0x46, 0xc7, 0xc1, 0x00, 0x00, 0x00, 0x00, /* 288 */
    0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x48, 0x00,
    0x00, 0x50, 0xb7, 0x75, 0xf3, 0x77, 0x99, 0xb2,
    0xd3, 0xdd, 0xcd, 0x83, 0x6e, 0xdb, 0x0a, 0x29,
    0x88, 0x05, 0xb5, 0x8a, 0x49, 0xd5, 0xa8, 0x5a, /* 320 */
    0xc3, 0xb7, 0x18, 0xc2, 0x3c, 0x1e, 0xde, 0xd3,
    0x8f, 0xdd, 0x21, 0x27, 0x84, 0xa0, 0xc8, 0x8d,
    0x65, 0xce, 0x5d, 0x3d, 0x46, 0x65, 0x88, 0xfc,
    0x35, 0x0a, 0x04, 0xa0, 0xda, 0xc1, 0xab, 0xbf, /* 352 */
    0xcd, 0xf1, 0x7e, 0x71, 0x7b, 0xf8, 0x4a, 0x78,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/*****************************************************************************/
static int
out_params(void)
{
    g_writeln("%s", "");
    g_writeln("xrdp rsa key gen utility examples");
    g_writeln("  xrdp-keygen xrdp ['path and file name' | auto] [512 or 2048]");
    g_writeln("  xrdp-keygen test");
    g_writeln("%s", "");
    return 0;
}

/*****************************************************************************/
/* this is the special key signing algorithm */
static int
sign_key(const char *e_data, int e_len, const char *n_data, int n_len,
         char *sign_data, int sign_len)
{
    char *key;
    char *md5_final;
    void *md5;

    if ((e_len != 4) || ((n_len != 64) && (n_len != 256)) || (sign_len != 64))
    {
        return 1;
    }

    if (n_len == 64)
    {
        key = (char *)g_malloc(184, 0);
        md5_final = (char *)g_malloc(64, 0);
        md5 = ssl_md5_info_create();
        /* copy the test key */
        g_memcpy(key, g_testkey512, 184);
        /* replace e and n */
        g_memcpy(key + 32, e_data, e_len);
        g_memcpy(key + 36, n_data, n_len);
        ssl_md5_clear(md5);
        /* the first 108 bytes */
        ssl_md5_transform(md5, key, 108);
        /* set the whole thing with 0xff */
        g_memset(md5_final, 0xff, 64);
        /* digest 16 bytes */
        ssl_md5_complete(md5, md5_final);
        /* set non 0xff array items */
        md5_final[16] = 0;
        md5_final[62] = 1;
        md5_final[63] = 0;
        /* encrypt */
        ssl_mod_exp(sign_data, sign_len, md5_final, 64, (char *)g_ppk_n, 64,
                    (char *)g_ppk_d, 64);
        /* cleanup */
        ssl_md5_info_delete(md5);
        g_free(key);
        g_free(md5_final);
    }
    else if (n_len == 256)
    {
        key = (char *)g_malloc(376, 0);
        md5_final = (char *)g_malloc(64, 0);
        md5 = ssl_md5_info_create();
        /* copy the test key */
        g_memcpy(key, g_testkey2048, 376);
        /* replace e and n */
        g_memcpy(key + 32, e_data, e_len);
        g_memcpy(key + 36, n_data, n_len);
        ssl_md5_clear(md5);
        /* the first 300 bytes */
        ssl_md5_transform(md5, key, 300);
        /* set the whole thing with 0xff */
        g_memset(md5_final, 0xff, 64);
        /* digest 16 bytes */
        ssl_md5_complete(md5, md5_final);
        /* set non 0xff array items */
        md5_final[16] = 0;
        md5_final[62] = 1;
        md5_final[63] = 0;
        /* encrypt */
        ssl_mod_exp(sign_data, sign_len, md5_final, 64, (char *)g_ppk_n, 64,
                    (char *)g_ppk_d, 64);
        /* cleanup */
        ssl_md5_info_delete(md5);
        g_free(key);
        g_free(md5_final);
    }

    return 0;
}

/*****************************************************************************/
static int
write_out_line(int fd, const char *name, const char *data, int len)
{
    int max;
    int error;
    int index;
    int data_item;
    int buf_pos;
    char *buf;
    char *text;

    text = (char *)g_malloc(256, 0);
    max = len;
    max = max * 10;
    buf_pos = g_strlen(name);
    max = max + buf_pos + 16;
    buf = (char *)g_malloc(max, 0);
    g_strncpy(buf, name, max - 1);
    buf[buf_pos] = '=';
    buf_pos++;

    for (index = 0; index < len; index++)
    {
        data_item = (tui8)(data[index]);
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
static int
save_all(const char *e_data, int e_len, const char *n_data, int n_len,
         const char *d_data, int d_len, const char *sign_data, int sign_len,
         const char *path_and_file_name)
{
    int fd;
    char filename[256];

    if (path_and_file_name == 0)
    {
        g_strncpy(filename, "rsakeys.ini", 255);
    }
    else
    {
        g_strncpy(filename, path_and_file_name, 255);
    }

    g_writeln("saving to %s", filename);
    g_writeln("%s", "");

    if (g_file_exist(filename))
    {
        if (g_file_delete(filename) == 0)
        {
            g_writeln("problem deleting %s, maybe no rights", filename);
            return 1;
        }
    }

    fd = g_file_open(filename);

    if (fd != -1)
    {
        if (g_file_write(fd, "[keys]\n", 7) == -1)
        {
            g_writeln("problem writing to %s, maybe no rights", filename);
            g_file_close(fd);
            return 1;
        }

        write_out_line(fd, "pub_exp", e_data, e_len);
        write_out_line(fd, "pub_mod", n_data, n_len);
        write_out_line(fd, "pub_sig", sign_data, sign_len);
        write_out_line(fd, "pri_exp", d_data, d_len);
    }
    else
    {
        g_writeln("problem opening %s, maybe no rights", filename);
        return 1;
    }

    g_file_close(fd);
    return 0;
}

/*****************************************************************************/
static int
key_gen(const char *path_and_file_name)
{
    char *e_data;
    char *n_data;
    char *d_data;
    char *sign_data;
    int e_len;
    int n_len;
    int d_len;
    int sign_len;
    int error;

    e_data = (char *)g_exponent;
    n_data = (char *)g_malloc(256, 0);
    d_data = (char *)g_malloc(256, 0);
    sign_data = (char *)g_malloc(64, 0);
    e_len = 4;
    n_len = g_key_size_bits / 8;
    d_len = n_len;
    sign_len = 64;
    error = 0;
    g_writeln("%s", "");
    g_writeln("Generating %d bit rsa key...", g_key_size_bits);
    g_writeln("%s", "");

    if (error == 0)
    {
        error = ssl_gen_key_xrdp1(g_key_size_bits, e_data, e_len, n_data, n_len,
                                  d_data, d_len);
        if (error != 0)
        {
            g_writeln("error %d in key_gen, ssl_gen_key_xrdp1", error);
        }
    }

    if (error == 0)
    {
        g_writeln("ssl_gen_key_xrdp1 ok");
        g_writeln("%s", "");
        error = sign_key(e_data, e_len, n_data, n_len, sign_data, sign_len);

        if (error != 0)
        {
            g_writeln("error %d in key_gen, sign_key", error);
        }
    }

    if (error == 0)
    {
        error = save_all(e_data, e_len, n_data, n_len, d_data, d_len,
                         sign_data, sign_len, path_and_file_name);

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
static int
key_gen_auto(void)
{
    return key_gen("/etc/xrdp/rsakeys.ini");
}

/*****************************************************************************/
static int
key_test512(void)
{
    char *md5_final;
    char *sig;
    void *md5;

    md5_final = (char *)g_malloc(64, 0);
    sig = (char *)g_malloc(64, 0);
    md5 = ssl_md5_info_create();
    g_writeln("original key is:");
    g_hexdump((char *)g_testkey512, 184);
    g_writeln("original exponent is:");
    g_hexdump((char *)g_testkey512 + 32, 4);
    g_writeln("original modulus is:");
    g_hexdump((char *)g_testkey512 + 36, 64);
    g_writeln("original signature is:");
    g_hexdump((char *)g_testkey512 + 112, 64);
    ssl_md5_clear(md5);
    ssl_md5_transform(md5, (char *)g_testkey512, 108);
    g_memset(md5_final, 0xff, 64);
    ssl_md5_complete(md5, md5_final);
    g_writeln("md5 hash of first 108 bytes of this key is:");
    g_hexdump(md5_final, 16);
    md5_final[16] = 0;
    md5_final[62] = 1;
    md5_final[63] = 0;
    ssl_mod_exp(sig, 64, md5_final, 64, (char *)g_ppk_n, 64, (char *)g_ppk_d, 64);
    g_writeln("produced signature(this should match original "
              "signature above) is:");
    g_hexdump(sig, 64);
    g_memset(md5_final, 0, 64);
    ssl_mod_exp(md5_final, 64, (char *)g_testkey512 + 112, 64, (char *)g_ppk_n, 64,
                (char *)g_ppk_e, 4);
    g_writeln("decrypted hash of first 108 bytes of this key is:");
    g_hexdump(md5_final, 64);
    ssl_md5_info_delete(md5);
    g_free(md5_final);
    g_free(sig);
    return 0;
}

/*****************************************************************************/
static int
key_test2048(void)
{
    char *md5_final;
    char *sig;
    void *md5;

    md5_final = (char *)g_malloc(64, 0);
    sig = (char *)g_malloc(64, 0);
    md5 = ssl_md5_info_create();
    g_writeln("original key is:");
    g_hexdump((char *)g_testkey2048, 376);
    g_writeln("original exponent is:");
    g_hexdump((char *)g_testkey2048 + 32, 4);
    g_writeln("original modulus is:");
    g_hexdump((char *)g_testkey2048 + 36, 256);
    g_writeln("original signature is:");
    g_hexdump((char *)g_testkey2048 + 304, 64);
    ssl_md5_clear(md5);
    ssl_md5_transform(md5, (char *)g_testkey2048, 300);
    g_memset(md5_final, 0xff, 64);
    ssl_md5_complete(md5, md5_final);
    g_writeln("md5 hash of first 300 bytes of this key is:");
    g_hexdump(md5_final, 16);
    md5_final[16] = 0;
    md5_final[62] = 1;
    md5_final[63] = 0;
    ssl_mod_exp(sig, 64, md5_final, 64, (char *)g_ppk_n, 64, (char *)g_ppk_d, 64);
    g_writeln("produced signature(this should match original "
              "signature above) is:");
    g_hexdump(sig, 64);
    g_memset(md5_final, 0, 64);
    ssl_mod_exp(md5_final, 64, (char *)g_testkey2048 + 304, 64, (char *)g_ppk_n, 64,
                (char *)g_ppk_e, 4);
    g_writeln("decrypted hash of first 108 bytes of this key is:");
    g_hexdump(md5_final, 64);
    ssl_md5_info_delete(md5);
    g_free(md5_final);
    g_free(sig);
    return 0;
}

/*****************************************************************************/
int
main(int argc, char **argv)
{
    if (argc > 1)
    {
        if (g_strcasecmp(argv[1], "xrdp") == 0)
        {
            if (argc > 2)
            {
                if (argc > 3)
                {
                    g_key_size_bits = g_atoi(argv[3]);
                    if ((g_key_size_bits != 512) && (g_key_size_bits != 2048))
                    {
                        out_params();
                        return 0;
                    }
                }
                if (g_strcasecmp(argv[2], "auto") == 0)
                {
                    if (g_getuid() != 0)
                    {
                        g_writeln("must run as root");
                        return 0;
                    }

                    return key_gen_auto();
                }
                else
                {
                    return key_gen(argv[2]);
                }
            }
            else
            {
                return key_gen(0);
            }
        }
        else if (g_strcasecmp(argv[1], "test") == 0)
        {
            g_writeln("%s", "");
            g_writeln("testing 512 bit key");
            key_test512();
            g_writeln("%s", "");
            g_writeln("testing 2048 bit key");
            key_test2048();
            return 0;
        }
    }

    out_params();
    return 0;
}
