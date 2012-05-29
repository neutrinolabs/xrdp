/*
   Copyright (c) 2008-2010 Jay Sorg

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.

   generic transport

*/

#include "os_calls.h"
#include "trans.h"
#include "arch.h"
#include "parse.h"

/*****************************************************************************/
struct trans* APP_CC
trans_create(int mode, int in_size, int out_size)
{
  struct trans* self = (struct trans *)NULL;

  self = (struct trans*)g_malloc(sizeof(struct trans), 1);
  if (self != NULL) {
    make_stream(self->in_s);
    init_stream(self->in_s, in_size);
    make_stream(self->out_s);
    init_stream(self->out_s, out_size);
    self->mode = mode;
  }
  return self;
}

/*****************************************************************************/
void APP_CC
trans_delete(struct trans* self)
{
  if (self == 0)
  {
    return;
  }
  free_stream(self->in_s);
  free_stream(self->out_s);
  if (self->sck > 0) {
    g_tcp_close(self->sck);
  }
  self->sck = 0;
  if (self->listen_filename != 0)
  {
    g_file_delete(self->listen_filename);
    g_free(self->listen_filename);
  }
  g_free(self);
}

/*****************************************************************************/
int APP_CC
trans_get_wait_objs(struct trans* self, tbus* objs, int* count)
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
trans_check_wait_objs(struct trans* self)
{
  tbus in_sck = (tbus)0;
  struct trans* in_trans = (struct trans *)NULL;
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
      in_sck = g_tcp_accept(self->sck);
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
          init_stream(self->in_s, 0);
        }
      }
    }
  }
  return rv;
}

/*****************************************************************************/
int APP_CC
trans_force_read_s(struct trans* self, struct stream* in_s, int size)
{
  int rcvd;

  if (self->status != TRANS_STATUS_UP)
  {
    return 1;
  }
  while (size > 0)
  {
    rcvd = g_tcp_recv(self->sck, in_s->end, size, 0);
    if (rcvd == -1)
    {
      if (g_tcp_last_error_would_block(self->sck))
      {
        if (!g_tcp_can_recv(self->sck, 10))
        {
          /* check for term here */
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
trans_force_read(struct trans* self, int size)
{
  return trans_force_read_s(self, self->in_s, size);
}

/*****************************************************************************/
int APP_CC
trans_force_write_s(struct trans* self, struct stream* out_s)
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
  while (total < size)
  {
    sent = g_tcp_send(self->sck, out_s->data + total, size - total, 0);
    if (sent == -1)
    {
      if (g_tcp_last_error_would_block(self->sck))
      {
        if (!g_tcp_can_send(self->sck, 10))
        {
          /* check for term here */
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
trans_force_write(struct trans* self)
{
  return trans_force_write_s(self, self->out_s);
}

/*****************************************************************************/
int APP_CC
trans_connect(struct trans* self, const char* server, const char* port,
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
trans_listen_address(struct trans* self, char* port, const char* address)
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
        g_chmod_hex(port, 0xffff);
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
trans_listen(struct trans* self, char* port)
{
  return trans_listen_address(self, port, "0.0.0.0");
}

/*****************************************************************************/
struct stream* APP_CC
trans_get_in_s(struct trans* self)
{
  struct stream * rv = (struct stream *)NULL;
  if (self == NULL) {
    rv = (struct stream *)NULL;
  }
  else {
    rv = self->in_s;
  }
  return rv;
}

/*****************************************************************************/
struct stream* APP_CC
trans_get_out_s(struct trans* self, int size)
{
  struct stream * rv = (struct stream *)NULL;
  if (self == NULL) {
    rv = (struct stream *)NULL;
  }
  else {
    init_stream(self->out_s, size);
    rv = self->out_s;
  }
  return rv;
}
