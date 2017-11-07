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
 *
 * generic transport
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "os_calls.h"
#include "trans.h"
#include "arch.h"
#include "parse.h"
#include "ssl_calls.h"

#define MAX_SBYTES 0

/*****************************************************************************/
int
trans_tls_recv(struct trans *self, char *ptr, int len)
{
    if (self->tls == NULL)
    {
        return 1;
    }
    return ssl_tls_read(self->tls, ptr, len);
}

/*****************************************************************************/
int
trans_tls_send(struct trans *self, const char *data, int len)
{
    if (self->tls == NULL)
    {
        return 1;
    }
    return ssl_tls_write(self->tls, data, len);
}

/*****************************************************************************/
int
trans_tls_can_recv(struct trans *self, int sck, int millis)
{
    if (self->tls == NULL)
    {
        return 1;
    }
    return ssl_tls_can_recv(self->tls, sck, millis);
}

/*****************************************************************************/
int
trans_tcp_recv(struct trans *self, char *ptr, int len)
{
    return g_tcp_recv(self->sck, ptr, len, 0);
}

/*****************************************************************************/
int
trans_tcp_send(struct trans *self, const char *data, int len)
{
    return g_tcp_send(self->sck, data, len, 0);
}

/*****************************************************************************/
int
trans_tcp_can_recv(struct trans *self, int sck, int millis)
{
    return g_sck_can_recv(sck, millis);
}

/*****************************************************************************/
struct trans *
trans_create(int mode, int in_size, int out_size)
{
    struct trans *self = (struct trans *) NULL;

    self = (struct trans *) g_malloc(sizeof(struct trans), 1);

    if (self != NULL)
    {
        make_stream(self->in_s);
        init_stream(self->in_s, in_size);
        make_stream(self->out_s);
        init_stream(self->out_s, out_size);
        self->mode = mode;
        self->tls = 0;
        /* assign tcp calls by default */
        self->trans_recv = trans_tcp_recv;
        self->trans_send = trans_tcp_send;
        self->trans_can_recv = trans_tcp_can_recv;
    }

    return self;
}

/*****************************************************************************/
void
trans_delete(struct trans *self)
{
    if (self == 0)
    {
        return;
    }

    free_stream(self->in_s);
    free_stream(self->out_s);

    if (self->sck > 0)
    {
        g_tcp_close(self->sck);
    }

    self->sck = 0;

    if (self->listen_filename != 0)
    {
        g_file_delete(self->listen_filename);
        g_free(self->listen_filename);
    }

    if (self->tls != 0)
    {
        ssl_tls_delete(self->tls);
    }

    g_free(self);
}

/*****************************************************************************/
int
trans_get_wait_objs(struct trans *self, tbus *objs, int *count)
{
    if (self == 0)
    {
        return 1;
    }

    if (self->status != TRANS_STATUS_UP)
    {
        return 1;
    }

    objs[*count] = self->sck;
    (*count)++;

    if (self->tls != 0)
    {
        if (self->tls->rwo != 0)
        {
            objs[*count] = self->tls->rwo;
            (*count)++;
        }
    }

    return 0;
}

/*****************************************************************************/
int
trans_get_wait_objs_rw(struct trans *self, tbus *robjs, int *rcount,
                       tbus *wobjs, int *wcount, int *timeout)
{
    if (self == 0)
    {
        return 1;
    }

    if (self->status != TRANS_STATUS_UP)
    {
        return 1;
    }

    if ((self->si != 0) && (self->si->source[self->my_source] > MAX_SBYTES))
    {
    }
    else
    {
        if (trans_get_wait_objs(self, robjs, rcount) != 0)
        {
            return 1;
        }
    }

    if (self->wait_s != 0)
    {
        wobjs[*wcount] = self->sck;
        (*wcount)++;
    }

    return 0;
}

