/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   Copyright (C) Jay Sorg 2004

   main include file

*/

#include "parse.h"
#include "xrdp_types.h"
#include "constants.h"

#ifdef XRDP_DEBUG
#define DEBUG(args) g_printf args;
#else
#define DEBUG(args)
#endif

/* os_calls.c */
void g_printf(char *format, ...);
void g_hexdump(char* p, int len);
void* g_malloc(int size, int zero);
void g_free(void* ptr);
void g_memset(void* ptr, int val, int size);
void g_memcpy(void* d_ptr, const void* s_ptr, int size);
int g_getchar(void);
int g_tcp_socket(void);
void g_tcp_close(int sck);
int g_tcp_set_non_blocking(int sck);
int g_tcp_bind(int sck, char* port);
int g_tcp_listen(int sck);
int g_tcp_accept(int sck);
int g_tcp_recv(int sck, void* ptr, int len, int flags);
int g_tcp_send(int sck, void* ptr, int len, int flags);
int g_tcp_last_error_would_block(int sck);
int g_tcp_select(int sck);
int g_is_term(void);
void g_set_term(int in_val);
void g_sleep(int msecs);
int g_thread_create(void* (*start_routine)(void*), void* arg);
void* g_rc4_info_create(void);
void g_rc4_info_delete(void* rc4_info);
void g_rc4_set_key(void* rc4_info, char* key, int len);
void g_rc4_crypt(void* rc4_info, char* data, int len);
void* g_sha1_info_create(void);
void g_sha1_info_delete(void* sha1_info);
void g_sha1_clear(void* sha1_info);
void g_sha1_transform(void* sha1_info, char* data, int len);
void g_sha1_complete(void* sha1_info, char* data);
void* g_md5_info_create(void);
void g_md5_info_delete(void* md5_info);
void g_md5_clear(void* md5_info);
void g_md5_transform(void* md5_info, char* data, int len);
void g_md5_complete(void* md5_info, char* data);
int g_mod_exp(char* out, char* in, char* mod, char* exp);
void g_random(char* data, int len);

/* xrdp_tcp.c */
struct xrdp_tcp* xrdp_tcp_create(struct xrdp_iso* owner);
void xrdp_tcp_delete(struct xrdp_tcp* self);
int xrdp_tcp_init(struct xrdp_tcp* self, int len);
int xrdp_tcp_recv(struct xrdp_tcp* self, int len);
int xrdp_tcp_send(struct xrdp_tcp* self);

/* xrdp_iso.c */
struct xrdp_iso* xrdp_iso_create(struct xrdp_mcs* owner);
void xrdp_iso_delete(struct xrdp_iso* self);
int xrdp_iso_init(struct xrdp_iso* self, int len);
int xrdp_iso_recv(struct xrdp_iso* self);
int xrdp_iso_send(struct xrdp_iso* self);
int xrdp_iso_incoming(struct xrdp_iso* self);

/* xrdp_mcs.c */
struct xrdp_mcs* xrdp_mcs_create(struct xrdp_sec* owner);
void xrdp_mcs_delete(struct xrdp_mcs* self);
int xrdp_mcs_init(struct xrdp_mcs* self, int len);
int xrdp_mcs_recv(struct xrdp_mcs* self);
int xrdp_mcs_send(struct xrdp_mcs* self);
int xrdp_mcs_incoming(struct xrdp_mcs* self);

/* xrdp_sec.c */
struct xrdp_sec* xrdp_sec_create(struct xrdp_rdp* owner);
void xrdp_sec_delete(struct xrdp_sec* self);
int xrdp_sec_init(struct xrdp_sec* self, int len);
int xrdp_sec_recv(struct xrdp_sec* self);
int xrdp_sec_send(struct xrdp_sec* self, int flags);
int xrdp_rdp_send_data(struct xrdp_rdp* self, int data_pdu_type);
int xrdp_sec_incoming(struct xrdp_sec* self);

/* xrdp_rdp.c */
struct xrdp_rdp* xrdp_rdp_create(struct xrdp_process* owner);
void xrdp_rdp_delete(struct xrdp_rdp* self);
int xrdp_rdp_init(struct xrdp_rdp* self, int len);
int xrdp_rdp_init_data(struct xrdp_rdp* self, int len);
int xrdp_rdp_recv(struct xrdp_rdp* self, int* code);
int xrdp_rdp_send(struct xrdp_rdp* self, int pdu_type);
int xrdp_rdp_incoming(struct xrdp_rdp* self);
int xrdp_rdp_send_demand_active(struct xrdp_rdp* self);
int xrdp_rdp_process_confirm_active(struct xrdp_rdp* self);
int xrdp_rdp_process_data(struct xrdp_rdp* self);

/* xrdp_process.c */
struct xrdp_process* xrdp_process_create(struct xrdp_listen* owner);
void xrdp_process_delete(struct xrdp_process* self);
int xrdp_process_main_loop(struct xrdp_process* self);

/* xrdp_listen.c */
struct xrdp_listen* xrdp_listen_create(void);
void xrdp_listen_delete(struct xrdp_listen* self);
int xrdp_listen_main_loop(struct xrdp_listen* self);
