/*
 * Minimal TLS 1.3 Client - Pure C Implementation
 * All crypto primitives implemented from scratch
 */

/* Set to 1 for debug output */
#ifndef TLS13_DEBUG
#define TLS13_DEBUG 1
#endif

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "neutrinotls.h"
#include "log.h"

#if TLS13_DEBUG
#define DPRINTF(...) do { \
    char _dprintf_buf[512]; \
    snprintf(_dprintf_buf, sizeof(_dprintf_buf), __VA_ARGS__); \
    LOG(LOG_LEVEL_DEBUG, "[NeutrinoTLS] %s", _dprintf_buf); \
} while(0)
#else
#define DPRINTF(...) ((void)0)
#endif
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

/* =============================================================================
 * SHA-256 Implementation
 * ============================================================================= */

static const uint32_t K256[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

#define ROR32(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define CH(x, y, z)  (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROR32(x, 2) ^ ROR32(x, 13) ^ ROR32(x, 22))
#define EP1(x) (ROR32(x, 6) ^ ROR32(x, 11) ^ ROR32(x, 25))
#define SIG0(x) (ROR32(x, 7) ^ ROR32(x, 18) ^ ((x) >> 3))
#define SIG1(x) (ROR32(x, 17) ^ ROR32(x, 19) ^ ((x) >> 10))

static void sha256_transform(sha256_ctx *ctx, const uint8_t block[64]) {
    uint32_t w[64], a, b, c, d, e, f, g, h, t1, t2;

    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[i*4] << 24) | ((uint32_t)block[i*4+1] << 16) |
               ((uint32_t)block[i*4+2] << 8) | block[i*4+3];
    }
    for (int i = 16; i < 64; i++) {
        w[i] = SIG1(w[i-2]) + w[i-7] + SIG0(w[i-15]) + w[i-16];
    }

    a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
    e = ctx->state[4]; f = ctx->state[5]; g = ctx->state[6]; h = ctx->state[7];

    for (int i = 0; i < 64; i++) {
        t1 = h + EP1(e) + CH(e, f, g) + K256[i] + w[i];
        t2 = EP0(a) + MAJ(a, b, c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

void sha256_init(sha256_ctx *ctx) {
    ctx->state[0] = 0x6a09e667; ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372; ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f; ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab; ctx->state[7] = 0x5be0cd19;
    ctx->count = 0;
}

void sha256_update(sha256_ctx *ctx, const uint8_t *data, size_t len) {
    if (len == 0) return;  /* Handle empty input safely */

    size_t i = 0;
    size_t index = (ctx->count >> 3) & 0x3f;
    ctx->count += len << 3;

    size_t part_len = 64 - index;
    if (len >= part_len) {
        memcpy(ctx->buffer + index, data, part_len);
        sha256_transform(ctx, ctx->buffer);
        for (i = part_len; i + 63 < len; i += 64) {
            sha256_transform(ctx, data + i);
        }
        index = 0;
    }
    memcpy(ctx->buffer + index, data + i, len - i);
}

void sha256_final(sha256_ctx *ctx, uint8_t hash[32]) {
    uint8_t pad[64];
    size_t index = (ctx->count >> 3) & 0x3f;
    size_t pad_len = (index < 56) ? (56 - index) : (120 - index);

    /* Save original message length BEFORE adding padding */
    uint64_t bits_count = ctx->count;

    memset(pad, 0, sizeof(pad));
    pad[0] = 0x80;
    sha256_update(ctx, pad, pad_len);

    /* Encode original length (not including padding) */
    uint8_t bits[8];
    for (int i = 0; i < 8; i++) bits[i] = (bits_count >> (56 - i * 8)) & 0xff;
    sha256_update(ctx, bits, 8);

    for (int i = 0; i < 8; i++) {
        hash[i*4] = (ctx->state[i] >> 24) & 0xff;
        hash[i*4+1] = (ctx->state[i] >> 16) & 0xff;
        hash[i*4+2] = (ctx->state[i] >> 8) & 0xff;
        hash[i*4+3] = ctx->state[i] & 0xff;
    }
}

void sha256(const uint8_t *data, size_t len, uint8_t hash[32]) {
    sha256_ctx ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, hash);
}

/* =============================================================================
 * HMAC-SHA256
 * ============================================================================= */

void hmac_sha256(const uint8_t *key, size_t key_len,
                 const uint8_t *data, size_t data_len,
                 uint8_t mac[32]) {
    uint8_t k[64], ipad[64], opad[64];
    sha256_ctx ctx;

    memset(k, 0, 64);
    if (key_len > 64) {
        sha256(key, key_len, k);
    } else {
        memcpy(k, key, key_len);
    }

    for (int i = 0; i < 64; i++) {
        ipad[i] = k[i] ^ 0x36;
        opad[i] = k[i] ^ 0x5c;
    }

    sha256_init(&ctx);
    sha256_update(&ctx, ipad, 64);
    sha256_update(&ctx, data, data_len);
    uint8_t inner[32];
    sha256_final(&ctx, inner);

    sha256_init(&ctx);
    sha256_update(&ctx, opad, 64);
    sha256_update(&ctx, inner, 32);
    sha256_final(&ctx, mac);
}

/* =============================================================================
 * HKDF (RFC 5869)
 * ============================================================================= */

void hkdf_extract(const uint8_t *salt, size_t salt_len,
                  const uint8_t *ikm, size_t ikm_len,
                  uint8_t prk[32]) {
    if (salt == NULL || salt_len == 0) {
        uint8_t zeros[32] = {0};
        hmac_sha256(zeros, 32, ikm, ikm_len, prk);
    } else {
        hmac_sha256(salt, salt_len, ikm, ikm_len, prk);
    }
}

void hkdf_expand(const uint8_t prk[32],
                 const uint8_t *info, size_t info_len,
                 uint8_t *okm, size_t okm_len) {
    uint8_t t[32] = {0};
    size_t t_len = 0;
    size_t pos = 0;
    uint8_t counter = 1;

    while (pos < okm_len) {
        uint8_t buf[32 + 256 + 1];
        size_t buf_len = 0;

        if (t_len > 0) {
            memcpy(buf, t, t_len);
            buf_len = t_len;
        }
        memcpy(buf + buf_len, info, info_len);
        buf_len += info_len;
        buf[buf_len++] = counter++;

        hmac_sha256(prk, 32, buf, buf_len, t);
        t_len = 32;

        size_t copy = okm_len - pos;
        if (copy > 32) copy = 32;
        memcpy(okm + pos, t, copy);
        pos += copy;
    }
}

void hkdf_expand_label(const uint8_t secret[32],
                       const char *label,
                       const uint8_t *context, size_t context_len,
                       uint8_t *out, size_t out_len) {
    /* TLS 1.3 HKDF-Expand-Label structure */
    uint8_t info[256];
    size_t pos = 0;

    /* Length (2 bytes) */
    info[pos++] = (out_len >> 8) & 0xff;
    info[pos++] = out_len & 0xff;

    /* Label with "tls13 " prefix */
    size_t label_len = strlen(label);
    info[pos++] = 6 + label_len;  /* "tls13 " + label */
    memcpy(info + pos, "tls13 ", 6);
    pos += 6;
    memcpy(info + pos, label, label_len);
    pos += label_len;

    /* Context */
    info[pos++] = context_len;
    if (context_len > 0) {
        memcpy(info + pos, context, context_len);
        pos += context_len;
    }

    hkdf_expand(secret, info, pos, out, out_len);
}

/* =============================================================================
 * X25519 Key Exchange (Curve25519)
 * Using 5 limbs of 51 bits each (uniform representation)
 * ============================================================================= */

typedef uint64_t fe51[5];
#define MASK51 ((1ULL << 51) - 1)

static void fe51_0(fe51 h) { memset(h, 0, sizeof(fe51)); }
static void fe51_1(fe51 h) { fe51_0(h); h[0] = 1; }
static void fe51_copy(fe51 h, const fe51 f) { memcpy(h, f, sizeof(fe51)); }

static void fe51_add(fe51 h, const fe51 f, const fe51 g) {
    for (int i = 0; i < 5; i++) h[i] = f[i] + g[i];
}

static void fe51_sub(fe51 h, const fe51 f, const fe51 g) {
    /* Add 2*p to avoid negative numbers */
    static const uint64_t two_p[5] = {
        0xfffffffffffda, 0xffffffffffffe, 0xffffffffffffe, 0xffffffffffffe, 0xffffffffffffe
    };
    for (int i = 0; i < 5; i++) h[i] = f[i] + two_p[i] - g[i];
}

static void fe51_carry(fe51 h) {
    uint64_t c;
    c = h[0] >> 51; h[1] += c; h[0] &= MASK51;
    c = h[1] >> 51; h[2] += c; h[1] &= MASK51;
    c = h[2] >> 51; h[3] += c; h[2] &= MASK51;
    c = h[3] >> 51; h[4] += c; h[3] &= MASK51;
    c = h[4] >> 51; h[0] += c * 19; h[4] &= MASK51;
    /* Second carry pass for potential overflow from h[0] += c*19 */
    c = h[0] >> 51; h[1] += c; h[0] &= MASK51;
}

static void fe51_mul(fe51 h, const fe51 f, const fe51 g) {
    /* Schoolbook multiplication with delayed reduction */
    __uint128_t t0, t1, t2, t3, t4;
    uint64_t g1_19 = g[1] * 19, g2_19 = g[2] * 19, g3_19 = g[3] * 19, g4_19 = g[4] * 19;

    t0 = (__uint128_t)f[0] * g[0] + (__uint128_t)f[1] * g4_19 + (__uint128_t)f[2] * g3_19 +
         (__uint128_t)f[3] * g2_19 + (__uint128_t)f[4] * g1_19;
    t1 = (__uint128_t)f[0] * g[1] + (__uint128_t)f[1] * g[0] + (__uint128_t)f[2] * g4_19 +
         (__uint128_t)f[3] * g3_19 + (__uint128_t)f[4] * g2_19;
    t2 = (__uint128_t)f[0] * g[2] + (__uint128_t)f[1] * g[1] + (__uint128_t)f[2] * g[0] +
         (__uint128_t)f[3] * g4_19 + (__uint128_t)f[4] * g3_19;
    t3 = (__uint128_t)f[0] * g[3] + (__uint128_t)f[1] * g[2] + (__uint128_t)f[2] * g[1] +
         (__uint128_t)f[3] * g[0] + (__uint128_t)f[4] * g4_19;
    t4 = (__uint128_t)f[0] * g[4] + (__uint128_t)f[1] * g[3] + (__uint128_t)f[2] * g[2] +
         (__uint128_t)f[3] * g[1] + (__uint128_t)f[4] * g[0];

    /* Reduce and carry */
    t1 += t0 >> 51; h[0] = (uint64_t)t0 & MASK51;
    t2 += t1 >> 51; h[1] = (uint64_t)t1 & MASK51;
    t3 += t2 >> 51; h[2] = (uint64_t)t2 & MASK51;
    t4 += t3 >> 51; h[3] = (uint64_t)t3 & MASK51;
    h[0] += ((uint64_t)(t4 >> 51)) * 19;
    h[4] = (uint64_t)t4 & MASK51;

    /* Final carry */
    uint64_t c = h[0] >> 51; h[1] += c; h[0] &= MASK51;
}

static void fe51_sq(fe51 h, const fe51 f) { fe51_mul(h, f, f); }

static void fe51_mul121666(fe51 h, const fe51 f) {
    __uint128_t t;
    t = (__uint128_t)f[0] * 121666; h[0] = (uint64_t)t & MASK51;
    t = (__uint128_t)f[1] * 121666 + (t >> 51); h[1] = (uint64_t)t & MASK51;
    t = (__uint128_t)f[2] * 121666 + (t >> 51); h[2] = (uint64_t)t & MASK51;
    t = (__uint128_t)f[3] * 121666 + (t >> 51); h[3] = (uint64_t)t & MASK51;
    t = (__uint128_t)f[4] * 121666 + (t >> 51); h[4] = (uint64_t)t & MASK51;
    h[0] += (uint64_t)(t >> 51) * 19;
}

static void fe51_invert(fe51 out, const fe51 z) {
    fe51 t0, t1, t2, t3;
    int i;

    fe51_sq(t0, z);
    fe51_sq(t1, t0);
    fe51_sq(t1, t1);
    fe51_mul(t1, z, t1);
    fe51_mul(t0, t0, t1);
    fe51_sq(t2, t0);
    fe51_mul(t1, t1, t2);
    fe51_sq(t2, t1);
    for (i = 0; i < 4; i++) fe51_sq(t2, t2);
    fe51_mul(t1, t2, t1);
    fe51_sq(t2, t1);
    for (i = 0; i < 9; i++) fe51_sq(t2, t2);
    fe51_mul(t2, t2, t1);
    fe51_sq(t3, t2);
    for (i = 0; i < 19; i++) fe51_sq(t3, t3);
    fe51_mul(t2, t3, t2);
    fe51_sq(t2, t2);
    for (i = 0; i < 9; i++) fe51_sq(t2, t2);
    fe51_mul(t1, t2, t1);
    fe51_sq(t2, t1);
    for (i = 0; i < 49; i++) fe51_sq(t2, t2);
    fe51_mul(t2, t2, t1);
    fe51_sq(t3, t2);
    for (i = 0; i < 99; i++) fe51_sq(t3, t3);
    fe51_mul(t2, t3, t2);
    fe51_sq(t2, t2);
    for (i = 0; i < 49; i++) fe51_sq(t2, t2);
    fe51_mul(t1, t2, t1);
    fe51_sq(t1, t1);
    for (i = 0; i < 4; i++) fe51_sq(t1, t1);
    fe51_mul(out, t1, t0);
}

static void fe51_frombytes(fe51 h, const uint8_t s[32]) {
    uint64_t t0 = ((uint64_t)s[0]) | ((uint64_t)s[1] << 8) | ((uint64_t)s[2] << 16) |
                  ((uint64_t)s[3] << 24) | ((uint64_t)s[4] << 32) | ((uint64_t)s[5] << 40) |
                  ((uint64_t)(s[6] & 0x07) << 48);
    uint64_t t1 = ((uint64_t)s[6] >> 3) | ((uint64_t)s[7] << 5) | ((uint64_t)s[8] << 13) |
                  ((uint64_t)s[9] << 21) | ((uint64_t)s[10] << 29) | ((uint64_t)s[11] << 37) |
                  ((uint64_t)(s[12] & 0x3f) << 45);
    uint64_t t2 = ((uint64_t)s[12] >> 6) | ((uint64_t)s[13] << 2) | ((uint64_t)s[14] << 10) |
                  ((uint64_t)s[15] << 18) | ((uint64_t)s[16] << 26) | ((uint64_t)s[17] << 34) |
                  ((uint64_t)s[18] << 42) | ((uint64_t)(s[19] & 0x01) << 50);
    uint64_t t3 = ((uint64_t)s[19] >> 1) | ((uint64_t)s[20] << 7) | ((uint64_t)s[21] << 15) |
                  ((uint64_t)s[22] << 23) | ((uint64_t)s[23] << 31) | ((uint64_t)s[24] << 39) |
                  ((uint64_t)(s[25] & 0x0f) << 47);
    uint64_t t4 = ((uint64_t)s[25] >> 4) | ((uint64_t)s[26] << 4) | ((uint64_t)s[27] << 12) |
                  ((uint64_t)s[28] << 20) | ((uint64_t)s[29] << 28) | ((uint64_t)s[30] << 36) |
                  ((uint64_t)(s[31] & 0x7f) << 44);
    h[0] = t0; h[1] = t1; h[2] = t2; h[3] = t3; h[4] = t4;
}

static void fe51_tobytes(uint8_t s[32], const fe51 h) {
    fe51 t;
    fe51_copy(t, h);
    fe51_carry(t);
    fe51_carry(t);

    /* Reduce mod p = 2^255 - 19 */
    uint64_t c = (t[0] + 19) >> 51;
    c = (t[1] + c) >> 51;
    c = (t[2] + c) >> 51;
    c = (t[3] + c) >> 51;
    c = (t[4] + c) >> 51;
    t[0] += 19 * c;
    fe51_carry(t);

    s[0] = t[0] & 0xff;
    s[1] = (t[0] >> 8) & 0xff;
    s[2] = (t[0] >> 16) & 0xff;
    s[3] = (t[0] >> 24) & 0xff;
    s[4] = (t[0] >> 32) & 0xff;
    s[5] = (t[0] >> 40) & 0xff;
    s[6] = ((t[0] >> 48) | (t[1] << 3)) & 0xff;
    s[7] = (t[1] >> 5) & 0xff;
    s[8] = (t[1] >> 13) & 0xff;
    s[9] = (t[1] >> 21) & 0xff;
    s[10] = (t[1] >> 29) & 0xff;
    s[11] = (t[1] >> 37) & 0xff;
    s[12] = ((t[1] >> 45) | (t[2] << 6)) & 0xff;
    s[13] = (t[2] >> 2) & 0xff;
    s[14] = (t[2] >> 10) & 0xff;
    s[15] = (t[2] >> 18) & 0xff;
    s[16] = (t[2] >> 26) & 0xff;
    s[17] = (t[2] >> 34) & 0xff;
    s[18] = (t[2] >> 42) & 0xff;
    s[19] = ((t[2] >> 50) | (t[3] << 1)) & 0xff;
    s[20] = (t[3] >> 7) & 0xff;
    s[21] = (t[3] >> 15) & 0xff;
    s[22] = (t[3] >> 23) & 0xff;
    s[23] = (t[3] >> 31) & 0xff;
    s[24] = (t[3] >> 39) & 0xff;
    s[25] = ((t[3] >> 47) | (t[4] << 4)) & 0xff;
    s[26] = (t[4] >> 4) & 0xff;
    s[27] = (t[4] >> 12) & 0xff;
    s[28] = (t[4] >> 20) & 0xff;
    s[29] = (t[4] >> 28) & 0xff;
    s[30] = (t[4] >> 36) & 0xff;
    s[31] = (t[4] >> 44) & 0xff;
}

static void fe51_cswap(fe51 f, fe51 g, int b) {
    uint64_t mask = -(uint64_t)b;
    for (int i = 0; i < 5; i++) {
        uint64_t x = mask & (f[i] ^ g[i]);
        f[i] ^= x;
        g[i] ^= x;
    }
}

void x25519(uint8_t shared[32], const uint8_t private_key[32], const uint8_t public_key[32]) {
    uint8_t e[32];
    memcpy(e, private_key, 32);
    e[0] &= 248;
    e[31] &= 127;
    e[31] |= 64;

    fe51 x1, x2, z2, x3, z3, tmp0, tmp1;
    fe51_frombytes(x1, public_key);
    fe51_1(x2);
    fe51_0(z2);
    fe51_copy(x3, x1);
    fe51_1(z3);

    int swap = 0;
    for (int pos = 254; pos >= 0; pos--) {
        int b = (e[pos / 8] >> (pos & 7)) & 1;
        swap ^= b;
        fe51_cswap(x2, x3, swap);
        fe51_cswap(z2, z3, swap);
        swap = b;

        fe51_sub(tmp0, x3, z3);
        fe51_sub(tmp1, x2, z2);
        fe51_add(x2, x2, z2);
        fe51_add(z2, x3, z3);
        fe51_mul(z3, tmp0, x2);
        fe51_mul(z2, z2, tmp1);
        fe51_sq(tmp0, tmp1);
        fe51_sq(tmp1, x2);
        fe51_add(x3, z3, z2);
        fe51_sub(z2, z3, z2);
        fe51_mul(x2, tmp1, tmp0);
        fe51_sub(tmp1, tmp1, tmp0);
        fe51_sq(z2, z2);
        fe51_mul121666(z3, tmp1);
        fe51_sq(x3, x3);
        fe51_add(tmp0, tmp0, z3);
        fe51_mul(z3, x1, z2);
        fe51_mul(z2, tmp1, tmp0);
    }
    fe51_cswap(x2, x3, swap);
    fe51_cswap(z2, z3, swap);

    fe51_invert(z2, z2);
    fe51_mul(x2, x2, z2);
    fe51_tobytes(shared, x2);
}

void x25519_keygen(uint8_t public_key[32], uint8_t private_key[32]) {
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        read(fd, private_key, 32);
        close(fd);
    }

    private_key[0] &= 248;
    private_key[31] &= 127;
    private_key[31] |= 64;

    static const uint8_t basepoint[32] = {9};
    x25519(public_key, private_key, basepoint);
}

