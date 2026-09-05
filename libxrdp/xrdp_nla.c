/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * CredSSP server support for Network Level Authentication.
 *
 * Licensed under the Apache License, Version 2.0.
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <limits.h>
#include <stdint.h>

#include <gssapi/gssapi.h>
#include <openssl/asn1.h>
#include <openssl/asn1t.h>
#include <openssl/crypto.h>
#include <openssl/sha.h>

#include "libxrdp.h"
#include "string_calls.h"
#include "xrdp_nla.h"

#define XRDP_NLA_VERSION 6
#define XRDP_NLA_MIN_VERSION 5
#define XRDP_NLA_NONCE_LENGTH 32
#define XRDP_NLA_HASH_LENGTH SHA256_DIGEST_LENGTH
#define XRDP_NLA_MAX_REQUEST_SIZE (64 * 1024)
#define XRDP_NLA_MAX_TOKEN_ROUNDS 16
#define XRDP_NLA_STATUS_LOGON_FAILURE ((int32_t)0xc000006d)
#define XRDP_NLA_RFC4121_HEADER_LENGTH 16
#define XRDP_NLA_RFC4121_CONFOUNDER_LENGTH 16

#if OPENSSL_VERSION_NUMBER < 0x10100000L
#define ASN1_STRING_get0_data ASN1_STRING_data
#endif

/*****************************************************************************/
int
xrdp_nla_is_enabled(const struct xrdp_client_info *client_info)
{
    return client_info != NULL &&
           (client_info->security_layer == SECURITY_LAYER_NLA ||
            (client_info->security_layer == SECURITY_LAYER_NEGOTIATE &&
             client_info->enable_nla));
}

typedef struct xrdp_nla_nego_token_st
{
    ASN1_OCTET_STRING *nego_token;
} XRDP_NLA_NEGO_TOKEN;

DECLARE_ASN1_FUNCTIONS(XRDP_NLA_NEGO_TOKEN)
DEFINE_STACK_OF(XRDP_NLA_NEGO_TOKEN)

ASN1_SEQUENCE(XRDP_NLA_NEGO_TOKEN) =
{
    ASN1_EXP(XRDP_NLA_NEGO_TOKEN, nego_token, ASN1_OCTET_STRING, 0)
}
ASN1_SEQUENCE_END(XRDP_NLA_NEGO_TOKEN)

IMPLEMENT_ASN1_FUNCTIONS(XRDP_NLA_NEGO_TOKEN)

typedef struct xrdp_nla_ts_request_st
{
    ASN1_INTEGER *version;
    STACK_OF(XRDP_NLA_NEGO_TOKEN) *nego_tokens;
    ASN1_OCTET_STRING *auth_info;
    ASN1_OCTET_STRING *pub_key_auth;
    ASN1_INTEGER *error_code;
    ASN1_OCTET_STRING *client_nonce;
} XRDP_NLA_TS_REQUEST;

DECLARE_ASN1_FUNCTIONS(XRDP_NLA_TS_REQUEST)

ASN1_SEQUENCE(XRDP_NLA_TS_REQUEST) =
{
    ASN1_EXP(XRDP_NLA_TS_REQUEST, version, ASN1_INTEGER, 0),
    ASN1_EXP_SEQUENCE_OF_OPT(XRDP_NLA_TS_REQUEST, nego_tokens,
    XRDP_NLA_NEGO_TOKEN, 1),
    ASN1_EXP_OPT(XRDP_NLA_TS_REQUEST, auth_info, ASN1_OCTET_STRING, 2),
    ASN1_EXP_OPT(XRDP_NLA_TS_REQUEST, pub_key_auth, ASN1_OCTET_STRING, 3),
    ASN1_EXP_OPT(XRDP_NLA_TS_REQUEST, error_code, ASN1_INTEGER, 4),
    ASN1_EXP_OPT(XRDP_NLA_TS_REQUEST, client_nonce, ASN1_OCTET_STRING, 5)
}
ASN1_SEQUENCE_END(XRDP_NLA_TS_REQUEST)

IMPLEMENT_ASN1_FUNCTIONS(XRDP_NLA_TS_REQUEST)

