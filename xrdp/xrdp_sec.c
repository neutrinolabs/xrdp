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

   Copyright (C) Jay Sorg 2004

   secure layer

*/

#include "xrdp.h"

char pad_54[40] =
{ 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54,
  54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54,
  54, 54, 54, 54, 54, 54, 54, 54 };

char pad_92[48] =
{ 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92,
  92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92,
  92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92 };

char pub_exp[4] = { 0x01, 0x00, 0x01, 0x00 };

char pub_mod[64] =
{ 0x67, 0xab, 0x0e, 0x6a, 0x9f, 0xd6, 0x2b, 0xa3,
  0x32, 0x2f, 0x41, 0xd1, 0xce, 0xee, 0x61, 0xc3,
  0x76, 0x0b, 0x26, 0x11, 0x70, 0x48, 0x8a, 0x8d,
  0x23, 0x81, 0x95, 0xa0, 0x39, 0xf7, 0x5b, 0xaa,
  0x3e, 0xf1, 0xed, 0xb8, 0xc4, 0xee, 0xce, 0x5f,
  0x6a, 0xf5, 0x43, 0xce, 0x5f, 0x60, 0xca, 0x6c,
  0x06, 0x75, 0xae, 0xc0, 0xd6, 0xa4, 0x0c, 0x92,
  0xa4, 0xc6, 0x75, 0xea, 0x64, 0xb2, 0x50, 0x5b };

char pub_sig[64] =
{ 0x6a, 0x41, 0xb1, 0x43, 0xcf, 0x47, 0x6f, 0xf1,
  0xe6, 0xcc, 0xa1, 0x72, 0x97, 0xd9, 0xe1, 0x85,
  0x15, 0xb3, 0xc2, 0x39, 0xa0, 0xa6, 0x26, 0x1a,
  0xb6, 0x49, 0x01, 0xfa, 0xa6, 0xda, 0x60, 0xd7,
  0x45, 0xf7, 0x2c, 0xee, 0xe4, 0x8e, 0x64, 0x2e,
  0x37, 0x49, 0xf0, 0x4c, 0x94, 0x6f, 0x08, 0xf5,
  0x63, 0x4c, 0x56, 0x29, 0x55, 0x5a, 0x63, 0x41,
  0x2c, 0x20, 0x65, 0x95, 0x99, 0xb1, 0x15, 0x7c };

char pri_exp[64] =
{ 0x41, 0x93, 0x05, 0xB1, 0xF4, 0x38, 0xFC, 0x47,
  0x88, 0xC4, 0x7F, 0x83, 0x8C, 0xEC, 0x90, 0xDA,
  0x0C, 0x8A, 0xB5, 0xAE, 0x61, 0x32, 0x72, 0xF5,
  0x2B, 0xD1, 0x7B, 0x5F, 0x44, 0xC0, 0x7C, 0xBD,
  0x8A, 0x35, 0xFA, 0xAE, 0x30, 0xF6, 0xC4, 0x6B,
  0x55, 0xA7, 0x65, 0xEF, 0xF4, 0xB2, 0xAB, 0x18,
  0x4E, 0xAA, 0xE6, 0xDC, 0x71, 0x17, 0x3B, 0x4C,
  0xC2, 0x15, 0x4C, 0xF7, 0x81, 0xBB, 0xF0, 0x03 };