/* =============================================================================
 * ChaCha20-Poly1305
 * ============================================================================= */

#define ROTL32(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

#define QUARTERROUND(a, b, c, d) \
    a += b; d ^= a; d = ROTL32(d, 16); \
    c += d; b ^= c; b = ROTL32(b, 12); \
    a += b; d ^= a; d = ROTL32(d, 8);  \
    c += d; b ^= c; b = ROTL32(b, 7);

static void chacha20_block(uint32_t out[16], const uint32_t in[16]) {
    uint32_t x[16];
    memcpy(x, in, 64);

    for (int i = 0; i < 10; i++) {
        QUARTERROUND(x[0], x[4], x[8], x[12])
        QUARTERROUND(x[1], x[5], x[9], x[13])
        QUARTERROUND(x[2], x[6], x[10], x[14])
        QUARTERROUND(x[3], x[7], x[11], x[15])
        QUARTERROUND(x[0], x[5], x[10], x[15])
        QUARTERROUND(x[1], x[6], x[11], x[12])
        QUARTERROUND(x[2], x[7], x[8], x[13])
        QUARTERROUND(x[3], x[4], x[9], x[14])
    }

    for (int i = 0; i < 16; i++) out[i] = x[i] + in[i];
}

static void chacha20_encrypt(const uint8_t key[32], const uint8_t nonce[12],
                              uint32_t counter, const uint8_t *in, uint8_t *out, size_t len) {
    uint32_t state[16] = {
        0x61707865, 0x3320646e, 0x79622d32, 0x6b206574,
        0, 0, 0, 0, 0, 0, 0, 0,
        counter, 0, 0, 0
    };

    for (int i = 0; i < 8; i++) {
        state[4 + i] = ((uint32_t)key[i*4]) | ((uint32_t)key[i*4+1] << 8) |
                       ((uint32_t)key[i*4+2] << 16) | ((uint32_t)key[i*4+3] << 24);
    }
    for (int i = 0; i < 3; i++) {
        state[13 + i] = ((uint32_t)nonce[i*4]) | ((uint32_t)nonce[i*4+1] << 8) |
                        ((uint32_t)nonce[i*4+2] << 16) | ((uint32_t)nonce[i*4+3] << 24);
    }

    size_t pos = 0;
    while (pos < len) {
        uint32_t block[16];
        chacha20_block(block, state);
        state[12]++;

        size_t chunk = len - pos;
        if (chunk > 64) chunk = 64;

        uint8_t keystream[64];
        for (int i = 0; i < 16; i++) {
            keystream[i*4] = block[i] & 0xff;
            keystream[i*4+1] = (block[i] >> 8) & 0xff;
            keystream[i*4+2] = (block[i] >> 16) & 0xff;
            keystream[i*4+3] = (block[i] >> 24) & 0xff;
        }

        for (size_t i = 0; i < chunk; i++) {
            out[pos + i] = in[pos + i] ^ keystream[i];
        }
        pos += chunk;
    }
}