typedef struct xrdp_nla_ts_credentials_st
{
    ASN1_INTEGER *cred_type;
    ASN1_OCTET_STRING *credentials;
} XRDP_NLA_TS_CREDENTIALS;

DECLARE_ASN1_FUNCTIONS(XRDP_NLA_TS_CREDENTIALS)

ASN1_SEQUENCE(XRDP_NLA_TS_CREDENTIALS) =
{
    ASN1_EXP(XRDP_NLA_TS_CREDENTIALS, cred_type, ASN1_INTEGER, 0),
    ASN1_EXP(XRDP_NLA_TS_CREDENTIALS, credentials, ASN1_OCTET_STRING, 1)
}
ASN1_SEQUENCE_END(XRDP_NLA_TS_CREDENTIALS)

IMPLEMENT_ASN1_FUNCTIONS(XRDP_NLA_TS_CREDENTIALS)

typedef struct xrdp_nla_ts_password_creds_st
{
    ASN1_OCTET_STRING *domain_name;
    ASN1_OCTET_STRING *user_name;
    ASN1_OCTET_STRING *password;
} XRDP_NLA_TS_PASSWORD_CREDS;

DECLARE_ASN1_FUNCTIONS(XRDP_NLA_TS_PASSWORD_CREDS)

ASN1_SEQUENCE(XRDP_NLA_TS_PASSWORD_CREDS) =
{
    ASN1_EXP(XRDP_NLA_TS_PASSWORD_CREDS, domain_name, ASN1_OCTET_STRING, 0),
    ASN1_EXP(XRDP_NLA_TS_PASSWORD_CREDS, user_name, ASN1_OCTET_STRING, 1),
    ASN1_EXP(XRDP_NLA_TS_PASSWORD_CREDS, password, ASN1_OCTET_STRING, 2)
}
ASN1_SEQUENCE_END(XRDP_NLA_TS_PASSWORD_CREDS)

IMPLEMENT_ASN1_FUNCTIONS(XRDP_NLA_TS_PASSWORD_CREDS)

static const unsigned char CLIENT_TO_SERVER_MAGIC[] =
    "CredSSP Client-To-Server Binding Hash";
static const unsigned char SERVER_TO_CLIENT_MAGIC[] =
    "CredSSP Server-To-Client Binding Hash";

/*****************************************************************************/
static int
nla_constant_time_equal(const unsigned char *left, const unsigned char *right,
                        unsigned int length)
{
    unsigned int index;
    unsigned char different = 0;

    for (index = 0; index < length; ++index)
    {
        different |= left[index] ^ right[index];
    }

    return different == 0;
}

/*****************************************************************************/
static int
nla_request_is_canonical(const XRDP_NLA_TS_REQUEST *request,
                         const unsigned char *data, int length)
{
    unsigned char *encoded;
    unsigned char *p;
    int encoded_length;
    int result;

    encoded_length = i2d_XRDP_NLA_TS_REQUEST(request, NULL);
    if (encoded_length != length)
    {
        return 0;
    }

    encoded = (unsigned char *)g_malloc(encoded_length, 0);
    if (encoded == NULL)
    {
        return 0;
    }
    p = encoded;
    result = i2d_XRDP_NLA_TS_REQUEST(request, &p) == encoded_length &&
             g_memcmp(encoded, data, length) == 0;
    g_free(encoded);
    return result;
}

