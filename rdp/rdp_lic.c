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

   licence

*/

#include "rdp.h"

/*****************************************************************************/
struct rdp_lic* APP_CC
rdp_lic_create(struct rdp_sec* owner)
{
  struct rdp_lic* self;

  self = (struct rdp_lic*)g_malloc(sizeof(struct rdp_lic), 1);
  self->sec_layer = owner;
  return self;
}

/*****************************************************************************/
void APP_CC
rdp_lic_delete(struct rdp_lic* self)
{
  if (self == 0)
  {
    return;
  }
  g_free(self);
}

/*****************************************************************************/
/* Generate a session key and RC4 keys, given client and server randoms */
static void APP_CC
rdp_lic_generate_keys(struct rdp_lic* self, char* client_random,
                      char* server_random, char* pre_master_secret)
{
  char master_secret[48];
  char key_block[48];

  /* Generate master secret and then key material */
  rdp_sec_hash_48(master_secret, pre_master_secret, client_random,
                  server_random, 65);
  rdp_sec_hash_48(key_block, master_secret, server_random,
                  client_random, 65);
  /* Store first 16 bytes of session key as MAC secret */
  g_memcpy(self->licence_sign_key, key_block, 16);
  /* Generate RC4 key from next 16 bytes */
  rdp_sec_hash_16(self->licence_key, key_block + 16, client_random,
                  server_random);
}

/*****************************************************************************/
static void APP_CC
rdp_lic_generate_hwid(struct rdp_lic* self, char* hwid)
{
  rdp_sec_buf_out_uint32(hwid, 2);
  g_strncpy(hwid + 4, self->sec_layer->rdp_layer->mod->hostname,
            LICENCE_HWID_SIZE - 4);
}

/*****************************************************************************/
/* Present an existing licence to the server */
static void APP_CC
rdp_lic_present(struct rdp_lic* self, char* client_random, char* rsa_data,
                char* licence_data, int licence_size, char* hwid,
                char* signature)
{
  int sec_flags;
  int length;
  struct stream* s;

  sec_flags = SEC_LICENCE_NEG;
  length = 16 + SEC_RANDOM_SIZE + SEC_MODULUS_SIZE + SEC_PADDING_SIZE +
           licence_size + LICENCE_HWID_SIZE + LICENCE_SIGNATURE_SIZE;
  make_stream(s);
  init_stream(s, 8192);
  rdp_sec_init(self->sec_layer, s, sec_flags);
  out_uint8(s, LICENCE_TAG_PRESENT);
  out_uint8(s, 2); /* version */
  out_uint16_le(s, length);
  out_uint32_le(s, 1);
  out_uint16_le(s, 0);
  out_uint16_le(s, 0x0201);
  out_uint8p(s, client_random, SEC_RANDOM_SIZE);
  out_uint16_le(s, 0);
  out_uint16_le(s, (SEC_MODULUS_SIZE + SEC_PADDING_SIZE));
  out_uint8p(s, rsa_data, SEC_MODULUS_SIZE);
  out_uint8s(s, SEC_PADDING_SIZE);
  out_uint16_le(s, 1);
  out_uint16_le(s, licence_size);
  out_uint8p(s, licence_data, licence_size);
  out_uint16_le(s, 1);
  out_uint16_le(s, LICENCE_HWID_SIZE);
  out_uint8p(s, hwid, LICENCE_HWID_SIZE);
  out_uint8p(s, signature, LICENCE_SIGNATURE_SIZE);
  s_mark_end(s);
  rdp_sec_send(self->sec_layer, s, sec_flags);
  free_stream(s);
}

