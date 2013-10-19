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

int APP_CC scard_pcsc_get_wait_objs(tbus *objs, int *count, int *timeout);
int APP_CC scard_pcsc_check_wait_objs(void);
int APP_CC scard_pcsc_init(void);
int APP_CC scard_pcsc_deinit(void);
int APP_CC scard_function_establish_context_return(struct trans *con,
                                                   struct stream *in_s,
                                                   int len);
int APP_CC scard_function_release_context_return(struct trans *con,
                                                 struct stream *in_s,
                                                 int len);
int APP_CC scard_function_list_readers_return(struct trans *con,
                                              struct stream *in_s,
                                              int len);

int APP_CC scard_function_transmit_return(struct trans *con,
                                          struct stream *in_s,
                                          int len);

int APP_CC scard_function_control_return(struct trans *con,
                                         struct stream *in_s,
                                         int len);

int APP_CC scard_function_get_status_change_return(struct trans *con,
                                                   struct stream *in_s,
                                                   int len);
int APP_CC scard_function_connect_return(struct trans *con,
                                         struct stream *in_s,
                                         int len);
int APP_CC scard_function_begin_transaction_return(struct trans *con,
                                                   struct stream *in_s,
                                                   int len);

#endif /* end #ifndef _SMARTCARD_PCSC_H */