char lic1[322] =
{ 0x80, 0x00, 0x3e, 0x01, 0x01, 0x02, 0x3e, 0x01,
  0x7b, 0x3c, 0x31, 0xa6, 0xae, 0xe8, 0x74, 0xf6,
  0xb4, 0xa5, 0x03, 0x90, 0xe7, 0xc2, 0xc7, 0x39,
  0xba, 0x53, 0x1c, 0x30, 0x54, 0x6e, 0x90, 0x05,
  0xd0, 0x05, 0xce, 0x44, 0x18, 0x91, 0x83, 0x81,
  0x00, 0x00, 0x04, 0x00, 0x2c, 0x00, 0x00, 0x00,
  0x4d, 0x00, 0x69, 0x00, 0x63, 0x00, 0x72, 0x00,
  0x6f, 0x00, 0x73, 0x00, 0x6f, 0x00, 0x66, 0x00,
  0x74, 0x00, 0x20, 0x00, 0x43, 0x00, 0x6f, 0x00,
  0x72, 0x00, 0x70, 0x00, 0x6f, 0x00, 0x72, 0x00,
  0x61, 0x00, 0x74, 0x00, 0x69, 0x00, 0x6f, 0x00,
  0x6e, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
  0x32, 0x00, 0x33, 0x00, 0x36, 0x00, 0x00, 0x00,
  0x0d, 0x00, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00,
  0x03, 0x00, 0xb8, 0x00, 0x01, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
  0x06, 0x00, 0x5c, 0x00, 0x52, 0x53, 0x41, 0x31,
  0x48, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
  0x3f, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00,
  0x01, 0xc7, 0xc9, 0xf7, 0x8e, 0x5a, 0x38, 0xe4,
  0x29, 0xc3, 0x00, 0x95, 0x2d, 0xdd, 0x4c, 0x3e,
  0x50, 0x45, 0x0b, 0x0d, 0x9e, 0x2a, 0x5d, 0x18,
  0x63, 0x64, 0xc4, 0x2c, 0xf7, 0x8f, 0x29, 0xd5,
  0x3f, 0xc5, 0x35, 0x22, 0x34, 0xff, 0xad, 0x3a,
  0xe6, 0xe3, 0x95, 0x06, 0xae, 0x55, 0x82, 0xe3,
  0xc8, 0xc7, 0xb4, 0xa8, 0x47, 0xc8, 0x50, 0x71,
  0x74, 0x29, 0x53, 0x89, 0x6d, 0x9c, 0xed, 0x70,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x08, 0x00, 0x48, 0x00, 0xa8, 0xf4, 0x31, 0xb9,
  0xab, 0x4b, 0xe6, 0xb4, 0xf4, 0x39, 0x89, 0xd6,
  0xb1, 0xda, 0xf6, 0x1e, 0xec, 0xb1, 0xf0, 0x54,
  0x3b, 0x5e, 0x3e, 0x6a, 0x71, 0xb4, 0xf7, 0x75,
  0xc8, 0x16, 0x2f, 0x24, 0x00, 0xde, 0xe9, 0x82,
  0x99, 0x5f, 0x33, 0x0b, 0xa9, 0xa6, 0x94, 0xaf,
  0xcb, 0x11, 0xc3, 0xf2, 0xdb, 0x09, 0x42, 0x68,
  0x29, 0x56, 0x58, 0x01, 0x56, 0xdb, 0x59, 0x03,
  0x69, 0xdb, 0x7d, 0x37, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
  0x0e, 0x00, 0x0e, 0x00, 0x6d, 0x69, 0x63, 0x72,
  0x6f, 0x73, 0x6f, 0x66, 0x74, 0x2e, 0x63, 0x6f,
  0x6d, 0x00 };

char lic2[20] =
{ 0x80, 0x00, 0x10, 0x00, 0xff, 0x02, 0x10, 0x00,
  0x07, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
  0x28, 0x14, 0x00, 0x00 };

/*****************************************************************************/
struct xrdp_sec* xrdp_sec_create(struct xrdp_rdp* owner)
{
  struct xrdp_sec* self;

  self = (struct xrdp_sec*)g_malloc(sizeof(struct xrdp_sec), 1);
  self->rdp_layer = owner;
  self->in_s = owner->in_s;
  self->out_s = owner->out_s;
  self->rc4_key_size = 1;
  self->decrypt_rc4_info = g_rc4_info_create();
  self->encrypt_rc4_info = g_rc4_info_create();
  g_random(self->server_random, 32);
  self->mcs_layer = xrdp_mcs_create(self);
  return self;
}

/*****************************************************************************/
void xrdp_sec_delete(struct xrdp_sec* self)
{
  if (self == 0)
    return;
  xrdp_mcs_delete(self->mcs_layer);
  g_rc4_info_delete(self->decrypt_rc4_info);
  g_rc4_info_delete(self->encrypt_rc4_info);
  g_free(self->client_mcs_data.data);
  g_free(self->server_mcs_data.data);
  g_free(self);
}

/*****************************************************************************/
/* returns error */
int xrdp_sec_init(struct xrdp_sec* self, int len)
{
  if (xrdp_mcs_init(self->mcs_layer, len + 4) != 0)
    return 1;
  s_push_layer(self->out_s, sec_hdr, 4);
  return 0;
}

/*****************************************************************************/
/* Reduce key entropy from 64 to 40 bits */
void xrdp_sec_make_40bit(char* key)
{
  key[0] = 0xd1;
  key[1] = 0x26;
  key[2] = 0x9e;
}

