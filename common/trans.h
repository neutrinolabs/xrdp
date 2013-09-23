/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2013
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
typedef int (*tis_term)(void);

struct trans
{
  tbus sck; /* socket handle */
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
  tis_term is_term; /* used to test for exit */
  int in_write;
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
trans_write_check(struct trans* self, int timeout);
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
