/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Licensed under the Apache License, Version 2.0.
 */

#if !defined(XRDP_NLA_H)
#define XRDP_NLA_H

struct trans;
struct xrdp_client_info;

int
xrdp_nla_accept(struct trans *trans, struct xrdp_client_info *client_info);

int
xrdp_nla_decode_ts_credentials(const unsigned char *data, unsigned int length,
                               struct xrdp_client_info *client_info);

int
xrdp_nla_prepare_wrap_token(unsigned char *token, unsigned int token_length,
                            unsigned int plaintext_length);

#endif
