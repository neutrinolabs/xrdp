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
   Copyright (C) Jay Sorg 2005-2007

   librdp secure layer

*/

#include "rdp.h"

static char g_pad_54[40] =
{ 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54,
  54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54,
  54, 54, 54, 54, 54, 54, 54, 54 };

static char g_pad_92[48] =
{ 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92,
  92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92,
  92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92 };

/*****************************************************************************/
struct rdp_sec* APP_CC
rdp_sec_create(struct rdp_rdp* owner)
{
  struct rdp_sec* self;

  self = (struct rdp_sec*)g_malloc(sizeof(struct rdp_sec), 1);
  self->rdp_layer = owner;
  make_stream(self->client_mcs_data);
  init_stream(self->client_mcs_data, 8192);
  make_stream(self->server_mcs_data);
  init_stream(self->server_mcs_data, 8192);
  self->mcs_layer = rdp_mcs_create(self, self->client_mcs_data,
                                   self->server_mcs_data);
  self->decrypt_rc4_info = ssl_rc4_info_create();
  self->encrypt_rc4_info = ssl_rc4_info_create();
  self->lic_layer = rdp_lic_create(self);
  return self;
}

/*****************************************************************************/
void APP_CC
rdp_sec_delete(struct rdp_sec* self)
{
  if (self == 0)
  {
    return;
  }
  rdp_lic_delete(self->lic_layer);
  rdp_mcs_delete(self->mcs_layer);
  free_stream(self->client_mcs_data);
  free_stream(self->server_mcs_data);
  ssl_rc4_info_delete(self->decrypt_rc4_info);
  ssl_rc4_info_delete(self->encrypt_rc4_info);
  g_free(self);
}

/*****************************************************************************/
/* Reduce key entropy from 64 to 40 bits */
static void APP_CC
rdp_sec_make_40bit(char* key)
{
  key[0] = 0xd1;
  key[1] = 0x26;
  key[2] = 0x9e;
}

/*****************************************************************************/
/* returns error */
/* update an encryption key */
static int APP_CC
rdp_sec_update(char* key, char* update_key, int key_len)
{
  char shasig[20];
  void* sha1_info;
  void* md5_info;
  void* rc4_info;

  sha1_info = ssl_sha1_info_create();
  md5_info = ssl_md5_info_create();
  rc4_info = ssl_rc4_info_create();
  ssl_sha1_clear(sha1_info);
  ssl_sha1_transform(sha1_info, update_key, key_len);
  ssl_sha1_transform(sha1_info, g_pad_54, 40);
  ssl_sha1_transform(sha1_info, key, key_len);
  ssl_sha1_complete(sha1_info, shasig);
  ssl_md5_clear(md5_info);
  ssl_md5_transform(md5_info, update_key, key_len);
  ssl_md5_transform(md5_info, g_pad_92, 48);
  ssl_md5_transform(md5_info, shasig, 20);
  ssl_md5_complete(md5_info, key);
  ssl_rc4_set_key(rc4_info, key, key_len);
  ssl_rc4_crypt(rc4_info, key, key_len);
  if (key_len == 8)
  {
    rdp_sec_make_40bit(key);
  }
  ssl_sha1_info_delete(sha1_info);
  ssl_md5_info_delete(md5_info);
  ssl_rc4_info_delete(rc4_info);
  return 0;
}

/*****************************************************************************/
static void APP_CC
rdp_sec_decrypt(struct rdp_sec* self, char* data, int len)
{
  if (self->decrypt_use_count == 4096)
  {
    rdp_sec_update(self->decrypt_key, self->decrypt_update_key,
                   self->rc4_key_len);
    ssl_rc4_set_key(self->decrypt_rc4_info, self->decrypt_key,
                    self->rc4_key_len);
    self->decrypt_use_count = 0;
  }
  ssl_rc4_crypt(self->decrypt_rc4_info, data, len);
  self->decrypt_use_count++;
}

