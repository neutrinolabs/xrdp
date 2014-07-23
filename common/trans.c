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

#include "os_calls.h"
#include "trans.h"
#include "arch.h"
#include "parse.h"

/*****************************************************************************/
struct trans *APP_CC
trans_create(int mode, int in_size, int out_size)
{
    struct trans *self = (struct trans *)NULL;

    self = (struct trans *)g_malloc(sizeof(struct trans), 1);

    if (self != NULL)
    {
        make_stream(self->in_s);
        init_stream(self->in_s, in_size);
        make_stream(self->out_s);
        init_stream(self->out_s, out_size);
        self->mode = mode;
        self->tls = 0;
        /* assign tcp functions */
        self->trans_read_call = trans_tcp_force_read_s;
        self->trans_write_call = trans_tcp_force_write_s;
    }

    return self;
}

/*****************************************************************************/
void APP_CC
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
    	xrdp_tls_delete(self->tls);
    }

    g_free(self);
}

/*****************************************************************************/
int APP_CC
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
    return 0;
}

/*****************************************************************************/
int APP_CC
trans_get_wait_objs_rw(struct trans *self,
                       tbus *robjs, int *rcount,
                       tbus *wobjs, int *wcount)
{
    if (self == 0)
    {
        return 1;
    }

    if (self->status != TRANS_STATUS_UP)
    {
        return 1;
    }

    robjs[*rcount] = self->sck;
    (*rcount)++;

    if (self->wait_s != 0)
    {
        wobjs[*wcount] = self->sck;
        (*wcount)++;
    }

    return 0;
}

/*****************************************************************************/
int APP_CC
send_waiting(struct trans *self, int block)
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
                sent = g_tcp_send(self->sck, temp_s->p, bytes, 0);
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

/*****************************************************************************/
int APP_CC
trans_check_wait_objs(struct trans *self)
{
    tbus in_sck = (tbus)0;
    struct trans *in_trans = (struct trans *)NULL;
    int read_bytes = 0;
    int to_read = 0;
    int read_so_far = 0;
    int rv = 0;

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
        if (g_tcp_can_recv(self->sck, 0))
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
                    g_strncpy(in_trans->addr, self->addr, sizeof(self->addr) - 1);
                    g_strncpy(in_trans->port, self->port, sizeof(self->port) - 1);

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
        if (g_tcp_can_recv(self->sck, 0))
        {
            read_so_far = (int)(self->in_s->end - self->in_s->data);
            to_read = self->header_size - read_so_far;

            if (to_read > 0)
            {
				read_bytes = g_tcp_recv(self->sck, self->in_s->end, to_read, 0);

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
                        return 1;
                    }
                }
                else if (read_bytes == 0)
                {
                    /* error */
                    self->status = TRANS_STATUS_DOWN;
                    return 1;
                }
                else
                {
                    self->in_s->end += read_bytes;
                }
            }

            read_so_far = (int)(self->in_s->end - self->in_s->data);

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
        }
        if (send_waiting(self, 0) != 0)
        {
            /* error */
            self->status = TRANS_STATUS_DOWN;
            return 1;
        }
    }

    return rv;
}
/*****************************************************************************/
int APP_CC
trans_force_read_s(struct trans *self, struct stream *in_s, int size)
{
	return self->trans_read_call(self, in_s, size);
}
/*****************************************************************************/
int APP_CC
trans_tcp_force_read_s(struct trans *self, struct stream *in_s, int size)
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

        rcvd = g_tcp_recv(self->sck, in_s->end, size, 0);

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
trans_force_read(struct trans *self, int size)
{
	if (self->tls != 0)
	{
		return xrdp_tls_force_read_s(self, self->in_s, size);
	}
    return trans_force_read_s(self, self->in_s, size);
}

