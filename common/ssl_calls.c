/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2014
 * Copyright (C) Idan Freiberg 2013-2014
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
 *
 * ssl calls
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <stdlib.h> /* needed for openssl headers */
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rc4.h>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/bn.h>
#include <openssl/rsa.h>
#include <openssl/dh.h>
#include <openssl/crypto.h>

#include "os_calls.h"
#include "arch.h"
#include "ssl_calls.h"
#include "trans.h"
#include "log.h"

#define SSL_WANT_READ_WRITE_TIMEOUT 100

#if OPENSSL_VERSION_NUMBER < 0x10100000L
static inline HMAC_CTX *
HMAC_CTX_new(void)
{
    HMAC_CTX *hmac_ctx = g_new(HMAC_CTX, 1);
    HMAC_CTX_init(hmac_ctx);
    return hmac_ctx;
}

static inline void
HMAC_CTX_free(HMAC_CTX *hmac_ctx)
{
    HMAC_CTX_cleanup(hmac_ctx);
    g_free(hmac_ctx);
}

static inline void
RSA_get0_key(const RSA *key, const BIGNUM **n, const BIGNUM **e,
             const BIGNUM **d)
{
     *n = key->n;
     *d = key->d;
}

static inline int
DH_set0_pqg(DH *dh, BIGNUM *p, BIGNUM *q, BIGNUM *g)
{
    /* If the fields p and g in d are NULL, the corresponding input
     * parameters MUST be non-NULL.  q may remain NULL.
     */
    if ((dh->p == NULL && p == NULL)
        || (dh->g == NULL && g == NULL))
    {
        return 0;
    }

    if (p != NULL)
    {
        BN_free(dh->p);
        dh->p = p;
    }
    if (q != NULL)
    {
        BN_free(dh->q);
        dh->q = q;
    }
    if (g != NULL)
    {
        BN_free(dh->g);
        dh->g = g;
    }

    if (q != NULL)
    {
        dh->length = BN_num_bits(q);
    }

    return 1;
}
#endif /* OPENSSL_VERSION_NUMBER >= 0x10100000L */


/*****************************************************************************/
int
ssl_init(void)
{
    SSL_load_error_strings();
    SSL_library_init();
    return 0;
}

/*****************************************************************************/
int
ssl_finish(void)
{
    return 0;
}

/* rc4 stuff */

/*****************************************************************************/
void *
ssl_rc4_info_create(void)
{
    return g_malloc(sizeof(RC4_KEY), 1);
}

/*****************************************************************************/
void
ssl_rc4_info_delete(void *rc4_info)
{
    g_free(rc4_info);
}

/*****************************************************************************/
void
ssl_rc4_set_key(void *rc4_info, char *key, int len)
{
    RC4_set_key((RC4_KEY *)rc4_info, len, (tui8 *)key);
}

/*****************************************************************************/
void
ssl_rc4_crypt(void *rc4_info, char *data, int len)
{
    RC4((RC4_KEY *)rc4_info, len, (tui8 *)data, (tui8 *)data);
}

/* sha1 stuff */

/*****************************************************************************/
void *
ssl_sha1_info_create(void)
{
    return g_malloc(sizeof(SHA_CTX), 1);
}

/*****************************************************************************/
void
ssl_sha1_info_delete(void *sha1_info)
{
    g_free(sha1_info);
}

/*****************************************************************************/
void
ssl_sha1_clear(void *sha1_info)
{
    SHA1_Init((SHA_CTX *)sha1_info);
}

/*****************************************************************************/
void
ssl_sha1_transform(void *sha1_info, const char *data, int len)
{
    SHA1_Update((SHA_CTX *)sha1_info, data, len);
}

/*****************************************************************************/
void
ssl_sha1_complete(void *sha1_info, char *data)
{
    SHA1_Final((tui8 *)data, (SHA_CTX *)sha1_info);
}

/* md5 stuff */

/*****************************************************************************/
void *
ssl_md5_info_create(void)
{
    return g_malloc(sizeof(MD5_CTX), 1);
}

