/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2013 jay.sorg@gmail.com
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
 */

/*
 * smartcard redirection support, PCSC daemon standin
 */

#ifndef _SMARTCARD_PCSC_H
#define _SMARTCARD_PCSC_H

int scard_pcsc_get_wait_objs(tbus *objs, int *count, int *timeout);
int scard_pcsc_check_wait_objs(void);
int scard_pcsc_init(void);
int scard_pcsc_deinit(void);
int scard_function_establish_context_return(void *user_data,
        struct stream *in_s,
        int len, int status);
int scard_function_release_context_return(void *user_data,
        struct stream *in_s,
        int len, int status);
int scard_function_list_readers_return(void *user_data,
                                       struct stream *in_s,
                                       int len, int status);

int scard_function_transmit_return(void *user_data,
                                   struct stream *in_s,
                                   int len, int status);

int scard_function_control_return(void *user_data,
                                  struct stream *in_s,
                                  int len, int status);

int scard_function_get_status_change_return(void *user_data,
        struct stream *in_s,
        int len, int status);

int scard_function_connect_return(void *user_data,
                                  struct stream *in_s,
                                  int len, int status);

int scard_function_status_return(void *user_data,
                                 struct stream *in_s,
                                 int len, int status);

int scard_function_begin_transaction_return(void *user_data,
        struct stream *in_s,
        int len, int status);

int scard_function_end_transaction_return(void *user_data,
        struct stream *in_s,
        int len, int status);

int scard_function_is_context_valid_return(void *user_data,
        struct stream *in_s,
        int len, int status);

int scard_function_reconnect_return(void *user_data,
                                    struct stream *in_s,
                                    int len, int status);

int scard_function_disconnect_return(void *user_data,
                                     struct stream *in_s,
                                     int len, int status);

int scard_function_cancel_return(void *user_data,
                                 struct stream *in_s,
                                 int len, int status);

int scard_function_get_attrib_return(void *user_data,
                                     struct stream *in_s,
                                     int len, int status);

#endif /* end #ifndef _SMARTCARD_PCSC_H */
