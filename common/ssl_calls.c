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

#include <stdlib.h> /* needed for openssl headers */
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rc4.h>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/bn.h>
#include <openssl/rsa.h>

#include "os_calls.h"
#include "arch.h"
#include "ssl_calls.h"
#include "trans.h"

#if defined(OPENSSL_VERSION_NUMBER) && (OPENSSL_VERSION_NUMBER >= 0x0090800f)
#undef OLD_RSA_GEN1
#else
#define OLD_RSA_GEN1
#endif

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
void *APP_CC
ssl_rc4_info_create(void)
{
    return g_malloc(sizeof(RC4_KEY), 1);
}

/*****************************************************************************/
void APP_CC
ssl_rc4_info_delete(void *rc4_info)
{
    g_free(rc4_info);
}

/*****************************************************************************/
void APP_CC
ssl_rc4_set_key(void *rc4_info, char *key, int len)
{
    RC4_set_key((RC4_KEY *)rc4_info, len, (tui8 *)key);
}

/*****************************************************************************/
void APP_CC
ssl_rc4_crypt(void *rc4_info, char *data, int len)
{
    RC4((RC4_KEY *)rc4_info, len, (tui8 *)data, (tui8 *)data);
}

/* sha1 stuff */

/*****************************************************************************/
void *APP_CC
ssl_sha1_info_create(void)
{
    return g_malloc(sizeof(SHA_CTX), 1);
}

/*****************************************************************************/
void APP_CC
ssl_sha1_info_delete(void *sha1_info)
{
    g_free(sha1_info);
}

/*****************************************************************************/
void APP_CC
ssl_sha1_clear(void *sha1_info)
{
    SHA1_Init((SHA_CTX *)sha1_info);
}

/*****************************************************************************/
void APP_CC
ssl_sha1_transform(void *sha1_info, char *data, int len)
{
    SHA1_Update((SHA_CTX *)sha1_info, data, len);
}

/*****************************************************************************/
void APP_CC
ssl_sha1_complete(void *sha1_info, char *data)
{
    SHA1_Final((tui8 *)data, (SHA_CTX *)sha1_info);
}

/* md5 stuff */

/*****************************************************************************/
void *APP_CC
ssl_md5_info_create(void)
{
    return g_malloc(sizeof(MD5_CTX), 1);
}

/*****************************************************************************/
void APP_CC
ssl_md5_info_delete(void *md5_info)
{
    g_free(md5_info);
}

/*****************************************************************************/
void APP_CC
ssl_md5_clear(void *md5_info)
{
    MD5_Init((MD5_CTX *)md5_info);
}

/*****************************************************************************/
void APP_CC
ssl_md5_transform(void *md5_info, char *data, int len)
{
    MD5_Update((MD5_CTX *)md5_info, data, len);
}

/*****************************************************************************/
void APP_CC
ssl_md5_complete(void *md5_info, char *data)
{
    MD5_Final((tui8 *)data, (MD5_CTX *)md5_info);
}

/* FIPS stuff */

/*****************************************************************************/
void *APP_CC
ssl_des3_encrypt_info_create(const char *key, const char* ivec)
{
    EVP_CIPHER_CTX *des3_ctx;
    const tui8 *lkey;
    const tui8 *livec;

    des3_ctx = (EVP_CIPHER_CTX *) g_malloc(sizeof(EVP_CIPHER_CTX), 1);
    EVP_CIPHER_CTX_init(des3_ctx);
    lkey = (const tui8 *) key;
    livec = (const tui8 *) ivec;
    EVP_EncryptInit_ex(des3_ctx, EVP_des_ede3_cbc(), NULL, lkey, livec);
    EVP_CIPHER_CTX_set_padding(des3_ctx, 0);
    return des3_ctx;
}