/*****************************************************************************/
void
ssl_md5_info_delete(void *md5_info)
{
    g_free(md5_info);
}

/*****************************************************************************/
void
ssl_md5_clear(void *md5_info)
{
    MD5_Init((MD5_CTX *)md5_info);
}

/*****************************************************************************/
void
ssl_md5_transform(void *md5_info, char *data, int len)
{
    MD5_Update((MD5_CTX *)md5_info, data, len);
}

/*****************************************************************************/
void
ssl_md5_complete(void *md5_info, char *data)
{
    MD5_Final((tui8 *)data, (MD5_CTX *)md5_info);
}

/* FIPS stuff */

/*****************************************************************************/
void *
ssl_des3_encrypt_info_create(const char *key, const char* ivec)
{
    EVP_CIPHER_CTX *des3_ctx;
    const tui8 *lkey;
    const tui8 *livec;

    des3_ctx = EVP_CIPHER_CTX_new();
    lkey = (const tui8 *) key;
    livec = (const tui8 *) ivec;
    EVP_EncryptInit_ex(des3_ctx, EVP_des_ede3_cbc(), NULL, lkey, livec);
    EVP_CIPHER_CTX_set_padding(des3_ctx, 0);
    return des3_ctx;
}

/*****************************************************************************/
void *
ssl_des3_decrypt_info_create(const char *key, const char* ivec)
{
    EVP_CIPHER_CTX *des3_ctx;
    const tui8 *lkey;
    const tui8 *livec;

    des3_ctx = EVP_CIPHER_CTX_new();
    lkey = (const tui8 *) key;
    livec = (const tui8 *) ivec;
    EVP_DecryptInit_ex(des3_ctx, EVP_des_ede3_cbc(), NULL, lkey, livec);
    EVP_CIPHER_CTX_set_padding(des3_ctx, 0);
    return des3_ctx;
}

/*****************************************************************************/
void
ssl_des3_info_delete(void *des3)
{
    EVP_CIPHER_CTX *des3_ctx;

    des3_ctx = (EVP_CIPHER_CTX *) des3;
    if (des3_ctx != 0)
    {
        EVP_CIPHER_CTX_free(des3_ctx);
    }
}

/*****************************************************************************/
int
ssl_des3_encrypt(void *des3, int length, const char *in_data, char *out_data)
{
    EVP_CIPHER_CTX *des3_ctx;
    int len;
    const tui8 *lin_data;
    tui8 *lout_data;

    des3_ctx = (EVP_CIPHER_CTX *) des3;
    lin_data = (const tui8 *) in_data;
    lout_data = (tui8 *) out_data;
    len = 0;
    EVP_EncryptUpdate(des3_ctx, lout_data, &len, lin_data, length);
    return 0;
}

/*****************************************************************************/
int
ssl_des3_decrypt(void *des3, int length, const char *in_data, char *out_data)
{
    EVP_CIPHER_CTX *des3_ctx;
    int len;
    const tui8 *lin_data;
    tui8 *lout_data;

    des3_ctx = (EVP_CIPHER_CTX *) des3;
    lin_data = (const tui8 *) in_data;
    lout_data = (tui8 *) out_data;
    len = 0;
    EVP_DecryptUpdate(des3_ctx, lout_data, &len, lin_data, length);
    return 0;
}

/*****************************************************************************/
void *
ssl_hmac_info_create(void)
{
    HMAC_CTX *hmac_ctx;

    hmac_ctx = HMAC_CTX_new();
    return hmac_ctx;
}

/*****************************************************************************/
void
ssl_hmac_info_delete(void *hmac)
{
    HMAC_CTX *hmac_ctx;

    hmac_ctx = (HMAC_CTX *) hmac;
    if (hmac_ctx != 0)
    {
        HMAC_CTX_free(hmac_ctx);
    }
}

