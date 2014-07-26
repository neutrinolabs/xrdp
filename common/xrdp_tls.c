/**
 * xrdp: A Remote Desktop Protocol server.
 *
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
 * transport layer security
 */

#include "trans.h"
#include "ssl_calls.h"

/*****************************************************************************/
struct xrdp_tls *
APP_CC
xrdp_tls_create(struct trans *trans, const char *key, const char *cert)
{
    struct xrdp_tls *self;
    self = (struct xrdp_tls *) g_malloc(sizeof(struct xrdp_tls), 1);

    if (self != NULL)
    {
        self->trans = trans;
        self->cert = (char *) cert;
        self->key = (char *) key;
    }

    return self;
}
/*****************************************************************************/
int APP_CC
xrdp_tls_accept(struct xrdp_tls *self)
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
        g_writeln("xrdp_tls_accept: SSL_CTX_new failed");
        return 1;
    }

    if (SSL_CTX_use_RSAPrivateKey_file(self->ctx, self->key, SSL_FILETYPE_PEM)
            <= 0)
    {
        g_writeln("xrdp_tls_accept: SSL_CTX_use_RSAPrivateKey_file failed");
        return 1;
    }

    self->ssl = SSL_new(self->ctx);

    if (self->ssl == NULL)
    {
        g_writeln("xrdp_tls_accept: SSL_new failed");
        return 1;
    }

    if (SSL_use_certificate_file(self->ssl, self->cert, SSL_FILETYPE_PEM) <= 0)
    {
        g_writeln("xrdp_tls_accept: SSL_use_certificate_file failed");
        return 1;
    }

    if (SSL_set_fd(self->ssl, self->trans->sck) < 1)
    {
        g_writeln("xrdp_tls_accept: SSL_set_fd failed");
        return 1;
    }

    connection_status = SSL_accept(self->ssl);

    if (connection_status <= 0)
    {
        if (xrdp_tls_print_error("SSL_accept", self->ssl, connection_status))
        {
            return 1;
        }
    }

    g_writeln("xrdp_tls_accept: TLS connection accepted");

    return 0;
}
/*****************************************************************************/
int APP_CC
xrdp_tls_print_error(char *func, SSL *connection, int value)
{
    switch (SSL_get_error(connection, value))
    {
    case SSL_ERROR_ZERO_RETURN:
        g_writeln("xrdp_tls_print_error: %s: Server closed TLS connection",
                  func);
        return 1;

    case SSL_ERROR_WANT_READ:
        g_writeln("xrdp_tls_print_error: SSL_ERROR_WANT_READ");
        return 0;

    case SSL_ERROR_WANT_WRITE:
        g_writeln("xrdp_tls_print_error: SSL_ERROR_WANT_WRITE");
        return 0;

    case SSL_ERROR_SYSCALL:
        g_writeln("xrdp_tls_print_error: %s: I/O error", func);
        return 1;

    case SSL_ERROR_SSL:
        g_writeln(
                "xrdp_tls_print_error: %s: Failure in SSL library (protocol error?)",
                func);
        return 1;

    default:
        g_writeln("xrdp_tls_print_error: %s: Unknown error", func);
        return 1;
    }
}
/*****************************************************************************/
int APP_CC
xrdp_tls_disconnect(struct xrdp_tls *self)
{
    int status = SSL_shutdown(self->ssl);
    while (status != 1)
    {
        status = SSL_shutdown(self->ssl);

        if (status <= 0)
        {
            if (xrdp_tls_print_error("SSL_shutdown", self->ssl, status))
            {
                return 1;
            }
        }
    }
    return 0;
}
/*****************************************************************************/
void APP_CC
xrdp_tls_delete(struct xrdp_tls *self)
{
    if (self != NULL)
    {
        if (self->ssl)
            SSL_free(self->ssl);

        if (self->ctx)
            SSL_CTX_free(self->ctx);

        g_free(self);
    }
}
/*****************************************************************************/
int APP_CC
xrdp_tls_read(struct xrdp_tls *tls, unsigned char *data, int length)
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
        xrdp_tls_print_error("SSL_read", tls->ssl, status);
        status = -1;
        break;
    }

    return status;
}
/*****************************************************************************/
int APP_CC
xrdp_tls_write(struct xrdp_tls *tls, unsigned char *data, int length)
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
        xrdp_tls_print_error("SSL_write", tls->ssl, status);
        status = -1;
        break;
    }

    return status;
}
/*****************************************************************************/
int APP_CC
xrdp_tls_force_read_s(struct trans *self, struct stream *in_s, int size)
{
    int rcvd;

    if (self->status != TRANS_STATUS_UP)
    {
        return 1;
    }

    while (size > 0)
    {
        /* make sure stream has room */
        if ((in_s->end + size) > (in_s->data + in_s->size))
        {
            return 1;
        }

        rcvd = xrdp_tls_read(self->tls, in_s->end, size);

        if (rcvd == -1)
        {
            if (g_tcp_last_error_would_block(self->sck))
            {
                if (!g_tcp_can_recv(self->sck, 100))
                {
                    /* check for term here */
                    if (self->is_term != 0)
                    {
                        if (self->is_term())
                        {
                            /* term */
                            self->status = TRANS_STATUS_DOWN;
                            return 1;
                        }
                    }
                }
            }
            else
            {
                /* error */
                self->status = TRANS_STATUS_DOWN;
                return 1;
            }
        }
        else if (rcvd == 0)
        {
            /* error */
            self->status = TRANS_STATUS_DOWN;
            return 1;
        }
        else
        {
            in_s->end += rcvd;
            size -= rcvd;
        }
    }

    return 0;
}

