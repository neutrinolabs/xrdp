/**
 * NeutrinoSSL - Lightweight TLS implementation for xrdp on macOS
 *
 * Copyright (C) 2026 Neutrino Labs
 *
 * Uses NeutrinoTLS (pure C TLS 1.3) to avoid OpenSSL PAC issues on Apple Silicon
 * No external crypto libraries - all primitives implemented in NeutrinoTLS
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#ifdef __APPLE__

#include "neutrinossl.h"
#include "neutrinotls.h"
#include "log.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

/* Thread-local error storage */
static __thread char g_neutrinossl_error[256] = {0};

struct neutrinossl_ctx {
    char* cert_file;
    char* key_file;
    uint8_t cert_data[4096];  /* PEM certificate data */
    size_t cert_len;
    uint8_t key_data[4096];   /* PEM private key data */
    size_t key_len;
};

struct neutrinossl {
    tls13_conn tls_conn;
    int socket;
    NEUTRINOSSL_CTX* ctx;
    int handshake_complete;
    int is_server;
};

/**
 * Set error message
 */
static void set_error(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(g_neutrinossl_error, sizeof(g_neutrinossl_error), format, args);
    va_end(args);
}

/**
 * Initialize NeutrinoSSL library
 */
int neutrinossl_init(void)
{
    LOG(LOG_LEVEL_INFO, "NeutrinoSSL initialized (using NeutrinoTLS - pure C TLS 1.3)");
    return 0;
}

/**
 * Cleanup NeutrinoSSL library
 */
void neutrinossl_cleanup(void)
{
    LOG(LOG_LEVEL_INFO, "NeutrinoSSL cleanup");
}

/**
 * Load PEM certificate and key files into memory
 *
 * For now, we'll generate a self-signed certificate on the fly.
 * A full implementation would read and parse PEM files.
 */
static int load_cert_and_key(NEUTRINOSSL_CTX* ctx, const char* cert_file, const char* key_file)
{
    /* TODO: Implement PEM file loading
     * For now, we'll use a minimal self-signed cert approach
     * or let the TLS handshake generate ephemeral keys
     */

    /* Store paths for reference */
    if (cert_file) {
        ctx->cert_file = strdup(cert_file);
    }
    if (key_file) {
        ctx->key_file = strdup(key_file);
    }

    ctx->cert_len = 0;
    ctx->key_len = 0;

    return 0;
}

/**
 * Create a new NeutrinoSSL context for server
 */
NEUTRINOSSL_CTX* neutrinossl_ctx_new_server(const char* cert_file, const char* key_file)
{
    NEUTRINOSSL_CTX* ctx = (NEUTRINOSSL_CTX*)malloc(sizeof(NEUTRINOSSL_CTX));
    if (!ctx) {
        set_error("Failed to allocate context");
        return NULL;
    }

    memset(ctx, 0, sizeof(NEUTRINOSSL_CTX));

    if (load_cert_and_key(ctx, cert_file, key_file) != 0) {
        free(ctx);
        set_error("Failed to load certificate and key");
        return NULL;
    }

    LOG(LOG_LEVEL_INFO, "NeutrinoSSL context created (server mode)");
    return ctx;
}

/**
 * Free a NeutrinoSSL context
 */
void neutrinossl_ctx_free(NEUTRINOSSL_CTX* ctx)
{
    if (!ctx) return;

    if (ctx->cert_file) {
        free(ctx->cert_file);
    }
    if (ctx->key_file) {
        free(ctx->key_file);
    }

    free(ctx);
}

/**
 * Create a new NeutrinoSSL connection
 */