/*****************************************************************************/
void
ssl_hmac_sha1_init(void *hmac, const char *data, int len)
{
    HMAC_CTX *hmac_ctx;

    hmac_ctx = (HMAC_CTX *) hmac;
    HMAC_Init_ex(hmac_ctx, data, len, EVP_sha1(), NULL);
}

/*****************************************************************************/
void
ssl_hmac_transform(void *hmac, const char *data, int len)
{
    HMAC_CTX *hmac_ctx;
    const tui8 *ldata;

    hmac_ctx = (HMAC_CTX *) hmac;
    ldata = (const tui8*) data;
    HMAC_Update(hmac_ctx, ldata, len);
}

/*****************************************************************************/
void
ssl_hmac_complete(void *hmac, char *data, int len)
{
    HMAC_CTX *hmac_ctx;
    tui8* ldata;
    tui32 llen;

    hmac_ctx = (HMAC_CTX *) hmac;
    ldata = (tui8 *) data;
    llen = len;
    HMAC_Final(hmac_ctx, ldata, &llen);
}

/*****************************************************************************/
static void
ssl_reverse_it(char *p, int len)
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
int
ssl_mod_exp(char *out, int out_len, const char *in, int in_len,
            const char *mod, int mod_len, const char *exp, int exp_len)
{
    BN_CTX *ctx;
    BIGNUM *lmod;
    BIGNUM *lexp;
    BIGNUM *lin;
    BIGNUM *lout;
    int rv;
    char *l_out;
    char *l_in;
    char *l_mod;
    char *l_exp;

    l_out = (char *)g_malloc(out_len, 1);
    l_in = (char *)g_malloc(in_len, 1);
    l_mod = (char *)g_malloc(mod_len, 1);
    l_exp = (char *)g_malloc(exp_len, 1);
    g_memcpy(l_in, in, in_len);
    g_memcpy(l_mod, mod, mod_len);
    g_memcpy(l_exp, exp, exp_len);
    ssl_reverse_it(l_in, in_len);
    ssl_reverse_it(l_mod, mod_len);
    ssl_reverse_it(l_exp, exp_len);
    ctx = BN_CTX_new();
    lmod = BN_new();
    lexp = BN_new();
    lin = BN_new();
    lout = BN_new();
    BN_bin2bn((tui8 *)l_mod, mod_len, lmod);
    BN_bin2bn((tui8 *)l_exp, exp_len, lexp);
    BN_bin2bn((tui8 *)l_in, in_len, lin);
    BN_mod_exp(lout, lin, lexp, lmod, ctx);
    rv = BN_bn2bin(lout, (tui8 *)l_out);

    if (rv <= out_len)
    {
        ssl_reverse_it(l_out, rv);
        g_memcpy(out, l_out, out_len);
    }
    else
    {
        rv = 0;
    }

    BN_free(lin);
    BN_free(lout);
    BN_free(lexp);
    BN_free(lmod);
    BN_CTX_free(ctx);
    g_free(l_out);
    g_free(l_in);
    g_free(l_mod);
    g_free(l_exp);
    return rv;
}

/*****************************************************************************/
/* returns error
   generates a new rsa key
   exp is passed in and mod and pri are passed out */
