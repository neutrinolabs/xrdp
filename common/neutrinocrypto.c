/**
 * NeutrinoCrypto - Pure C cryptographic primitives for xrdp
 *
 * Provides crypto functions needed for RDP protocol without external dependencies
 * Copyright (C) 2026 Neutrinos Software Corporation
 *
 * SECURITY NOTE: These implementations are for RDP's legacy crypto only.
 * Modern TLS 1.3 crypto is handled by NeutrinoTLS.
 */

#include "neutrinocrypto.h"
#include <string.h>

/* ========================================================================
 * RC4 Stream Cipher
 * ======================================================================== */

void nc_rc4_set_key(nc_rc4_ctx *ctx, const uint8_t *key, size_t key_len)
{
    int i, j = 0;
    uint8_t t;

    /* Initialize S-box */
    for (i = 0; i < 256; i++)
    {
        ctx->S[i] = i;
    }

    /* Key-scheduling algorithm (KSA) */
    for (i = 0; i < 256; i++)
    {
        j = (j + ctx->S[i] + key[i % key_len]) & 0xff;
        t = ctx->S[i];
        ctx->S[i] = ctx->S[j];
        ctx->S[j] = t;
    }

    ctx->i = 0;
    ctx->j = 0;
}

void nc_rc4_crypt(nc_rc4_ctx *ctx, uint8_t *data, size_t len)
{
    int i = ctx->i;
    int j = ctx->j;
    uint8_t *S = ctx->S;
    uint8_t t, k;

    /* Pseudo-random generation algorithm (PRGA) */
    while (len-- > 0)
    {
        i = (i + 1) & 0xff;
        j = (j + S[i]) & 0xff;
        t = S[i];
        S[i] = S[j];
        S[j] = t;
        k = S[(S[i] + S[j]) & 0xff];
        *data++ ^= k;
    }

    ctx->i = i;
    ctx->j = j;
}

/* ========================================================================
 * MD5 Hash
 * ======================================================================== */

#define MD5_F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define MD5_G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define MD5_H(x, y, z) ((x) ^ (y) ^ (z))
#define MD5_I(x, y, z) ((y) ^ ((x) | (~z)))

#define MD5_ROTLEFT(a, b) (((a) << (b)) | ((a) >> (32 - (b))))

#define MD5_FF(a, b, c, d, x, s, ac) { \
    (a) += MD5_F((b), (c), (d)) + (x) + (uint32_t)(ac); \
    (a) = MD5_ROTLEFT((a), (s)); \
    (a) += (b); \
}

#define MD5_GG(a, b, c, d, x, s, ac) { \
    (a) += MD5_G((b), (c), (d)) + (x) + (uint32_t)(ac); \
    (a) = MD5_ROTLEFT((a), (s)); \
    (a) += (b); \
}

#define MD5_HH(a, b, c, d, x, s, ac) { \
    (a) += MD5_H((b), (c), (d)) + (x) + (uint32_t)(ac); \
    (a) = MD5_ROTLEFT((a), (s)); \
    (a) += (b); \
}

#define MD5_II(a, b, c, d, x, s, ac) { \
    (a) += MD5_I((b), (c), (d)) + (x) + (uint32_t)(ac); \
    (a) = MD5_ROTLEFT((a), (s)); \
    (a) += (b); \
}