/* Poly1305 */
typedef struct {
    uint32_t r[5];
    uint32_t h[5];
    uint32_t pad[4];
} poly1305_ctx;

static void poly1305_init(poly1305_ctx *ctx, const uint8_t key[32]) {
    /* Clamp r according to RFC 7539 */
    uint8_t r[16];
    memcpy(r, key, 16);
    r[3] &= 0x0f;
    r[7] &= 0x0f;
    r[11] &= 0x0f;
    r[15] &= 0x0f;
    r[4] &= 0xfc;
    r[8] &= 0xfc;
    r[12] &= 0xfc;

    /* Convert r to radix-2^26 representation */
    ctx->r[0] = (((uint32_t)r[0]) | ((uint32_t)r[1] << 8) |
                 ((uint32_t)r[2] << 16) | ((uint32_t)(r[3] & 0x03) << 24)) & 0x03ffffff;
    ctx->r[1] = (((uint32_t)(r[3] >> 2)) | ((uint32_t)r[4] << 6) |
                 ((uint32_t)r[5] << 14) | ((uint32_t)(r[6] & 0x0f) << 22)) & 0x03ffffff;
    ctx->r[2] = (((uint32_t)(r[6] >> 4)) | ((uint32_t)r[7] << 4) |
                 ((uint32_t)r[8] << 12) | ((uint32_t)(r[9] & 0x3f) << 20)) & 0x03ffffff;
    ctx->r[3] = (((uint32_t)(r[9] >> 6)) | ((uint32_t)r[10] << 2) |
                 ((uint32_t)r[11] << 10) | ((uint32_t)r[12] << 18)) & 0x03ffffff;
    ctx->r[4] = (((uint32_t)r[13]) | ((uint32_t)r[14] << 8) |
                 ((uint32_t)r[15] << 16)) & 0x03ffffff;

    for (int i = 0; i < 5; i++) ctx->h[i] = 0;

    for (int i = 0; i < 4; i++) {
        ctx->pad[i] = ((uint32_t)key[16 + i*4]) | ((uint32_t)key[17 + i*4] << 8) |
                      ((uint32_t)key[18 + i*4] << 16) | ((uint32_t)key[19 + i*4] << 24);
    }
}

static void poly1305_blocks(poly1305_ctx *ctx, const uint8_t *m, size_t len, int final) {
    uint32_t hibit = final ? 0 : (1 << 24);

    while (len >= 16) {
        uint32_t h0 = ctx->h[0], h1 = ctx->h[1], h2 = ctx->h[2], h3 = ctx->h[3], h4 = ctx->h[4];
        uint32_t r0 = ctx->r[0], r1 = ctx->r[1], r2 = ctx->r[2], r3 = ctx->r[3], r4 = ctx->r[4];

        h0 += (((uint32_t)m[0]) | ((uint32_t)m[1] << 8) | ((uint32_t)m[2] << 16) | ((uint32_t)m[3] << 24)) & 0x3ffffff;
        h1 += ((((uint32_t)m[3]) | ((uint32_t)m[4] << 8) | ((uint32_t)m[5] << 16) | ((uint32_t)m[6] << 24)) >> 2) & 0x3ffffff;
        h2 += ((((uint32_t)m[6]) | ((uint32_t)m[7] << 8) | ((uint32_t)m[8] << 16) | ((uint32_t)m[9] << 24)) >> 4) & 0x3ffffff;
        h3 += ((((uint32_t)m[9]) | ((uint32_t)m[10] << 8) | ((uint32_t)m[11] << 16) | ((uint32_t)m[12] << 24)) >> 6) & 0x3ffffff;
        h4 += ((((uint32_t)m[12]) | ((uint32_t)m[13] << 8) | ((uint32_t)m[14] << 16) | ((uint32_t)m[15] << 24)) >> 8) | hibit;

        uint64_t d0 = (uint64_t)h0 * r0 + (uint64_t)h1 * (5 * r4) + (uint64_t)h2 * (5 * r3) +
                      (uint64_t)h3 * (5 * r2) + (uint64_t)h4 * (5 * r1);
        uint64_t d1 = (uint64_t)h0 * r1 + (uint64_t)h1 * r0 + (uint64_t)h2 * (5 * r4) +
                      (uint64_t)h3 * (5 * r3) + (uint64_t)h4 * (5 * r2);
        uint64_t d2 = (uint64_t)h0 * r2 + (uint64_t)h1 * r1 + (uint64_t)h2 * r0 +
                      (uint64_t)h3 * (5 * r4) + (uint64_t)h4 * (5 * r3);
        uint64_t d3 = (uint64_t)h0 * r3 + (uint64_t)h1 * r2 + (uint64_t)h2 * r1 +
                      (uint64_t)h3 * r0 + (uint64_t)h4 * (5 * r4);
        uint64_t d4 = (uint64_t)h0 * r4 + (uint64_t)h1 * r3 + (uint64_t)h2 * r2 +
                      (uint64_t)h3 * r1 + (uint64_t)h4 * r0;

        uint32_t c = (uint32_t)(d0 >> 26); h0 = (uint32_t)d0 & 0x3ffffff;
        d1 += c; c = (uint32_t)(d1 >> 26); h1 = (uint32_t)d1 & 0x3ffffff;
        d2 += c; c = (uint32_t)(d2 >> 26); h2 = (uint32_t)d2 & 0x3ffffff;
        d3 += c; c = (uint32_t)(d3 >> 26); h3 = (uint32_t)d3 & 0x3ffffff;
        d4 += c; c = (uint32_t)(d4 >> 26); h4 = (uint32_t)d4 & 0x3ffffff;
        h0 += c * 5; c = h0 >> 26; h0 &= 0x3ffffff;
        h1 += c;

        ctx->h[0] = h0; ctx->h[1] = h1; ctx->h[2] = h2; ctx->h[3] = h3; ctx->h[4] = h4;

        m += 16;
        len -= 16;
    }
}

static void poly1305_final(poly1305_ctx *ctx, uint8_t mac[16]) {
    uint32_t h0 = ctx->h[0], h1 = ctx->h[1], h2 = ctx->h[2], h3 = ctx->h[3], h4 = ctx->h[4];

    uint32_t c = h1 >> 26; h1 &= 0x3ffffff;
    h2 += c; c = h2 >> 26; h2 &= 0x3ffffff;
    h3 += c; c = h3 >> 26; h3 &= 0x3ffffff;
    h4 += c; c = h4 >> 26; h4 &= 0x3ffffff;
    h0 += c * 5; c = h0 >> 26; h0 &= 0x3ffffff;
    h1 += c;

    uint32_t g0 = h0 + 5; c = g0 >> 26; g0 &= 0x3ffffff;
    uint32_t g1 = h1 + c; c = g1 >> 26; g1 &= 0x3ffffff;
    uint32_t g2 = h2 + c; c = g2 >> 26; g2 &= 0x3ffffff;
    uint32_t g3 = h3 + c; c = g3 >> 26; g3 &= 0x3ffffff;
    uint32_t g4 = h4 + c - (1 << 26);

    uint32_t mask = (g4 >> 31) - 1;
    g0 &= mask; g1 &= mask; g2 &= mask; g3 &= mask; g4 &= mask;
    mask = ~mask;
    h0 = (h0 & mask) | g0; h1 = (h1 & mask) | g1;
    h2 = (h2 & mask) | g2; h3 = (h3 & mask) | g3;
    h4 = (h4 & mask) | g4;

    h0 |= h1 << 26; h1 = (h1 >> 6) | (h2 << 20);
    h2 = (h2 >> 12) | (h3 << 14); h3 = (h3 >> 18) | (h4 << 8);

    uint64_t f = (uint64_t)h0 + ctx->pad[0]; h0 = (uint32_t)f;
    f = (uint64_t)h1 + ctx->pad[1] + (f >> 32); h1 = (uint32_t)f;
    f = (uint64_t)h2 + ctx->pad[2] + (f >> 32); h2 = (uint32_t)f;
    f = (uint64_t)h3 + ctx->pad[3] + (f >> 32); h3 = (uint32_t)f;

    mac[0] = h0; mac[1] = h0 >> 8; mac[2] = h0 >> 16; mac[3] = h0 >> 24;
    mac[4] = h1; mac[5] = h1 >> 8; mac[6] = h1 >> 16; mac[7] = h1 >> 24;
    mac[8] = h2; mac[9] = h2 >> 8; mac[10] = h2 >> 16; mac[11] = h2 >> 24;
    mac[12] = h3; mac[13] = h3 >> 8; mac[14] = h3 >> 16; mac[15] = h3 >> 24;
}