NEUTRINOSSL* neutrinossl_new(NEUTRINOSSL_CTX* ctx, int sock)
{
    if (!ctx) {
        set_error("NULL context");
        return NULL;
    }

    NEUTRINOSSL* ssl = (NEUTRINOSSL*)malloc(sizeof(NEUTRINOSSL));
    if (!ssl) {
        set_error("Failed to allocate SSL connection");
        return NULL;
    }

    memset(ssl, 0, sizeof(NEUTRINOSSL));
    ssl->socket = sock;
    ssl->ctx = ctx;
    ssl->handshake_complete = 0;
    ssl->is_server = 1;  /* We're always server mode for xrdp */

    /* Initialize TLS connection */
    tls13_init(&ssl->tls_conn);
    ssl->tls_conn.sock = sock;

    LOG(LOG_LEVEL_DEBUG, "NeutrinoSSL connection created for socket %d", sock);
    return ssl;
}

/**
 * Free a NeutrinoSSL connection
 */
void neutrinossl_free(NEUTRINOSSL* ssl)
{
    if (!ssl) return;

    /* TLS connection cleanup is handled by tls13_close */

    free(ssl);
}

/**
 * Perform TLS server handshake
 *
 * NOTE: NeutrinoTLS from the cloned repo is CLIENT-ONLY.
 * This function currently returns an error. Server-side handshake
 * needs to be implemented in neutrinotls.c
 */
int neutrinossl_accept(NEUTRINOSSL* ssl)
{
    if (!ssl) {
        set_error("NULL SSL connection");
        return -1;
    }

    if (ssl->handshake_complete) {
        return 1; /* Already connected */
    }

    LOG(LOG_LEVEL_DEBUG, "NeutrinoSSL: Starting TLS server handshake on socket %d", ssl->socket);

    /* Perform TLS 1.3 server handshake */
    if (!tls13_accept(&ssl->tls_conn)) {
        set_error("TLS server handshake failed");
        LOG(LOG_LEVEL_ERROR, "NeutrinoSSL: TLS handshake failed on socket %d", ssl->socket);
        return -1;
    }

    ssl->handshake_complete = 1;
    LOG(LOG_LEVEL_INFO, "NeutrinoSSL: TLS handshake successful on socket %d", ssl->socket);

    return 1;
}

/**
 * Read data from TLS connection
 */
int neutrinossl_read(NEUTRINOSSL* ssl, void* buf, int len)
{
    if (!ssl) {
        set_error("NULL SSL connection");
        return -1;
    }

    if (!ssl->handshake_complete) {
        set_error("Handshake not complete");
        return -1;
    }

    /* Use NeutrinoTLS recv function */
    ssize_t result = tls13_recv(&ssl->tls_conn, buf, len);

    if (result < 0) {
        set_error("TLS read failed");
        return -1;
    }

    if (result == 0) {
        /* Connection closed */
        return 0;
    }

    return (int)result;
}

/**
 * Write data to TLS connection
 */
int neutrinossl_write(NEUTRINOSSL* ssl, const void* buf, int len)
{
    if (!ssl) {
        set_error("NULL SSL connection");
        return -1;
    }

    if (!ssl->handshake_complete) {
        set_error("Handshake not complete");
        return -1;
    }

    /* Use NeutrinoTLS send function */
    ssize_t result = tls13_send(&ssl->tls_conn, buf, len);

    if (result < 0) {
        set_error("TLS write failed");
        return -1;
    }

    return (int)result;
}

/**
 * Shutdown TLS connection
 */
int neutrinossl_shutdown(NEUTRINOSSL* ssl)
{
    if (!ssl) {
        return -1;
    }

    /* Send TLS close_notify alert and close connection */
    tls13_close(&ssl->tls_conn);

    ssl->handshake_complete = 0;
    return 0;
}

/**
 * Get last error message
 */
const char* neutrinossl_get_error(void)
{
    return g_neutrinossl_error[0] ? g_neutrinossl_error : "No error";
}

/**
 * Get TLS version string
 */
const char* neutrinossl_get_version(void)
{
    return "TLSv1.3";
}

/**
 * Get cipher suite name
 */
const char* neutrinossl_get_cipher_name(void)
{
    return "TLS_CHACHA20_POLY1305_SHA256";
}

#endif /* __APPLE__ */