/*****************************************************************************/
static XRDP_NLA_TS_REQUEST *
nla_receive_request(struct trans *trans)
{
    struct stream *s;
    XRDP_NLA_TS_REQUEST *request;
    const unsigned char *p;
    unsigned int content_length;
    unsigned int length_bytes;
    unsigned int total_length;
    unsigned int index;

    s = trans_get_in_s(trans);
    init_stream(s, XRDP_NLA_MAX_REQUEST_SIZE);
    if (trans_force_read(trans, 2) != 0 ||
            (unsigned char)s->data[0] != 0x30)
    {
        LOG(LOG_LEVEL_ERROR, "NLA: invalid TSRequest header");
        return NULL;
    }

    content_length = (unsigned char)s->data[1];
    if ((content_length & 0x80) == 0)
    {
        length_bytes = 0;
    }
    else
    {
        length_bytes = content_length & 0x7f;
        if (length_bytes == 0 || length_bytes > 3 ||
                trans_force_read(trans, length_bytes) != 0 ||
                (unsigned char)s->data[2] == 0)
        {
            LOG(LOG_LEVEL_ERROR, "NLA: invalid TSRequest length");
            return NULL;
        }

        content_length = 0;
        for (index = 0; index < length_bytes; ++index)
        {
            content_length = (content_length << 8) |
                             (unsigned char)s->data[2 + index];
        }
        if (content_length < 128)
        {
            LOG(LOG_LEVEL_ERROR, "NLA: non-canonical TSRequest length");
            return NULL;
        }
    }

    total_length = 2 + length_bytes + content_length;
    if (total_length > XRDP_NLA_MAX_REQUEST_SIZE ||
            trans_force_read(trans, content_length) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "NLA: TSRequest is too large or incomplete");
        return NULL;
    }

    p = (const unsigned char *)s->data;
    request = d2i_XRDP_NLA_TS_REQUEST(NULL, &p, total_length);
    if (request == NULL || p != (unsigned char *)s->data + total_length ||
            !nla_request_is_canonical(request,
                                      (const unsigned char *)s->data,
                                      total_length))
    {
        XRDP_NLA_TS_REQUEST_free(request);
        LOG(LOG_LEVEL_ERROR, "NLA: malformed TSRequest");
        return NULL;
    }

    return request;
}

/*****************************************************************************/
static int
nla_set_octet_string(ASN1_OCTET_STRING **field,
                     const void *data, unsigned int length)
{
    if (length > INT_MAX)
    {
        return 1;
    }
    *field = ASN1_OCTET_STRING_new();
    return *field == NULL ||
           ASN1_OCTET_STRING_set(*field, data, length) != 1;
}

/*****************************************************************************/
static int
nla_send_request(struct trans *trans, const gss_buffer_desc *nego_token,
                 const gss_buffer_desc *pub_key_auth, int32_t error_code)
{
    XRDP_NLA_TS_REQUEST *request;
    XRDP_NLA_NEGO_TOKEN *token = NULL;
    struct stream *s;
    unsigned char *encoded = NULL;
    unsigned char *p;
    int length;
    int result = 1;

    request = XRDP_NLA_TS_REQUEST_new();
    if (request == NULL || ASN1_INTEGER_set(request->version,
                                            XRDP_NLA_VERSION) != 1)
    {
        goto cleanup;
    }

    if (nego_token != NULL && nego_token->length > 0)
    {
        if (nego_token->length > XRDP_NLA_MAX_REQUEST_SIZE)
        {
            goto cleanup;
        }
        request->nego_tokens = sk_XRDP_NLA_NEGO_TOKEN_new_null();
        token = XRDP_NLA_NEGO_TOKEN_new();
        if (request->nego_tokens == NULL || token == NULL ||
                ASN1_OCTET_STRING_set(token->nego_token, nego_token->value,
                                      nego_token->length) != 1 ||
                sk_XRDP_NLA_NEGO_TOKEN_push(request->nego_tokens, token) != 1)
        {
            goto cleanup;
        }
        token = NULL;
    }

    if (pub_key_auth != NULL && pub_key_auth->length > 0 &&
            nla_set_octet_string(&request->pub_key_auth,
                                 pub_key_auth->value,
                                 pub_key_auth->length) != 0)
    {
        goto cleanup;
    }

    if (error_code != 0)
    {
        request->error_code = ASN1_INTEGER_new();
        if (request->error_code == NULL ||
                ASN1_INTEGER_set(request->error_code, error_code) != 1)
        {
            goto cleanup;
        }
    }

    length = i2d_XRDP_NLA_TS_REQUEST(request, NULL);
    if (length <= 0 || length > XRDP_NLA_MAX_REQUEST_SIZE)
    {
        goto cleanup;
    }
    encoded = (unsigned char *)g_malloc(length, 0);
    p = encoded;
    if (encoded == NULL || i2d_XRDP_NLA_TS_REQUEST(request, &p) != length)
    {
        goto cleanup;
    }

    s = trans_get_out_s(trans, length);
    out_uint8a(s, encoded, length);
    s_mark_end(s);
    result = trans_force_write_s(trans, s);

cleanup:
    XRDP_NLA_NEGO_TOKEN_free(token);
    XRDP_NLA_TS_REQUEST_free(request);
    g_free(encoded);
    return result;
}

