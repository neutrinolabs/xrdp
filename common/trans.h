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

#if !defined(TRANS_H)
#define TRANS_H

#include "arch.h"
#include "parse.h"

#define TRANS_MODE_TCP 1
#define TRANS_MODE_UNIX 2
#define TRANS_MODE_VSOCK 3

#define TRANS_TYPE_LISTENER 1
#define TRANS_TYPE_SERVER 2
#define TRANS_TYPE_CLIENT 3

#define TRANS_STATUS_DOWN 0
#define TRANS_STATUS_UP 1

struct trans; /* forward declaration */
struct xrdp_tls;

typedef int (*ttrans_data_in)(struct trans* self);
typedef int (*ttrans_conn_in)(struct trans* self,
                                         struct trans* new_self);
typedef int (*tis_term)(void);
typedef int (*trans_recv_proc) (struct trans *self, char *ptr, int len);
typedef int (*trans_send_proc) (struct trans *self, const char *data, int len);
typedef int (*trans_can_recv_proc) (struct trans *self, int sck, int millis);

/* optional source info */

#define XRDP_SOURCE_NONE    0
#define XRDP_SOURCE_CLIENT  1
#define XRDP_SOURCE_SESMAN  2
#define XRDP_SOURCE_CHANSRV 3
#define XRDP_SOURCE_MOD     4

struct source_info
{
    int cur_source;
    int source[7];
};

struct trans
{
    tbus sck; /* socket handle */
    int mode; /* 1 tcp, 2 unix socket, 3 vsock */
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
    struct stream* wait_s;
    char addr[256];
    char port[256];
    int no_stream_init_on_data_in;
    int extra_flags; /* user defined */
    struct ssl_tls *tls;
    const char *ssl_protocol; /* e.g. TLSv1, TLSv1.1, TLSv1.2, unknown */
    const char *cipher_name;  /* e.g. AES256-GCM-SHA384 */
    trans_recv_proc trans_recv;
    trans_send_proc trans_send;
    trans_can_recv_proc trans_can_recv;
    struct source_info *si;
    int my_source;
};

struct trans*
trans_create(int mode, int in_size, int out_size);
void
trans_delete(struct trans* self);
int
trans_get_wait_objs(struct trans* self, tbus* objs, int* count);
int
trans_get_wait_objs_rw(struct trans *self,
                       tbus *robjs, int *rcount,
                       tbus *wobjs, int *wcount, int *timeout);
int
trans_check_wait_objs(struct trans* self);
int
trans_force_read_s(struct trans* self, struct stream* in_s, int size);
int
trans_force_write_s(struct trans* self, struct stream* out_s);
int
trans_force_read(struct trans* self, int size);
int
trans_force_write(struct trans* self);
int
trans_write_copy(struct trans* self);
int
trans_write_copy_s(struct trans* self, struct stream* out_s);
int
trans_connect(struct trans* self, const char* server, const char* port,
              int timeout);
int
trans_listen_address(struct trans* self, char* port, const char* address);
int
trans_listen(struct trans* self, char* port);
struct stream*
trans_get_in_s(struct trans* self);
struct stream*
trans_get_out_s(struct trans* self, int size);
int
trans_set_tls_mode(struct trans *self, const char *key, const char *cert,
                   long ssl_protocols, const char *tls_ciphers);
int
trans_shutdown_tls_mode(struct trans *self);
int
trans_tcp_force_read_s(struct trans *self, struct stream *in_s, int size);

#endif
