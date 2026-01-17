/**
 * NeutrinoCrypto - Pure C cryptographic primitives for xrdp
 *
 * Provides crypto functions needed for RDP protocol without external dependencies
 * Copyright (C) 2026 Neutrinos Software Corporation
 */

#ifndef NEUTRINOCRYPTO_H
#define NEUTRINOCRYPTO_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* RC4 stream cipher */
typedef struct {
    unsigned char S[256];
    int i;
    int j;
} nc_rc4_ctx;

void nc_rc4_set_key(nc_rc4_ctx *ctx, const uint8_t *key, size_t key_len);
void nc_rc4_crypt(nc_rc4_ctx *ctx, uint8_t *data, size_t len);

/* MD5 hash */
typedef struct {
    uint32_t state[4];
    uint64_t count;
    uint8_t buffer[64];
} nc_md5_ctx;

void nc_md5_init(nc_md5_ctx *ctx);
void nc_md5_update(nc_md5_ctx *ctx, const uint8_t *data, size_t len);
void nc_md5_final(nc_md5_ctx *ctx, uint8_t digest[16]);

/* SHA-1 hash */
typedef struct {
    uint32_t state[5];
    uint64_t count;
    uint8_t buffer[64];
} nc_sha1_ctx;

void nc_sha1_init(nc_sha1_ctx *ctx);
void nc_sha1_update(nc_sha1_ctx *ctx, const uint8_t *data, size_t len);
void nc_sha1_final(nc_sha1_ctx *ctx, uint8_t digest[20]);

/* DES3-CBC (Triple DES) */
typedef struct {
    uint64_t keys[3][16]; /* 3 keys, 16 subkeys each */
} nc_des3_ctx;

void nc_des3_set_key(nc_des3_ctx *ctx, const uint8_t key[24]);
void nc_des3_cbc_encrypt(nc_des3_ctx *ctx, const uint8_t *iv,
                         const uint8_t *in, uint8_t *out, size_t len);
void nc_des3_cbc_decrypt(nc_des3_ctx *ctx, const uint8_t *iv,
                         const uint8_t *in, uint8_t *out, size_t len);

/* AES-128-ECB (for Apple ARD) */
typedef struct {
    uint32_t round_keys[44]; /* 11 round keys for AES-128 */
} nc_aes128_ctx;

void nc_aes128_set_key(nc_aes128_ctx *ctx, const uint8_t key[16]);
void nc_aes128_ecb_encrypt(nc_aes128_ctx *ctx, const uint8_t in[16], uint8_t out[16]);

/* HMAC-SHA1 */
void nc_hmac_sha1(const uint8_t *key, size_t key_len,
                  const uint8_t *data, size_t data_len,
                  uint8_t output[20]);

/* HMAC-MD5 */
void nc_hmac_md5(const uint8_t *key, size_t key_len,
                 const uint8_t *data, size_t data_len,
                 uint8_t output[16]);

#ifdef __cplusplus
}
#endif

#endif /* NEUTRINOCRYPTO_H */