/*****************************************************************************/
static void
nla_log_gss_status(const char *operation, OM_uint32 major, OM_uint32 minor)
{
    OM_uint32 display_minor;
    OM_uint32 context = 0;
    gss_buffer_desc message = {0, NULL};

    do
    {
        if (gss_display_status(&display_minor, major, GSS_C_GSS_CODE,
                               GSS_C_NO_OID, &context, &message) == GSS_S_COMPLETE)
        {
            LOG(LOG_LEVEL_ERROR, "NLA: %s: %.*s", operation,
                message.length > INT_MAX ? INT_MAX : (int)message.length,
                (const char *)message.value);
            gss_release_buffer(&display_minor, &message);
        }
    }
    while (context != 0);

    context = 0;
    do
    {
        if (gss_display_status(&display_minor, minor, GSS_C_MECH_CODE,
                               GSS_C_NO_OID, &context, &message) == GSS_S_COMPLETE)
        {
            LOG(LOG_LEVEL_ERROR, "NLA: %s mechanism: %.*s", operation,
                message.length > INT_MAX ? INT_MAX : (int)message.length,
                (const char *)message.value);
            gss_release_buffer(&display_minor, &message);
        }
    }
    while (context != 0);
}

/*****************************************************************************/
static int
nla_update_peer_state(const XRDP_NLA_TS_REQUEST *request, long *peer_version,
                      unsigned char *nonce, int *nonce_set)
{
    long version;
    int nonce_length;
    const unsigned char *nonce_data;

    version = ASN1_INTEGER_get(request->version);
    if (version < XRDP_NLA_MIN_VERSION)
    {
        LOG(LOG_LEVEL_ERROR, "NLA: CredSSP version %ld is not secure", version);
        return 1;
    }
    if (*peer_version != 0 && *peer_version != version)
    {
        LOG(LOG_LEVEL_ERROR, "NLA: client changed CredSSP version");
        return 1;
    }
    *peer_version = version;

    if (request->client_nonce != NULL)
    {
        nonce_length = ASN1_STRING_length(request->client_nonce);
        nonce_data = ASN1_STRING_get0_data(request->client_nonce);
        if (nonce_length != XRDP_NLA_NONCE_LENGTH ||
                (*nonce_set &&
                 !nla_constant_time_equal(nonce, nonce_data, nonce_length)))
        {
            LOG(LOG_LEVEL_ERROR, "NLA: invalid or changed client nonce");
            return 1;
        }
        g_memcpy(nonce, nonce_data, nonce_length);
        *nonce_set = 1;
    }

    return 0;
}

/*****************************************************************************/
static XRDP_NLA_TS_REQUEST *
nla_receive_validated_request(struct trans *trans, long *peer_version,
                              unsigned char *nonce, int *nonce_set)
{
    XRDP_NLA_TS_REQUEST *request = nla_receive_request(trans);

    if (request != NULL &&
            nla_update_peer_state(request, peer_version, nonce,
                                  nonce_set) != 0)
    {
        XRDP_NLA_TS_REQUEST_free(request);
        request = NULL;
    }
    return request;
}

/*****************************************************************************/
static int
nla_make_binding_hash(const unsigned char *magic, unsigned int magic_length,
                      const unsigned char *nonce,
                      const unsigned char *public_key,
                      unsigned int public_key_length,
                      unsigned char *hash)
{
    unsigned char *input;
    unsigned int length = magic_length + XRDP_NLA_NONCE_LENGTH +
                          public_key_length;

    input = (unsigned char *)g_malloc(length, 0);
    if (input == NULL)
    {
        return 1;
    }
    g_memcpy(input, magic, magic_length);
    g_memcpy(input + magic_length, nonce, XRDP_NLA_NONCE_LENGTH);
    g_memcpy(input + magic_length + XRDP_NLA_NONCE_LENGTH,
             public_key, public_key_length);
    SHA256(input, length, hash);
    g_free(input);
    return 0;
}