/*****************************************************************************/
int APP_CC
trans_force_write_s(struct trans *self, struct stream *out_s)
{
	return self->trans_write_call(self, out_s);
}
/*****************************************************************************/
int APP_CC
trans_tcp_force_write_s(struct trans *self, struct stream *out_s)
{
    int size;
    int total;
    int sent;

    if (self->status != TRANS_STATUS_UP)
    {
        return 1;
    }

    size = (int)(out_s->end - out_s->data);
    total = 0;

    if (send_waiting(self, 1) != 0)
    {
        self->status = TRANS_STATUS_DOWN;
        return 1;
    }

    while (total < size)
    {
		sent = g_tcp_send(self->sck, out_s->data + total, size - total, 0);

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
trans_force_write(struct trans *self)
{
    return trans_force_write_s(self, self->out_s);
}

/*****************************************************************************/
int APP_CC
trans_write_copy(struct trans *self)
{
    int size;
    struct stream *out_s;
    struct stream *wait_s;
    struct stream *temp_s;

    if (self->status != TRANS_STATUS_UP)
    {
        return 1;
    }

    out_s = self->out_s;
    size = (int)(out_s->end - out_s->data);
    make_stream(wait_s);
    init_stream(wait_s, size);
    out_uint8a(wait_s, out_s->data, size);
    s_mark_end(wait_s);
    wait_s->p = wait_s->data;
    if (self->wait_s == 0)
    {
        self->wait_s = wait_s;
    }
    else
    {
        temp_s = self->wait_s;
        while (temp_s->next_packet != 0)
        {
            temp_s = (struct stream *) (temp_s->next_packet);
        }
        temp_s->next_packet = (char *) wait_s;
    }

    /* try to send */
    if (send_waiting(self, 0) != 0)
    {
        /* error */
        self->status = TRANS_STATUS_DOWN;
        return 1;
    }

    return 0;
}

/*****************************************************************************/
int APP_CC
trans_connect(struct trans *self, const char *server, const char *port,
              int timeout)
{
    int error;

    if (self->sck != 0)
    {
        g_tcp_close(self->sck);
    }

    if (self->mode == TRANS_MODE_TCP) /* tcp */
    {
        self->sck = g_tcp_socket();
        g_tcp_set_non_blocking(self->sck);
        error = g_tcp_connect(self->sck, server, port);
    }
    else if (self->mode == TRANS_MODE_UNIX) /* unix socket */
    {
        self->sck = g_tcp_local_socket();
        g_tcp_set_non_blocking(self->sck);
        error = g_tcp_local_connect(self->sck, port);
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
int APP_CC
trans_listen_address(struct trans *self, char *port, const char *address)
{
    if (self->sck != 0)
    {
        g_tcp_close(self->sck);
    }

    if (self->mode == TRANS_MODE_TCP) /* tcp */
    {
        self->sck = g_tcp_socket();
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

    return 1;
}

/*****************************************************************************/
int APP_CC
trans_listen(struct trans *self, char *port)
{
    return trans_listen_address(self, port, "0.0.0.0");
}

/*****************************************************************************/
struct stream *APP_CC
trans_get_in_s(struct trans *self)
{
    struct stream *rv = (struct stream *)NULL;

    if (self == NULL)
    {
        rv = (struct stream *)NULL;
    }
    else
    {
        rv = self->in_s;
    }

    return rv;
}

/*****************************************************************************/
struct stream *APP_CC
trans_get_out_s(struct trans *self, int size)
{
    struct stream *rv = (struct stream *)NULL;

    if (self == NULL)
    {
        rv = (struct stream *)NULL;
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
int APP_CC
trans_set_tls_mode(struct trans *self, const char *key, const char *cert)
{
	self->tls = xrdp_tls_create(self, key, cert);
	if (self->tls == NULL)
	{
		g_writeln("trans_set_tls_mode: xrdp_tls_create malloc error");
		return 1;
	}

	if (xrdp_tls_accept(self->tls) != 0)
	{
		g_writeln("trans_set_tls_mode: xrdp_tls_accept failed");
		return 1;
	}

	/* assign tls functions */
	self->trans_read_call = xrdp_tls_force_read_s;
	self->trans_write_call = xrdp_tls_force_write_s;

	return 0;
}
/*****************************************************************************/
/* returns error */
int APP_CC
trans_shutdown_tls_mode(struct trans *self)
{
	if (self->tls != NULL)
	{
		return xrdp_tls_disconnect(self->tls);
	}

	/* set callback to tls */
	self->trans_read_call = trans_tcp_force_read_s;
	self->trans_write_call = trans_tcp_force_write_s;
	return 0;
}
