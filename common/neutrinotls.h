/*
 * Minimal TLS 1.3 Client - Pure C Implementation
 * No external crypto libraries - all primitives implemented here
 *
 * Implements:
 * - SHA-256, HMAC-SHA256, HKDF
 * - X25519 key exchange
 * - ChaCha20-Poly1305 AEAD
 * - TLS 1.3 handshake and record layer
 */

#ifndef TLS13_H
#define TLS13_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>

/* =============================================================================
 * SHA-256
 * ============================================================================= */

typedef struct {
    uint32_t state[8];
    uint64_t count;
    uint8_t buffer[64];
} sha256_ctx;

void sha256_init(sha256_ctx *ctx);
void sha256_update(sha256_ctx *ctx, const uint8_t *data, size_t len);
void sha256_final(sha256_ctx *ctx, uint8_t hash[32]);
void sha256(const uint8_t *data, size_t len, uint8_t hash[32]);

/* =============================================================================
 * HMAC-SHA256
 * ============================================================================= */

void hmac_sha256(const uint8_t *key, size_t key_len,
                 const uint8_t *data, size_t data_len,
                 uint8_t mac[32]);

/* =============================================================================
 * HKDF (RFC 5869)
 * ============================================================================= */

void hkdf_extract(const uint8_t *salt, size_t salt_len,
                  const uint8_t *ikm, size_t ikm_len,
                  uint8_t prk[32]);

void hkdf_expand(const uint8_t prk[32],
                 const uint8_t *info, size_t info_len,
                 uint8_t *okm, size_t okm_len);

void hkdf_expand_label(const uint8_t secret[32],
                       const char *label,
                       const uint8_t *context, size_t context_len,
                       uint8_t *out, size_t out_len);

/* =============================================================================
 * X25519 Key Exchange
 * ============================================================================= */

void x25519_keygen(uint8_t public_key[32], uint8_t private_key[32]);
void x25519(uint8_t shared[32], const uint8_t private_key[32], const uint8_t public_key[32]);

/* =============================================================================
 * ChaCha20-Poly1305 AEAD
 * ============================================================================= */

void chacha20_poly1305_encrypt(const uint8_t key[32], const uint8_t nonce[12],
                                const uint8_t *aad, size_t aad_len,
                                const uint8_t *plaintext, size_t plaintext_len,
                                uint8_t *ciphertext, uint8_t tag[16]);

bool chacha20_poly1305_decrypt(const uint8_t key[32], const uint8_t nonce[12],
                                const uint8_t *aad, size_t aad_len,
                                const uint8_t *ciphertext, size_t ciphertext_len,
                                const uint8_t tag[16], uint8_t *plaintext);

/* =============================================================================
 * TLS 1.3 Connection
 * ============================================================================= */

#define TLS13_MAX_RECORD_SIZE 16384
#define TLS13_MAX_PLAINTEXT   16384

typedef struct {
    int sock;

    /* Handshake state */
    bool handshake_complete;
    bool is_server;          /* true if this is server mode, false if client mode */
    bool server_encrypted;   /* Server is sending encrypted after ServerHello */
    bool client_encrypted;   /* Client is sending encrypted after client Finished */
    uint8_t client_random[32];
    uint8_t server_random[32];

    /* X25519 keys */
    uint8_t client_private[32];
    uint8_t client_public[32];
    uint8_t server_public[32];
    uint8_t shared_secret[32];

    /* Traffic secrets */
    uint8_t client_handshake_secret[32];
    uint8_t server_handshake_secret[32];
    uint8_t client_traffic_secret[32];
    uint8_t server_traffic_secret[32];

    /* Traffic keys */
    uint8_t client_key[32];
    uint8_t client_iv[12];
    uint8_t server_key[32];
    uint8_t server_iv[12];

    /* Sequence numbers */
    uint64_t client_seq;
    uint64_t server_seq;

    /* Transcript hash */
    sha256_ctx transcript;

    /* Buffers */
    uint8_t recv_buf[TLS13_MAX_RECORD_SIZE + 256];
    size_t recv_len;

    /* Server name (for SNI) */
    char server_name[256];

} tls13_conn;

/* Initialize connection */
void tls13_init(tls13_conn *conn);

/* Connect and perform handshake */
bool tls13_connect(tls13_conn *conn, const char *host, int port);

/* Accept incoming TLS connection (server mode) */
bool tls13_accept(tls13_conn *conn);

/* Send/receive application data */
ssize_t tls13_send(tls13_conn *conn, const void *data, size_t len);
ssize_t tls13_recv(tls13_conn *conn, void *data, size_t len);

/* Close connection */
void tls13_close(tls13_conn *conn);

/* Simple HTTPS GET */
bool tls13_https_get(const char *host, int port, const char *path,
                     uint8_t **response, size_t *response_len);

#endif /* TLS13_H */