static void md5_transform(uint32_t state[4], const uint8_t block[64])
{
    uint32_t a, b, c, d, x[16];
    int i;

    /* Decode input block */
    for (i = 0; i < 16; i++)
    {
        x[i] = ((uint32_t)block[i * 4 + 0]) |
               ((uint32_t)block[i * 4 + 1] << 8) |
               ((uint32_t)block[i * 4 + 2] << 16) |
               ((uint32_t)block[i * 4 + 3] << 24);
    }

    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];

    /* Round 1 */
    MD5_FF(a, b, c, d, x[ 0],  7, 0xd76aa478);
    MD5_FF(d, a, b, c, x[ 1], 12, 0xe8c7b756);
    MD5_FF(c, d, a, b, x[ 2], 17, 0x242070db);
    MD5_FF(b, c, d, a, x[ 3], 22, 0xc1bdceee);
    MD5_FF(a, b, c, d, x[ 4],  7, 0xf57c0faf);
    MD5_FF(d, a, b, c, x[ 5], 12, 0x4787c62a);
    MD5_FF(c, d, a, b, x[ 6], 17, 0xa8304613);
    MD5_FF(b, c, d, a, x[ 7], 22, 0xfd469501);
    MD5_FF(a, b, c, d, x[ 8],  7, 0x698098d8);
    MD5_FF(d, a, b, c, x[ 9], 12, 0x8b44f7af);
    MD5_FF(c, d, a, b, x[10], 17, 0xffff5bb1);
    MD5_FF(b, c, d, a, x[11], 22, 0x895cd7be);
    MD5_FF(a, b, c, d, x[12],  7, 0x6b901122);
    MD5_FF(d, a, b, c, x[13], 12, 0xfd987193);
    MD5_FF(c, d, a, b, x[14], 17, 0xa679438e);
    MD5_FF(b, c, d, a, x[15], 22, 0x49b40821);

    /* Round 2 */
    MD5_GG(a, b, c, d, x[ 1],  5, 0xf61e2562);
    MD5_GG(d, a, b, c, x[ 6],  9, 0xc040b340);
    MD5_GG(c, d, a, b, x[11], 14, 0x265e5a51);
    MD5_GG(b, c, d, a, x[ 0], 20, 0xe9b6c7aa);
    MD5_GG(a, b, c, d, x[ 5],  5, 0xd62f105d);
    MD5_GG(d, a, b, c, x[10],  9, 0x02441453);
    MD5_GG(c, d, a, b, x[15], 14, 0xd8a1e681);
    MD5_GG(b, c, d, a, x[ 4], 20, 0xe7d3fbc8);
    MD5_GG(a, b, c, d, x[ 9],  5, 0x21e1cde6);
    MD5_GG(d, a, b, c, x[14],  9, 0xc33707d6);
    MD5_GG(c, d, a, b, x[ 3], 14, 0xf4d50d87);
    MD5_GG(b, c, d, a, x[ 8], 20, 0x455a14ed);
    MD5_GG(a, b, c, d, x[13],  5, 0xa9e3e905);
    MD5_GG(d, a, b, c, x[ 2],  9, 0xfcefa3f8);
    MD5_GG(c, d, a, b, x[ 7], 14, 0x676f02d9);
    MD5_GG(b, c, d, a, x[12], 20, 0x8d2a4c8a);

    /* Round 3 */
    MD5_HH(a, b, c, d, x[ 5],  4, 0xfffa3942);
    MD5_HH(d, a, b, c, x[ 8], 11, 0x8771f681);
    MD5_HH(c, d, a, b, x[11], 16, 0x6d9d6122);
    MD5_HH(b, c, d, a, x[14], 23, 0xfde5380c);
    MD5_HH(a, b, c, d, x[ 1],  4, 0xa4beea44);
    MD5_HH(d, a, b, c, x[ 4], 11, 0x4bdecfa9);
    MD5_HH(c, d, a, b, x[ 7], 16, 0xf6bb4b60);
    MD5_HH(b, c, d, a, x[10], 23, 0xbebfbc70);
    MD5_HH(a, b, c, d, x[13],  4, 0x289b7ec6);
    MD5_HH(d, a, b, c, x[ 0], 11, 0xeaa127fa);
    MD5_HH(c, d, a, b, x[ 3], 16, 0xd4ef3085);
    MD5_HH(b, c, d, a, x[ 6], 23, 0x04881d05);
    MD5_HH(a, b, c, d, x[ 9],  4, 0xd9d4d039);
    MD5_HH(d, a, b, c, x[12], 11, 0xe6db99e5);
    MD5_HH(c, d, a, b, x[15], 16, 0x1fa27cf8);
    MD5_HH(b, c, d, a, x[ 2], 23, 0xc4ac5665);

    /* Round 4 */
    MD5_II(a, b, c, d, x[ 0],  6, 0xf4292244);
    MD5_II(d, a, b, c, x[ 7], 10, 0x432aff97);
    MD5_II(c, d, a, b, x[14], 15, 0xab9423a7);
    MD5_II(b, c, d, a, x[ 5], 21, 0xfc93a039);
    MD5_II(a, b, c, d, x[12],  6, 0x655b59c3);
    MD5_II(d, a, b, c, x[ 3], 10, 0x8f0ccc92);
    MD5_II(c, d, a, b, x[10], 15, 0xffeff47d);
    MD5_II(b, c, d, a, x[ 1], 21, 0x85845dd1);
    MD5_II(a, b, c, d, x[ 8],  6, 0x6fa87e4f);
    MD5_II(d, a, b, c, x[15], 10, 0xfe2ce6e0);
    MD5_II(c, d, a, b, x[ 6], 15, 0xa3014314);
    MD5_II(b, c, d, a, x[13], 21, 0x4e0811a1);
    MD5_II(a, b, c, d, x[ 4],  6, 0xf7537e82);
    MD5_II(d, a, b, c, x[11], 10, 0xbd3af235);
    MD5_II(c, d, a, b, x[ 2], 15, 0x2ad7d2bb);
    MD5_II(b, c, d, a, x[ 9], 21, 0xeb86d391);

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
}