/*****************************************************************************/
void *APP_CC
ssl_des3_decrypt_info_create(const char *key, const char* ivec)
{
    EVP_CIPHER_CTX *des3_ctx;
    const tui8 *lkey;
    const tui8 *livec;

    des3_ctx = g_malloc(sizeof(EVP_CIPHER_CTX), 1);
    EVP_CIPHER_CTX_init(des3_ctx);
    lkey = (const tui8 *) key;
    livec = (const tui8 *) ivec;
    EVP_DecryptInit_ex(des3_ctx, EVP_des_ede3_cbc(), NULL, lkey, livec);
    EVP_CIPHER_CTX_set_padding(des3_ctx, 0);
    return des3_ctx;
}

/*****************************************************************************/
void APP_CC
ssl_des3_info_delete(void *des3)
{
    EVP_CIPHER_CTX *des3_ctx;

    des3_ctx = (EVP_CIPHER_CTX *) des3;
    if (des3_ctx != 0)
    {
        EVP_CIPHER_CTX_cleanup(des3_ctx);
        g_free(des3_ctx);
    }
}

/*****************************************************************************/
int APP_CC
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
int APP_CC
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
void * APP_CC
ssl_hmac_info_create(void)
{
    HMAC_CTX *hmac_ctx;

    hmac_ctx = (HMAC_CTX *) g_malloc(sizeof(HMAC_CTX), 1);
    HMAC_CTX_init(hmac_ctx);
    return hmac_ctx;
}

/*****************************************************************************/
void APP_CC
ssl_hmac_info_delete(void *hmac)
{
    HMAC_CTX *hmac_ctx;

    hmac_ctx = (HMAC_CTX *) hmac;
    if (hmac_ctx != 0)
    {
        HMAC_CTX_cleanup(hmac_ctx);
        g_free(hmac_ctx);
    }
}

/*****************************************************************************/
void APP_CC
ssl_hmac_sha1_init(void *hmac, const char *data, int len)
{
    HMAC_CTX *hmac_ctx;

    hmac_ctx = (HMAC_CTX *) hmac;
    HMAC_Init_ex(hmac_ctx, data, len, EVP_sha1(), NULL);
}

/*****************************************************************************/
void APP_CC
ssl_hmac_transform(void *hmac, const char *data, int len)
{
    HMAC_CTX *hmac_ctx;
    const tui8 *ldata;

    hmac_ctx = (HMAC_CTX *) hmac;
    ldata = (const tui8*) data;
    HMAC_Update(hmac_ctx, ldata, len);
}

/*****************************************************************************/
void APP_CC
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
static void APP_CC
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
int APP_CC
ssl_mod_exp(char *out, int out_len, char *in, int in_len,
            char *mod, int mod_len, char *exp, int exp_len)
{
    BN_CTX *ctx;
    BIGNUM lmod;
    BIGNUM lexp;
    BIGNUM lin;
    BIGNUM lout;
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
    BN_init(&lmod);
    BN_init(&lexp);
    BN_init(&lin);
    BN_init(&lout);
    BN_bin2bn((tui8 *)l_mod, mod_len, &lmod);
    BN_bin2bn((tui8 *)l_exp, exp_len, &lexp);
    BN_bin2bn((tui8 *)l_in, in_len, &lin);
    BN_mod_exp(&lout, &lin, &lexp, &lmod, ctx);
    rv = BN_bn2bin(&lout, (tui8 *)l_out);

    if (rv <= out_len)
    {
        ssl_reverse_it(l_out, rv);
        g_memcpy(out, l_out, out_len);
    }
    else
    {
        rv = 0;
    }

    BN_free(&lin);
    BN_free(&lout);
    BN_free(&lexp);
    BN_free(&lmod);
    BN_CTX_free(ctx);
    g_free(l_out);
    g_free(l_in);
    g_free(l_mod);
    g_free(l_exp);
    return rv;
}

#if defined(OLD_RSA_GEN1)
/*****************************************************************************/
/* returns error
   generates a new rsa key
   exp is passed in and mod and pri are passed out */