int
ssl_gen_key_xrdp1(int key_size_in_bits, const char *exp, int exp_len,
                  char *mod, int mod_len, char *pri, int pri_len)
{
    BIGNUM *my_e;
    RSA *my_key;
    char *lexp;
    char *lmod;
    char *lpri;
    int error;
    int len;
    int diff;

    if ((exp_len != 4) || ((mod_len != 64) && (mod_len != 256)) ||
                          ((pri_len != 64) && (pri_len != 256)))
    {
        return 1;
    }

    diff = 0;
    lexp = (char *)g_malloc(exp_len, 1);
    lmod = (char *)g_malloc(mod_len, 1);
    lpri = (char *)g_malloc(pri_len, 1);
    g_memcpy(lexp, exp, exp_len);
    ssl_reverse_it(lexp, exp_len);
    my_e = BN_new();
    BN_bin2bn((tui8 *)lexp, exp_len, my_e);
    my_key = RSA_new();
    error = RSA_generate_key_ex(my_key, key_size_in_bits, my_e, 0) == 0;

    const BIGNUM *n;
    const BIGNUM *d;
    RSA_get0_key(my_key, &n, NULL, &d);

    if (error == 0)
    {
        len = BN_num_bytes(n);
        error = (len < 1) || (len > mod_len);
        diff = mod_len - len;
    }

    if (error == 0)
    {
        BN_bn2bin(n, (tui8 *)(lmod + diff));
        ssl_reverse_it(lmod, mod_len);
    }

    if (error == 0)
    {
        len = BN_num_bytes(d);
        error = (len < 1) || (len > pri_len);
        diff = pri_len - len;
    }

    if (error == 0)
    {
        BN_bn2bin(d, (tui8 *)(lpri + diff));
        ssl_reverse_it(lpri, pri_len);
    }

    if (error == 0)
    {
        g_memcpy(mod, lmod, mod_len);
        g_memcpy(pri, lpri, pri_len);
    }

    BN_free(my_e);
    RSA_free(my_key);
    g_free(lexp);
    g_free(lmod);
    g_free(lpri);
    return error;
}

/*****************************************************************************/
/** static DH parameter, can be used if no custom parameter is specified
see also
 * https://wiki.openssl.org/index.php/Diffie-Hellman_parameters
 * https://wiki.openssl.org/index.php/Manual:SSL_CTX_set_tmp_dh_callback(3)
*/
static DH *ssl_get_dh2236()
{
    static unsigned char dh2236_p[] = {
        0x0E, 0xF8, 0x69, 0x0B, 0x35, 0x2F, 0x62, 0x59, 0xF7, 0xAF, 0x4E, 0x19,
        0xB5, 0x9B, 0xD2, 0xEB, 0x33, 0x78, 0x1D, 0x43, 0x1D, 0xB6, 0xE4, 0xA3,
        0x63, 0x47, 0x6A, 0xD4, 0xA8, 0x28, 0x11, 0x8C, 0x3F, 0xC8, 0xF1, 0x32,
        0x2B, 0x5D, 0x9F, 0xF8, 0xA6, 0xCA, 0x21, 0x71, 0xDE, 0x30, 0xD7, 0xB5,
        0xD6, 0xA4, 0xC2, 0xEE, 0xC0, 0x49, 0x30, 0xE7, 0x8C, 0x9B, 0x1A, 0x5A,
        0x08, 0x2A, 0x11, 0x84, 0xE2, 0xC8, 0x36, 0x6C, 0xDC, 0x06, 0x79, 0x59,
        0x51, 0xA4, 0xA0, 0x8F, 0xE1, 0x20, 0x94, 0x80, 0xAC, 0x6D, 0xFD, 0x3B,
        0xA6, 0xA6, 0x70, 0x51, 0x93, 0x59, 0x28, 0x51, 0x54, 0xA3, 0xC5, 0x15,
        0x44, 0x2C, 0x12, 0xE7, 0x95, 0x62, 0x0E, 0x65, 0x2F, 0x8C, 0x0D, 0xF8,
        0x63, 0x52, 0x00, 0x2A, 0xA5, 0xD7, 0x59, 0xEF, 0x13, 0x18, 0x33, 0x25,
        0xBC, 0xAD, 0xC8, 0x0A, 0x72, 0x8D, 0x26, 0x63, 0xD5, 0xB3, 0xBC, 0x43,
        0x35, 0x0B, 0x5D, 0xC7, 0xCA, 0x45, 0x17, 0x06, 0x24, 0x71, 0xCA, 0x20,
        0x73, 0xE8, 0x18, 0xD3, 0x8E, 0xE9, 0xE9, 0x8F, 0x67, 0xC0, 0x2C, 0x14,
        0x7E, 0x41, 0x18, 0x6C, 0x74, 0x72, 0x56, 0x34, 0xC0, 0xDB, 0xDD, 0x85,
        0x8B, 0xE0, 0x99, 0xE8, 0x5E, 0xC8, 0xF7, 0xD1, 0x0C, 0xF8, 0x83, 0x34,
        0x37, 0x9E, 0x01, 0xDF, 0x1C, 0xD9, 0xE9, 0x95, 0xC1, 0x4C, 0x64, 0x37,
        0x9B, 0xF5, 0x8F, 0x99, 0x97, 0x55, 0x68, 0x2E, 0x23, 0xB0, 0x35, 0xF3,
        0xA5, 0x97, 0x92, 0xA0, 0x6D, 0xB4, 0xF8, 0xD8, 0x47, 0xCE, 0x3F, 0x0B,
        0x36, 0x0E, 0xEB, 0x13, 0x15, 0xFD, 0x4F, 0x98, 0x4F, 0x14, 0x26, 0xE2,
        0xAC, 0xD9, 0x42, 0xC6, 0x43, 0x8A, 0x95, 0x6B, 0x2B, 0x44, 0x38, 0x7F,
        0x60, 0x97, 0x77, 0xD8, 0x7C, 0x6F, 0x5D, 0x62, 0x7C, 0xE1, 0xC8, 0x83,
        0x12, 0x8B, 0x5E, 0x5E, 0xC7, 0x5E, 0xD5, 0x60, 0xF3, 0x2F, 0xFC, 0xFE,
        0x70, 0xAC, 0x58, 0x3A, 0x3C, 0x18, 0x15, 0x54, 0x84, 0xA8, 0xAA, 0x41,
        0x26, 0x7B, 0xE0, 0xA3,
    };
    static unsigned char dh2236_g[] = {
        0x02,
    };

    DH *dh = DH_new();
    if (dh == NULL)
    {
        return NULL;
    }

    BIGNUM *p = BN_bin2bn(dh2236_p, sizeof(dh2236_p), NULL);
    BIGNUM *g = BN_bin2bn(dh2236_g, sizeof(dh2236_g), NULL);
    if (p == NULL || g == NULL)
    {
        BN_free(p);
        BN_free(g);
        DH_free(dh);
        return NULL;
    }

    // p, g are freed later by DH_free()
    if (0 == DH_set0_pqg(dh, p, NULL, g))
    {
        BN_free(p);
        BN_free(g);
        DH_free(dh);
        return NULL;
    }

    return dh;
}