void nc_md5_init(nc_md5_ctx *ctx)
{
    ctx->count = 0;
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xefcdab89;
    ctx->state[2] = 0x98badcfe;
    ctx->state[3] = 0x10325476;
}

void nc_md5_update(nc_md5_ctx *ctx, const uint8_t *data, size_t len)
{
    size_t i, index, part_len;

    index = (size_t)((ctx->count >> 3) & 0x3f);
    ctx->count += (uint64_t)(len << 3);

    part_len = 64 - index;

    if (len >= part_len)
    {
        memcpy(&ctx->buffer[index], data, part_len);
        md5_transform(ctx->state, ctx->buffer);

        for (i = part_len; i + 63 < len; i += 64)
        {
            md5_transform(ctx->state, &data[i]);
        }

        index = 0;
    }
    else
    {
        i = 0;
    }

    memcpy(&ctx->buffer[index], &data[i], len - i);
}

void nc_md5_final(nc_md5_ctx *ctx, uint8_t digest[16])
{
    uint8_t bits[8];
    size_t index, pad_len;
    static const uint8_t padding[64] = { 0x80 };
    int i;

    /* Save length */
    for (i = 0; i < 8; i++)
    {
        bits[i] = (uint8_t)((ctx->count >> (i * 8)) & 0xff);
    }

    /* Pad to 56 mod 64 */
    index = (size_t)((ctx->count >> 3) & 0x3f);
    pad_len = (index < 56) ? (56 - index) : (120 - index);
    nc_md5_update(ctx, padding, pad_len);

    /* Append length */
    nc_md5_update(ctx, bits, 8);

    /* Store state in digest */
    for (i = 0; i < 4; i++)
    {
        digest[i * 4 + 0] = (uint8_t)((ctx->state[i] >> 0) & 0xff);
        digest[i * 4 + 1] = (uint8_t)((ctx->state[i] >> 8) & 0xff);
        digest[i * 4 + 2] = (uint8_t)((ctx->state[i] >> 16) & 0xff);
        digest[i * 4 + 3] = (uint8_t)((ctx->state[i] >> 24) & 0xff);
    }
}

/* ========================================================================
 * SHA-1 Hash
 * ======================================================================== */

#define SHA1_ROTLEFT(a, b) (((a) << (b)) | ((a) >> (32 - (b))))