/*****************************************************************************/
int
xrdp_nla_prepare_wrap_token(unsigned char *token, unsigned int token_length,
                            unsigned int plaintext_length)
{
    unsigned char *body;
    unsigned char *rotated;
    unsigned int body_length;
    unsigned int current_rrc;
    unsigned int required_rrc;
    unsigned int rotation;

    if (token == NULL || token_length < XRDP_NLA_RFC4121_HEADER_LENGTH)
    {
        return 1;
    }

    /* RFC 4121 wrap tokens from GSSAPI have RRC=0. SSPI expects the
       security trailer before the encrypted data. */
    if (token[0] != 0x05 || token[1] != 0x04)
    {
        return 0;
    }
    if ((token[2] & 0x02) == 0 || plaintext_length > token_length ||
            token_length - plaintext_length <
            XRDP_NLA_RFC4121_HEADER_LENGTH +
            XRDP_NLA_RFC4121_CONFOUNDER_LENGTH)
    {
        return 1;
    }

    body = token + XRDP_NLA_RFC4121_HEADER_LENGTH;
    body_length = token_length - XRDP_NLA_RFC4121_HEADER_LENGTH;
    current_rrc = ((unsigned int)token[6] << 8) | token[7];
    required_rrc = token_length - plaintext_length -
                   XRDP_NLA_RFC4121_HEADER_LENGTH -
                   XRDP_NLA_RFC4121_CONFOUNDER_LENGTH;
    if (required_rrc > UINT16_MAX)
    {
        return 1;
    }
    rotation = (required_rrc + body_length -
                (current_rrc % body_length)) % body_length;

    if (rotation != 0)
    {
        rotated = (unsigned char *)g_malloc(body_length, 0);
        if (rotated == NULL)
        {
            return 1;
        }
        g_memcpy(rotated, body + body_length - rotation, rotation);
        g_memcpy(rotated + rotation, body, body_length - rotation);
        g_memcpy(body, rotated, body_length);
        g_free(rotated);
    }
    token[6] = required_rrc >> 8;
    token[7] = required_rrc;
    return 0;
}

/*****************************************************************************/
static int
nla_utf16_to_utf8(const ASN1_OCTET_STRING *source, char *destination,
                  unsigned int destination_length)
{
    struct stream s = {0};
    const unsigned char *data = ASN1_STRING_get0_data(source);
    int length = ASN1_STRING_length(source);
    int index;
    unsigned int required;

    if (length < 0 || (length & 1) != 0)
    {
        return 1;
    }
    for (index = 0; index < length; index += 2)
    {
        if (data[index] == 0 && data[index + 1] == 0)
        {
            return 1;
        }
    }

    s.data = (char *)data;
    s.p = s.data;
    s.end = s.data + length;
    s.size = length;
    required = in_utf16_le_fixed_as_utf8(&s, length / 2,
                                         destination,
                                         destination_length);
    return required > destination_length;
}

/*****************************************************************************/
int
xrdp_nla_decode_ts_credentials(const unsigned char *data, unsigned int length,
                               struct xrdp_client_info *client_info)
{
    XRDP_NLA_TS_CREDENTIALS *credentials = NULL;
    XRDP_NLA_TS_PASSWORD_CREDS *password_credentials = NULL;
    const unsigned char *p;
    const unsigned char *password_data;
    unsigned char *encoded = NULL;
    unsigned char *encoded_p;
    char domain[INFO_CLIENT_MAX_CB_LEN];
    char username[INFO_CLIENT_MAX_CB_LEN];
    char password[INFO_CLIENT_MAX_CB_LEN];
    int encoded_length;
    int password_length;
    int result = 1;

    if (data == NULL || client_info == NULL || length == 0 || length > INT_MAX)
    {
        return 1;
    }

    p = data;
    credentials = d2i_XRDP_NLA_TS_CREDENTIALS(NULL, &p, length);
    if (credentials == NULL || p != data + length ||
            ASN1_INTEGER_get(credentials->cred_type) != 1)
    {
        LOG(LOG_LEVEL_ERROR, "NLA: unsupported or malformed TSCredentials");
        goto cleanup;
    }

