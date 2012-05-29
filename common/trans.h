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

#if !defined(TRANS_H)
#define TRANS_H

#include "arch.h"
#include "parse.h"

#define TRANS_MODE_TCP 1
#define TRANS_MODE_UNIX 2

#define TRANS_TYPE_LISTENER 1
#define TRANS_TYPE_SERVER 2
#define TRANS_TYPE_CLIENT 3

#define TRANS_STATUS_DOWN 0
#define TRANS_STATUS_UP 1

struct trans; /* forward declaration */

typedef int (*ttrans_data_in)(struct trans* self);
typedef int (*ttrans_conn_in)(struct trans* self, struct trans* new_self);

struct trans
{
  tbus sck;
  int mode; /* 1 tcp, 2 unix socket */
  int status;
  int type1; /* 1 listener 2 server 3 client */
  ttrans_data_in trans_data_in;
  ttrans_conn_in trans_conn_in;
  void* callback_data;
  int header_size;
  struct stream* in_s;
  struct stream* out_s;
  char* listen_filename;
};

struct trans* APP_CC
trans_create(int mode, int in_size, int out_size);
void APP_CC
trans_delete(struct trans* self);
int APP_CC
trans_get_wait_objs(struct trans* self, tbus* objs, int* count);
int APP_CC
trans_check_wait_objs(struct trans* self);
int APP_CC
trans_force_read_s(struct trans* self, struct stream* in_s, int size);
int APP_CC
trans_force_write_s(struct trans* self, struct stream* out_s);
int APP_CC
trans_force_read(struct trans* self, int size);
int APP_CC
trans_force_write(struct trans* self);
int APP_CC
trans_connect(struct trans* self, const char* server, const char* port,
              int timeout);
int APP_CC
trans_listen_address(struct trans* self, char* port, const char* address);
int APP_CC
trans_listen(struct trans* self, char* port);
struct stream* APP_CC
trans_get_in_s(struct trans* self);
struct stream* APP_CC
trans_get_out_s(struct trans* self, int size);

#endif
