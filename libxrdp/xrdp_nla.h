/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Idan Freiberg 2013-2026
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

/**
 * Selects the Kerberos keytab for the GSSAPI acceptor
 *
 * Sets KRB5_KTNAME unless the environment already provides it.
 *
 * @param keytab Absolute path of the keytab file
 * @return 0 if the keytab was accepted
 */
int
xrdp_nla_set_keytab(const char *keytab);

/**
 * Checks the Kerberos keytab against the TLS certificate names
 *
 * A warning is logged for each DNS name in the certificate which has no
 * TERMSRV acceptor key in the keytab.
 *
 * @param cert_file Path of the PEM certificate
 * @return Number of certificate names without an acceptor key
 */
int
xrdp_nla_check_keytab(const char *cert_file);

int
xrdp_nla_decode_ts_credentials(const unsigned char *data, unsigned int length,
                               struct xrdp_client_info *client_info);

int
xrdp_nla_prepare_wrap_token(unsigned char *token, unsigned int token_length,
                            unsigned int plaintext_length);

#endif
