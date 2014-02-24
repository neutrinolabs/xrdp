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
 */

#if !defined(SSL_CALLS_H)
#define SSL_CALLS_H

#include "arch.h"

int
ssl_init(void);
int
ssl_finish(void);
void* APP_CC
ssl_rc4_info_create(void);
void APP_CC
ssl_rc4_info_delete(void* rc4_info);
void APP_CC
ssl_rc4_set_key(void* rc4_info, char* key, int len);
void APP_CC
ssl_rc4_crypt(void* rc4_info, char* data, int len);
void* APP_CC
ssl_sha1_info_create(void);
void APP_CC
ssl_sha1_info_delete(void* sha1_info);
void APP_CC
ssl_sha1_clear(void* sha1_info);
void APP_CC
ssl_sha1_transform(void* sha1_info, char* data, int len);
void APP_CC
ssl_sha1_complete(void* sha1_info, char* data);
void* APP_CC
ssl_md5_info_create(void);
void APP_CC
ssl_md5_info_delete(void* md5_info);
void APP_CC
ssl_md5_clear(void* md5_info);
void APP_CC
ssl_md5_transform(void* md5_info, char* data, int len);
void APP_CC
ssl_md5_complete(void* md5_info, char* data);
void *APP_CC
ssl_des3_encrypt_info_create(const char *key, const char* ivec);
void *APP_CC
ssl_des3_decrypt_info_create(const char *key, const char* ivec);
void APP_CC
ssl_des3_info_delete(void *des3);
int APP_CC
ssl_des3_encrypt(void *des3, int length, const char *in_data, char *out_data);
int APP_CC
ssl_des3_decrypt(void *des3, int length, const char *in_data, char *out_data);
void * APP_CC
ssl_hmac_info_create(void);
void APP_CC
ssl_hmac_info_delete(void *hmac);
void APP_CC
ssl_hmac_sha1_init(void *hmac, const char *data, int len);
void APP_CC
ssl_hmac_transform(void *hmac, const char *data, int len);
void APP_CC
ssl_hmac_complete(void *hmac, char *data, int len);
int APP_CC
ssl_mod_exp(char* out, int out_len, char* in, int in_len,
            char* mod, int mod_len, char* exp, int exp_len);
int APP_CC
ssl_gen_key_xrdp1(int key_size_in_bits, char* exp, int exp_len,
                  char* mod, int mod_len, char* pri, int pri_len);

#endif
