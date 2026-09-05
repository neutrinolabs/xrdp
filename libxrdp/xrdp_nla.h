/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Idan Freiberg 2013-2014
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
 */

#if !defined(XRDP_NLA_H)
#define XRDP_NLA_H

struct trans;
struct xrdp_client_info;

int
xrdp_nla_is_enabled(const struct xrdp_client_info *client_info);

int
xrdp_nla_accept(struct trans *trans, struct xrdp_client_info *client_info);

int
xrdp_nla_decode_ts_credentials(const unsigned char *data, unsigned int length,
                               struct xrdp_client_info *client_info);

int
xrdp_nla_prepare_wrap_token(unsigned char *token, unsigned int token_length,
                            unsigned int plaintext_length);

#endif