/*****************************************************************************/
/* Send a licence request packet */
static void APP_CC
rdp_lic_send_request(struct rdp_lic* self, char* client_random,
                     char* rsa_data, char* user, char* host)
{
  int sec_flags;
  int userlen;
  int hostlen;
  int length;
  struct stream* s;

  sec_flags = SEC_LICENCE_NEG;
  userlen = g_strlen(user) + 1;
  hostlen = g_strlen(host) + 1;
  length = 128 + userlen + hostlen;
  make_stream(s);
  init_stream(s, 8192);
  rdp_sec_init(self->sec_layer, s, sec_flags);
  out_uint8(s, LICENCE_TAG_REQUEST);
  out_uint8(s, 2); /* version */
  out_uint16_le(s, length);
  out_uint32_le(s, 1);
  out_uint16_le(s, 0);
  out_uint16_le(s, 0xff01);
  out_uint8p(s, client_random, SEC_RANDOM_SIZE);
  out_uint16_le(s, 0);
  out_uint16_le(s, (SEC_MODULUS_SIZE + SEC_PADDING_SIZE));
  out_uint8p(s, rsa_data, SEC_MODULUS_SIZE);
  out_uint8s(s, SEC_PADDING_SIZE);
  out_uint16_le(s, LICENCE_TAG_USER);
  out_uint16_le(s, userlen);
  out_uint8p(s, user, userlen);
  out_uint16_le(s, LICENCE_TAG_HOST);
  out_uint16_le(s, hostlen);
  out_uint8p(s, host, hostlen);
  s_mark_end(s);
  rdp_sec_send(self->sec_layer, s, sec_flags);
  free_stream(s);
}

/*****************************************************************************/
/* Process a licence demand packet */
static void APP_CC
rdp_lic_process_demand(struct rdp_lic* self, struct stream* s)
{
  char null_data[SEC_MODULUS_SIZE];
  char* server_random;
  char signature[LICENCE_SIGNATURE_SIZE];
  char hwid[LICENCE_HWID_SIZE];
  char* licence_data;
  int licence_size;
  void* crypt_key;

  licence_data = 0;
  /* Retrieve the server random from the incoming packet */
  in_uint8p(s, server_random, SEC_RANDOM_SIZE);
  /* We currently use null client keys. This is a bit naughty but, hey,
     the security of licence negotiation isn't exactly paramount. */
  g_memset(null_data, 0, sizeof(null_data));
  rdp_lic_generate_keys(self, null_data, server_random, null_data);
  licence_size = 0; /* todo load_licence(&licence_data); */
  if (licence_size > 0)
  {
    /* Generate a signature for the HWID buffer */
    rdp_lic_generate_hwid(self, hwid);
    rdp_sec_sign(signature, 16, self->licence_sign_key, 16,
                 hwid, sizeof(hwid));
    /* Now encrypt the HWID */
    crypt_key = ssl_rc4_info_create();
    ssl_rc4_set_key(crypt_key, self->licence_key, 16);
    ssl_rc4_crypt(crypt_key, hwid, sizeof(hwid));
    ssl_rc4_info_delete(crypt_key);
    rdp_lic_present(self, null_data, null_data, licence_data,
                    licence_size, hwid, signature);
    g_free(licence_data);
    return;
  }
  rdp_lic_send_request(self, null_data, null_data,
                       self->sec_layer->rdp_layer->mod->username,
                       self->sec_layer->rdp_layer->mod->hostname);
}

/*****************************************************************************/
/* Send an authentication response packet */
static void APP_CC
rdp_lic_send_authresp(struct rdp_lic* self, char* token, char* crypt_hwid,
                      char* signature)
{
  int sec_flags;
  int length;
  struct stream* s;

  sec_flags = SEC_LICENCE_NEG;
  length = 58;
  make_stream(s);
  init_stream(s, 8192);
  rdp_sec_init(self->sec_layer, s, sec_flags);
  out_uint8(s, LICENCE_TAG_AUTHRESP);
  out_uint8(s, 2); /* version */
  out_uint16_le(s, length);
  out_uint16_le(s, 1);
  out_uint16_le(s, LICENCE_TOKEN_SIZE);
  out_uint8p(s, token, LICENCE_TOKEN_SIZE);
  out_uint16_le(s, 1);
  out_uint16_le(s, LICENCE_HWID_SIZE);
  out_uint8p(s, crypt_hwid, LICENCE_HWID_SIZE);
  out_uint8p(s, signature, LICENCE_SIGNATURE_SIZE);
  s_mark_end(s);
  rdp_sec_send(self->sec_layer, s, sec_flags);
  free_stream(s);
}

/*****************************************************************************/
/* Parse an authentication request packet */
/* returns boolean */
static int APP_CC
rdp_lic_parse_authreq(struct rdp_lic* self, struct stream* s,
                      char** token, char** signature)
{
  int tokenlen;

  in_uint8s(s, 6); /* unknown: f8 3d 15 00 04 f6 */
  in_uint16_le(s, tokenlen);
  if (tokenlen != LICENCE_TOKEN_SIZE)
  {
    /* error("token len %d\n", tokenlen); */
    return 0;
  }
  in_uint8p(s, *token, tokenlen);
  in_uint8p(s, *signature, LICENCE_SIGNATURE_SIZE);
  return s_check_end(s);
}