/*****************************************************************************/
int APP_CC
xrdp_tls_force_write_s(struct trans *self, struct stream *out_s)
{
    int size;
    int total;
    int sent;

    if (self->status != TRANS_STATUS_UP)
    {
        return 1;
    }

    size = (int) (out_s->end - out_s->data);
    total = 0;

    if (xrdp_tls_send_waiting(self, 1) != 0)
    {
        self->status = TRANS_STATUS_DOWN;
        return 1;
    }

    while (total < size)
    {
        sent = xrdp_tls_write(self->tls, out_s->data + total, size - total);

        if (sent == -1)
        {
            if (g_tcp_last_error_would_block(self->sck))
            {
                if (!g_tcp_can_send(self->sck, 100))
                {
                    /* check for term here */
                    if (self->is_term != 0)
                    {
                        if (self->is_term())
                        {
                            /* term */
                            self->status = TRANS_STATUS_DOWN;
                            return 1;
                        }
                    }
                }
            }
            else
            {
                /* error */
                self->status = TRANS_STATUS_DOWN;
                return 1;
            }
        }
        else if (sent == 0)
        {
            /* error */
            self->status = TRANS_STATUS_DOWN;
            return 1;
        }
        else
        {
            total = total + sent;
        }
    }

    return 0;
}
/*****************************************************************************/
int APP_CC
xrdp_tls_send_waiting(struct trans *self, int block)
{
    struct stream *temp_s;
    int bytes;
    int sent;
    int timeout;
    int cont;

    timeout = block ? 100 : 0;
    cont = 1;
    while (cont)
    {
        if (self->wait_s != 0)
        {
            temp_s = self->wait_s;
            if (g_tcp_can_send(self->sck, timeout))
            {
                bytes = (int) (temp_s->end - temp_s->p);
                sent = xrdp_tls_write(self->tls, temp_s->p, bytes);
                if (sent > 0)
                {
                    temp_s->p += sent;
                    if (temp_s->p >= temp_s->end)
                    {
                        self->wait_s = (struct stream *) (temp_s->next_packet);
                        free_stream(temp_s);
                    }
                }
                else if (sent == 0)
                {
                    return 1;
                }
                else
                {
                    if (!g_tcp_last_error_would_block(self->sck))
                    {
                        return 1;
                    }
                }
            }
        }
        else
        {
            break;
        }
        cont = block;
    }
    return 0;
}