/*****************************************************************************/
/* returns error */
/* update an encryption key */
int xrdp_sec_update(char* key, char* update_key, int key_len)
{
  char shasig[20];
  void* sha1_info;
  void* md5_info;
  void* rc4_info;

  sha1_info = g_sha1_info_create();
  md5_info = g_md5_info_create();
  rc4_info = g_rc4_info_create();
  g_sha1_clear(sha1_info);
  g_sha1_transform(sha1_info, update_key, key_len);
  g_sha1_transform(sha1_info, pad_54, 40);
  g_sha1_transform(sha1_info, key, key_len);
  g_sha1_complete(sha1_info, shasig);
  g_md5_clear(md5_info);
  g_md5_transform(md5_info, update_key, key_len);
  g_md5_transform(md5_info, pad_92, 48);
  g_md5_transform(md5_info, shasig, 20);
  g_md5_complete(md5_info, key);
  g_rc4_set_key(rc4_info, key, key_len);
  g_rc4_crypt(rc4_info, key, key_len);
  if (key_len == 8)
    xrdp_sec_make_40bit(key);
  g_sha1_info_delete(sha1_info);
  g_md5_info_delete(md5_info);
  g_rc4_info_delete(rc4_info);
  return 0;
}

/*****************************************************************************/
void xrdp_sec_decrypt(struct xrdp_sec* self, char* data, int len)
{
  if (self->decrypt_use_count == 4096)
  {
    xrdp_sec_update(self->decrypt_key, self->decrypt_update_key,
                    self->rc4_key_len);
    g_rc4_set_key(self->decrypt_rc4_info, self->decrypt_key,
                  self->rc4_key_len);
    self->decrypt_use_count = 0;
  }
  g_rc4_crypt(self->decrypt_rc4_info, data, len);
  self->decrypt_use_count++;
}

/*****************************************************************************/
/* returns error */
int xrdp_sec_process_logon_info(struct xrdp_sec* self)
{
  int flags;
  int len_domain;
  int len_user;
  int len_password;
  int len_program;
  int len_directory;

  in_uint8s(self->in_s, 4);
  in_uint32_le(self->in_s, flags);
  DEBUG(("in xrdp_sec_process_logon_info flags $%x\n", flags));
  /* this is the first test that the decrypt is working */
  if ((flags & RDP_LOGON_NORMAL) != RDP_LOGON_NORMAL) /* 0x33 */
    return 1;                                         /* must be or error */
  if (flags & RDP_LOGON_AUTO)
    ;
  if (flags & RDP_COMPRESSION)
    ;
  in_uint16_le(self->in_s, len_domain);
  in_uint16_le(self->in_s, len_user);
  in_uint16_le(self->in_s, len_password);
  in_uint16_le(self->in_s, len_program);
  in_uint16_le(self->in_s, len_directory);
  in_uint8s(self->in_s, len_domain + 2);
  in_uint8s(self->in_s, len_user + 2);
  in_uint8s(self->in_s, len_password + 2);
  in_uint8s(self->in_s, len_program + 2);
  in_uint8s(self->in_s, len_directory + 2);
  return 0;
}

/*****************************************************************************/
/* returns error */
int xrdp_sec_send_lic_initial(struct xrdp_sec* self)
{
  if (xrdp_mcs_init(self->mcs_layer, 322) != 0)
    return 1;
  out_uint8a(self->out_s, lic1, 322);
  s_mark_end(self->out_s);
  if (xrdp_mcs_send(self->mcs_layer) != 0)
    return 1;
  return 0;
}

/*****************************************************************************/
/* returns error */
int xrdp_sec_send_lic_response(struct xrdp_sec* self)
{
  if (xrdp_mcs_init(self->mcs_layer, 20) != 0)
    return 1;
  out_uint8a(self->out_s, lic2, 20);
  s_mark_end(self->out_s);
  if (xrdp_mcs_send(self->mcs_layer) != 0)
    return 1;
  return 0;
}

/*****************************************************************************/
void xrdp_sec_reverse(char* p, int len)
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
void xrdp_sec_rsa_op(char* out, char* in, char* mod, char* exp)
{
  char lexp[64];
  char lmod[64];
  char lin[64];
  char lout[64];
  int len;

  g_memcpy(lexp, exp, 64);
  g_memcpy(lmod, mod, 64);
  g_memcpy(lin, in, 64);
  xrdp_sec_reverse(lexp, 64);
  xrdp_sec_reverse(lmod, 64);
  xrdp_sec_reverse(lin, 64);
  g_memset(lout, 0, 64);
  len = g_mod_exp(lout, lin, lmod, lexp);
  xrdp_sec_reverse(lout, len);
  g_memcpy(out, lout, 64);
}