static void sha1_transform(uint32_t state[5], const uint8_t block[64])
{
    uint32_t a, b, c, d, e, t, w[80];
    int i;

    /* Prepare message schedule */
    for (i = 0; i < 16; i++)
    {
        w[i] = ((uint32_t)block[i * 4 + 0] << 24) |
               ((uint32_t)block[i * 4 + 1] << 16) |
               ((uint32_t)block[i * 4 + 2] << 8) |
               ((uint32_t)block[i * 4 + 3]);
    }
    for (i = 16; i < 80; i++)
    {
        w[i] = SHA1_ROTLEFT(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }

    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];
    e = state[4];

    /* Main loop */
    for (i = 0; i < 20; i++)
    {
        t = SHA1_ROTLEFT(a, 5) + ((b & c) | ((~b) & d)) + e + w[i] + 0x5a827999;
        e = d;
        d = c;
        c = SHA1_ROTLEFT(b, 30);
        b = a;
        a = t;
    }
    for (i = 20; i < 40; i++)
    {
        t = SHA1_ROTLEFT(a, 5) + (b ^ c ^ d) + e + w[i] + 0x6ed9eba1;
        e = d;
        d = c;
        c = SHA1_ROTLEFT(b, 30);
        b = a;
        a = t;
    }
    for (i = 40; i < 60; i++)
    {
        t = SHA1_ROTLEFT(a, 5) + ((b & c) | (b & d) | (c & d)) + e + w[i] + 0x8f1bbcdc;
        e = d;
        d = c;
        c = SHA1_ROTLEFT(b, 30);
        b = a;
        a = t;
    }
    for (i = 60; i < 80; i++)
    {
        t = SHA1_ROTLEFT(a, 5) + (b ^ c ^ d) + e + w[i] + 0xca62c1d6;
        e = d;
        d = c;
        c = SHA1_ROTLEFT(b, 30);
        b = a;
        a = t;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}

void nc_sha1_init(nc_sha1_ctx *ctx)
{
    ctx->count = 0;
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xefcdab89;
    ctx->state[2] = 0x98badcfe;
    ctx->state[3] = 0x10325476;
    ctx->state[4] = 0xc3d2e1f0;
}

void nc_sha1_update(nc_sha1_ctx *ctx, const uint8_t *data, size_t len)
{
    size_t i, index, part_len;

    index = (size_t)((ctx->count >> 3) & 0x3f);
    ctx->count += (uint64_t)(len << 3);

    part_len = 64 - index;

    if (len >= part_len)
    {
        memcpy(&ctx->buffer[index], data, part_len);
        sha1_transform(ctx->state, ctx->buffer);

        for (i = part_len; i + 63 < len; i += 64)
        {
            sha1_transform(ctx->state, &data[i]);
        }

        index = 0;
    }
    else
    {
        i = 0;
    }

    memcpy(&ctx->buffer[index], &data[i], len - i);
}

void nc_sha1_final(nc_sha1_ctx *ctx, uint8_t digest[20])
{
    uint8_t bits[8];
    size_t index, pad_len;
    static const uint8_t padding[64] = { 0x80 };
    int i;

    /* Save length (big-endian) */
    for (i = 0; i < 8; i++)
    {
        bits[i] = (uint8_t)((ctx->count >> ((7 - i) * 8)) & 0xff);
    }

    /* Pad to 56 mod 64 */
    index = (size_t)((ctx->count >> 3) & 0x3f);
    pad_len = (index < 56) ? (56 - index) : (120 - index);
    nc_sha1_update(ctx, padding, pad_len);

    /* Append length */
    nc_sha1_update(ctx, bits, 8);

    /* Store state in digest (big-endian) */
    for (i = 0; i < 5; i++)
    {
        digest[i * 4 + 0] = (uint8_t)((ctx->state[i] >> 24) & 0xff);
        digest[i * 4 + 1] = (uint8_t)((ctx->state[i] >> 16) & 0xff);
        digest[i * 4 + 2] = (uint8_t)((ctx->state[i] >> 8) & 0xff);
        digest[i * 4 + 3] = (uint8_t)((ctx->state[i] >> 0) & 0xff);
    }
}

/* ========================================================================
 * HMAC-SHA1
 * ======================================================================== */

void nc_hmac_sha1(const uint8_t *key, size_t key_len,
                  const uint8_t *data, size_t data_len,
                  uint8_t output[20])
{
    nc_sha1_ctx ctx;
    uint8_t k[64];
    uint8_t ipad[64], opad[64];
    uint8_t inner[20];
    size_t i;

    /* Prepare key */
    memset(k, 0, 64);
    if (key_len > 64)
    {
        nc_sha1_init(&ctx);
        nc_sha1_update(&ctx, key, key_len);
        nc_sha1_final(&ctx, k);
    }
    else
    {
        memcpy(k, key, key_len);
    }

    /* Compute inner hash */
    for (i = 0; i < 64; i++)
    {
        ipad[i] = k[i] ^ 0x36;
    }
    nc_sha1_init(&ctx);
    nc_sha1_update(&ctx, ipad, 64);
    nc_sha1_update(&ctx, data, data_len);
    nc_sha1_final(&ctx, inner);

    /* Compute outer hash */
    for (i = 0; i < 64; i++)
    {
        opad[i] = k[i] ^ 0x5c;
    }
    nc_sha1_init(&ctx);
    nc_sha1_update(&ctx, opad, 64);
    nc_sha1_update(&ctx, inner, 20);
    nc_sha1_final(&ctx, output);
}

/* ========================================================================
 * HMAC-MD5
 * ======================================================================== */

void nc_hmac_md5(const uint8_t *key, size_t key_len,
                 const uint8_t *data, size_t data_len,
                 uint8_t output[16])
{
    nc_md5_ctx ctx;
    uint8_t k[64];
    uint8_t ipad[64], opad[64];
    uint8_t inner[16];
    size_t i;

    /* Prepare key */
    memset(k, 0, 64);
    if (key_len > 64)
    {
        nc_md5_init(&ctx);
        nc_md5_update(&ctx, key, key_len);
        nc_md5_final(&ctx, k);
    }
    else
    {
        memcpy(k, key, key_len);
    }

    /* Compute inner hash */
    for (i = 0; i < 64; i++)
    {
        ipad[i] = k[i] ^ 0x36;
    }
    nc_md5_init(&ctx);
    nc_md5_update(&ctx, ipad, 64);
    nc_md5_update(&ctx, data, data_len);
    nc_md5_final(&ctx, inner);

    /* Compute outer hash */
    for (i = 0; i < 64; i++)
    {
        opad[i] = k[i] ^ 0x5c;
    }
    nc_md5_init(&ctx);
    nc_md5_update(&ctx, opad, 64);
    nc_md5_update(&ctx, inner, 16);
    nc_md5_final(&ctx, output);
}

/* ========================================================================
 * DES3-CBC and AES-128-ECB stubs (not yet implemented)
 * These are needed for specific RDP features but can be added later
 * ======================================================================== */

void nc_des3_set_key(nc_des3_ctx *ctx, const uint8_t key[24])
{
    /* TODO: Implement DES3 if needed for RDP legacy crypto */
    (void)ctx;
    (void)key;
}

void nc_des3_cbc_encrypt(nc_des3_ctx *ctx, const uint8_t *iv,
                         const uint8_t *in, uint8_t *out, size_t len)
{
    /* TODO: Implement DES3 CBC encryption if needed */
    (void)ctx;
    (void)iv;
    (void)in;
    (void)out;
    (void)len;
}

void nc_des3_cbc_decrypt(nc_des3_ctx *ctx, const uint8_t *iv,
                         const uint8_t *in, uint8_t *out, size_t len)
{
    /* TODO: Implement DES3 CBC decryption if needed */
    (void)ctx;
    (void)iv;
    (void)in;
    (void)out;
    (void)len;
}

void nc_aes128_set_key(nc_aes128_ctx *ctx, const uint8_t key[16])
{
    /* TODO: Implement AES-128 if needed for Apple ARD */
    (void)ctx;
    (void)key;
}

void nc_aes128_ecb_encrypt(nc_aes128_ctx *ctx, const uint8_t in[16], uint8_t out[16])
{
    /* TODO: Implement AES-128 ECB encryption if needed */
    (void)ctx;
    (void)in;
    (void)out;
}