/*****************************************************************************/
struct ssl_tls *
ssl_tls_create(struct trans *trans, const char *key, const char *cert)
{
    struct ssl_tls *self;
    int pid;
    char buf[1024];

    self = (struct ssl_tls *) g_malloc(sizeof(struct ssl_tls), 1);
    if (self != NULL)
    {
        self->trans = trans;
        self->cert = (char *) cert;
        self->key = (char *) key;
        pid = g_getpid();
        g_snprintf(buf, 1024, "xrdp_%8.8x_tls_rwo", pid);
        self->rwo = g_create_wait_obj(buf);
    }

    return self;
}

/*****************************************************************************/
int
ssl_tls_print_error(const char *func, SSL *connection, int value)
{
    switch (SSL_get_error(connection, value))
    {
        case SSL_ERROR_ZERO_RETURN:
            g_writeln("ssl_tls_print_error: %s: Server closed TLS connection",
                      func);
            return 1;

        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:
            return 0;

        case SSL_ERROR_SYSCALL:
            g_writeln("ssl_tls_print_error: %s: I/O error", func);
            return 1;

        case SSL_ERROR_SSL:
            g_writeln("ssl_tls_print_error: %s: Failure in SSL library (protocol error?)",
                      func);
            return 1;

        default:
            g_writeln("ssl_tls_print_error: %s: Unknown error", func);
            return 1;
    }
}