/*****************************************************************************/
int
trans_send_waiting(struct trans *self, int block)
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
                sent = self->trans_send(self, temp_s->p, bytes);
                if (sent > 0)
                {
                    temp_s->p += sent;
                    if (temp_s->source != 0)
                    {
                        temp_s->source[0] -= sent;
                    }
                    if (temp_s->p >= temp_s->end)
                    {
                        self->wait_s = temp_s->next;
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
            else if (block)
            {
                /* check for term here */
                if (self->is_term != 0)
                {
                    if (self->is_term())
                    {
                        /* term */
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

/*****************************************************************************/
int
trans_check_wait_objs(struct trans *self)
{
    tbus in_sck = (tbus) 0;
    struct trans *in_trans = (struct trans *) NULL;
    int read_bytes = 0;
    int to_read = 0;
    int read_so_far = 0;
    int rv = 0;
    int cur_source;

    if (self == 0)
    {
        return 1;
    }

    if (self->status != TRANS_STATUS_UP)
    {
        return 1;
    }

    rv = 0;

    if (self->type1 == TRANS_TYPE_LISTENER) /* listening */
    {
        if (g_sck_can_recv(self->sck, 0))
        {
            in_sck = g_sck_accept(self->sck, self->addr, sizeof(self->addr),
                                  self->port, sizeof(self->port));

            if (in_sck == -1)
            {
                if (g_tcp_last_error_would_block(self->sck))
                {
                    /* ok, but shouldn't happen */
                }
                else
                {
                    /* error */
                    self->status = TRANS_STATUS_DOWN;
                    return 1;
                }
            }

            if (in_sck != -1)
            {
                if (self->trans_conn_in != 0) /* is function assigned */
                {
                    in_trans = trans_create(self->mode, self->in_s->size,
                                            self->out_s->size);
                    in_trans->sck = in_sck;
                    in_trans->type1 = TRANS_TYPE_SERVER;
                    in_trans->status = TRANS_STATUS_UP;
                    in_trans->is_term = self->is_term;
                    g_strncpy(in_trans->addr, self->addr,
                              sizeof(self->addr) - 1);
                    g_strncpy(in_trans->port, self->port,
                              sizeof(self->port) - 1);
                    g_sck_set_non_blocking(in_sck);
                    if (self->trans_conn_in(self, in_trans) != 0)
                    {
                        trans_delete(in_trans);
                    }
                }
                else
                {
                    g_tcp_close(in_sck);
                }
            }
        }
    }
    else /* connected server or client (2 or 3) */
    {
        if (self->si != 0 && self->si->source[self->my_source] > MAX_SBYTES)
        {
        }
        else if (self->trans_can_recv(self, self->sck, 0))
        {
            cur_source = 0;
            if (self->si != 0)
            {
                cur_source = self->si->cur_source;
                self->si->cur_source = self->my_source;
            }
            read_so_far = (int) (self->in_s->end - self->in_s->data);
            to_read = self->header_size - read_so_far;

            if (to_read > 0)
            {
                read_bytes = self->trans_recv(self, self->in_s->end, to_read);

                if (read_bytes == -1)
                {
                    if (g_tcp_last_error_would_block(self->sck))
                    {
                        /* ok, but shouldn't happen */
                    }
                    else
                    {
                        /* error */
                        self->status = TRANS_STATUS_DOWN;
                        if (self->si != 0)
                        {
                            self->si->cur_source = cur_source;
                        }
                        return 1;
                    }
                }
                else if (read_bytes == 0)
                {
                    /* error */
                    self->status = TRANS_STATUS_DOWN;
                    if (self->si != 0)
                    {
                        self->si->cur_source = cur_source;
                    }
                    return 1;
                }
                else
                {
                    self->in_s->end += read_bytes;
                }
            }

            read_so_far = (int) (self->in_s->end - self->in_s->data);

            if (read_so_far == self->header_size)
            {
                if (self->trans_data_in != 0)
                {
                    rv = self->trans_data_in(self);
                    if (self->no_stream_init_on_data_in == 0)
                    {
                        init_stream(self->in_s, 0);
                    }
                }
            }
            if (self->si != 0)
            {
                self->si->cur_source = cur_source;
            }
        }
        if (trans_send_waiting(self, 0) != 0)
        {
            /* error */
            self->status = TRANS_STATUS_DOWN;
            return 1;
        }
    }

    return rv;
}

/*****************************************************************************/
int
trans_force_read_s(struct trans *self, struct stream *in_s, int size)
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
        rcvd = self->trans_recv(self, in_s->end, size);
        if (rcvd == -1)
        {
            if (g_tcp_last_error_would_block(self->sck))
            {
                if (!self->trans_can_recv(self, self->sck, 100))
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
int
trans_force_read(struct trans *self, int size)
{
    return trans_force_read_s(self, self->in_s, size);
}

/*****************************************************************************/
int
trans_force_write_s(struct trans *self, struct stream *out_s)
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
    if (trans_send_waiting(self, 1) != 0)
    {
        self->status = TRANS_STATUS_DOWN;
        return 1;
    }
    while (total < size)
    {
        sent = self->trans_send(self, out_s->data + total, size - total);
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
int
trans_force_write(struct trans *self)
{
    return trans_force_write_s(self, self->out_s);
}

/*****************************************************************************/
int
trans_write_copy_s(struct trans *self, struct stream *out_s)
{
    int size;
    int sent;
    struct stream *wait_s;
    struct stream *temp_s;
    char *out_data;

    if (self->status != TRANS_STATUS_UP)
    {
        return 1;
    }
    /* try to send any left over */
    if (trans_send_waiting(self, 0) != 0)
    {
        /* error */
        self->status = TRANS_STATUS_DOWN;
        return 1;
    }
    out_data = out_s->data;
    sent = 0;
    size = (int) (out_s->end - out_s->data);
    if (self->wait_s == 0)
    {
        /* if no left over, try to send this new data */
        if (g_tcp_can_send(self->sck, 0))
        {
            sent = self->trans_send(self, out_s->data, size);
            if (sent > 0)
            {
                out_data += sent;
                size -= sent;
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
    if (size < 1)
    {
        return 0;
    }
    /* did not send right away, have to copy */
    make_stream(wait_s);
    init_stream(wait_s, size);
    if (self->si != 0)
    {
        if ((self->si->cur_source != 0) &&
            (self->si->cur_source != self->my_source))
        {
            self->si->source[self->si->cur_source] += size;
            wait_s->source = self->si->source + self->si->cur_source;
        }
    }
    out_uint8a(wait_s, out_data, size);
    s_mark_end(wait_s);
    wait_s->p = wait_s->data;
    if (self->wait_s == 0)
    {
        self->wait_s = wait_s;
    }
    else
    {
        temp_s = self->wait_s;
        while (temp_s->next != 0)
        {
            temp_s = temp_s->next;
        }
        temp_s->next = wait_s;
    }
    return 0;
}

/*****************************************************************************/
int
trans_write_copy(struct trans* self)
{
    return trans_write_copy_s(self, self->out_s);
}

/*****************************************************************************/
int
trans_connect(struct trans *self, const char *server, const char *port,
              int timeout)
{
    int error;
    int now;
    int start_time;

    start_time = g_time3();

    if (self->sck != 0)
    {
        g_tcp_close(self->sck);
        self->sck = 0;
    }

    if (self->mode == TRANS_MODE_TCP) /* tcp */
    {
        self->sck = g_tcp_socket();
        if (self->sck < 0)
        {
            self->status = TRANS_STATUS_DOWN;
            return 1;
        }
        g_tcp_set_non_blocking(self->sck);
        while (1)
        {
            error = g_tcp_connect(self->sck, server, port);
            if (error == 0)
            {
                break;
            }
            else
            {
                if (timeout < 1)
                {
                    self->status = TRANS_STATUS_DOWN;
                    return 1;
                }
                now = g_time3();
                if (now - start_time < timeout)
                {
                    g_sleep(timeout / 5);
                }
                else
                {
                    self->status = TRANS_STATUS_DOWN;
                    return 1;
                }
            }
        }
    }
    else if (self->mode == TRANS_MODE_UNIX) /* unix socket */
    {
        self->sck = g_tcp_local_socket();
        if (self->sck < 0)
        {
            self->status = TRANS_STATUS_DOWN;
            return 1;
        }
        g_tcp_set_non_blocking(self->sck);
        while (1)
        {
            error = g_tcp_local_connect(self->sck, port);
            if (error == 0)
            {
                break;
            }
            else
            {
                if (timeout < 1)
                {
                    self->status = TRANS_STATUS_DOWN;
                    return 1;
                }
                now = g_time3();
                if (now - start_time < timeout)
                {
                    g_sleep(timeout / 5);
                }
                else
                {
                    self->status = TRANS_STATUS_DOWN;
                    return 1;
                }
            }
        }
    }
    else
    {
        self->status = TRANS_STATUS_DOWN;
        return 1;
    }

    if (error == -1)
    {
        if (g_tcp_last_error_would_block(self->sck))
        {
            now = g_time3();
            if (now - start_time < timeout)
            {
                timeout = timeout - (now - start_time);
            }
            else
            {
                timeout = 0;
            }
            if (g_tcp_can_send(self->sck, timeout))
            {
                self->status = TRANS_STATUS_UP; /* ok */
                self->type1 = TRANS_TYPE_CLIENT; /* client */
                return 0;
            }
        }

        return 1;
    }

    self->status = TRANS_STATUS_UP; /* ok */
    self->type1 = TRANS_TYPE_CLIENT; /* client */
    return 0;
}

/*****************************************************************************/

/**
 * @return 0 on success, 1 on failure
 */
int
trans_listen_address(struct trans *self, char *port, const char *address)
{
    if (self->sck != 0)
    {
        g_tcp_close(self->sck);
    }

    if (self->mode == TRANS_MODE_TCP) /* tcp */
    {
        self->sck = g_tcp_socket();
        if (self->sck < 0)
            return 1;

        g_tcp_set_non_blocking(self->sck);

        if (g_tcp_bind_address(self->sck, port, address) == 0)
        {
            if (g_tcp_listen(self->sck) == 0)
            {
                self->status = TRANS_STATUS_UP; /* ok */
                self->type1 = TRANS_TYPE_LISTENER; /* listener */
                return 0;
            }
        }
    }
    else if (self->mode == TRANS_MODE_UNIX) /* unix socket */
    {
        g_free(self->listen_filename);
        self->listen_filename = 0;
        g_file_delete(port);

        self->sck = g_tcp_local_socket();
        if (self->sck < 0)
            return 1;

        g_tcp_set_non_blocking(self->sck);

        if (g_tcp_local_bind(self->sck, port) == 0)
        {
            self->listen_filename = g_strdup(port);

            if (g_tcp_listen(self->sck) == 0)
            {
                g_chmod_hex(port, 0x0660);
                self->status = TRANS_STATUS_UP; /* ok */
                self->type1 = TRANS_TYPE_LISTENER; /* listener */
                return 0;
            }
        }
    }
    else if (self->mode == TRANS_MODE_VSOCK) /* vsock socket */
    {
        self->sck = g_sck_vsock_socket();
        if (self->sck < 0)
        {
            return 1;
        }

        g_tcp_set_non_blocking(self->sck);

        if (g_sck_vsock_bind(self->sck, port) == 0)
        {
            if (g_tcp_listen(self->sck) == 0)
            {
                self->status = TRANS_STATUS_UP; /* ok */
                self->type1 = TRANS_TYPE_LISTENER; /* listener */
                return 0;
            }
        }
    }

    return 1;
}

/*****************************************************************************/
int
trans_listen(struct trans *self, char *port)
{
    return trans_listen_address(self, port, "0.0.0.0");
}

/*****************************************************************************/
struct stream *
trans_get_in_s(struct trans *self)
{
    struct stream *rv = (struct stream *) NULL;

    if (self == NULL)
    {
        rv = (struct stream *) NULL;
    }
    else
    {
        rv = self->in_s;
    }

    return rv;
}

/*****************************************************************************/
struct stream *
trans_get_out_s(struct trans *self, int size)
{
    struct stream *rv = (struct stream *) NULL;

    if (self == NULL)
    {
        rv = (struct stream *) NULL;
    }
    else
    {
        init_stream(self->out_s, size);
        rv = self->out_s;
    }

    return rv;
}

/*****************************************************************************/
/* returns error */
int
trans_set_tls_mode(struct trans *self, const char *key, const char *cert,
                   long ssl_protocols, const char *tls_ciphers)
{
    self->tls = ssl_tls_create(self, key, cert);
    if (self->tls == NULL)
    {
        g_writeln("trans_set_tls_mode: ssl_tls_create malloc error");
        return 1;
    }

    if (ssl_tls_accept(self->tls, ssl_protocols, tls_ciphers) != 0)
    {
        g_writeln("trans_set_tls_mode: ssl_tls_accept failed");
        return 1;
    }

    /* assign tls functions */
    self->trans_recv = trans_tls_recv;
    self->trans_send = trans_tls_send;
    self->trans_can_recv = trans_tls_can_recv;

    self->ssl_protocol = ssl_get_version(self->tls->ssl);
    self->cipher_name = ssl_get_cipher_name(self->tls->ssl);

    return 0;
}

/*****************************************************************************/
/* returns error */
int
trans_shutdown_tls_mode(struct trans *self)
{
    if (self->tls != NULL)
    {
        return ssl_tls_disconnect(self->tls);
    }

    /* assign callback back to tcp cal */
    self->trans_recv = trans_tcp_recv;
    self->trans_send = trans_tcp_send;
    self->trans_can_recv = trans_tcp_can_recv;

    return 0;
}