    encoded_length = i2d_XRDP_NLA_TS_CREDENTIALS(credentials, NULL);
    if (encoded_length <= 0)
    {
        goto cleanup;
    }
    encoded = (unsigned char *)g_malloc(encoded_length, 0);
    encoded_p = encoded;
    if (encoded == NULL || encoded_length != (int)length ||
            i2d_XRDP_NLA_TS_CREDENTIALS(credentials, &encoded_p) !=
            encoded_length || g_memcmp(encoded, data, length) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "NLA: non-canonical TSCredentials");
        goto cleanup;
    }
    OPENSSL_cleanse(encoded, encoded_length);
    g_free(encoded);
    encoded = NULL;

    password_data = ASN1_STRING_get0_data(credentials->credentials);
    password_length = ASN1_STRING_length(credentials->credentials);
    p = password_data;
    password_credentials = d2i_XRDP_NLA_TS_PASSWORD_CREDS(NULL, &p,
                           password_length);
    if (password_credentials == NULL || p != password_data + password_length)
    {
        LOG(LOG_LEVEL_ERROR, "NLA: malformed TSPasswordCreds");
        goto cleanup;
    }

    encoded_length = i2d_XRDP_NLA_TS_PASSWORD_CREDS(password_credentials,
                     NULL);
    if (encoded_length <= 0)
    {
        goto cleanup;
    }
    encoded = (unsigned char *)g_malloc(encoded_length, 0);
    encoded_p = encoded;
    if (encoded == NULL || encoded_length != password_length ||
            i2d_XRDP_NLA_TS_PASSWORD_CREDS(password_credentials,
                                           &encoded_p) != encoded_length ||
            g_memcmp(encoded, password_data, password_length) != 0 ||
            nla_utf16_to_utf8(password_credentials->domain_name,
                              domain, sizeof(domain)) != 0 ||
            nla_utf16_to_utf8(password_credentials->user_name,
                              username, sizeof(username)) != 0 ||
            nla_utf16_to_utf8(password_credentials->password,
                              password, sizeof(password)) != 0 ||
            username[0] == '\0')
    {
        LOG(LOG_LEVEL_ERROR, "NLA: invalid TSPasswordCreds");
        goto cleanup;
    }

    g_strncpy(client_info->domain, domain, sizeof(client_info->domain) - 1);
    g_strncpy(client_info->username, username,
              sizeof(client_info->username) - 1);
    g_strncpy(client_info->password, password,
              sizeof(client_info->password) - 1);
    client_info->rdp_autologin = 1;
    result = 0;

cleanup:
    if (credentials != NULL && credentials->credentials != NULL)
    {
        OPENSSL_cleanse((void *)ASN1_STRING_get0_data(credentials->credentials),
                        ASN1_STRING_length(credentials->credentials));
    }
    if (password_credentials != NULL && password_credentials->password != NULL)
    {
        OPENSSL_cleanse((void *)ASN1_STRING_get0_data(password_credentials->password),
                        ASN1_STRING_length(password_credentials->password));
    }
    OPENSSL_cleanse(password, sizeof(password));
    XRDP_NLA_TS_PASSWORD_CREDS_free(password_credentials);
    XRDP_NLA_TS_CREDENTIALS_free(credentials);
    if (encoded != NULL)
    {
        OPENSSL_cleanse(encoded, encoded_length);
        g_free(encoded);
    }
    return result;
}