/*****************************************************************************/
/* Process an authentication request packet */
static void APP_CC
rdp_lic_process_authreq(struct rdp_lic* self, struct stream* s)
{
  char* in_token;
  char* in_sig;
  char out_token[LICENCE_TOKEN_SIZE];
  char decrypt_token[LICENCE_TOKEN_SIZE];
  char hwid[LICENCE_HWID_SIZE];
  char crypt_hwid[LICENCE_HWID_SIZE];
  char sealed_buffer[LICENCE_TOKEN_SIZE + LICENCE_HWID_SIZE];
  char out_sig[LICENCE_SIGNATURE_SIZE];
  void* crypt_key;

  in_token = 0;
  in_sig = 0;
  /* Parse incoming packet and save the encrypted token */
  rdp_lic_parse_authreq(self, s, &in_token, &in_sig);
  g_memcpy(out_token, in_token, LICENCE_TOKEN_SIZE);
  /* Decrypt the token. It should read TEST in Unicode. */
  crypt_key = ssl_rc4_info_create();
  ssl_rc4_set_key(crypt_key, self->licence_key, 16);
  g_memcpy(decrypt_token, in_token, LICENCE_TOKEN_SIZE);
  ssl_rc4_crypt(crypt_key, decrypt_token, LICENCE_TOKEN_SIZE);
  /* Generate a signature for a buffer of token and HWID */
  rdp_lic_generate_hwid(self, hwid);
  g_memcpy(sealed_buffer, decrypt_token, LICENCE_TOKEN_SIZE);
  g_memcpy(sealed_buffer + LICENCE_TOKEN_SIZE, hwid, LICENCE_HWID_SIZE);
  rdp_sec_sign(out_sig, 16, self->licence_sign_key, 16, sealed_buffer,
               sizeof(sealed_buffer));
  /* Now encrypt the HWID */
  ssl_rc4_set_key(crypt_key, self->licence_key, 16);
  g_memcpy(crypt_hwid, hwid, LICENCE_HWID_SIZE);
  ssl_rc4_crypt(crypt_key, crypt_hwid, LICENCE_HWID_SIZE);
  rdp_lic_send_authresp(self, out_token, crypt_hwid, out_sig);
  ssl_rc4_info_delete(crypt_key);
}

/*****************************************************************************/
/* Process an licence issue packet */
static void APP_CC
rdp_lic_process_issue(struct rdp_lic* self, struct stream* s)
{
  void* crypt_key;
  int length;
  int check;
  int i;

  in_uint8s(s, 2); /* 3d 45 - unknown */
  in_uint16_le(s, length);
  if (!s_check_rem(s, length))
  {
    return;
  }
  crypt_key = ssl_rc4_info_create();
  ssl_rc4_set_key(crypt_key, self->licence_key, 16);
  ssl_rc4_crypt(crypt_key, s->p, length);
  ssl_rc4_info_delete(crypt_key);
  in_uint16_le(s, check);
  if (check != 0)
  {
    return;
  }
  self->licence_issued = 1;
  in_uint8s(s, 2); /* pad */
  /* advance to fourth string */
  length = 0;
  for (i = 0; i < 4; i++)
  {
    in_uint8s(s, length);
    in_uint32_le(s, length);
    if (!s_check_rem(s, length))
    {
      return;
    }
  }
  /* todo save_licence(s->p, length); */
}

/******************************************************************************/
/* Process a licence packet */
void APP_CC
rdp_lic_process(struct rdp_lic* self, struct stream* s)
{
  int tag;

  in_uint8(s, tag);
  in_uint8s(s, 3); /* version, length */
  switch (tag)
  {
    case LICENCE_TAG_DEMAND:
      rdp_lic_process_demand(self, s);
      break;
    case LICENCE_TAG_AUTHREQ:
      rdp_lic_process_authreq(self, s);
      break;
    case LICENCE_TAG_ISSUE:
      rdp_lic_process_issue(self, s);
      break;
    case LICENCE_TAG_REISSUE:
    case LICENCE_TAG_RESULT:
      break;
    default:
      break;
      /* todo unimpl("licence tag 0x%x\n", tag); */
  }
}