/*****************************************************************************/
/* returns error */
int APP_CC
rdp_sec_recv(struct rdp_sec* self, struct stream* s, int* chan)
{
  int flags;

  DEBUG((" in rdp_sec_recv"));
  if (rdp_mcs_recv(self->mcs_layer, s, chan) != 0)
  {
    DEBUG((" error in rdp_sec_recv, rdp_mcs_recv failed"));
    return 1;
  }
  in_uint32_le(s, flags);
  DEBUG((" rdp_sec_recv flags %8.8x", flags));
  if (flags & SEC_ENCRYPT) /* 0x08 */
  {
    in_uint8s(s, 8); /* signature */
    rdp_sec_decrypt(self, s->p, s->end - s->p);
  }
  if (flags & SEC_LICENCE_NEG) /* 0x80 */
  {
    DEBUG((" in rdp_sec_recv, got SEC_LICENCE_NEG"));
    rdp_lic_process(self->lic_layer, s);
    *chan = 0;
  }
  DEBUG((" out rdp_sec_recv"));
  return 0;
}

/*****************************************************************************/
/* prepare client mcs data to send in mcs layer */
static void APP_CC
rdp_sec_out_mcs_data(struct rdp_sec* self)
{
  struct stream* s;
  int hostlen;
  int length;

  s = self->client_mcs_data;
  init_stream(s, 512);
  self->rdp_layer->mod->hostname[15] = 0; /* limit length to 15 */
  hostlen = 2 * g_strlen(self->rdp_layer->mod->hostname);
  length = 158 + 76 + 12 + 4;
  /* Generic Conference Control (T.124) ConferenceCreateRequest */
  out_uint16_be(s, 5);
  out_uint16_be(s, 0x14);
  out_uint8(s, 0x7c);
  out_uint16_be(s, 1);
  out_uint16_be(s, (length | 0x8000)); /* remaining length */
  out_uint16_be(s, 8); /* length? */
  out_uint16_be(s, 16);
  out_uint8(s, 0);
  out_uint16_le(s, 0xc001);
  out_uint8(s, 0);
  out_uint32_le(s, 0x61637544); /* OEM ID: "Duca", as in Ducati. */
  out_uint16_be(s, ((length - 14) | 0x8000)); /* remaining length */
  /* Client information */
  out_uint16_le(s, SEC_TAG_CLI_INFO);
  out_uint16_le(s, 212); /* length */
  out_uint16_le(s, 1); /* RDP version. 1 == RDP4, 4 == RDP5. */
  out_uint16_le(s, 8);
  out_uint16_le(s, self->rdp_layer->mod->width);
  out_uint16_le(s, self->rdp_layer->mod->height);
  out_uint16_le(s, 0xca01);
  out_uint16_le(s, 0xaa03);
  out_uint32_le(s, self->rdp_layer->mod->keylayout);
  out_uint32_le(s, 2600); /* Client build */
  /* Unicode name of client, padded to 32 bytes */
  rdp_rdp_out_unistr(s, self->rdp_layer->mod->hostname);
  out_uint8s(s, 30 - hostlen);
  out_uint32_le(s, 4);
  out_uint32_le(s, 0);
  out_uint32_le(s, 12);
  out_uint8s(s, 64); /* reserved? 4 + 12 doublewords */
  out_uint16_le(s, 0xca01); /* color depth? */
  out_uint16_le(s, 1);
  out_uint32_le(s, 0);
  out_uint8(s, self->rdp_layer->mod->rdp_bpp);
  out_uint16_le(s, 0x0700);
  out_uint8(s, 0);
  out_uint32_le(s, 1);
  out_uint8s(s, 64); /* End of client info */
  out_uint16_le(s, SEC_TAG_CLI_4);
  out_uint16_le(s, 12);
  out_uint32_le(s, 9);
  out_uint32_le(s, 0);
  /* Client encryption settings */
  out_uint16_le(s, SEC_TAG_CLI_CRYPT);
  out_uint16_le(s, 12); /* length */
  /* encryption supported, 128-bit supported */
  out_uint32_le(s, 0x3);
  out_uint32_le(s, 0); /* Unknown */
  s_mark_end(s);
}

/*****************************************************************************/
/* Parse a public key structure */
/* returns boolean */
static int APP_CC
rdp_sec_parse_public_key(struct rdp_sec* self, struct stream* s,
                         char* modulus, char* exponent)
{
  int magic;
  int modulus_len;

  in_uint32_le(s, magic);
  if (magic != SEC_RSA_MAGIC)
  {
    return 0;
  }
  in_uint32_le(s, modulus_len);
  if (modulus_len != SEC_MODULUS_SIZE + SEC_PADDING_SIZE)
  {
    return 0;
  }
  in_uint8s(s, 8);
  in_uint8a(s, exponent, SEC_EXPONENT_SIZE);
  in_uint8a(s, modulus, SEC_MODULUS_SIZE);
  in_uint8s(s, SEC_PADDING_SIZE);
  return s_check(s);
}

