/**
 * NeutrinoSSL - Lightweight TLS implementation for xrdp on macOS
 *
 * Copyright (C) 2026 Neutrino Labs
 *
 * Uses macOS Secure Transport API to avoid OpenSSL PAC issues on Apple Silicon
 */

#ifndef NEUTRINOSSL_H
#define NEUTRINOSSL_H

#ifdef __APPLE__

#include <stddef.h>

/* Forward declarations */
struct neutrinossl_ctx;
struct neutrinossl;

/* NeutrinoSSL context (equivalent to SSL_CTX) */
typedef struct neutrinossl_ctx NEUTRINOSSL_CTX;

/* NeutrinoSSL connection (equivalent to SSL) */
typedef struct neutrinossl NEUTRINOSSL;

/**
 * Initialize NeutrinoSSL library
 * @return 0 on success, non-zero on failure
 */
int neutrinossl_init(void);

/**
 * Cleanup NeutrinoSSL library
 */
void neutrinossl_cleanup(void);

/**
 * Create a new NeutrinoSSL context for server
 * @param cert_file Path to certificate file (PEM format)
 * @param key_file Path to private key file (PEM format)
 * @return New context or NULL on failure
 */
NEUTRINOSSL_CTX* neutrinossl_ctx_new_server(const char* cert_file, const char* key_file);

/**
 * Free a NeutrinoSSL context
 * @param ctx Context to free
 */
void neutrinossl_ctx_free(NEUTRINOSSL_CTX* ctx);

/**
 * Create a new NeutrinoSSL connection
 * @param ctx NeutrinoSSL context
 * @param sock Socket file descriptor
 * @return New connection or NULL on failure
 */
NEUTRINOSSL* neutrinossl_new(NEUTRINOSSL_CTX* ctx, int sock);

/**
 * Free a NeutrinoSSL connection
 * @param ssl Connection to free
 */
void neutrinossl_free(NEUTRINOSSL* ssl);

/**
 * Perform TLS server handshake
 * @param ssl NeutrinoSSL connection
 * @return 1 on success, 0 or negative on failure
 */
int neutrinossl_accept(NEUTRINOSSL* ssl);

/**
 * Read data from TLS connection
 * @param ssl NeutrinoSSL connection
 * @param buf Buffer to read into
 * @param len Maximum bytes to read
 * @return Number of bytes read, 0 on connection close, negative on error
 */
int neutrinossl_read(NEUTRINOSSL* ssl, void* buf, int len);

/**
 * Write data to TLS connection
 * @param ssl NeutrinoSSL connection
 * @param buf Buffer to write from
 * @param len Number of bytes to write
 * @return Number of bytes written, negative on error
 */
int neutrinossl_write(NEUTRINOSSL* ssl, const void* buf, int len);

/**
 * Shutdown TLS connection
 * @param ssl NeutrinoSSL connection
 * @return 0 on success, negative on error
 */
int neutrinossl_shutdown(NEUTRINOSSL* ssl);

/**
 * Get last error message
 * @return Error string
 */
const char* neutrinossl_get_error(void);

/**
 * Get TLS version string
 * @return TLS version (e.g., "TLSv1.3")
 */
const char* neutrinossl_get_version(void);

/**
 * Get cipher suite name
 * @return Cipher suite name
 */
const char* neutrinossl_get_cipher_name(void);

#endif /* __APPLE__ */

#endif /* NEUTRINOSSL_H */