int APP_CC
ssl_gen_key_xrdp1(int key_size_in_bits, char *exp, int exp_len,
                  char *mod, int mod_len, char *pri, int pri_len)
{
    int my_e;
    RSA *my_key;
    char *lmod;
    char *lpri;
    tui8 *lexp;
    int error;
    int len;

    if ((exp_len != 4) || ((mod_len != 64) && (mod_len != 256)) ||
                          ((pri_len != 64) && (pri_len != 256)))
    {
        return 1;
    }

    lmod = (char *)g_malloc(mod_len, 0);
    lpri = (char *)g_malloc(pri_len, 0);
    lexp = (tui8 *)exp;
    my_e = lexp[0];
    my_e |= lexp[1] << 8;
    my_e |= lexp[2] << 16;
    my_e |= lexp[3] << 24;
    /* srand is in stdlib.h */
    srand(g_time1());
    my_key = RSA_generate_key(key_size_in_bits, my_e, 0, 0);
    error = my_key == 0;

    if (error == 0)
    {
        len = BN_num_bytes(my_key->n);
        error = len != mod_len;
    }

    if (error == 0)
    {
        BN_bn2bin(my_key->n, (tui8 *)lmod);
        ssl_reverse_it(lmod, mod_len);
    }

    if (error == 0)
    {
        len = BN_num_bytes(my_key->d);
        error = len != pri_len;
    }

    if (error == 0)
    {
        BN_bn2bin(my_key->d, (tui8 *)lpri);
        ssl_reverse_it(lpri, pri_len);
    }

    if (error == 0)
    {
        g_memcpy(mod, lmod, mod_len);
        g_memcpy(pri, lpri, pri_len);
    }

    RSA_free(my_key);
    g_free(lmod);
    g_free(lpri);
    return error;
}
#else
/*****************************************************************************/
/* returns error
   generates a new rsa key
   exp is passed in and mod and pri are passed out */