/*****************************************************************************/
/* Parse a crypto information structure */
/* returns boolean */
static int APP_CC
rdp_sec_parse_crypt_info(struct rdp_sec* self, struct stream* s,
                         char* modulus, char* exponent)
{
  int random_len;
  int rsa_info_len;
  int flags;
  int tag;
  int length;
  char* next_tag;
  char* end;

  in_uint32_le(s, self->rc4_key_size); /* 1 = 40-bit, 2 = 128-bit */
  in_uint32_le(s, self->crypt_level); /* 1 = low, 2 = medium, 3 = high */
  if (self->crypt_level == 0) /* no encryption */
  {
    return 0;
  }
  in_uint32_le(s, random_len);
  in_uint32_le(s, rsa_info_len);
  if (random_len != SEC_RANDOM_SIZE)
  {
    return 0;
  }
  in_uint8a(s, self->server_random, random_len);
  /* RSA info */
  end = s->p + rsa_info_len;
  if (end > s->end)
  {
    return 0;
  }
  in_uint32_le(s, flags); /* 1 = RDP4-style, 0x80000002 = X.509 */
  if (flags & 1)
  {
    in_uint8s(s, 8); /* unknown */
    while (s->p < end)
    {
      in_uint16_le(s, tag);
      in_uint16_le(s, length);
      next_tag = s->p + length;
      DEBUG((" rdp_sec_parse_crypt_info tag %d length %d", tag, length));
      switch (tag)
      {
        case SEC_TAG_PUBKEY:
          if (!rdp_sec_parse_public_key(self, s, modulus, exponent))
          {
            return 0;
          }
          break;
        case SEC_TAG_KEYSIG:
          break;
        default:
          break;
      }
      s->p = next_tag;
    }
  }
  else
  {
    /* todo */
    return 0;
  }
  return s_check_end(s);
}

/*****************************************************************************/
static void APP_CC
rdp_sec_rsa_op(char* out, char* in, char* mod, char* exp)
{
  ssl_mod_exp(out, SEC_MODULUS_SIZE, /* 64 */
              in, SEC_RANDOM_SIZE, /* 32 */
              mod, SEC_MODULUS_SIZE, /* 64 */
              exp, SEC_EXPONENT_SIZE); /* 4 */
}

/*****************************************************************************/
void APP_CC
rdp_sec_hash_48(char* out, char* in, char* salt1, char* salt2, int salt)
{
  int i;
  void* sha1_info;
  void* md5_info;
  char pad[4];
  char sha1_sig[20];
  char md5_sig[16];

  sha1_info = ssl_sha1_info_create();
  md5_info = ssl_md5_info_create();
  for (i = 0; i < 3; i++)
  {
    g_memset(pad, salt + i, 4);
    ssl_sha1_clear(sha1_info);
    ssl_sha1_transform(sha1_info, pad, i + 1);
    ssl_sha1_transform(sha1_info, in, 48);
    ssl_sha1_transform(sha1_info, salt1, 32);
    ssl_sha1_transform(sha1_info, salt2, 32);
    ssl_sha1_complete(sha1_info, sha1_sig);
    ssl_md5_clear(md5_info);
    ssl_md5_transform(md5_info, in, 48);
    ssl_md5_transform(md5_info, sha1_sig, 20);
    ssl_md5_complete(md5_info, md5_sig);
    g_memcpy(out + i * 16, md5_sig, 16);
  }
  ssl_sha1_info_delete(sha1_info);
  ssl_md5_info_delete(md5_info);
}

/*****************************************************************************/
void APP_CC
rdp_sec_hash_16(char* out, char* in, char* salt1, char* salt2)
{
  void* md5_info;

  md5_info = ssl_md5_info_create();
  ssl_md5_clear(md5_info);
  ssl_md5_transform(md5_info, in, 16);
  ssl_md5_transform(md5_info, salt1, 32);
  ssl_md5_transform(md5_info, salt2, 32);
  ssl_md5_complete(md5_info, out);
  ssl_md5_info_delete(md5_info);
}