void chacha20_poly1305_encrypt(const uint8_t key[32], const uint8_t nonce[12],
                                const uint8_t *aad, size_t aad_len,
                                const uint8_t *plaintext, size_t plaintext_len,
                                uint8_t *ciphertext, uint8_t tag[16]) {
    /* Generate Poly1305 key */
    uint8_t poly_key[64] = {0};
    chacha20_encrypt(key, nonce, 0, poly_key, poly_key, 64);

    /* Debug: print the Poly1305 key */
    DPRINTF("Poly1305 key (first 32 bytes): ");
    for (int i = 0; i < 32; i++) DPRINTF("%02x", poly_key[i]);
    DPRINTF("\n");

    /* Encrypt */
    chacha20_encrypt(key, nonce, 1, plaintext, ciphertext, plaintext_len);

    /* Compute tag */
    poly1305_ctx ctx;
    poly1305_init(&ctx, poly_key);

    /* AAD */
    if (aad_len > 0) {
        poly1305_blocks(&ctx, aad, aad_len & ~15, 0);
        if (aad_len & 15) {
            uint8_t pad[16] = {0};
            memcpy(pad, aad + (aad_len & ~15), aad_len & 15);
            poly1305_blocks(&ctx, pad, 16, 0);
        }
    }

    /* Ciphertext */
    if (plaintext_len > 0) {
        poly1305_blocks(&ctx, ciphertext, plaintext_len & ~15, 0);
        if (plaintext_len & 15) {
            uint8_t pad[16] = {0};
            memcpy(pad, ciphertext + (plaintext_len & ~15), plaintext_len & 15);
            poly1305_blocks(&ctx, pad, 16, 0);
        }
    }

    /* Lengths */
    uint8_t lens[16];
    for (int i = 0; i < 8; i++) lens[i] = (aad_len >> (i * 8)) & 0xff;
    for (int i = 0; i < 8; i++) lens[8 + i] = (plaintext_len >> (i * 8)) & 0xff;
    poly1305_blocks(&ctx, lens, 16, 0);  /* Not final - all AEAD blocks have hibit=1 */

    poly1305_final(&ctx, tag);
}

bool chacha20_poly1305_decrypt(const uint8_t key[32], const uint8_t nonce[12],
                                const uint8_t *aad, size_t aad_len,
                                const uint8_t *ciphertext, size_t ciphertext_len,
                                const uint8_t tag[16], uint8_t *plaintext) {
    /* Generate Poly1305 key */
    uint8_t poly_key[64] = {0};
    chacha20_encrypt(key, nonce, 0, poly_key, poly_key, 64);

    /* Verify tag */
    poly1305_ctx ctx;
    poly1305_init(&ctx, poly_key);

    if (aad_len > 0) {
        poly1305_blocks(&ctx, aad, aad_len & ~15, 0);
        if (aad_len & 15) {
            uint8_t pad[16] = {0};
            memcpy(pad, aad + (aad_len & ~15), aad_len & 15);
            poly1305_blocks(&ctx, pad, 16, 0);
        }
    }

    if (ciphertext_len > 0) {
        poly1305_blocks(&ctx, ciphertext, ciphertext_len & ~15, 0);
        if (ciphertext_len & 15) {
            uint8_t pad[16] = {0};
            memcpy(pad, ciphertext + (ciphertext_len & ~15), ciphertext_len & 15);
            poly1305_blocks(&ctx, pad, 16, 0);
        }
    }

    uint8_t lens[16];
    for (int i = 0; i < 8; i++) lens[i] = (aad_len >> (i * 8)) & 0xff;
    for (int i = 0; i < 8; i++) lens[8 + i] = (ciphertext_len >> (i * 8)) & 0xff;
    poly1305_blocks(&ctx, lens, 16, 0);  /* Not final - all AEAD blocks have hibit=1 */

    uint8_t computed_tag[16];
    poly1305_final(&ctx, computed_tag);

    /* Constant-time compare */
    uint8_t diff = 0;
    for (int i = 0; i < 16; i++) diff |= computed_tag[i] ^ tag[i];
    if (diff != 0) return false;

    /* Decrypt */
    chacha20_encrypt(key, nonce, 1, ciphertext, plaintext, ciphertext_len);
    return true;
}

/* =============================================================================
 * TLS 1.3 Protocol
 * ============================================================================= */

/* Content types */
#define TLS_CHANGE_CIPHER_SPEC  20
#define TLS_ALERT               21
#define TLS_HANDSHAKE           22
#define TLS_APPLICATION_DATA    23

/* Handshake types */
#define TLS_CLIENT_HELLO        1
#define TLS_SERVER_HELLO        2
#define TLS_ENCRYPTED_EXTENSIONS 8
#define TLS_CERTIFICATE         11
#define TLS_CERTIFICATE_VERIFY  15
#define TLS_FINISHED            20

/* Extensions */
#define TLS_EXT_SERVER_NAME     0
#define TLS_EXT_SUPPORTED_GROUPS 10
#define TLS_EXT_SIGNATURE_ALGOS 13
#define TLS_EXT_SUPPORTED_VERSIONS 43
#define TLS_EXT_KEY_SHARE       51

/* Cipher suite: TLS_CHACHA20_POLY1305_SHA256 */
#define TLS_CHACHA20_POLY1305_SHA256 0x1303

static int sock_read(int sock, void *buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t n = recv(sock, (char*)buf + total, len - total, 0);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                /* Socket is non-blocking and no data available, wait a bit */
                usleep(1000);  /* 1ms */
                continue;
            }
            DPRINTF("[TLS] sock_read: recv returned %zd (errno=%d), wanted %zu more bytes\n", n, errno, len - total);
            return -1;
        }
        if (n == 0) {
            /* Connection closed */
            DPRINTF("[TLS] sock_read: connection closed, got %zu/%zu bytes\n", total, len);
            return -1;
        }
        total += n;
    }
    return 0;
}

static int sock_write(int sock, const void *buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t n = send(sock, (const char*)buf + total, len - total, 0);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                /* Socket is non-blocking and buffer full, wait a bit */
                usleep(1000);  /* 1ms */
                continue;
            }
            DPRINTF("[TLS] sock_write: send returned %zd (errno=%d), wanted %zu more bytes\n", n, errno, len - total);
            return -1;
        }
        if (n == 0) {
            /* Should not happen with send(), but handle it */
            DPRINTF("[TLS] sock_write: send returned 0\n");
            return -1;
        }
        total += n;
    }
    return 0;
}

void tls13_init(tls13_conn *conn) {
    memset(conn, 0, sizeof(*conn));
    conn->sock = -1;
    sha256_init(&conn->transcript);
}