/*****************************************************************************/
void xrdp_sec_hash_48(char* out, char* in, char* salt1, char* salt2, int salt)
{
  int i;
  void* sha1_info;
  void* md5_info;
  char pad[4];
  char sha1_sig[20];
  char md5_sig[16];

  sha1_info = g_sha1_info_create();
  md5_info = g_md5_info_create();
  for (i = 0; i < 3; i++)
  {
    g_memset(pad, salt + i, 4);
    g_sha1_clear(sha1_info);
    g_sha1_transform(sha1_info, pad, i + 1);
    g_sha1_transform(sha1_info, in, 48);
    g_sha1_transform(sha1_info, salt1, 32);
    g_sha1_transform(sha1_info, salt2, 32);
    g_sha1_complete(sha1_info, sha1_sig);
    g_md5_clear(md5_info);
    g_md5_transform(md5_info, in, 48);
    g_md5_transform(md5_info, sha1_sig, 20);
    g_md5_complete(md5_info, md5_sig);
    g_memcpy(out + i * 16, md5_sig, 16);
  }
  g_sha1_info_delete(sha1_info);
  g_md5_info_delete(md5_info);
}

/*****************************************************************************/
void xrdp_sec_hash_16(char* out, char* in, char* salt1, char* salt2)
{
  void* md5_info;

  md5_info = g_md5_info_create();
  g_md5_clear(md5_info);
  g_md5_transform(md5_info, in, 16);
  g_md5_transform(md5_info, salt1, 32);
  g_md5_transform(md5_info, salt2, 32);
  g_md5_complete(md5_info, out);
  g_md5_info_delete(md5_info);
}

/*****************************************************************************/
void xrdp_sec_establish_keys(struct xrdp_sec* self)
{
  char session_key[48];
  char temp_hash[48];
  char input[48];

  g_memcpy(input, self->client_random, 24);
  g_memcpy(input + 24, self->server_random, 24);
  xrdp_sec_hash_48(temp_hash, input, self->client_random,
                   self->server_random, 65);
  xrdp_sec_hash_48(session_key, temp_hash, self->client_random,
                   self->server_random, 88);
  g_memcpy(self->sign_key, session_key, 16);
  xrdp_sec_hash_16(self->encrypt_key, session_key + 16, self->client_random,
                   self->server_random);
  xrdp_sec_hash_16(self->decrypt_key, session_key + 32, self->client_random,
                   self->server_random);
  if (self->rc4_key_size == 1)
  {
    xrdp_sec_make_40bit(self->sign_key);
    xrdp_sec_make_40bit(self->encrypt_key);
    xrdp_sec_make_40bit(self->decrypt_key);
    self->rc4_key_len = 8;
  }
  else
    self->rc4_key_len = 16;
  g_memcpy(self->decrypt_update_key, self->decrypt_key, 16);
  g_memcpy(self->encrypt_update_key, self->encrypt_key, 16);
  g_rc4_set_key(self->decrypt_rc4_info, self->decrypt_key, self->rc4_key_len);
  g_rc4_set_key(self->encrypt_rc4_info, self->encrypt_key, self->rc4_key_len);
}

/*****************************************************************************/
/* returns error */
int xrdp_sec_recv(struct xrdp_sec* self, int* chan)
{
  int flags;
  int len;

  DEBUG((" in xrdp_sec_recv\n\r"));
  if (xrdp_mcs_recv(self->mcs_layer, chan) != 0)
    return 1;
  in_uint32_le(self->in_s, flags);
  DEBUG((" in xrdp_sec_recv flags $%x\n\r", flags));
  if (flags & SEC_ENCRYPT) /* 0x08 */
  {
    in_uint8s(self->in_s, 8); /* signature */
    xrdp_sec_decrypt(self, self->in_s->p, self->in_s->end - self->in_s->p);
  }
  if (flags & SEC_CLIENT_RANDOM) /* 0x01 */
  {
    in_uint32_le(self->in_s, len);
    in_uint8a(self->in_s, self->client_crypt_random, 64);
    xrdp_sec_rsa_op(self->client_random, self->client_crypt_random,
                    pub_mod, pri_exp);
    xrdp_sec_establish_keys(self);
    *chan = 1; /* just set a non existing channel and exit */
    return 0;
  }
  if (flags & SEC_LOGON_INFO) /* 0x40 */
  {
    if (xrdp_sec_process_logon_info(self) != 0)
      return 1;
    if (xrdp_sec_send_lic_initial(self) != 0)
      return 1;
    *chan = 1; /* just set a non existing channel and exit */
    return 0;
  }
  if (flags & SEC_LICENCE_NEG) /* 0x80 */
  {
    if (xrdp_sec_send_lic_response(self) != 0)
      return 1;
    return -1; /* special error that means send demand active */
  }
  DEBUG((" out xrdp_sec_recv error\n\r"));
  return 0;
}