/*****************************************************************************/
int
ssl_tls_accept(struct ssl_tls *self, long ssl_protocols,
               const char *tls_ciphers)
{
    int connection_status;
    long options = 0;

    /**
     * SSL_OP_NO_SSLv2
     * SSLv3 is used by, eg. Microsoft RDC for Mac OS X.
     */
    options |= SSL_OP_NO_SSLv2;

    /**
     * Disable SSL protocols not listed in ssl_protocols.
     */
    options |= ssl_protocols;


#if defined(SSL_OP_NO_COMPRESSION)
    /**
     * SSL_OP_NO_COMPRESSION:
     *
     * The Microsoft RDP server does not advertise support
     * for TLS compression, but alternative servers may support it.
     * This was observed between early versions of the FreeRDP server
     * and the FreeRDP client, and caused major performance issues,
     * which is why we're disabling it.
     */
    options |= SSL_OP_NO_COMPRESSION;
#endif

    /**
     * SSL_OP_TLS_BLOCK_PADDING_BUG:
     *
     * The Microsoft RDP server does *not* support TLS padding.
     * It absolutely needs to be disabled otherwise it won't work.
     */
    options |= SSL_OP_TLS_BLOCK_PADDING_BUG;

    /**
     * SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS:
     *
     * Just like TLS padding, the Microsoft RDP server does not
     * support empty fragments. This needs to be disabled.
     */
    options |= SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS;

    self->ctx = SSL_CTX_new(SSLv23_server_method());
    if (self->ctx == NULL)
    {
        log_message(LOG_LEVEL_ERROR, "ssl_tls_accept: SSL_CTX_new failed");
        return 1;
    }

    /* set context options */
    SSL_CTX_set_mode(self->ctx,
                     SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER |
                     SSL_MODE_ENABLE_PARTIAL_WRITE);
    SSL_CTX_set_options(self->ctx, options);

    /* set DH parameters */
    DH *dh = ssl_get_dh2236();
    if (dh == NULL)
    {
        log_message(LOG_LEVEL_ERROR, "ssl_tls_accept: ssl_get_dh2236 failed");
        return 1;
    }

    if (SSL_CTX_set_tmp_dh(self->ctx, dh) != 1)
    {
        log_message(LOG_LEVEL_ERROR,
                    "ssl_tls_accept: SSL_CTX_set_tmp_dh failed");
        return 1;
    }
    DH_free(dh); // ok to free, copied into ctx by SSL_CTX_set_tmp_dh()

#if defined(SSL_CTX_set_ecdh_auto)
    SSL_CTX_set_ecdh_auto(self->ctx, 1);
#endif

    if (g_strlen(tls_ciphers) > 1)
    {
        log_message(LOG_LEVEL_TRACE, "ssl_tls_accept: tls_ciphers=%s",
            tls_ciphers);
        if (SSL_CTX_set_cipher_list(self->ctx, tls_ciphers) == 0)
        {
            g_writeln("ssl_tls_accept: invalid cipher options");
            return 1;
        }
    }

    SSL_CTX_set_read_ahead(self->ctx, 0);

    if (SSL_CTX_use_RSAPrivateKey_file(self->ctx, self->key, SSL_FILETYPE_PEM)
            <= 0)
    {
        g_writeln("ssl_tls_accept: SSL_CTX_use_RSAPrivateKey_file failed");
        return 1;
    }

    if (SSL_CTX_use_certificate_chain_file(self->ctx, self->cert) <= 0)
    {
        g_writeln("ssl_tls_accept: SSL_CTX_use_certificate_chain_file failed");
        return 1;
    }

    self->ssl = SSL_new(self->ctx);

    if (self->ssl == NULL)
    {
        g_writeln("ssl_tls_accept: SSL_new failed");
        return 1;
    }

    if (SSL_set_fd(self->ssl, self->trans->sck) < 1)
    {
        g_writeln("ssl_tls_accept: SSL_set_fd failed");
        return 1;
    }

    while(1) {
        connection_status = SSL_accept(self->ssl);

        if (connection_status <= 0)
        {
            if (ssl_tls_print_error("SSL_accept", self->ssl, connection_status))
            {
                return 1;
            }
            /**
             * retry when SSL_get_error returns:
             *     SSL_ERROR_WANT_READ
             *     SSL_ERROR_WANT_WRITE
             */
            switch (SSL_get_error(self->ssl, connection_status))
            {
                case SSL_ERROR_WANT_READ:
                    g_sck_can_recv(self->trans->sck, SSL_WANT_READ_WRITE_TIMEOUT);
                    break;
                case SSL_ERROR_WANT_WRITE:
                    g_sck_can_send(self->trans->sck, SSL_WANT_READ_WRITE_TIMEOUT);
                    break;
            }
        }
        else
        {
            break;
        }
    }

    g_writeln("ssl_tls_accept: TLS connection accepted");

    return 0;
}