static bool tls13_tcp_connect(tls13_conn *conn, const char *host, int port) {
    struct addrinfo hints = {0}, *res;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);
    if (getaddrinfo(host, port_str, &hints, &res) != 0) return false;

    conn->sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (conn->sock < 0) { freeaddrinfo(res); return false; }

    /* Set timeouts */
    struct timeval tv = {30, 0};
    setsockopt(conn->sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(conn->sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    if (connect(conn->sock, res->ai_addr, res->ai_addrlen) < 0) {
        close(conn->sock);
        conn->sock = -1;
        freeaddrinfo(res);
        return false;
    }

    freeaddrinfo(res);
    strncpy(conn->server_name, host, sizeof(conn->server_name) - 1);
    return true;
}

static void build_nonce(uint8_t nonce[12], const uint8_t iv[12], uint64_t seq) {
    memcpy(nonce, iv, 12);
    for (int i = 0; i < 8; i++) {
        nonce[12 - 1 - i] ^= (seq >> (i * 8)) & 0xff;
    }
}

static bool tls13_send_record(tls13_conn *conn, uint8_t type, const uint8_t *data, size_t len) {
    /* In server mode, use server_encrypted flag; in client mode, use client_encrypted */
    bool should_encrypt = conn->server_encrypted || conn->client_encrypted;

    DPRINTF("[TLS] send_record: type=%d len=%zu server_enc=%d client_enc=%d should_enc=%d\n",
            type, len, conn->server_encrypted, conn->client_encrypted, should_encrypt);

    if (!should_encrypt) {
        /* Plaintext record */
        uint8_t header[5];
        header[0] = type;
        header[1] = 0x03; header[2] = 0x01;  /* TLS 1.0 for compatibility */
        header[3] = (len >> 8) & 0xff;
        header[4] = len & 0xff;

        DPRINTF("[TLS] Sending record: type=%d len=%zu header=[%02x %02x %02x %02x %02x]\n",
                type, len, header[0], header[1], header[2], header[3], header[4]);
        DPRINTF("[TLS] Full data (%zu bytes):\n", len);
        for (size_t i = 0; i < len; i++) {
            DPRINTF("%02x ", data[i]);
            if ((i + 1) % 16 == 0) DPRINTF("\n");
        }
        DPRINTF("\n");

        if (sock_write(conn->sock, header, 5) < 0) return false;
        if (sock_write(conn->sock, data, len) < 0) return false;
    } else {
        /* Encrypted record */
        size_t plaintext_len = len + 1;  /* + content type */
        uint8_t *plaintext = malloc(plaintext_len);
        memcpy(plaintext, data, len);
        plaintext[len] = type;  /* Inner content type */

        uint8_t *ciphertext = malloc(plaintext_len + 16);
        uint8_t tag[16];
        uint8_t nonce[12];

        /* Choose keys and IV based on who is sending:
         * - If server_encrypted is true and client_encrypted is false, we're the server sending (use server keys)
         * - If client_encrypted is true, we're the client sending (use client keys)
         * Note: server_key/server_iv and client_key/client_iv are populated with handshake keys first,
         * then overwritten with traffic keys after derive_traffic_secrets().
         */
        const uint8_t *send_key;
        const uint8_t *send_iv;
        uint64_t *send_seq;

        if (conn->server_encrypted && !conn->client_encrypted) {
            /* Server mode - use server keys (handshake keys at this point) */
            send_key = conn->server_key;
            send_iv = conn->server_iv;
            send_seq = &conn->server_seq;
            DPRINTF("[TLS] Using SERVER keys for send (seq=%llu)\n", conn->server_seq);
        } else {
            /* Client mode - use client keys */
            send_key = conn->client_key;
            send_iv = conn->client_iv;
            send_seq = &conn->client_seq;
            DPRINTF("[TLS] Using CLIENT keys for send (seq=%llu)\n", conn->client_seq);
        }

        build_nonce(nonce, send_iv, *send_seq);

        /* AAD: record header with encrypted length */
        uint8_t aad[5] = {TLS_APPLICATION_DATA, 0x03, 0x03, 0, 0};
        size_t record_len = plaintext_len + 16;
        aad[3] = (record_len >> 8) & 0xff;
        aad[4] = record_len & 0xff;

        chacha20_poly1305_encrypt(send_key, nonce, aad, 5,
                                   plaintext, plaintext_len, ciphertext, tag);

        if (sock_write(conn->sock, aad, 5) < 0) { free(plaintext); free(ciphertext); return false; }
        if (sock_write(conn->sock, ciphertext, plaintext_len) < 0) { free(plaintext); free(ciphertext); return false; }
        if (sock_write(conn->sock, tag, 16) < 0) { free(plaintext); free(ciphertext); return false; }

        (*send_seq)++;
        free(plaintext);
        free(ciphertext);
    }
    return true;
}

static int tls13_recv_record(tls13_conn *conn, uint8_t *type, uint8_t *data, size_t max_len) {
    uint8_t header[5];
    DPRINTF("[TLS] recv_record: reading 5-byte header...\n");
    if (sock_read(conn->sock, header, 5) < 0) {
        DPRINTF("[TLS] recv_record: header read failed\n");
        return -1;
    }

    *type = header[0];
    size_t len = ((size_t)header[3] << 8) | header[4];
    DPRINTF("[TLS] recv_record: header type=%d len=%zu\n", *type, len);
    if (len > max_len) {
        DPRINTF("[TLS] recv_record: len %zu > max_len %zu\n", len, max_len);
        return -1;
    }

    if (sock_read(conn->sock, data, len) < 0) {
        DPRINTF("[TLS] recv_record: data read failed\n");
        return -1;
    }

    bool should_decrypt = conn->server_encrypted || conn->client_encrypted;

    if (should_decrypt && *type == TLS_APPLICATION_DATA) {
        /* Decrypt - choose keys based on mode:
         * - Server mode (server_encrypted && !client_encrypted): receive uses client keys
         * - Client mode: receive uses server keys
         */
        const uint8_t *recv_key;
        const uint8_t *recv_iv;
        uint64_t *recv_seq;

        if (conn->is_server) {
            /* Server mode - decrypt client's messages with client keys */
            recv_key = conn->client_key;
            recv_iv = conn->client_iv;
            recv_seq = &conn->client_seq;
            DPRINTF("[TLS] Server receiving: using CLIENT keys (seq=%llu)\n", conn->client_seq);
        } else {
            /* Client mode - decrypt server's messages with server keys */
            recv_key = conn->server_key;
            recv_iv = conn->server_iv;
            recv_seq = &conn->server_seq;
            DPRINTF("[TLS] Client receiving: using SERVER keys (seq=%llu)\n", conn->server_seq);
        }

        DPRINTF("[TLS] Decrypting record: len=%zu seq=%llu\n", len, *recv_seq);
        if (len < 17) { DPRINTF("[TLS] Record too short for tag\n"); return -1; }
        size_t ciphertext_len = len - 16;

        uint8_t nonce[12];
        build_nonce(nonce, recv_iv, *recv_seq);

        DPRINTF("[TLS] Nonce: ");
        for (int i = 0; i < 12; i++) DPRINTF("%02x", nonce[i]);
        DPRINTF("\n");
        DPRINTF("[TLS] Ciphertext (%zu bytes): ", ciphertext_len);
        for (size_t i = 0; i < ciphertext_len && i < 16; i++) DPRINTF("%02x", data[i]);
        DPRINTF("\n");
        DPRINTF("[TLS] Tag (16 bytes): ");
        for (int i = 0; i < 16; i++) DPRINTF("%02x", data[ciphertext_len + i]);
        DPRINTF("\n");

        uint8_t aad[5] = {TLS_APPLICATION_DATA, 0x03, 0x03, 0, 0};
        aad[3] = (len >> 8) & 0xff;
        aad[4] = len & 0xff;
        DPRINTF("[TLS] AAD: %02x %02x %02x %02x %02x\n", aad[0], aad[1], aad[2], aad[3], aad[4]);

        uint8_t *plaintext = malloc(ciphertext_len);
        if (!chacha20_poly1305_decrypt(recv_key, nonce, aad, 5,
                                        data, ciphertext_len, data + ciphertext_len, plaintext)) {
            DPRINTF("[TLS] Decryption failed (bad MAC)\n");
            free(plaintext);
            return -1;
        }
        DPRINTF("[TLS] Decryption successful, ciphertext_len=%zu\n", ciphertext_len);

        /* Find inner content type (last non-zero byte) */
        size_t pt_len = ciphertext_len;
        while (pt_len > 0 && plaintext[pt_len - 1] == 0) pt_len--;
        if (pt_len == 0) { free(plaintext); return -1; }
        *type = plaintext[pt_len - 1];
        pt_len--;

        memcpy(data, plaintext, pt_len);
        free(plaintext);
        (*recv_seq)++;
        return pt_len;
    }

    return len;
}

static bool tls13_send_client_hello(tls13_conn *conn) {
    uint8_t msg[1024];
    size_t pos = 0;
    uint8_t session_id[32];

    /* Generate keys and session ID */
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        read(fd, conn->client_random, 32);
        read(fd, session_id, 32);
        close(fd);
    }
    x25519_keygen(conn->client_public, conn->client_private);

    /* Handshake header (filled later) */
    msg[pos++] = TLS_CLIENT_HELLO;
    size_t len_pos = pos;
    pos += 3;

    /* Client version (TLS 1.2 for compatibility) */
    msg[pos++] = 0x03; msg[pos++] = 0x03;

    /* Random */
    memcpy(msg + pos, conn->client_random, 32);
    pos += 32;

    /* Session ID - 32 bytes for middlebox compatibility */
    msg[pos++] = 32;
    memcpy(msg + pos, session_id, 32);
    pos += 32;

    /* Cipher suites - ChaCha20-Poly1305 only (what we implement) */
    msg[pos++] = 0; msg[pos++] = 2;  /* Length */
    msg[pos++] = 0x13; msg[pos++] = 0x03;  /* TLS_CHACHA20_POLY1305_SHA256 */

    /* Compression */
    msg[pos++] = 1; msg[pos++] = 0;  /* null compression */

    /* Extensions length (filled later) */
    size_t ext_len_pos = pos;
    pos += 2;
    size_t ext_start = pos;

    /* SNI extension */
    size_t name_len = strlen(conn->server_name);
    msg[pos++] = 0; msg[pos++] = TLS_EXT_SERVER_NAME;
    msg[pos++] = ((name_len + 5) >> 8) & 0xff;
    msg[pos++] = (name_len + 5) & 0xff;
    msg[pos++] = ((name_len + 3) >> 8) & 0xff;
    msg[pos++] = (name_len + 3) & 0xff;
    msg[pos++] = 0;  /* Host name type */
    msg[pos++] = (name_len >> 8) & 0xff;
    msg[pos++] = name_len & 0xff;
    memcpy(msg + pos, conn->server_name, name_len);
    pos += name_len;

    /* Supported versions extension (TLS 1.3) */
    msg[pos++] = 0; msg[pos++] = TLS_EXT_SUPPORTED_VERSIONS;
    msg[pos++] = 0; msg[pos++] = 3;
    msg[pos++] = 2;
    msg[pos++] = 0x03; msg[pos++] = 0x04;  /* TLS 1.3 */

    /* Supported groups extension */
    msg[pos++] = 0; msg[pos++] = TLS_EXT_SUPPORTED_GROUPS;
    msg[pos++] = 0; msg[pos++] = 4;
    msg[pos++] = 0; msg[pos++] = 2;
    msg[pos++] = 0x00; msg[pos++] = 0x1d;  /* x25519 */

    /* Signature algorithms */
    msg[pos++] = 0; msg[pos++] = TLS_EXT_SIGNATURE_ALGOS;
    msg[pos++] = 0; msg[pos++] = 18;  /* Extension length */
    msg[pos++] = 0; msg[pos++] = 16;  /* Algorithms list length */
    msg[pos++] = 0x04; msg[pos++] = 0x03;  /* ecdsa_secp256r1_sha256 */
    msg[pos++] = 0x05; msg[pos++] = 0x03;  /* ecdsa_secp384r1_sha384 */
    msg[pos++] = 0x06; msg[pos++] = 0x03;  /* ecdsa_secp521r1_sha512 */
    msg[pos++] = 0x08; msg[pos++] = 0x04;  /* rsa_pss_rsae_sha256 */
    msg[pos++] = 0x08; msg[pos++] = 0x05;  /* rsa_pss_rsae_sha384 */
    msg[pos++] = 0x08; msg[pos++] = 0x06;  /* rsa_pss_rsae_sha512 */
    msg[pos++] = 0x04; msg[pos++] = 0x01;  /* rsa_pkcs1_sha256 */
    msg[pos++] = 0x05; msg[pos++] = 0x01;  /* rsa_pkcs1_sha384 */

    /* Key share extension */
    msg[pos++] = 0; msg[pos++] = TLS_EXT_KEY_SHARE;
    msg[pos++] = 0; msg[pos++] = 38;  /* Extension length: 2 + 2 + 2 + 32 */
    msg[pos++] = 0; msg[pos++] = 36;  /* Key share entries length: 2 + 2 + 32 */
    msg[pos++] = 0x00; msg[pos++] = 0x1d;  /* x25519 */
    msg[pos++] = 0; msg[pos++] = 32;  /* Key length */
    memcpy(msg + pos, conn->client_public, 32);
    pos += 32;

    /* Fill extension length */
    size_t ext_len = pos - ext_start;
    msg[ext_len_pos] = (ext_len >> 8) & 0xff;
    msg[ext_len_pos + 1] = ext_len & 0xff;

    /* Fill handshake length */
    size_t hs_len = pos - 4;
    msg[len_pos] = (hs_len >> 16) & 0xff;
    msg[len_pos + 1] = (hs_len >> 8) & 0xff;
    msg[len_pos + 2] = hs_len & 0xff;

    /* Update transcript */
    sha256_update(&conn->transcript, msg, pos);

    DPRINTF("[TLS] ClientHello: %zu bytes\n", pos);

    return tls13_send_record(conn, TLS_HANDSHAKE, msg, pos);
}