/*****************************************************************************/
static int APP_CC
rdp_sec_generate_keys(struct rdp_sec* self)
{
  char session_key[48];
  char temp_hash[48];
  char input[48];

  g_memcpy(input, self->client_random, 24);
  g_memcpy(input + 24, self->server_random, 24);
  rdp_sec_hash_48(temp_hash, input, self->client_random,
                  self->server_random, 65);
  rdp_sec_hash_48(session_key, temp_hash, self->client_random,
                  self->server_random, 88);
  g_memcpy(self->sign_key, session_key, 16);
  rdp_sec_hash_16(self->decrypt_key, session_key + 16, self->client_random,
                  self->server_random);
  rdp_sec_hash_16(self->encrypt_key, session_key + 32, self->client_random,
                  self->server_random);
  DEBUG((" rdp_sec_generate_keys, rc4_key_size is %d", self->rc4_key_size));
  DEBUG((" rdp_sec_generate_keys, crypt_level is %d", self->crypt_level));
  if (self->rc4_key_size == 1)
  {
    rdp_sec_make_40bit(self->sign_key);
    rdp_sec_make_40bit(self->encrypt_key);
    rdp_sec_make_40bit(self->decrypt_key);
    self->rc4_key_len = 8;
  }
  else
  {
    self->rc4_key_len = 16;
  }
  g_memcpy(self->decrypt_update_key, self->decrypt_key, 16);
  g_memcpy(self->encrypt_update_key, self->encrypt_key, 16);
  ssl_rc4_set_key(self->decrypt_rc4_info, self->decrypt_key, self->rc4_key_len);
  ssl_rc4_set_key(self->encrypt_rc4_info, self->encrypt_key, self->rc4_key_len);
  return 0;
}

/*****************************************************************************/
/* Process crypto information blob */
static void APP_CC
rdp_sec_process_crypt_info(struct rdp_sec* self, struct stream* s)
{
  char modulus[64];
  char exponent[64];

  g_memset(modulus, 0, sizeof(modulus));
  g_memset(exponent, 0, sizeof(exponent));
  if (!rdp_sec_parse_crypt_info(self, s, modulus, exponent))
  {
    DEBUG((" error in rdp_sec_process_crypt_info"));
    return;
  }
  /* Generate a client random, and determine encryption keys */
  g_random(self->client_random, 32);
  rdp_sec_rsa_op(self->client_crypt_random, self->client_random,
                 modulus, exponent);
  rdp_sec_generate_keys(self);
}

/*****************************************************************************/
/* Process connect response data blob */
static void APP_CC
rdp_sec_process_mcs_data(struct rdp_sec* self)
{
  int tag;
  int length;
  int len;
  char* next_tag;
  struct stream* s;

  s = self->server_mcs_data;
  s->p = s->data;
  in_uint8s(s, 21); /* header (T.124 ConferenceCreateResponse) */
  in_uint8(s, len);
  if (len & 0x80)
  {
    in_uint8(s, len);
  }
  while (s->p < s->end)
  {
    in_uint16_le(s, tag);
    in_uint16_le(s, length);
    DEBUG((" rdp_sec_process_mcs_data tag %d length %d", tag, length));
    if (length <= 4)
    {
      return;
    }
    next_tag = (s->p + length) - 4;
    switch (tag)
    {
      case SEC_TAG_SRV_INFO:
        //rdp_sec_process_srv_info(self, s);
        break;
      case SEC_TAG_SRV_CRYPT:
        rdp_sec_process_crypt_info(self, s);
        break;
      case SEC_TAG_SRV_CHANNELS:
        break;
      default:
        break;
    }
    s->p = next_tag;
  }
}

/*****************************************************************************/
/* Transfer the client random to the server */
/* returns error */
static int APP_CC
rdp_sec_establish_key(struct rdp_sec* self)
{
  int length;
  int flags;
  struct stream* s;

  DEBUG((" sending client random"));
  make_stream(s);
  init_stream(s, 8192);
  length = SEC_MODULUS_SIZE + SEC_PADDING_SIZE;
  flags = SEC_CLIENT_RANDOM;
  if (rdp_sec_init(self, s, flags) != 0)
  {
    free_stream(s);
    return 1;
  }
  out_uint32_le(s, length);
  out_uint8p(s, self->client_crypt_random, SEC_MODULUS_SIZE);
  out_uint8s(s, SEC_PADDING_SIZE);
  s_mark_end(s);
  if (rdp_sec_send(self, s, flags) != 0)
  {
    free_stream(s);
    return 1;
  }
  free_stream(s);
  return 0;
}