/*****************************************************************************/
/* returns error, */
int
ssl_tls_disconnect(struct ssl_tls *self)
{
    int status;

    if (self == NULL)
    {
        return 0;
    }
    if (self->ssl == NULL)
    {
        return 0;
    }
    status = SSL_shutdown(self->ssl);
    if (status != 1)
    {
        status = SSL_shutdown(self->ssl);
        if (status <= 0)
        {
            if (ssl_tls_print_error("SSL_shutdown", self->ssl, status))
            {
                return 1;
            }
            /**
             * retry when SSL_get_error returns:
             *     SSL_ERROR_WANT_READ
             *     SSL_ERROR_WANT_WRITE
             */
        }
    }
    return 0;
}

/*****************************************************************************/
void
ssl_tls_delete(struct ssl_tls *self)
{
    if (self != NULL)
    {
        if (self->ssl)
            SSL_free(self->ssl);

        if (self->ctx)
            SSL_CTX_free(self->ctx);

        g_delete_wait_obj(self->rwo);

        g_free(self);
    }
}

/*****************************************************************************/
int
ssl_tls_read(struct ssl_tls *tls, char *data, int length)
{
    int status;
    int break_flag;

    while(1) {
        status = SSL_read(tls->ssl, data, length);

        switch (SSL_get_error(tls->ssl, status))
        {
            case SSL_ERROR_NONE:
                break_flag = 1;
                break;

            /**
             * retry when SSL_get_error returns:
             *     SSL_ERROR_WANT_READ
             *     SSL_ERROR_WANT_WRITE
             */
            case SSL_ERROR_WANT_READ:
                g_sck_can_recv(tls->trans->sck, SSL_WANT_READ_WRITE_TIMEOUT);
                continue;
            case SSL_ERROR_WANT_WRITE:
                g_sck_can_send(tls->trans->sck, SSL_WANT_READ_WRITE_TIMEOUT);
                continue;

            /* socket closed */
            case SSL_ERROR_ZERO_RETURN:
                return 0;

            default:
                ssl_tls_print_error("SSL_read", tls->ssl, status);
                status = -1;
                break_flag = 1;
                break;
        }

        if (break_flag)
        {
            break;
        }
    }

    if (SSL_pending(tls->ssl) > 0)
    {
        g_set_wait_obj(tls->rwo);
    }

    return status;
}

/*****************************************************************************/
int
ssl_tls_write(struct ssl_tls *tls, const char *data, int length)
{
    int status;
    int break_flag;

    while(1) {
        status = SSL_write(tls->ssl, data, length);

        switch (SSL_get_error(tls->ssl, status))
        {
            case SSL_ERROR_NONE:
                break_flag = 1;
                break;

            /**
             * retry when SSL_get_error returns:
             *     SSL_ERROR_WANT_READ
             *     SSL_ERROR_WANT_WRITE
             */
            case SSL_ERROR_WANT_READ:
                g_sck_can_recv(tls->trans->sck, SSL_WANT_READ_WRITE_TIMEOUT);
                continue;
            case SSL_ERROR_WANT_WRITE:
                g_sck_can_send(tls->trans->sck, SSL_WANT_READ_WRITE_TIMEOUT);
                continue;

            /* socket closed */
            case SSL_ERROR_ZERO_RETURN:
                return 0;

            default:
                ssl_tls_print_error("SSL_write", tls->ssl, status);
                status = -1;
                break_flag = 1;
                break;
        }

        if (break_flag)
        {
            break;
        }
    }

    return status;
}