int APP_CC
ssl_gen_key_xrdp1(int key_size_in_bits, char *exp, int exp_len,
                  char *mod, int mod_len, char *pri, int pri_len)
{
    BIGNUM *my_e;
    RSA *my_key;
    char *lexp;
    char *lmod;
    char *lpri;
    int error;
    int len;

    if ((exp_len != 4) || ((mod_len != 64) && (mod_len != 256)) ||
                          ((pri_len != 64) && (pri_len != 256)))
    {
        return 1;
    }

    lexp = (char *)g_malloc(exp_len, 0);
    lmod = (char *)g_malloc(mod_len, 0);
    lpri = (char *)g_malloc(pri_len, 0);
    g_memcpy(lexp, exp, exp_len);
    ssl_reverse_it(lexp, exp_len);
    my_e = BN_new();
    BN_bin2bn((tui8 *)lexp, exp_len, my_e);
    my_key = RSA_new();
    error = RSA_generate_key_ex(my_key, key_size_in_bits, my_e, 0) == 0;

    if (error == 0)
    {
        len = BN_num_bytes(my_key->n);
        error = len != mod_len;
    }

    if (error == 0)
    {
        BN_bn2bin(my_key->n, (tui8 *)lmod);
        ssl_reverse_it(lmod, mod_len);
    }

    if (error == 0)
    {
        len = BN_num_bytes(my_key->d);
        error = len != pri_len;
    }

    if (error == 0)
    {
        BN_bn2bin(my_key->d, (tui8 *)lpri);
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
#endif

/*****************************************************************************/
struct ssl_tls *
APP_CC
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
int APP_CC
ssl_tls_print_error(char *func, SSL *connection, int value)
{
    switch (SSL_get_error(connection, value))
    {
        case SSL_ERROR_ZERO_RETURN:
            g_writeln("ssl_tls_print_error: %s: Server closed TLS connection",
                      func);
            return 1;

        case SSL_ERROR_WANT_READ:
            g_writeln("ssl_tls_print_error: SSL_ERROR_WANT_READ");
            return 0;

        case SSL_ERROR_WANT_WRITE:
            g_writeln("ssl_tls_print_error: SSL_ERROR_WANT_WRITE");
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
int APP_CC
ssl_tls_accept(struct ssl_tls *self)
{
    int connection_status;
    long options = 0;

    /**
     * SSL_OP_NO_SSLv2:
     *
     * We only want SSLv3 and TLSv1, so disable SSLv2.
     * SSLv3 is used by, eg. Microsoft RDC for Mac OS X.
     */
    options |= SSL_OP_NO_SSLv2;

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
    /* set context options */
    SSL_CTX_set_mode(self->ctx,
                     SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER |
                     SSL_MODE_ENABLE_PARTIAL_WRITE);
    SSL_CTX_set_options(self->ctx, options);
    SSL_CTX_set_read_ahead(self->ctx, 1);

    if (self->ctx == NULL)
    {
        g_writeln("ssl_tls_accept: SSL_CTX_new failed");
        return 1;
    }

    if (SSL_CTX_use_RSAPrivateKey_file(self->ctx, self->key, SSL_FILETYPE_PEM)
            <= 0)
    {
        g_writeln("ssl_tls_accept: SSL_CTX_use_RSAPrivateKey_file failed");
        return 1;
    }

    self->ssl = SSL_new(self->ctx);

    if (self->ssl == NULL)
    {
        g_writeln("ssl_tls_accept: SSL_new failed");
        return 1;
    }

    if (SSL_use_certificate_file(self->ssl, self->cert, SSL_FILETYPE_PEM) <= 0)
    {
        g_writeln("ssl_tls_accept: SSL_use_certificate_file failed");
        return 1;
    }

    if (SSL_set_fd(self->ssl, self->trans->sck) < 1)
    {
        g_writeln("ssl_tls_accept: SSL_set_fd failed");
        return 1;
    }

    connection_status = SSL_accept(self->ssl);

    if (connection_status <= 0)
    {
        if (ssl_tls_print_error("SSL_accept", self->ssl, connection_status))
        {
            return 1;
        }
    }

    g_writeln("ssl_tls_accept: TLS connection accepted");

    return 0;
}

/*****************************************************************************/
/* returns error, */
int APP_CC
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
    while (status != 1)
    {
        status = SSL_shutdown(self->ssl);
        if (status <= 0)
        {
            if (ssl_tls_print_error("SSL_shutdown", self->ssl, status))
            {
                return 1;
            }
        }
    }
    return 0;
}

/*****************************************************************************/
void APP_CC
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
int APP_CC
ssl_tls_read(struct ssl_tls *tls, char *data, int length)
{
    int status;

    status = SSL_read(tls->ssl, data, length);

    switch (SSL_get_error(tls->ssl, status))
    {
        case SSL_ERROR_NONE:
            break;

        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:
            status = 0;
            break;

        default:
            ssl_tls_print_error("SSL_read", tls->ssl, status);
            status = -1;
            break;
    }

    if (SSL_pending(tls->ssl) > 0)
    {
        g_set_wait_obj(tls->rwo);
    }

    return status;
}

/*****************************************************************************/
int APP_CC
ssl_tls_write(struct ssl_tls *tls, const char *data, int length)
{
    int status;

    status = SSL_write(tls->ssl, data, length);

    switch (SSL_get_error(tls->ssl, status))
    {
        case SSL_ERROR_NONE:
            break;

        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:
            status = 0;
            break;

        default:
            ssl_tls_print_error("SSL_write", tls->ssl, status);
            status = -1;
            break;
    }

    return status;
}

/*****************************************************************************/
/* returns boolean */
int APP_CC
ssl_tls_can_recv(struct ssl_tls *tls, int sck, int millis)
{
    if (SSL_pending(tls->ssl) > 0)
    {
        return 1;
    }
    g_reset_wait_obj(tls->rwo);
    return g_tcp_can_recv(sck, millis);
}