/*****************************************************************************/
/* returns error */
/* TODO needs outgoing encryption */
int xrdp_sec_send(struct xrdp_sec* self, int flags)
{
  DEBUG((" in xrdp_sec_send\n\r"));
  s_pop_layer(self->out_s, sec_hdr);
  out_uint32_le(self->out_s, flags);
  if (xrdp_mcs_send(self->mcs_layer) != 0)
    return 1;
  DEBUG((" out xrdp_sec_send\n\r"));
  return 0;
}

/*****************************************************************************/
/* prepare server mcs data to send in mcs layer */
void xrdp_sec_out_mcs_data(struct xrdp_sec* self)
{
  struct stream* p;

  p = &self->server_mcs_data;
  init_stream(p, 512);
  out_uint16_be(p, 5);
  out_uint16_be(p, 0x14);
  out_uint8(p, 0x7c);
  out_uint16_be(p, 1);
  out_uint8(p, 0x2a);
  out_uint8(p, 0x14);
  out_uint8(p, 0x76);
  out_uint8(p, 0x0a);
  out_uint8(p, 1);
  out_uint8(p, 1);
  out_uint8(p, 0);
  out_uint16_le(p, 0xc001);
  out_uint8(p, 0);
  out_uint8(p, 0x4d); /* M */
  out_uint8(p, 0x63); /* c */
  out_uint8(p, 0x44); /* D */
  out_uint8(p, 0x6e); /* n */
  out_uint16_be(p, 0x80fc);
  out_uint16_le(p, SEC_TAG_SRV_INFO);
  out_uint16_le(p, 8); /* len */
  out_uint8(p, 4); /* 4 = rdp5 1 = rdp4 */
  out_uint8(p, 0);
  out_uint8(p, 8);
  out_uint8(p, 0);
  out_uint16_le(p, SEC_TAG_SRV_CHANNELS);
  out_uint16_le(p, 8); /* len */
  out_uint8(p, 0xeb);
  out_uint8(p, 3);
  out_uint8(p, 0);
  out_uint8(p, 0);
  out_uint16_le(p, SEC_TAG_SRV_CRYPT);
  out_uint16_le(p, 0x00ec); /* len is 236 */
  out_uint32_le(p, 1);      /* key len 1 = 40 bit 2 = 128 bit */
  out_uint32_le(p, 1);      /* crypt level 1 = low 2 = medium 3 = high */
  out_uint32_le(p, 32);     /* 32 bytes random len */
  out_uint32_le(p, 0xb8);   /* 184 bytes rsa info(certificate) len */
  out_uint8a(p, self->server_random, 32);
  /* here to end is certificate */
  /* HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\ */
  /* TermService\Parameters\Certificate */
  out_uint32_le(p, 1);
  out_uint32_le(p, 1);
  out_uint32_le(p, 1);
  out_uint16_le(p, SEC_TAG_PUBKEY);
  out_uint16_le(p, 0x005c); /* 92 bytes length of SEC_TAG_PUBKEY */
  out_uint32_le(p, SEC_RSA_MAGIC);
  out_uint32_le(p, 0x48); /* 72 bytes modulus len */
  out_uint32_be(p, 0x00020000);
  out_uint32_be(p, 0x3f000000);
  out_uint8a(p, pub_exp, 4); /* pub exp */
  out_uint8a(p, pub_mod, 64); /* pub mod */
  out_uint8s(p, 8); /* pad */
  out_uint16_le(p, SEC_TAG_KEYSIG);
  out_uint16_le(p, 72); /* len */
  out_uint8a(p, pub_sig, 64); /* pub sig */
  out_uint8s(p, 8); /* pad */
  /* end certificate */
  s_mark_end(p);
}

/*****************************************************************************/
int xrdp_sec_incoming(struct xrdp_sec* self)
{
  DEBUG(("in xrdp_sec_incoming\n\r"));
  xrdp_sec_out_mcs_data(self);
  if (xrdp_mcs_incoming(self->mcs_layer) != 0)
    return 1;
#ifdef XRDP_DEBUG
  g_printf("client mcs data received\n\r");
  g_hexdump(self->client_mcs_data.data,
            self->client_mcs_data.end - self->client_mcs_data.data);
  g_printf("server mcs data sent\n\r");
  g_hexdump(self->server_mcs_data.data,
            self->server_mcs_data.end - self->server_mcs_data.data);
#endif
  DEBUG(("out xrdp_sec_incoming\n\r"));
  return 0;
}

/*****************************************************************************/
int xrdp_sec_disconnect(struct xrdp_sec* self)
{
  return xrdp_mcs_disconnect(self->mcs_layer);
}