/*****************************************************************************/
/* returns boolean */
int
ssl_tls_can_recv(struct ssl_tls *tls, int sck, int millis)
{
    if (SSL_pending(tls->ssl) > 0)
    {
        return 1;
    }
    g_reset_wait_obj(tls->rwo);
    return g_sck_can_recv(sck, millis);
}

/*****************************************************************************/
const char *
ssl_get_version(const struct ssl_st *ssl)
{
    return SSL_get_version(ssl);
}

/*****************************************************************************/
const char *
ssl_get_cipher_name(const struct ssl_st *ssl)
{
    return SSL_get_cipher_name(ssl);
}

/*****************************************************************************/
int
ssl_get_protocols_from_string(const char *str, long *ssl_protocols)
{
    long protocols;
    long bad_protocols;
    int rv;

    if ((str == NULL) || (ssl_protocols == NULL))
    {
        return 1;
    }
    rv = 0;
    protocols = 0;
#if defined(SSL_OP_NO_SSLv3)
    protocols |= SSL_OP_NO_SSLv3;
#endif
#if defined(SSL_OP_NO_TLSv1)
    protocols |= SSL_OP_NO_TLSv1;
#endif
#if defined(SSL_OP_NO_TLSv1_1)
    protocols |= SSL_OP_NO_TLSv1_1;
#endif
#if defined(SSL_OP_NO_TLSv1_2)
    protocols |= SSL_OP_NO_TLSv1_2;
#endif
    bad_protocols = protocols;
    if (g_pos(str, ",TLSv1.2,") >= 0)
    {
#if defined(SSL_OP_NO_TLSv1_2)
        log_message(LOG_LEVEL_DEBUG, "TLSv1.2 enabled");
        protocols &= ~SSL_OP_NO_TLSv1_2;
#else
        log_message(LOG_LEVEL_WARNING,
                    "TLSv1.2 enabled by config, "
                    "but not supported by system OpenSSL");
        rv |= (1 << 1);
#endif
    }
    if (g_pos(str, ",TLSv1.1,") >= 0)
    {
#if defined(SSL_OP_NO_TLSv1_1)
        log_message(LOG_LEVEL_DEBUG, "TLSv1.1 enabled");
        protocols &= ~SSL_OP_NO_TLSv1_1;
#else
        log_message(LOG_LEVEL_WARNING,
                    "TLSv1.1 enabled by config, "
                    "but not supported by system OpenSSL");
        rv |= (1 << 2);
#endif
    }
    if (g_pos(str, ",TLSv1,") >= 0)
    {
#if defined(SSL_OP_NO_TLSv1)
        log_message(LOG_LEVEL_DEBUG, "TLSv1 enabled");
        protocols &= ~SSL_OP_NO_TLSv1;
#else
        log_message(LOG_LEVEL_WARNING,
                    "TLSv1 enabled by config, "
                    "but not supported by system OpenSSL");
        rv |= (1 << 3);
#endif
    }
    if (g_pos(str, ",SSLv3,") >= 0)
    {
#if defined(SSL_OP_NO_SSLv3)
        log_message(LOG_LEVEL_DEBUG, "SSLv3 enabled");
        protocols &= ~SSL_OP_NO_SSLv3;
#else
        log_message(LOG_LEVEL_WARNING,
                    "SSLv3 enabled by config, "
                    "but not supported by system OpenSSL");
        rv |= (1 << 4);
#endif
    }
    if (protocols == bad_protocols)
    {
        log_message(LOG_LEVEL_WARNING, "No SSL/TLS protocols enabled. "
                    "At least one protocol should be enabled to accept "
                    "TLS connections.");
        rv |= (1 << 5);
    }
    *ssl_protocols = protocols;
    return rv;
}

/*****************************************************************************/
const char
*get_openssl_version()
{
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    return SSLeay_version(SSLEAY_VERSION);
#else
    return OpenSSL_version(OPENSSL_VERSION);
#endif

}