/*****************************************************************************/
int
xrdp_nla_accept(struct trans *trans, struct xrdp_client_info *client_info)
{
    XRDP_NLA_TS_REQUEST *request = NULL;
    XRDP_NLA_NEGO_TOKEN *nego_token;
    gss_ctx_id_t context = GSS_C_NO_CONTEXT;
    gss_name_t client_name = GSS_C_NO_NAME;
    gss_buffer_desc input = {0, NULL};
    gss_buffer_desc output = {0, NULL};
    gss_buffer_desc wrapped = {0, NULL};
    gss_buffer_desc unwrapped = {0, NULL};
    unsigned char nonce[XRDP_NLA_NONCE_LENGTH];
    unsigned char expected_hash[XRDP_NLA_HASH_LENGTH];
    unsigned char server_hash[XRDP_NLA_HASH_LENGTH];
    unsigned char *public_key = NULL;
    unsigned int public_key_length = 0;
    OM_uint32 major = GSS_S_FAILURE;
    OM_uint32 minor = 0;
    OM_uint32 cleanup_minor;
    OM_uint32 context_flags = 0;
    long peer_version = 0;
    int nonce_set = 0;
    int conf_state;
    gss_qop_t qop_state;
    int round;
    int result = 1;

    g_memset(nonce, 0, sizeof(nonce));
    if (trans == NULL || client_info == NULL || trans->tls == NULL ||
            ssl_tls_get_public_key(trans->tls, &public_key,
                                   &public_key_length) != 0)
    {
        return 1;
    }

    for (round = 0; round < XRDP_NLA_MAX_TOKEN_ROUNDS; ++round)
    {
        request = nla_receive_validated_request(trans, &peer_version,
                                                nonce, &nonce_set);
        if (request == NULL || request->nego_tokens == NULL ||
                sk_XRDP_NLA_NEGO_TOKEN_num(request->nego_tokens) != 1)
        {
            LOG(LOG_LEVEL_ERROR, "NLA: expected one SPNEGO token");
            goto cleanup;
        }

        nego_token = sk_XRDP_NLA_NEGO_TOKEN_value(request->nego_tokens, 0);
        input.value = (void *)ASN1_STRING_get0_data(nego_token->nego_token);
        input.length = ASN1_STRING_length(nego_token->nego_token);
        if (input.length == 0 || input.length > XRDP_NLA_MAX_REQUEST_SIZE)
        {
            LOG(LOG_LEVEL_ERROR, "NLA: invalid SPNEGO token length");
            goto cleanup;
        }
        major = gss_accept_sec_context(&minor, &context, GSS_C_NO_CREDENTIAL,
                                       &input, GSS_C_NO_CHANNEL_BINDINGS,
                                       &client_name, NULL, &output,
                                       &context_flags, NULL, NULL);
        if (GSS_ERROR(major))
        {
            nla_log_gss_status("authentication failed", major, minor);
            if (peer_version >= 6)
            {
                nla_send_request(trans, NULL, NULL,
                                 XRDP_NLA_STATUS_LOGON_FAILURE);
            }
            goto cleanup;
        }

        if ((major & GSS_S_CONTINUE_NEEDED) != 0)
        {
            if (nla_send_request(trans, &output, NULL, 0) != 0)
            {
                goto cleanup;
            }
            gss_release_buffer(&cleanup_minor, &output);
            output.value = NULL;
            output.length = 0;
            XRDP_NLA_TS_REQUEST_free(request);
            request = NULL;
            continue;
        }

        if (major != GSS_S_COMPLETE)
        {
            LOG(LOG_LEVEL_ERROR, "NLA: unexpected GSSAPI status");
            goto cleanup;
        }

        if (output.length > 0 || request->pub_key_auth == NULL)
        {
            if (nla_send_request(trans, &output, NULL, 0) != 0)
            {
                goto cleanup;
            }
            gss_release_buffer(&cleanup_minor, &output);
            output.value = NULL;
            output.length = 0;
            XRDP_NLA_TS_REQUEST_free(request);
            request = nla_receive_validated_request(trans, &peer_version,
                                                    nonce, &nonce_set);
            if (request == NULL)
            {
                goto cleanup;
            }
        }
        break;
    }

    if (major != GSS_S_COMPLETE || round == XRDP_NLA_MAX_TOKEN_ROUNDS ||
            request == NULL || request->pub_key_auth == NULL || !nonce_set ||
            (context_flags & (GSS_C_CONF_FLAG | GSS_C_INTEG_FLAG)) !=
            (GSS_C_CONF_FLAG | GSS_C_INTEG_FLAG))
    {
        LOG(LOG_LEVEL_ERROR, "NLA: incomplete or insufficiently protected authentication");
        goto cleanup;
    }

    input.value = (void *)ASN1_STRING_get0_data(request->pub_key_auth);
    input.length = ASN1_STRING_length(request->pub_key_auth);
    major = gss_unwrap(&minor, context, &input, &unwrapped,
                       &conf_state, &qop_state);
    if (GSS_ERROR(major) || !conf_state ||
            unwrapped.length != XRDP_NLA_HASH_LENGTH ||
            nla_make_binding_hash(CLIENT_TO_SERVER_MAGIC,
                                  sizeof(CLIENT_TO_SERVER_MAGIC), nonce,
                                  public_key, public_key_length,
                                  expected_hash) != 0 ||
            !nla_constant_time_equal(expected_hash, unwrapped.value,
                                     XRDP_NLA_HASH_LENGTH))
    {
        if (GSS_ERROR(major))
        {
            nla_log_gss_status("public-key binding failed", major, minor);
        }
        else
        {
            LOG(LOG_LEVEL_ERROR, "NLA: public-key binding did not match");
        }
        goto cleanup;
    }
    gss_release_buffer(&cleanup_minor, &unwrapped);
    unwrapped.value = NULL;
    unwrapped.length = 0;

    if (nla_make_binding_hash(SERVER_TO_CLIENT_MAGIC,
                              sizeof(SERVER_TO_CLIENT_MAGIC), nonce,
                              public_key, public_key_length, server_hash) != 0)
    {
        goto cleanup;
    }
    input.value = server_hash;
    input.length = sizeof(server_hash);
    major = gss_wrap(&minor, context, 1, GSS_C_QOP_DEFAULT,
                     &input, &conf_state, &wrapped);
    if (GSS_ERROR(major) || !conf_state)
    {
        if (GSS_ERROR(major))
        {
            nla_log_gss_status("public-key response failed", major, minor);
        }
        goto cleanup;
    }
    if (wrapped.length > UINT_MAX ||
            xrdp_nla_prepare_wrap_token(wrapped.value, wrapped.length,
                                        input.length) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "NLA: invalid GSSAPI wrap token");
        goto cleanup;
    }
    if (nla_send_request(trans, NULL, &wrapped, 0) != 0)
    {
        goto cleanup;
    }
    gss_release_buffer(&cleanup_minor, &wrapped);
    wrapped.value = NULL;
    wrapped.length = 0;
    XRDP_NLA_TS_REQUEST_free(request);
    request = nla_receive_validated_request(trans, &peer_version,
                                            nonce, &nonce_set);
    if (request == NULL || request->auth_info == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "NLA: client did not delegate credentials");
        goto cleanup;
    }

    input.value = (void *)ASN1_STRING_get0_data(request->auth_info);
    input.length = ASN1_STRING_length(request->auth_info);
    major = gss_unwrap(&minor, context, &input, &unwrapped,
                       &conf_state, &qop_state);
    if (GSS_ERROR(major) || !conf_state || unwrapped.length > UINT_MAX ||
            xrdp_nla_decode_ts_credentials(unwrapped.value,
                                           unwrapped.length,
                                           client_info) != 0)
    {
        if (GSS_ERROR(major))
        {
            nla_log_gss_status("credential decryption failed", major, minor);
        }
        goto cleanup;
    }

    LOG(LOG_LEVEL_INFO, "NLA authentication completed for %s",
        client_info->username);
    result = 0;

cleanup:
    if (unwrapped.value != NULL)
    {
        OPENSSL_cleanse(unwrapped.value, unwrapped.length);
        gss_release_buffer(&cleanup_minor, &unwrapped);
    }
    if (wrapped.value != NULL)
    {
        gss_release_buffer(&cleanup_minor, &wrapped);
    }
    if (output.value != NULL)
    {
        gss_release_buffer(&cleanup_minor, &output);
    }
    if (context != GSS_C_NO_CONTEXT)
    {
        gss_delete_sec_context(&cleanup_minor, &context, GSS_C_NO_BUFFER);
    }
    if (client_name != GSS_C_NO_NAME)
    {
        gss_release_name(&cleanup_minor, &client_name);
    }
    XRDP_NLA_TS_REQUEST_free(request);
    OPENSSL_cleanse(expected_hash, sizeof(expected_hash));
    OPENSSL_cleanse(server_hash, sizeof(server_hash));
    g_free(public_key);
    return result;
}