static bool tls13_recv_server_hello(tls13_conn *conn) {
    uint8_t buf[16384];
    uint8_t type;
    int len = tls13_recv_record(conn, &type, buf, sizeof(buf));
    DPRINTF("[TLS] recv_record: len=%d type=%d\n", len, type);
    if (len < 0 || type != TLS_HANDSHAKE) {
        DPRINTF("[TLS] Expected HANDSHAKE(%d), got type=%d len=%d\n", TLS_HANDSHAKE, type, len);
        if (type == TLS_ALERT && len >= 2) {
            DPRINTF("[TLS] ALERT: level=%d desc=%d\n", buf[0], buf[1]);
            /* Common alerts: 40=handshake_failure, 70=protocol_version, 50=decode_error */
        }
        return false;
    }

    DPRINTF("[TLS] Handshake type: %d (expected ServerHello=%d)\n", buf[0], TLS_SERVER_HELLO);
    if (buf[0] != TLS_SERVER_HELLO) return false;

    size_t pos = 4;  /* Skip type and length */

    /* Server version */
    pos += 2;

    /* Server random */
    memcpy(conn->server_random, buf + pos, 32);
    pos += 32;

    /* Session ID */
    uint8_t sid_len = buf[pos++];
    pos += sid_len;

    /* Cipher suite */
    uint16_t cipher = ((uint16_t)buf[pos] << 8) | buf[pos + 1];
    pos += 2;
    DPRINTF("[TLS] Server selected cipher: 0x%04x (ChaCha20=0x1303, AES128GCM=0x1301)\n", cipher);
    if (cipher != 0x1303) {
        DPRINTF("[TLS] Server didn't select ChaCha20-Poly1305, we don't support other ciphers\n");
        /* Continue anyway to see what happens */
    }

    /* Compression */
    pos++;

    /* Extensions */
    if (pos + 2 > (size_t)len) return false;
    size_t ext_len = ((size_t)buf[pos] << 8) | buf[pos + 1];
    pos += 2;

    size_t ext_end = pos + ext_len;
    while (pos + 4 <= ext_end) {
        uint16_t ext_type = ((uint16_t)buf[pos] << 8) | buf[pos + 1];
        uint16_t elen = ((uint16_t)buf[pos + 2] << 8) | buf[pos + 3];
        pos += 4;

        if (ext_type == TLS_EXT_KEY_SHARE) {
            /* x25519 key share */
            DPRINTF("[TLS] Found key_share ext, elen=%d, group=%02x%02x\n",
                    elen, buf[pos], buf[pos + 1]);
            if (elen >= 36 && buf[pos] == 0x00 && buf[pos + 1] == 0x1d) {
                memcpy(conn->server_public, buf + pos + 4, 32);
                DPRINTF("[TLS] Server public key: ");
                for (int i = 0; i < 8; i++) DPRINTF("%02x", conn->server_public[i]);
                DPRINTF("...\n");
            }
        }

        pos += elen;
    }

    /* Update transcript */
    sha256_update(&conn->transcript, buf, len);

    /* Compute shared secret */
    x25519(conn->shared_secret, conn->client_private, conn->server_public);

    return true;
}

static void derive_traffic_secrets(tls13_conn *conn);  /* Forward declaration */

static void derive_handshake_secrets(tls13_conn *conn) {
    /* Get transcript hash so far */
    sha256_ctx ctx_copy;
    memcpy(&ctx_copy, &conn->transcript, sizeof(ctx_copy));
    uint8_t transcript_hash[32];
    sha256_final(&ctx_copy, transcript_hash);

    DPRINTF("[TLS] Shared secret: ");
    for (int i = 0; i < 32; i++) DPRINTF("%02x", conn->shared_secret[i]);
    DPRINTF("\n");

    DPRINTF("[TLS] Transcript hash: ");
    for (int i = 0; i < 32; i++) DPRINTF("%02x", transcript_hash[i]);
    DPRINTF("\n");

    /* Early secret (from 0) */
    uint8_t early_secret[32];
    uint8_t zeros[32] = {0};
    hkdf_extract(NULL, 0, zeros, 32, early_secret);

    DPRINTF("[TLS] Early secret: ");
    for (int i = 0; i < 32; i++) DPRINTF("%02x", early_secret[i]);
    DPRINTF("\n");

    /* Derived secret */
    uint8_t derived[32];
    uint8_t empty_hash[32];
    sha256(NULL, 0, empty_hash);

    DPRINTF("[TLS] Empty hash: ");
    for (int i = 0; i < 32; i++) DPRINTF("%02x", empty_hash[i]);
    DPRINTF("\n");

    hkdf_expand_label(early_secret, "derived", empty_hash, 32, derived, 32);

    DPRINTF("[TLS] Derived: ");
    for (int i = 0; i < 32; i++) DPRINTF("%02x", derived[i]);
    DPRINTF("\n");

    /* Handshake secret */
    uint8_t handshake_secret[32];
    hkdf_extract(derived, 32, conn->shared_secret, 32, handshake_secret);

    DPRINTF("[TLS] Handshake secret: ");
    for (int i = 0; i < 32; i++) DPRINTF("%02x", handshake_secret[i]);
    DPRINTF("\n");

    /* Client/server handshake secrets */
    hkdf_expand_label(handshake_secret, "c hs traffic", transcript_hash, 32,
                      conn->client_handshake_secret, 32);
    hkdf_expand_label(handshake_secret, "s hs traffic", transcript_hash, 32,
                      conn->server_handshake_secret, 32);

    DPRINTF("[TLS] Server HS traffic secret: ");
    for (int i = 0; i < 32; i++) DPRINTF("%02x", conn->server_handshake_secret[i]);
    DPRINTF("\n");

    /* Derive keys and IVs (both client and server for bidirectional communication) */
    hkdf_expand_label(conn->client_handshake_secret, "key", NULL, 0, conn->client_key, 32);
    hkdf_expand_label(conn->client_handshake_secret, "iv", NULL, 0, conn->client_iv, 12);
    hkdf_expand_label(conn->server_handshake_secret, "key", NULL, 0, conn->server_key, 32);
    hkdf_expand_label(conn->server_handshake_secret, "iv", NULL, 0, conn->server_iv, 12);

    DPRINTF("[TLS] Client key: ");
    for (int i = 0; i < 32; i++) DPRINTF("%02x", conn->client_key[i]);
    DPRINTF("\n");
    DPRINTF("[TLS] Client IV: ");
    for (int i = 0; i < 12; i++) DPRINTF("%02x", conn->client_iv[i]);
    DPRINTF("\n");
    DPRINTF("[TLS] Server key: ");
    for (int i = 0; i < 32; i++) DPRINTF("%02x", conn->server_key[i]);
    DPRINTF("\n");
    DPRINTF("[TLS] Server IV: ");
    for (int i = 0; i < 12; i++) DPRINTF("%02x", conn->server_iv[i]);
    DPRINTF("\n");

    /* Store for later traffic secret derivation */
    hkdf_expand_label(handshake_secret, "derived", empty_hash, 32, derived, 32);
    hkdf_extract(derived, 32, zeros, 32, conn->client_traffic_secret);  /* Reuse as master_secret temp */
}

static bool tls13_recv_encrypted_extensions(tls13_conn *conn) {
    uint8_t buf[16384];
    uint8_t type;

    /* Need to decrypt with handshake keys */
    hkdf_expand_label(conn->server_handshake_secret, "key", NULL, 0, conn->server_key, 32);
    hkdf_expand_label(conn->server_handshake_secret, "iv", NULL, 0, conn->server_iv, 12);

    /* Mark server as sending encrypted data now */
    conn->server_encrypted = true;
    conn->server_seq = 0;

    DPRINTF("[TLS] Server encryption enabled\n");
    DPRINTF("[TLS] Server key: ");
    for (int i = 0; i < 8; i++) DPRINTF("%02x", conn->server_key[i]);
    DPRINTF("...\n");
    DPRINTF("[TLS] Server IV: ");
    for (int i = 0; i < 12; i++) DPRINTF("%02x", conn->server_iv[i]);
    DPRINTF("\n");

    int len = tls13_recv_record(conn, &type, buf, sizeof(buf));
    DPRINTF("[TLS] recv_encrypted_extensions: len=%d type=%d\n", len, type);

    /* Skip change cipher spec if present */
    if (type == TLS_CHANGE_CIPHER_SPEC) {
        DPRINTF("[TLS] Skipping ChangeCipherSpec, reading next record...\n");
        len = tls13_recv_record(conn, &type, buf, sizeof(buf));
        DPRINTF("[TLS] After CCS skip: len=%d type=%d\n", len, type);
    }

    if (len < 0) {
        DPRINTF("[TLS] recv failed with len=%d\n", len);
        return false;
    }

    /* Process all handshake messages */
    size_t pos = 0;
    while (pos < (size_t)len) {
        if (pos + 4 > (size_t)len) break;
        size_t hs_len = ((size_t)buf[pos + 1] << 16) | ((size_t)buf[pos + 2] << 8) | buf[pos + 3];

        sha256_update(&conn->transcript, buf + pos, 4 + hs_len);
        pos += 4 + hs_len;
    }

    return true;
}

static bool tls13_recv_remaining_handshake(tls13_conn *conn) {
    uint8_t buf[16384];
    uint8_t type;
    bool got_finished = false;

    while (!got_finished) {
        int len = tls13_recv_record(conn, &type, buf, sizeof(buf));
        if (len < 0) return false;

        if (type == TLS_CHANGE_CIPHER_SPEC) continue;
        if (type == TLS_ALERT) return false;
        if (type != TLS_HANDSHAKE) continue;

        size_t pos = 0;
        while (pos < (size_t)len) {
            if (pos + 4 > (size_t)len) break;
            uint8_t hs_type = buf[pos];
            size_t hs_len = ((size_t)buf[pos + 1] << 16) | ((size_t)buf[pos + 2] << 8) | buf[pos + 3];

            if (hs_type == TLS_FINISHED) {
                /* Update transcript before Finished */
                sha256_update(&conn->transcript, buf + pos, 4 + hs_len);
                got_finished = true;
            } else {
                sha256_update(&conn->transcript, buf + pos, 4 + hs_len);
            }

            pos += 4 + hs_len;
        }
    }

    return true;
}

static bool tls13_send_finished(tls13_conn *conn) {
    /* Set client handshake keys for sending */
    hkdf_expand_label(conn->client_handshake_secret, "key", NULL, 0, conn->client_key, 32);
    hkdf_expand_label(conn->client_handshake_secret, "iv", NULL, 0, conn->client_iv, 12);

    /* Get transcript hash */
    sha256_ctx ctx_copy;
    memcpy(&ctx_copy, &conn->transcript, sizeof(ctx_copy));
    uint8_t transcript_hash[32];
    sha256_final(&ctx_copy, transcript_hash);

    /* Compute finished key and verify data */
    uint8_t finished_key[32];
    hkdf_expand_label(conn->client_handshake_secret, "finished", NULL, 0, finished_key, 32);

    uint8_t verify_data[32];
    hmac_sha256(finished_key, 32, transcript_hash, 32, verify_data);

    /* Build Finished message */
    uint8_t msg[36];
    msg[0] = TLS_FINISHED;
    msg[1] = 0; msg[2] = 0; msg[3] = 32;
    memcpy(msg + 4, verify_data, 32);

    /* Enable client encryption for Finished message */
    conn->client_encrypted = true;
    conn->client_seq = 0;
    DPRINTF("[TLS] Client encryption enabled, sending Finished\n");

    bool ok = tls13_send_record(conn, TLS_HANDSHAKE, msg, 36);
    if (!ok) return false;

    /* Derive traffic secrets BEFORE updating transcript
     * Traffic secrets use transcript ending at server Finished (not including client Finished) */
    derive_traffic_secrets(conn);

    /* Now update transcript with client Finished (for resumption tickets, etc.) */
    sha256_update(&conn->transcript, msg, 36);

    return true;
}

