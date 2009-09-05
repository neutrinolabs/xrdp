/*
   Copyright (c) 2008-2009 Jay Sorg

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
  struct trans* self;

  self = (struct trans*)g_malloc(sizeof(struct trans), 1);
  make_stream(self->in_s);
  init_stream(self->in_s, in_size);
  make_stream(self->out_s);
  init_stream(self->out_s, out_size);
  self->mode = mode;
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
  g_tcp_close(self->sck);
  if (self->listen_filename != 0)
  {
    g_file_delete(self->listen_filename);
    g_free(self->listen_filename);
  }
  g_free(self);
}

/*****************************************************************************/
int APP_CC
trans_get_wait_objs(struct trans* self, tbus* objs, int* count, int* timeout)
{
  if (self == 0)
  {
    return 1;
  }
  if (self->status != 1)
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
  tbus in_sck;
  struct trans* in_trans;
  int read_bytes;
  int to_read;
  int read_so_far;
  int rv;

  if (self == 0)
  {
    return 1;
  }
  if (self->status != 1)
  {
    return 1;
  }
  rv = 0;
  if (self->type1 == 1) /* listening */
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
          self->status = 0;
          rv = 1;
        }
      }
      if (in_sck != -1)
      {
        if (self->trans_conn_in != 0) /* is function assigned */
        {
          in_trans = trans_create(self->mode, self->in_s->size,
                                  self->out_s->size);
          in_trans->sck = in_sck;
          in_trans->type1 = 2;
          in_trans->status = 1;
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
          self->status = 0;
          rv = 1;
        }
      }
      else if (read_bytes == 0)
      {
        /* error */
        self->status = 0;
        rv = 1;
      }
      else
      {
        self->in_s->end += read_bytes;
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
trans_force_read(struct trans* self, int size)
{
  int rv;
  int rcvd;

  if (self->status != 1)
  {
    return 1;
  }
  rv = 0;
  while (size > 0)
  {
    rcvd = g_tcp_recv(self->sck, self->in_s->end, size, 0);
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
        self->status = 0;
        rv = 1;
      }
    }
    else if (rcvd == 0)
    {
      /* error */
      self->status = 0;
      rv = 1;
    }
    else
    {
      self->in_s->end += rcvd;
      size -= rcvd;
    }
  }
  return rv;
}

/*****************************************************************************/
int APP_CC
trans_force_write(struct trans* self)
{
  int size;
  int total;
  int rv;
  int sent;

  if (self->status != 1)
  {
    return 1;
  }
  rv = 0;
  size = (int)(self->out_s->end - self->out_s->data);
  total = 0;
  while (total < size)
  {
    sent = g_tcp_send(self->sck, self->out_s->data + total, size - total, 0);
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
        self->status = 0;
        rv = 1;
      }
    }
    else if (sent == 0)
    {
      /* error */
      self->status = 0;
      rv = 1;
    }
    else
    {
      total = total + sent;
    }
  }
  return rv;
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
  if (self->mode == 1) /* tcp */
  {
    self->sck = g_tcp_socket();
    g_tcp_set_non_blocking(self->sck);
    error = g_tcp_connect(self->sck, server, port);
  }
  else if (self->mode == 2) /* unix socket */
  {
    self->sck = g_tcp_local_socket();
    g_tcp_set_non_blocking(self->sck);
    error = g_tcp_local_connect(self->sck, port);
  }
  else
  {
    self->status = 0;
    return 1;
  }
  if (error == -1)
  {
    if (g_tcp_last_error_would_block(self->sck))
    {
      if (g_tcp_can_send(self->sck, timeout))
      {
        self->status = 1; /* ok */
        self->type1 = 3; /* client */
        return 0;
      }
    }
    return 1;
  }
  self->status = 1; /* ok */
  self->type1 = 3; /* client */
  return 0;
}

/*****************************************************************************/
int APP_CC
trans_listen(struct trans* self, char* port)
{
  if (self->sck != 0)
  {
    g_tcp_close(self->sck);
  }
  if (self->mode == 1) /* tcp */
  {
    self->sck = g_tcp_socket();
    g_tcp_set_non_blocking(self->sck);
    if (g_tcp_bind(self->sck, port) == 0)
    {
      if (g_tcp_listen(self->sck) == 0)
      {
        self->status = 1; /* ok */
        self->type1 = 1; /* listener */
        return 0;
      }
    }
  }
  else if (self->mode == 2) /* unix socket */
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
        self->status = 1; /* ok */
        self->type1 = 1; /* listener */
        return 0;
      }
    }
  }
  return 1;
}

/*****************************************************************************/
struct stream* APP_CC
trans_get_in_s(struct trans* self)
{
  return self->in_s;
}

/*****************************************************************************/
struct stream* APP_CC
trans_get_out_s(struct trans* self, int size)
{
  init_stream(self->out_s, size);
  return self->out_s;
}