/*****************************************************************************/
/* Establish a secure connection */
int APP_CC
rdp_sec_connect(struct rdp_sec* self, char* ip, char* port)
{
  DEBUG((" in rdp_sec_connect"));
  rdp_sec_out_mcs_data(self);
  if (rdp_mcs_connect(self->mcs_layer, ip, port) != 0)
  {
    DEBUG((" out rdp_sec_connect error rdp_mcs_connect failed"));
    return 1;
  }
  rdp_sec_process_mcs_data(self);
  if (rdp_sec_establish_key(self) != 0)
  {
    DEBUG((" out rdp_sec_connect error rdp_sec_establish_key failed"));
    return 1;
  }
  DEBUG((" out rdp_sec_connect"));
  return 0;
}

/*****************************************************************************/
/* returns error */
int APP_CC
rdp_sec_init(struct rdp_sec* self, struct stream* s, int flags)
{
  if (rdp_mcs_init(self->mcs_layer, s) != 0)
  {
    return 1;
  }
  if (flags & SEC_ENCRYPT)
  {
    s_push_layer(s, sec_hdr, 12);
  }
  else
  {
    s_push_layer(s, sec_hdr, 4);
  }
  return 0;
}

/*****************************************************************************/
/* Output a uint32 into a buffer (little-endian) */
void APP_CC
rdp_sec_buf_out_uint32(char* buffer, int value)
{
  buffer[0] = value & 0xff;
  buffer[1] = (value >> 8) & 0xff;
  buffer[2] = (value >> 16) & 0xff;
  buffer[3] = (value >> 24) & 0xff;
}

/*****************************************************************************/
/* Generate a MAC hash (5.2.3.1), using a combination of SHA1 and MD5 */
void APP_CC
rdp_sec_sign(char* signature, int siglen, char* session_key, int keylen,
             char* data, int datalen)
{
  char shasig[20];
  char md5sig[16];
  char lenhdr[4];
  void* sha1_context;
  void* md5_context;

  rdp_sec_buf_out_uint32(lenhdr, datalen);
  sha1_context = ssl_sha1_info_create();
  ssl_sha1_clear(sha1_context);
  ssl_sha1_transform(sha1_context, session_key, keylen);
  ssl_sha1_transform(sha1_context, g_pad_54, 40);
  ssl_sha1_transform(sha1_context, lenhdr, 4);
  ssl_sha1_transform(sha1_context, data, datalen);
  ssl_sha1_complete(sha1_context, shasig);
  ssl_sha1_info_delete(sha1_context);
  md5_context = ssl_md5_info_create();
  ssl_md5_clear(md5_context);
  ssl_md5_transform(md5_context, session_key, keylen);
  ssl_md5_transform(md5_context, g_pad_92, 48);
  ssl_md5_transform(md5_context, shasig, 20);
  ssl_md5_complete(md5_context, md5sig);
  ssl_md5_info_delete(md5_context);
  g_memcpy(signature, md5sig, siglen);
}

/*****************************************************************************/
/* Encrypt data using RC4 */
static void APP_CC
rdp_sec_encrypt(struct rdp_sec* self, char* data, int length)
{
  if (self->encrypt_use_count == 4096)
  {
    rdp_sec_update(self->encrypt_key, self->encrypt_update_key,
                   self->rc4_key_len);
    ssl_rc4_set_key(self->encrypt_rc4_info, self->encrypt_key,
                    self->rc4_key_len);
    self->encrypt_use_count = 0;
  }
  ssl_rc4_crypt(self->encrypt_rc4_info, data, length);
  self->encrypt_use_count++;
}

/*****************************************************************************/
/* returns error */
int APP_CC
rdp_sec_send(struct rdp_sec* self, struct stream* s, int flags)
{
  int datalen;

  DEBUG((" in rdp_sec_send flags %8.8x", flags));
  s_pop_layer(s, sec_hdr);
  out_uint32_le(s, flags);
  if (flags & SEC_ENCRYPT)
  {
    datalen = (s->end - s->p) - 8;
    rdp_sec_sign(s->p, 8, self->sign_key, self->rc4_key_len, s->p + 8,
                 datalen);
    rdp_sec_encrypt(self, s->p + 8, datalen);
  }
  if (rdp_mcs_send(self->mcs_layer, s) != 0)
  {
    DEBUG((" out rdp_sec_send, rdp_mcs_send failed"));
    return 1;
  }
  DEBUG((" out rdp_sec_send"));
  return 0;
}