static void derive_traffic_secrets(tls13_conn *conn) {
    /* Get final transcript hash */
    sha256_ctx ctx_copy;
    memcpy(&ctx_copy, &conn->transcript, sizeof(ctx_copy));
    uint8_t transcript_hash[32];
    sha256_final(&ctx_copy, transcript_hash);

    /* Master secret is in client_traffic_secret from earlier derivation */
    uint8_t master_secret[32];
    uint8_t zeros[32] = {0};
    uint8_t empty_hash[32];
    sha256(NULL, 0, empty_hash);

    /* Re-derive master secret properly */
    uint8_t early_secret[32];
    hkdf_extract(NULL, 0, zeros, 32, early_secret);
    uint8_t derived[32];
    hkdf_expand_label(early_secret, "derived", empty_hash, 32, derived, 32);
    uint8_t handshake_secret[32];
    hkdf_extract(derived, 32, conn->shared_secret, 32, handshake_secret);
    hkdf_expand_label(handshake_secret, "derived", empty_hash, 32, derived, 32);
    hkdf_extract(derived, 32, zeros, 32, master_secret);

    /* Derive traffic secrets */
    hkdf_expand_label(master_secret, "c ap traffic", transcript_hash, 32,
                      conn->client_traffic_secret, 32);
    hkdf_expand_label(master_secret, "s ap traffic", transcript_hash, 32,
                      conn->server_traffic_secret, 32);

    /* Derive traffic keys */
    hkdf_expand_label(conn->client_traffic_secret, "key", NULL, 0, conn->client_key, 32);
    hkdf_expand_label(conn->client_traffic_secret, "iv", NULL, 0, conn->client_iv, 12);
    hkdf_expand_label(conn->server_traffic_secret, "key", NULL, 0, conn->server_key, 32);
    hkdf_expand_label(conn->server_traffic_secret, "iv", NULL, 0, conn->server_iv, 12);

    /* Reset sequence numbers */
    conn->client_seq = 0;
    conn->server_seq = 0;
}

bool tls13_connect(tls13_conn *conn, const char *host, int port) {
    tls13_init(conn);
    conn->is_server = false;  /* Client mode */

    DPRINTF("[TLS] TCP connecting...\n");
    if (!tls13_tcp_connect(conn, host, port)) { DPRINTF("[TLS] TCP connect failed\n"); return false; }
    DPRINTF("[TLS] TCP connected, sending ClientHello...\n");
    if (!tls13_send_client_hello(conn)) { DPRINTF("[TLS] ClientHello failed\n"); return false; }
    DPRINTF("[TLS] Waiting for ServerHello...\n");
    if (!tls13_recv_server_hello(conn)) { DPRINTF("[TLS] ServerHello failed\n"); return false; }
    DPRINTF("[TLS] Got ServerHello, deriving secrets...\n");

    derive_handshake_secrets(conn);

    DPRINTF("[TLS] Waiting for EncryptedExtensions...\n");
    if (!tls13_recv_encrypted_extensions(conn)) { DPRINTF("[TLS] EncryptedExtensions failed\n"); return false; }
    DPRINTF("[TLS] Waiting for remaining handshake...\n");
    if (!tls13_recv_remaining_handshake(conn)) { DPRINTF("[TLS] Remaining handshake failed\n"); return false; }
    DPRINTF("[TLS] Sending Finished...\n");
    if (!tls13_send_finished(conn)) { DPRINTF("[TLS] Finished failed\n"); return false; }

    conn->handshake_complete = true;
    DPRINTF("[TLS] Handshake complete!\n");

    return true;
}

/* =============================================================================
 * TLS 1.3 Server Handshake
 * ============================================================================= */

bool tls13_accept(tls13_conn *conn) {
    /* Server-side TLS 1.3 handshake
     *
     * 1. Receive ClientHello
     * 2. Send ServerHello with key share
     * 3. Derive handshake secrets
     * 4. Send EncryptedExtensions, Certificate, CertificateVerify, Finished
     * 5. Receive client Finished
     * 6. Derive application traffic secrets
     */

    /* Mark this connection as server mode */
    conn->is_server = true;

    uint8_t buf[16384];
    uint8_t type;

    /* Step 1: Receive ClientHello */
    DPRINTF("[TLS Server] Waiting for ClientHello...\n");
    int len = tls13_recv_record(conn, &type, buf, sizeof(buf));
    if (len < 0 || type != TLS_HANDSHAKE || buf[0] != TLS_CLIENT_HELLO) {
        DPRINTF("[TLS Server] Failed to receive ClientHello\n");
        return false;
    }

    /* Parse ClientHello */
    size_t pos = 4;  /* Skip handshake type and length */

    /* Client version */
    if (pos + 2 > (size_t)len) {
        DPRINTF("[TLS Server] ClientHello too short for version\n");
        return false;
    }
    pos += 2;

    /* Client random */
    if (pos + 32 > (size_t)len) {
        DPRINTF("[TLS Server] ClientHello too short for random\n");
        return false;
    }
    memcpy(conn->client_random, buf + pos, 32);
    pos += 32;

    /* Session ID */
    if (pos + 1 > (size_t)len) {
        DPRINTF("[TLS Server] ClientHello too short for session ID length\n");
        return false;
    }
    uint8_t sid_len = buf[pos++];
    uint8_t session_id[32];
    if (sid_len > 0) {
        if (pos + sid_len > (size_t)len) {
            DPRINTF("[TLS Server] ClientHello too short for session ID\n");
            return false;
        }
        memcpy(session_id, buf + pos, sid_len);
        pos += sid_len;
    }

    /* Cipher suites */
    if (pos + 2 > (size_t)len) {
        DPRINTF("[TLS Server] ClientHello too short for cipher suites length\n");
        return false;
    }
    uint16_t cs_len = ((uint16_t)buf[pos] << 8) | buf[pos + 1];
    pos += 2;
    if (pos + cs_len > (size_t)len) {
        DPRINTF("[TLS Server] ClientHello too short for cipher suites\n");
        return false;
    }
    pos += cs_len;  /* Skip cipher suites for now */

    /* Compression methods */
    if (pos + 1 > (size_t)len) {
        DPRINTF("[TLS Server] ClientHello too short for compression length\n");
        return false;
    }
    uint8_t comp_len = buf[pos++];
    if (pos + comp_len > (size_t)len) {
        DPRINTF("[TLS Server] ClientHello too short for compression methods\n");
        return false;
    }
    pos += comp_len;

    /* Extensions */
    if (pos + 2 > (size_t)len) {
        DPRINTF("[TLS Server] ClientHello too short for extensions length\n");
        return false;
    }
    uint16_t ext_total_len = ((uint16_t)buf[pos] << 8) | buf[pos + 1];
    pos += 2;
    size_t ext_end = pos + ext_total_len;
    if (ext_end > (size_t)len) {
        DPRINTF("[TLS Server] ClientHello extensions exceed message length\n");
        return false;
    }

    DPRINTF("[TLS Server] Extensions total length: %d, parsing from pos=%zu to %zu\n", ext_total_len, pos, ext_end);

    uint8_t client_public[32] = {0};
    bool got_key_share = false;

    while (pos + 4 <= ext_end) {
        uint16_t ext_type = ((uint16_t)buf[pos] << 8) | buf[pos + 1];
        uint16_t elen = ((uint16_t)buf[pos + 2] << 8) | buf[pos + 3];
        pos += 4;

        if (pos + elen > ext_end) {
            DPRINTF("[TLS Server] Extension length exceeds extensions block\n");
            return false;
        }

        DPRINTF("[TLS Server] Extension type=0x%04x len=%d at pos=%zu\n", ext_type, elen, pos - 4);

        if (ext_type == TLS_EXT_KEY_SHARE) {
            DPRINTF("[TLS Server] Found key_share extension, elen=%d\n", elen);
            /* key_share extension for ClientHello: key_share_entry list length(2) + entries */
            if (elen >= 38 && pos + 2 <= pos + elen) {
                uint16_t kse_list_len = ((uint16_t)buf[pos] << 8) | buf[pos + 1];
                pos += 2;
                DPRINTF("[TLS Server] key_share_entry list length: %d\n", kse_list_len);

                size_t kse_end = pos + kse_list_len;
                if (kse_end > pos + elen - 2) {
                    DPRINTF("[TLS Server] key_share_entry list exceeds extension\n");
                    return false;
                }

                while (pos + 6 <= kse_end) {
                    uint16_t group = ((uint16_t)buf[pos] << 8) | buf[pos + 1];
                    pos += 2;
                    uint16_t key_len = ((uint16_t)buf[pos] << 8) | buf[pos + 1];
                    pos += 2;

                    DPRINTF("[TLS Server] key_share entry: group=0x%04x key_len=%d\n", group, key_len);

                    if (pos + key_len > kse_end) {
                        DPRINTF("[TLS Server] key_share entry exceeds list\n");
                        return false;
                    }

                    if (group == 0x001d && key_len == 32) {  /* x25519 */
                        memcpy(client_public, buf + pos, 32);
                        got_key_share = true;
                        DPRINTF("[TLS Server] Got x25519 client public key\n");
                    }
                    pos += key_len;
                }
            } else {
                pos += elen;
            }
        } else {
            pos += elen;
        }
    }

    if (!got_key_share) {
        DPRINTF("[TLS Server] No x25519 key share in ClientHello\n");
        return false;
    }

    /* Update transcript with ClientHello */
    sha256_update(&conn->transcript, buf, len);

    /* Generate server keys */
    x25519_keygen(conn->server_public, conn->client_private);  /* Reuse client_private field for server */

    /* Compute shared secret */
    x25519(conn->shared_secret, conn->client_private, client_public);

    /* Step 2: Send ServerHello */
    DPRINTF("[TLS Server] Sending ServerHello...\n");
    uint8_t sh_msg[256];
    size_t sh_pos = 0;

    /* Handshake type */
    sh_msg[sh_pos++] = TLS_SERVER_HELLO;
    sh_msg[sh_pos++] = 0;  /* Length placeholder */
    sh_msg[sh_pos++] = 0;
    sh_msg[sh_pos++] = 0;

    /* Server version (0x0303 = TLS 1.2 for compatibility) */
    sh_msg[sh_pos++] = 0x03;
    sh_msg[sh_pos++] = 0x03;

    /* Server random */
    for (int i = 0; i < 32; i++) {
        conn->server_random[i] = (uint8_t)rand();
    }
    memcpy(sh_msg + sh_pos, conn->server_random, 32);
    sh_pos += 32;

    /* Session ID echo */
    sh_msg[sh_pos++] = sid_len;
    if (sid_len > 0) {
        memcpy(sh_msg + sh_pos, session_id, sid_len);
        sh_pos += sid_len;
    }

    /* Cipher suite: TLS_CHACHA20_POLY1305_SHA256 */
    sh_msg[sh_pos++] = 0x13;
    sh_msg[sh_pos++] = 0x03;

    /* Compression method: none */
    sh_msg[sh_pos++] = 0x00;

    /* Extensions */
    size_t ext_len_pos = sh_pos;
    sh_msg[sh_pos++] = 0;  /* Extensions length placeholder */
    sh_msg[sh_pos++] = 0;

    /* supported_versions extension */
    sh_msg[sh_pos++] = 0x00;  /* Extension type: supported_versions */
    sh_msg[sh_pos++] = 0x2b;
    sh_msg[sh_pos++] = 0x00;  /* Length: 2 */
    sh_msg[sh_pos++] = 0x02;
    sh_msg[sh_pos++] = 0x03;  /* TLS 1.3 */
    sh_msg[sh_pos++] = 0x04;

    /* key_share extension */
    sh_msg[sh_pos++] = 0x00;  /* Extension type: key_share */
    sh_msg[sh_pos++] = 0x33;
    sh_msg[sh_pos++] = 0x00;  /* Length: 36 */
    sh_msg[sh_pos++] = 0x24;
    sh_msg[sh_pos++] = 0x00;  /* Group: x25519 */
    sh_msg[sh_pos++] = 0x1d;
    sh_msg[sh_pos++] = 0x00;  /* Key length: 32 */
    sh_msg[sh_pos++] = 0x20;
    memcpy(sh_msg + sh_pos, conn->server_public, 32);
    sh_pos += 32;

    /* Fill in extensions length */
    size_t ext_len = sh_pos - ext_len_pos - 2;
    sh_msg[ext_len_pos] = (ext_len >> 8) & 0xff;
    sh_msg[ext_len_pos + 1] = ext_len & 0xff;

    /* Fill in handshake message length */
    size_t sh_len = sh_pos - 4;
    sh_msg[1] = (sh_len >> 16) & 0xff;
    sh_msg[2] = (sh_len >> 8) & 0xff;
    sh_msg[3] = sh_len & 0xff;

    /* Update transcript with ServerHello */
    sha256_update(&conn->transcript, sh_msg, sh_pos);

    /* Send ServerHello */
    if (!tls13_send_record(conn, TLS_HANDSHAKE, sh_msg, sh_pos)) {
        DPRINTF("[TLS Server] Failed to send ServerHello\n");
        return false;
    }

    /* Step 3: Derive handshake secrets */
    derive_handshake_secrets(conn);
    conn->server_encrypted = true;

    /* Step 4: Send EncryptedExtensions */
    DPRINTF("[TLS Server] Sending EncryptedExtensions...\n");
    uint8_t ee_msg[16];
    ee_msg[0] = TLS_ENCRYPTED_EXTENSIONS;
    ee_msg[1] = 0x00;
    ee_msg[2] = 0x00;
    ee_msg[3] = 0x02;  /* Length: 2 */
    ee_msg[4] = 0x00;  /* Extensions length: 0 */
    ee_msg[5] = 0x00;

    sha256_update(&conn->transcript, ee_msg, 6);
    if (!tls13_send_record(conn, TLS_HANDSHAKE, ee_msg, 6)) {
        return false;
    }

    /* Step 5: Send Certificate (simplified - self-signed placeholder) */
    DPRINTF("[TLS Server] Sending Certificate...\n");
    uint8_t cert_msg[128];
    size_t cert_pos = 0;

    cert_msg[cert_pos++] = TLS_CERTIFICATE;
    cert_msg[cert_pos++] = 0x00;
    cert_msg[cert_pos++] = 0x00;
    cert_msg[cert_pos++] = 0x09;  /* Length: 9 */
    cert_msg[cert_pos++] = 0x00;  /* Certificate request context: 0 */
    cert_msg[cert_pos++] = 0x00;  /* Certificate list length: 5 */
    cert_msg[cert_pos++] = 0x00;
    cert_msg[cert_pos++] = 0x05;
    cert_msg[cert_pos++] = 0x00;  /* Empty cert entry length: 2 */
    cert_msg[cert_pos++] = 0x00;
    cert_msg[cert_pos++] = 0x02;
    cert_msg[cert_pos++] = 0x00;  /* Extensions: 0 */
    cert_msg[cert_pos++] = 0x00;

    sha256_update(&conn->transcript, cert_msg, cert_pos);
    if (!tls13_send_record(conn, TLS_HANDSHAKE, cert_msg, cert_pos)) {
        return false;
    }

    /* Step 6: Send CertificateVerify (simplified - skip signature for now) */
    /* In a real implementation, we'd sign the transcript hash with our private key */

    /* Step 7: Send server Finished */
    DPRINTF("[TLS Server] Sending Finished...\n");
    sha256_ctx ctx_copy;
    memcpy(&ctx_copy, &conn->transcript, sizeof(ctx_copy));
    uint8_t transcript_hash[32];
    sha256_final(&ctx_copy, transcript_hash);

    uint8_t finished_key[32];
    hkdf_expand_label(conn->server_handshake_secret, "finished", NULL, 0, finished_key, 32);

    uint8_t verify_data[32];
    hmac_sha256(finished_key, 32, transcript_hash, 32, verify_data);

    uint8_t fin_msg[36];
    fin_msg[0] = TLS_FINISHED;
    fin_msg[1] = 0x00;
    fin_msg[2] = 0x00;
    fin_msg[3] = 0x20;  /* Length: 32 */
    memcpy(fin_msg + 4, verify_data, 32);

    sha256_update(&conn->transcript, fin_msg, 36);
    if (!tls13_send_record(conn, TLS_HANDSHAKE, fin_msg, 36)) {
        return false;
    }

    /* Step 8: Receive Client Finished (still using handshake keys) */
    DPRINTF("[TLS Server] Waiting for client Finished...\n");
    uint8_t client_fin_buf[256];
    uint8_t client_fin_type;
    int client_fin_len = tls13_recv_record(conn, &client_fin_type, client_fin_buf, sizeof(client_fin_buf));
    if (client_fin_len < 0) {
        DPRINTF("[TLS Server] Failed to receive client Finished\n");
        return false;
    }

    if (client_fin_type != TLS_HANDSHAKE || client_fin_buf[0] != TLS_FINISHED) {
        DPRINTF("[TLS Server] Expected client Finished, got type=%d msg=%02x\n", client_fin_type, client_fin_buf[0]);
        return false;
    }
    DPRINTF("[TLS Server] Received client Finished (%d bytes)\n", client_fin_len);

    /* TODO: Verify client's finished message using client_handshake_secret */
    /* For now just accept it */

    /* Step 9: Now derive application traffic secrets */
    derive_traffic_secrets(conn);

    conn->handshake_complete = true;
    conn->client_encrypted = true;

    DPRINTF("[TLS Server] Handshake complete!\n");
    return true;
}

ssize_t tls13_send(tls13_conn *conn, const void *data, size_t len) {
    if (!conn->handshake_complete) return -1;
    if (!tls13_send_record(conn, TLS_APPLICATION_DATA, data, len)) return -1;
    return len;
}

ssize_t tls13_recv(tls13_conn *conn, void *data, size_t len) {
    if (!conn->handshake_complete) return -1;

    DPRINTF("[TLS] tls13_recv called, len=%zu, pending=%zu\n", len, conn->recv_len);

    /* First, return any pending data from previous reads */
    if (conn->recv_len > 0) {
        size_t copy = conn->recv_len < len ? conn->recv_len : len;
        memcpy(data, conn->recv_buf, copy);
        /* Move remaining data to front of buffer */
        if (copy < conn->recv_len) {
            memmove(conn->recv_buf, conn->recv_buf + copy, conn->recv_len - copy);
        }
        conn->recv_len -= copy;
        DPRINTF("[TLS] Returned %zu bytes from pending buffer, %zu remaining\n", copy, conn->recv_len);
        return copy;
    }

    /* No pending data, read a new TLS record */
    uint8_t buf[16657];
    uint8_t type;
    int n;

    /* Keep reading until we get application data (skip post-handshake messages like NewSessionTicket) */
    while (1) {
        n = tls13_recv_record(conn, &type, buf, sizeof(buf));
        DPRINTF("[TLS] tls13_recv_record returned n=%d, type=%d\n", n, type);
        if (n < 0) { DPRINTF("[TLS] recv_record failed!\n"); return -1; }

        if (type == TLS_ALERT) {
            if (n >= 2 && buf[0] == 1 && buf[1] == 0) {
                /* close_notify warning - end of data */
                return 0;
            }
            return -1;
        }
        if (type == TLS_HANDSHAKE) {
            /* Post-handshake message (NewSessionTicket etc), skip */
            continue;
        }
        if (type == TLS_APPLICATION_DATA) {
            break;
        }
        /* Unknown type, skip */
    }

    /* Copy as much as requested, buffer the rest */
    size_t copy = (size_t)n < len ? (size_t)n : len;
    memcpy(data, buf, copy);

    /* Store leftover data in pending buffer */
    if ((size_t)n > copy) {
        size_t leftover = (size_t)n - copy;
        if (leftover <= sizeof(conn->recv_buf)) {
            memcpy(conn->recv_buf, buf + copy, leftover);
            conn->recv_len = leftover;
            DPRINTF("[TLS] Buffered %zu leftover bytes\n", leftover);
        } else {
            DPRINTF("[TLS] Warning: leftover %zu bytes exceeds buffer size!\n", leftover);
        }
    }

    return copy;
}

void tls13_close(tls13_conn *conn) {
    if (conn->sock >= 0) {
        /* Send close_notify alert */
        if (conn->handshake_complete) {
            uint8_t alert[2] = {1, 0};  /* warning, close_notify */
            tls13_send_record(conn, TLS_ALERT, alert, 2);
        }
        close(conn->sock);
        conn->sock = -1;
    }
}

/* =============================================================================
 * Simple HTTPS GET
 * ============================================================================= */

bool tls13_https_get(const char *host, int port, const char *path,
                     uint8_t **response, size_t *response_len) {
    tls13_conn conn;
    *response = NULL;
    *response_len = 0;

    if (!tls13_connect(&conn, host, port)) {
        return false;
    }

    /* Build HTTP request */
    char request[4096];
    int req_len = snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: TLS13-C-Client/1.0\r\n"
        "Accept: */*\r\n"
        "Connection: close\r\n\r\n",
        path, host);

    if (tls13_send(&conn, request, req_len) < 0) {
        tls13_close(&conn);
        return false;
    }

    /* Read response */
    size_t capacity = 65536;
    *response = malloc(capacity);
    *response_len = 0;

    while (1) {
        if (*response_len >= capacity - 1) {
            capacity *= 2;
            *response = realloc(*response, capacity);
        }
        ssize_t n = tls13_recv(&conn, *response + *response_len, capacity - *response_len - 1);
        if (n <= 0) break;
        *response_len += n;
    }

    (*response)[*response_len] = '\0';
    tls13_close(&conn);
    return *response_len > 0;
}
