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
 * secure layer
 */

#include "libxrdp.h"
#include "log.h"

/* some compilers need unsigned char to avoid warnings */
static tui8 g_pad_54[40] =
{
    54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54,
    54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54,
    54, 54, 54, 54, 54, 54, 54, 54
};

/* some compilers need unsigned char to avoid warnings */
static tui8 g_pad_92[48] =
{
    92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92,
    92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92,
    92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92
};

/* some compilers need unsigned char to avoid warnings */
static tui8 g_lic1[322] =
{
    0x80, 0x00, 0x3e, 0x01, 0x01, 0x02, 0x3e, 0x01,
    0x7b, 0x3c, 0x31, 0xa6, 0xae, 0xe8, 0x74, 0xf6,
    0xb4, 0xa5, 0x03, 0x90, 0xe7, 0xc2, 0xc7, 0x39,
    0xba, 0x53, 0x1c, 0x30, 0x54, 0x6e, 0x90, 0x05,
    0xd0, 0x05, 0xce, 0x44, 0x18, 0x91, 0x83, 0x81,
    0x00, 0x00, 0x04, 0x00, 0x2c, 0x00, 0x00, 0x00,
    0x4d, 0x00, 0x69, 0x00, 0x63, 0x00, 0x72, 0x00,
    0x6f, 0x00, 0x73, 0x00, 0x6f, 0x00, 0x66, 0x00,
    0x74, 0x00, 0x20, 0x00, 0x43, 0x00, 0x6f, 0x00,
    0x72, 0x00, 0x70, 0x00, 0x6f, 0x00, 0x72, 0x00,
    0x61, 0x00, 0x74, 0x00, 0x69, 0x00, 0x6f, 0x00,
    0x6e, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
    0x32, 0x00, 0x33, 0x00, 0x36, 0x00, 0x00, 0x00,
    0x0d, 0x00, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x03, 0x00, 0xb8, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x5c, 0x00, 0x52, 0x53, 0x41, 0x31,
    0x48, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
    0x3f, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00,
    0x01, 0xc7, 0xc9, 0xf7, 0x8e, 0x5a, 0x38, 0xe4,
    0x29, 0xc3, 0x00, 0x95, 0x2d, 0xdd, 0x4c, 0x3e,
    0x50, 0x45, 0x0b, 0x0d, 0x9e, 0x2a, 0x5d, 0x18,
    0x63, 0x64, 0xc4, 0x2c, 0xf7, 0x8f, 0x29, 0xd5,
    0x3f, 0xc5, 0x35, 0x22, 0x34, 0xff, 0xad, 0x3a,
    0xe6, 0xe3, 0x95, 0x06, 0xae, 0x55, 0x82, 0xe3,
    0xc8, 0xc7, 0xb4, 0xa8, 0x47, 0xc8, 0x50, 0x71,
    0x74, 0x29, 0x53, 0x89, 0x6d, 0x9c, 0xed, 0x70,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x08, 0x00, 0x48, 0x00, 0xa8, 0xf4, 0x31, 0xb9,
    0xab, 0x4b, 0xe6, 0xb4, 0xf4, 0x39, 0x89, 0xd6,
    0xb1, 0xda, 0xf6, 0x1e, 0xec, 0xb1, 0xf0, 0x54,
    0x3b, 0x5e, 0x3e, 0x6a, 0x71, 0xb4, 0xf7, 0x75,
    0xc8, 0x16, 0x2f, 0x24, 0x00, 0xde, 0xe9, 0x82,
    0x99, 0x5f, 0x33, 0x0b, 0xa9, 0xa6, 0x94, 0xaf,
    0xcb, 0x11, 0xc3, 0xf2, 0xdb, 0x09, 0x42, 0x68,
    0x29, 0x56, 0x58, 0x01, 0x56, 0xdb, 0x59, 0x03,
    0x69, 0xdb, 0x7d, 0x37, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x0e, 0x00, 0x0e, 0x00, 0x6d, 0x69, 0x63, 0x72,
    0x6f, 0x73, 0x6f, 0x66, 0x74, 0x2e, 0x63, 0x6f,
    0x6d, 0x00
};

/* some compilers need unsigned char to avoid warnings */
static tui8 g_lic2[20] =
{
    0x80, 0x00, 0x10, 0x00, 0xff, 0x02, 0x10, 0x00,
    0x07, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
    0x28, 0x14, 0x00, 0x00
};

/* mce */
/* some compilers need unsigned char to avoid warnings */
static tui8 g_lic3[20] =
{
    0x80, 0x02, 0x10, 0x00, 0xff, 0x03, 0x10, 0x00,
    0x07, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
    0xf3, 0x99, 0x00, 0x00
};

/*****************************************************************************/
static void APP_CC
hex_str_to_bin(char *in, char *out, int out_len)
{
    int in_index;
    int in_len;
    int out_index;
    int val;
    char hex[16];

    in_len = g_strlen(in);
    out_index = 0;
    in_index = 0;

    while (in_index <= (in_len - 4))
    {
        if ((in[in_index] == '0') && (in[in_index + 1] == 'x'))
        {
            hex[0] = in[in_index + 2];
            hex[1] = in[in_index + 3];
            hex[2] = 0;

            if (out_index < out_len)
            {
                val = g_htoi(hex);
                out[out_index] = val;
            }

            out_index++;
        }

        in_index++;
    }
}

/*****************************************************************************/
struct xrdp_sec *APP_CC
xrdp_sec_create(struct xrdp_rdp *owner, struct trans *trans, int crypt_level,
                int channel_code, int multimon)
{
    struct xrdp_sec *self;

    DEBUG((" in xrdp_sec_create"));
    self = (struct xrdp_sec *)g_malloc(sizeof(struct xrdp_sec), 1);
    self->rdp_layer = owner;
    self->rc4_key_size = 1; /* 1 = 40 bit, 2 = 128 bit */
    self->crypt_level = 1; /* 1, 2, 3 = low, medium, high */

    switch (crypt_level)
    {
        case 1:
            self->rc4_key_size = 1;
            self->crypt_level = 1;
            break;
        case 2:
            self->rc4_key_size = 1;
            self->crypt_level = 2;
            break;
        case 3:
            self->rc4_key_size = 2;
            self->crypt_level = 3;
            break;
        default:
            g_writeln("Fatal : Illegal crypt_level");
            break ;
    }

    self->channel_code = channel_code;
    self->multimon = multimon;

    if (self->decrypt_rc4_info != NULL)
    {
        g_writeln("xrdp_sec_create - decrypt_rc4_info already created !!!");
    }

    self->decrypt_rc4_info = ssl_rc4_info_create();

    if (self->encrypt_rc4_info != NULL)
    {
        g_writeln("xrdp_sec_create - encrypt_rc4_info already created !!!");
    }

    self->encrypt_rc4_info = ssl_rc4_info_create();
    self->mcs_layer = xrdp_mcs_create(self, trans, &self->client_mcs_data,
                                      &self->server_mcs_data);
    self->fastpath_layer = xrdp_fastpath_create(self, trans);
    self->chan_layer = xrdp_channel_create(self, self->mcs_layer);
    DEBUG((" out xrdp_sec_create"));
    return self;
}

/*****************************************************************************/
void APP_CC
xrdp_sec_delete(struct xrdp_sec *self)
{
    if (self == 0)
    {
        g_writeln("xrdp_sec_delete:  indata is null");
        return;
    }

    xrdp_channel_delete(self->chan_layer);
    xrdp_mcs_delete(self->mcs_layer);
    xrdp_fastpath_delete(self->fastpath_layer);
    ssl_rc4_info_delete(self->decrypt_rc4_info); /* TODO clear all data */
    ssl_rc4_info_delete(self->encrypt_rc4_info); /* TODO clear all data */
    g_free(self->client_mcs_data.data);
    g_free(self->server_mcs_data.data);
    /* Crypto information must always be cleared */
    g_memset(self, 0, sizeof(struct xrdp_sec));
    g_free(self);
}

/*****************************************************************************/
/* returns error */
int APP_CC
xrdp_sec_init(struct xrdp_sec *self, struct stream *s)
{
    if (xrdp_mcs_init(self->mcs_layer, s) != 0)
    {
        return 1;
    }

    if (self->crypt_level > 1)
    {
        s_push_layer(s, sec_hdr, 4 + 8);
    }
    else
    {
        s_push_layer(s, sec_hdr, 4);
    }

    return 0;
}

/*****************************************************************************/
/* Reduce key entropy from 64 to 40 bits */
static void APP_CC
xrdp_sec_make_40bit(char *key)
{
    key[0] = 0xd1;
    key[1] = 0x26;
    key[2] = 0x9e;
}

/*****************************************************************************/
/* returns error */
/* update an encryption key */
static int APP_CC
xrdp_sec_update(char *key, char *update_key, int key_len)
{
    char shasig[20];
    void *sha1_info;
    void *md5_info;
    void *rc4_info;

    sha1_info = ssl_sha1_info_create();
    md5_info = ssl_md5_info_create();
    rc4_info = ssl_rc4_info_create();
    ssl_sha1_clear(sha1_info);
    ssl_sha1_transform(sha1_info, update_key, key_len);
    ssl_sha1_transform(sha1_info, (char *)g_pad_54, 40);
    ssl_sha1_transform(sha1_info, key, key_len);
    ssl_sha1_complete(sha1_info, shasig);
    ssl_md5_clear(md5_info);
    ssl_md5_transform(md5_info, update_key, key_len);
    ssl_md5_transform(md5_info, (char *)g_pad_92, 48);
    ssl_md5_transform(md5_info, shasig, 20);
    ssl_md5_complete(md5_info, key);
    ssl_rc4_set_key(rc4_info, key, key_len);
    ssl_rc4_crypt(rc4_info, key, key_len);

    if (key_len == 8)
    {
        xrdp_sec_make_40bit(key);
    }

    ssl_sha1_info_delete(sha1_info);
    ssl_md5_info_delete(md5_info);
    ssl_rc4_info_delete(rc4_info);
    return 0;
}

/*****************************************************************************/
static void APP_CC
xrdp_sec_decrypt(struct xrdp_sec *self, char *data, int len)
{
    if (self->decrypt_use_count == 4096)
    {
        xrdp_sec_update(self->decrypt_key, self->decrypt_update_key,
                        self->rc4_key_len);
        ssl_rc4_set_key(self->decrypt_rc4_info, self->decrypt_key,
                        self->rc4_key_len);
        self->decrypt_use_count = 0;
    }

    ssl_rc4_crypt(self->decrypt_rc4_info, data, len);
    self->decrypt_use_count++;
}

/*****************************************************************************/
static void APP_CC
xrdp_sec_encrypt(struct xrdp_sec *self, char *data, int len)
{
    if (self->encrypt_use_count == 4096)
    {
        xrdp_sec_update(self->encrypt_key, self->encrypt_update_key,
                        self->rc4_key_len);
        ssl_rc4_set_key(self->encrypt_rc4_info, self->encrypt_key,
                        self->rc4_key_len);
        self->encrypt_use_count = 0;
    }

    ssl_rc4_crypt(self->encrypt_rc4_info, data, len);
    self->encrypt_use_count++;
}

/*****************************************************************************/
static int APP_CC
unicode_in(struct stream *s, int uni_len, char *dst, int dst_len)
{
    int dst_index;
    int src_index;

    dst_index = 0;
    src_index = 0;

    while (src_index < uni_len)
    {
        if (dst_index >= dst_len || src_index > 512)
        {
            break;
        }

        if (!s_check_rem(s, 2))
        {
            return 1;
        }
        in_uint8(s, dst[dst_index]);
        in_uint8s(s, 1);
        dst_index++;
        src_index += 2;
    }

    if (!s_check_rem(s, 2))
    {
        return 1;
    }
    in_uint8s(s, 2);
    return 0;
}

/*****************************************************************************/
/* returns error */
static int APP_CC
xrdp_sec_process_logon_info(struct xrdp_sec *self, struct stream *s)
{
    int flags = 0;
    int len_domain = 0;
    int len_user = 0;
    int len_password = 0;
    int len_program = 0;
    int len_directory = 0;
    int len_ip = 0;
    int len_dll = 0;
    int tzone = 0;
    char tmpdata[256];

    /* initialize (zero out) local variables */
    g_memset(tmpdata, 0, sizeof(char) * 256);
    if (!s_check_rem(s, 8))
    {
        return 1;
    }
    in_uint8s(s, 4);
    in_uint32_le(s, flags);
    DEBUG(("in xrdp_sec_process_logon_info flags $%x", flags));

    /* this is the first test that the decrypt is working */
    if ((flags & RDP_LOGON_NORMAL) != RDP_LOGON_NORMAL) /* 0x33 */
    {
        /* must be or error */
        DEBUG(("xrdp_sec_process_logon_info: flags wrong, major error"));
        return 1;
    }

    if (flags & RDP_LOGON_LEAVE_AUDIO)
    {
        self->rdp_layer->client_info.sound_code = 1;
        DEBUG(("flag RDP_LOGON_LEAVE_AUDIO found"));
    }

    if ((flags & RDP_LOGON_AUTO) && (!self->rdp_layer->client_info.is_mce))
        /* todo, for now not allowing autologon and mce both */
    {
        self->rdp_layer->client_info.rdp_autologin = 1;
        DEBUG(("flag RDP_LOGON_AUTO found"));
    }

    if (flags & RDP_COMPRESSION)
    {
        DEBUG(("flag RDP_COMPRESSION found"));
        if (self->rdp_layer->client_info.use_bulk_comp)
        {
            DEBUG(("flag RDP_COMPRESSION set"));
            self->rdp_layer->client_info.rdp_compression = 1;
        }
        else
        {
            DEBUG(("flag RDP_COMPRESSION not set"));
        }
    }

    if (!s_check_rem(s, 2))
    {
        return 1;
    }
    in_uint16_le(s, len_domain);

    if (len_domain > 511)
    {
        DEBUG(("ERROR [xrdp_sec_process_logon_info()]: len_domain > 511"));
        return 1;
    }

    if (!s_check_rem(s, 2))
    {
        return 1;
    }
    in_uint16_le(s, len_user);

    if (len_user > 511)
    {
        DEBUG(("ERROR [xrdp_sec_process_logon_info()]: len_user > 511"));
        return 1;
    }

    if (!s_check_rem(s, 2))
    {
        return 1;
    }
    in_uint16_le(s, len_password);

    if (len_password > 511)
    {
        DEBUG(("ERROR [xrdp_sec_process_logon_info()]: len_password > 511"));
        return 1;
    }

    if (!s_check_rem(s, 2))
    {
        return 1;
    }
    in_uint16_le(s, len_program);

    if (len_program > 511)
    {
        DEBUG(("ERROR [xrdp_sec_process_logon_info()]: len_program > 511"));
        return 1;
    }

    if (!s_check_rem(s, 2))
    {
        return 1;
    }
    in_uint16_le(s, len_directory);

    if (len_directory > 511)
    {
        DEBUG(("ERROR [xrdp_sec_process_logon_info()]: len_directory > 511"));
        return 1;
    }

    if (unicode_in(s, len_domain, self->rdp_layer->client_info.domain, 255) != 0)
    {
        return 1;
    }
    DEBUG(("domain %s", self->rdp_layer->client_info.domain));
    if (unicode_in(s, len_user, self->rdp_layer->client_info.username, 255) != 0)
    {
        return 1;
    }
    DEBUG(("username %s", self->rdp_layer->client_info.username));

    if (flags & RDP_LOGON_AUTO)
    {
        if (unicode_in(s, len_password, self->rdp_layer->client_info.password, 255) != 0)
        {
            return 1;
        }
        DEBUG(("flag RDP_LOGON_AUTO found"));
    }
    else
    {
        if (!s_check_rem(s, len_password + 2))
        {
            return 1;
        }
        in_uint8s(s, len_password + 2);
        if (self->rdp_layer->client_info.require_credentials)
        {
            g_writeln("xrdp_sec_process_logon_info: credentials on cmd line is mandatory");
            return 1; /* credentials on cmd line is mandatory */
        }
    }

    if (unicode_in(s, len_program, self->rdp_layer->client_info.program, 255) != 0)
    {
        return 1;
    }
    DEBUG(("program %s", self->rdp_layer->client_info.program));
    if (unicode_in(s, len_directory, self->rdp_layer->client_info.directory, 255) != 0)
    {
        return 1;
    }
    DEBUG(("directory %s", self->rdp_layer->client_info.directory));

    if (flags & RDP_LOGON_BLOB)
    {
        if (!s_check_rem(s, 4))
        {
            return 1;
        }
        in_uint8s(s, 2);                                    /* unknown */
        in_uint16_le(s, len_ip);
        if (unicode_in(s, len_ip - 2, tmpdata, 255) != 0)
        {
            return 1;
        }
        if (!s_check_rem(s, 2))
        {
            return 1;
        }
        in_uint16_le(s, len_dll);
        if (unicode_in(s, len_dll - 2, tmpdata, 255) != 0)
        {
            return 1;
        }
        if (!s_check_rem(s, 4 + 62 + 22 + 62 + 26 + 4))
        {
            return 1;
        }
        in_uint32_le(s, tzone);                             /* len of timezone */
        in_uint8s(s, 62);                                   /* skip */
        in_uint8s(s, 22);                                   /* skip misc. */
        in_uint8s(s, 62);                                   /* skip */
        in_uint8s(s, 26);                                   /* skip stuff */
        in_uint32_le(s, self->rdp_layer->client_info.rdp5_performanceflags);
    }

    DEBUG(("out xrdp_sec_process_logon_info"));
    return 0;
}

/*****************************************************************************/
/* returns error */
static int APP_CC
xrdp_sec_send_lic_initial(struct xrdp_sec *self)
{
    struct stream *s = (struct stream *)NULL;

    make_stream(s);
    init_stream(s, 8192);

    if (xrdp_mcs_init(self->mcs_layer, s) != 0)
    {
        free_stream(s);
        return 1;
    }

    out_uint8a(s, g_lic1, 322);
    s_mark_end(s);

    if (xrdp_mcs_send(self->mcs_layer, s, MCS_GLOBAL_CHANNEL) != 0)
    {
        free_stream(s);
        return 1;
    }

    free_stream(s);
    return 0;
}

/*****************************************************************************/
/* returns error */
static int APP_CC
xrdp_sec_send_lic_response(struct xrdp_sec *self)
{
    struct stream *s;

    make_stream(s);
    init_stream(s, 8192);

    if (xrdp_mcs_init(self->mcs_layer, s) != 0)
    {
        free_stream(s);
        return 1;
    }

    out_uint8a(s, g_lic2, 20);
    s_mark_end(s);

    if (xrdp_mcs_send(self->mcs_layer, s, MCS_GLOBAL_CHANNEL) != 0)
    {
        free_stream(s);
        return 1;
    }

    free_stream(s);
    return 0;
}

/*****************************************************************************/
/* returns error */
static int APP_CC
xrdp_sec_send_media_lic_response(struct xrdp_sec *self)
{
    struct stream *s;

    make_stream(s);
    init_stream(s, 8192);

    if (xrdp_mcs_init(self->mcs_layer, s) != 0)
    {
        free_stream(s);
        return 1;
    }

    out_uint8a(s, g_lic3, sizeof(g_lic3));
    s_mark_end(s);

    if (xrdp_mcs_send(self->mcs_layer, s, MCS_GLOBAL_CHANNEL) != 0)
    {
        free_stream(s);
        return 1;
    }

    free_stream(s);
    return 0;
}

/*****************************************************************************/
static void APP_CC
xrdp_sec_rsa_op(char *out, char *in, char *mod, char *exp)
{
    ssl_mod_exp(out, 64, in, 64, mod, 64, exp, 64);
}

/*****************************************************************************/
static void APP_CC
xrdp_sec_hash_48(char *out, char *in, char *salt1, char *salt2, int salt)
{
    int i;
    void *sha1_info;
    void *md5_info;
    char pad[4];
    char sha1_sig[20];
    char md5_sig[16];

    sha1_info = ssl_sha1_info_create();
    md5_info = ssl_md5_info_create();

    for (i = 0; i < 3; i++)
    {
        g_memset(pad, salt + i, 4);
        ssl_sha1_clear(sha1_info);
        ssl_sha1_transform(sha1_info, pad, i + 1);
        ssl_sha1_transform(sha1_info, in, 48);
        ssl_sha1_transform(sha1_info, salt1, 32);
        ssl_sha1_transform(sha1_info, salt2, 32);
        ssl_sha1_complete(sha1_info, sha1_sig);
        ssl_md5_clear(md5_info);
        ssl_md5_transform(md5_info, in, 48);
        ssl_md5_transform(md5_info, sha1_sig, 20);
        ssl_md5_complete(md5_info, md5_sig);
        g_memcpy(out + i * 16, md5_sig, 16);
    }

    ssl_sha1_info_delete(sha1_info);
    ssl_md5_info_delete(md5_info);
}

/*****************************************************************************/
static void APP_CC
xrdp_sec_hash_16(char *out, char *in, char *salt1, char *salt2)
{
    void *md5_info;

    md5_info = ssl_md5_info_create();
    ssl_md5_clear(md5_info);
    ssl_md5_transform(md5_info, in, 16);
    ssl_md5_transform(md5_info, salt1, 32);
    ssl_md5_transform(md5_info, salt2, 32);
    ssl_md5_complete(md5_info, out);
    ssl_md5_info_delete(md5_info);
}

/*****************************************************************************/
static void APP_CC
xrdp_sec_establish_keys(struct xrdp_sec *self)
{
    char session_key[48];
    char temp_hash[48];
    char input[48];

    g_memcpy(input, self->client_random, 24);
    g_memcpy(input + 24, self->server_random, 24);
    xrdp_sec_hash_48(temp_hash, input, self->client_random,
                     self->server_random, 65);
    xrdp_sec_hash_48(session_key, temp_hash, self->client_random,
                     self->server_random, 88);
    g_memcpy(self->sign_key, session_key, 16);
    xrdp_sec_hash_16(self->encrypt_key, session_key + 16, self->client_random,
                     self->server_random);
    xrdp_sec_hash_16(self->decrypt_key, session_key + 32, self->client_random,
                     self->server_random);

    if (self->rc4_key_size == 1)
    {
        xrdp_sec_make_40bit(self->sign_key);
        xrdp_sec_make_40bit(self->encrypt_key);
        xrdp_sec_make_40bit(self->decrypt_key);
        self->rc4_key_len = 8;
    }
    else
    {
        self->rc4_key_len = 16;
    }

    g_memcpy(self->decrypt_update_key, self->decrypt_key, 16);
    g_memcpy(self->encrypt_update_key, self->encrypt_key, 16);
    ssl_rc4_set_key(self->decrypt_rc4_info, self->decrypt_key, self->rc4_key_len);
    ssl_rc4_set_key(self->encrypt_rc4_info, self->encrypt_key, self->rc4_key_len);
}
/*****************************************************************************/
/* returns error */
int APP_CC
xrdp_sec_recv_fastpath(struct xrdp_sec *self, struct stream *s)
{
  if (xrdp_fastpath_recv(self->fastpath_layer, s) != 0) {
      return 1;
  }

  in_uint8s(s, 8); /* dataSignature, skip for now */

  if (self->fastpath_layer->secFlags & FASTPATH_INPUT_ENCRYPTED) {
      xrdp_sec_decrypt(self, s->p, (int)(s->end - s->p));
  }

  if (self->fastpath_layer->numEvents == 0) {
      /**
       * If numberEvents is not provided in fpInputHeader, it will be provided
       * as one additional byte here.
       */
      in_uint8(s, self->fastpath_layer->numEvents); /* numEvents (optional) */
  }

  return 0;
}
/*****************************************************************************/
/* returns error */
int APP_CC
xrdp_sec_recv(struct xrdp_sec *self, struct stream *s, int *chan)
{
    int flags;
    int len;
    int mcs_msg;

    DEBUG((" in xrdp_sec_recv"));

    mcs_msg = xrdp_mcs_recv(self->mcs_layer, s, chan);

    if (mcs_msg == 2)
    {
        DEBUG((" out xrdp_sec_recv : non-TPKT msg detected, we try fastpath"));
        xrdp_sec_recv_fastpath(self, s);
        return mcs_msg;
    }

    if (mcs_msg == 1)
    {
        DEBUG((" out xrdp_sec_recv : error"));
        return 1;
    }

    if (!s_check_rem(s, 4))
    {
        return 1;
    }
    in_uint32_le(s, flags);
    DEBUG((" in xrdp_sec_recv flags $%x", flags));

    if (flags & SEC_ENCRYPT) /* 0x08 */
    {
        if (!s_check_rem(s, 8))
        {
            return 1;
        }
        in_uint8s(s, 8); /* signature */
        xrdp_sec_decrypt(self, s->p, (int)(s->end - s->p));
    }

    if (flags & SEC_CLIENT_RANDOM) /* 0x01 */
    {
        if (!s_check_rem(s, 4 + 64))
        {
            return 1;
        }
        in_uint32_le(s, len);
        in_uint8a(s, self->client_crypt_random, 64);
        xrdp_sec_rsa_op(self->client_random, self->client_crypt_random,
                        self->pub_mod, self->pri_exp);
        xrdp_sec_establish_keys(self);
        *chan = 1; /* just set a non existing channel and exit */
        DEBUG((" out xrdp_sec_recv"));
        return 0;
    }

    if (flags & SEC_LOGON_INFO) /* 0x40 */
    {
        if (xrdp_sec_process_logon_info(self, s) != 0)
        {
            DEBUG((" out xrdp_sec_recv error"));
            return 1;
        }

        if (self->rdp_layer->client_info.is_mce)
        {
            if (xrdp_sec_send_media_lic_response(self) != 0)
            {
                DEBUG((" out xrdp_sec_recv error"));
                return 1;
            }

            DEBUG((" out xrdp_sec_recv"));
            return -1; /* special error that means send demand active */
        }

        if (xrdp_sec_send_lic_initial(self) != 0)
        {
            DEBUG((" out xrdp_sec_recv error"));
            return 1;
        }

        *chan = 1; /* just set a non existing channel and exit */
        DEBUG((" out xrdp_sec_recv"));
        return 0;
    }

    if (flags & SEC_LICENCE_NEG) /* 0x80 */
    {
        if (xrdp_sec_send_lic_response(self) != 0)
        {
            DEBUG((" out xrdp_sec_recv error"));
            return 1;
        }

        DEBUG((" out xrdp_sec_recv"));
        return -1; /* special error that means send demand active */
    }

    DEBUG((" out xrdp_sec_recv"));
    return 0;
}

/*****************************************************************************/
/* Output a uint32 into a buffer (little-endian) */
static void
buf_out_uint32(char *buffer, int value)
{
    buffer[0] = (value) & 0xff;
    buffer[1] = (value >> 8) & 0xff;
    buffer[2] = (value >> 16) & 0xff;
    buffer[3] = (value >> 24) & 0xff;
}

/*****************************************************************************/
/* Generate a MAC hash (5.2.3.1), using a combination of SHA1 and MD5 */
static void APP_CC
xrdp_sec_sign(struct xrdp_sec *self, char *out, int out_len,
              char *data, int data_len)
{
    char shasig[20];
    char md5sig[16];
    char lenhdr[4];
    void *sha1_info;
    void *md5_info;

    buf_out_uint32(lenhdr, data_len);
    sha1_info = ssl_sha1_info_create();
    md5_info = ssl_md5_info_create();
    ssl_sha1_clear(sha1_info);
    ssl_sha1_transform(sha1_info, self->sign_key, self->rc4_key_len);
    ssl_sha1_transform(sha1_info, (char *)g_pad_54, 40);
    ssl_sha1_transform(sha1_info, lenhdr, 4);
    ssl_sha1_transform(sha1_info, data, data_len);
    ssl_sha1_complete(sha1_info, shasig);
    ssl_md5_clear(md5_info);
    ssl_md5_transform(md5_info, self->sign_key, self->rc4_key_len);
    ssl_md5_transform(md5_info, (char *)g_pad_92, 48);
    ssl_md5_transform(md5_info, shasig, 20);
    ssl_md5_complete(md5_info, md5sig);
    g_memcpy(out, md5sig, out_len);
    ssl_sha1_info_delete(sha1_info);
    ssl_md5_info_delete(md5_info);
}

/*****************************************************************************/
/* returns error */
int APP_CC
xrdp_sec_send(struct xrdp_sec *self, struct stream *s, int chan)
{
    int datalen;

    DEBUG((" in xrdp_sec_send"));
    s_pop_layer(s, sec_hdr);

    if (self->crypt_level > 1)
    {
        out_uint32_le(s, SEC_ENCRYPT);
        datalen = (int)((s->end - s->p) - 8);
        xrdp_sec_sign(self, s->p, 8, s->p + 8, datalen);
        xrdp_sec_encrypt(self, s->p + 8, datalen);
    }
    else
    {
        out_uint32_le(s, 0);
    }

    if (xrdp_mcs_send(self->mcs_layer, s, chan) != 0)
    {
        return 1;
    }

    DEBUG((" out xrdp_sec_send"));
    return 0;
}
/*****************************************************************************/
/* returns error */
static int APP_CC
xrdp_sec_process_mcs_cli_info(struct xrdp_sec *self, struct stream *s)
{
    in_uint8s(s, 4); /* RDP client version */
    in_uint8s(s, 2); /* desktopWidth */
    in_uint8s(s, 2); /* desktopHeight */
    in_uint8s(s, 2); /* colorDepth */
    in_uint8s(s, 2);
    in_uint8s(s, 4);
    in_uint8s(s, 4);
    in_uint8s(s, 32);
    in_uint8s(s, 4);
    in_uint8s(s, 4);
    in_uint8s(s, 64);
    in_uint8s(s, 2);
    in_uint8s(s, 2);
    in_uint8s(s, 4);
    in_uint8s(s, 2);
    in_uint8s(s, 2);
    in_uint8s(s, 2); /* earlyCapabilityFlags */
    in_uint8s(s, 64);
    in_uint8s(s, 1);
    in_uint8s(s, 1); /* pad1octet */
    in_uint8s(s, 4);
    in_uint8s(s, 4);
    in_uint8s(s, 4);
    in_uint8s(s, 2);
    in_uint8s(s, 4);
    in_uint8s(s, 4);

}
/*****************************************************************************/
/* this adds the mcs channels in the list of channels to be used when
   creating the server mcs data */
static int APP_CC
xrdp_sec_process_mcs_data_channels(struct xrdp_sec *self, struct stream *s)
{
    int num_channels;
    int index;
    struct mcs_channel_item *channel_item;

    DEBUG(("processing channels, channel_code is %d", self->channel_code));

    /* this is an option set in xrdp.ini */
    if (self->channel_code != 1) /* are channels on? */
    {
        g_writeln("Processing channel data from client - The channel is off");
        return 0;
    }

    if (!s_check_rem(s, 4))
    {
        return 1;
    }

    in_uint32_le(s, num_channels);

    if (num_channels > 31)
    {
        return 1;
    }

    for (index = 0; index < num_channels; index++)
    {
        channel_item = (struct mcs_channel_item *)
                       g_malloc(sizeof(struct mcs_channel_item), 1);
        if (!s_check_rem(s, 12))
        {
            return 1;
        }
        in_uint8a(s, channel_item->name, 8);
        in_uint32_le(s, channel_item->flags);
        channel_item->chanid = MCS_GLOBAL_CHANNEL + (index + 1);
        list_add_item(self->mcs_layer->channel_list, (tintptr)channel_item);
        DEBUG(("got channel flags %8.8x name %s", channel_item->flags,
               channel_item->name));
    }

    return 0;
}
/*****************************************************************************/
/* reads the client monitors data */
static int APP_CC
xrdp_sec_process_mcs_data_monitors(struct xrdp_sec *self, struct stream *s)
{
    int index;
    int monitorCount;
    int flags;
    struct xrdp_client_info *client_info = (struct xrdp_client_info *)NULL;

    client_info = &(self->rdp_layer->client_info);

    DEBUG(("processing monitors data, allow_multimon is %d", self->multimon));
    /* this is an option set in xrdp.ini */
    if (self->multimon != 1) /* are multi-monitors allowed ? */
    {
        DEBUG(("[INFO] xrdp_sec_process_mcs_data_monitors: multimon is not "
               "allowed, skipping"));
        return 0;
    }
    in_uint32_le(s, flags); /* flags */
    //verify flags - must be 0x0
    if (flags != 0)
    {
        DEBUG(("[ERROR] xrdp_sec_process_mcs_data_monitors: flags MUST be "
               "zero, detected: %d", flags));
        return 1;
    }
    in_uint32_le(s, monitorCount);
    //verify monitorCount - max 16
    if (monitorCount > 16)
    {
        DEBUG(("[ERROR] xrdp_sec_process_mcs_data_monitors: max allowed "
               "monitors is 16, detected: %d", monitorCount));
        return 1;
    }

    g_writeln("monitorCount= %d", monitorCount); // for debugging only

    client_info->monitorCount = monitorCount;

    /* Add client_monitor_data to client_info struct, will later pass to X11rdp */
    for (index = 0; index < monitorCount; index++)
    {
        in_uint32_le(s, client_info->minfo[index].left);
        in_uint32_le(s, client_info->minfo[index].top);
        in_uint32_le(s, client_info->minfo[index].right);
        in_uint32_le(s, client_info->minfo[index].bottom);
        in_uint32_le(s, client_info->minfo[index].is_primary);

        g_writeln("got a monitor: left= %d, top= %d, right= %d, bottom= %d, is_primary?= %d", client_info->minfo[index].left,
            client_info->minfo[index].top, client_info->minfo[index].right, client_info->minfo[index].bottom, client_info->minfo[index].is_primary);
    }
    return 0;
}
/*****************************************************************************/
/* process client mcs data, we need some things in here to create the server
   mcs data */
int APP_CC
xrdp_sec_process_mcs_data(struct xrdp_sec *self)
{
    struct stream *s = (struct stream *)NULL;
    char *hold_p = (char *)NULL;
    int tag = 0;
    int size = 0;

    s = &(self->client_mcs_data);
    /* set p to beginning */
    s->p = s->data;
    /* skip header */
    if (!s_check_rem(s, 23))
    {
        return 1;
    }
    in_uint8s(s, 23);

    while (s_check_rem(s, 4))
    {
        hold_p = s->p;
        in_uint16_le(s, tag);
        in_uint16_le(s, size);

        if (size < 4 || !s_check_rem(s, size - 4))
        {
            g_writeln("error in xrdp_sec_process_mcs_data tag %d size %d",
                      tag, size);
            break;
        }

        switch (tag)
        {
            case SEC_TAG_CLI_INFO:
//                if (xrdp_sec_process_mcs_cli_info(self, s) != 0)
//                {
//                  return 1;
//                }
                break;
            case SEC_TAG_CLI_CRYPT:
                break;
            case SEC_TAG_CLI_CHANNELS:
                if (xrdp_sec_process_mcs_data_channels(self, s) != 0)
                {
                    return 1;
                }
                break;
            case SEC_TAG_CLI_4:
                break;
            case SEC_TAG_CLI_MONITOR:
                if (xrdp_sec_process_mcs_data_monitors(self, s) != 0)
                {
                    return 1;
                }
                break;
            default:
                g_writeln("error unknown xrdp_sec_process_mcs_data tag %d size %d",
                          tag, size);
                break;
        }

        s->p = hold_p + size;
    }

    /* set p to beginning */
    s->p = s->data;
    return 0;
}

/*****************************************************************************/
/* prepare server mcs data to send in mcs layer */
int APP_CC
xrdp_sec_out_mcs_data(struct xrdp_sec *self)
{
    struct stream *s;
    int num_channels_even;
    int num_channels;
    int index;
    int channel;

    num_channels = self->mcs_layer->channel_list->count;
    num_channels_even = num_channels + (num_channels & 1);
    s = &self->server_mcs_data;
    init_stream(s, 512);
    out_uint16_be(s, 5);
    out_uint16_be(s, 0x14);
    out_uint8(s, 0x7c);
    out_uint16_be(s, 1);
    out_uint8(s, 0x2a);
    out_uint8(s, 0x14);
    out_uint8(s, 0x76);
    out_uint8(s, 0x0a);
    out_uint8(s, 1);
    out_uint8(s, 1);
    out_uint8(s, 0);
    out_uint16_le(s, 0xc001);
    out_uint8(s, 0);
    out_uint8(s, 0x4d); /* M */
    out_uint8(s, 0x63); /* c */
    out_uint8(s, 0x44); /* D */
    out_uint8(s, 0x6e); /* n */
    if (self->mcs_layer->iso_layer->selectedProtocol != -1)
    { // Check for RDPNEGDATA availability
        out_uint16_be(s, 0x80fc + (num_channels_even * 2) + 4);
    }
    else
    {
        out_uint16_be(s, 0x80fc + (num_channels_even * 2));
    }
    out_uint16_le(s, SEC_TAG_SRV_INFO);
    if (self->mcs_layer->iso_layer->selectedProtocol != -1)
    {
        out_uint16_le(s, 12); /* len */
    }
    else
    {
        out_uint16_le(s, 8); /* len */
    }
    out_uint8(s, 4); /* 4 = rdp5 1 = rdp4 */
    out_uint8(s, 0);
    out_uint8(s, 8);
    out_uint8(s, 0);
    if (self->mcs_layer->iso_layer->selectedProtocol != -1)
    {
         /* clientReqeustedProtocol */
        out_uint32_le(s, self->mcs_layer->iso_layer->selectedProtocol);
    }
    out_uint16_le(s, SEC_TAG_SRV_CHANNELS);
    out_uint16_le(s, 8 + (num_channels_even * 2)); /* len */
    out_uint16_le(s, MCS_GLOBAL_CHANNEL); /* 1003, 0x03eb main channel */
    out_uint16_le(s, num_channels); /* number of other channels */

    for (index = 0; index < num_channels_even; index++)
    {
        if (index < num_channels)
        {
            channel = MCS_GLOBAL_CHANNEL + (index + 1);
            out_uint16_le(s, channel);
        }
        else
        {
            out_uint16_le(s, 0);
        }
    }

    out_uint16_le(s, SEC_TAG_SRV_CRYPT);
    out_uint16_le(s, 0x00ec); /* len is 236 */
    out_uint32_le(s, self->rc4_key_size); /* key len 1 = 40 bit 2 = 128 bit */
    out_uint32_le(s, self->crypt_level); /* crypt level 1 = low 2 = medium */
    /* 3 = high */
    out_uint32_le(s, 32);     /* 32 bytes random len */
    out_uint32_le(s, 0xb8);   /* 184 bytes rsa info(certificate) len */
    out_uint8a(s, self->server_random, 32);
    /* here to end is certificate */
    /* HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\ */
    /* TermService\Parameters\Certificate */
    out_uint32_le(s, 1);
    out_uint32_le(s, 1);
    out_uint32_le(s, 1);
    out_uint16_le(s, SEC_TAG_PUBKEY);
    out_uint16_le(s, 0x005c); /* 92 bytes length of SEC_TAG_PUBKEY */
    out_uint32_le(s, SEC_RSA_MAGIC);
    out_uint32_le(s, 0x48); /* 72 bytes modulus len */
    out_uint32_be(s, 0x00020000);
    out_uint32_be(s, 0x3f000000);
    out_uint8a(s, self->pub_exp, 4); /* pub exp */
    out_uint8a(s, self->pub_mod, 64); /* pub mod */
    out_uint8s(s, 8); /* pad */
    out_uint16_le(s, SEC_TAG_KEYSIG);
    out_uint16_le(s, 72); /* len */
    out_uint8a(s, self->pub_sig, 64); /* pub sig */
    out_uint8s(s, 8); /* pad */
    /* end certificate */
    s_mark_end(s);
    return 0;
}

/*****************************************************************************/
/* process the mcs client data we received from the mcs layer */
static int APP_CC
xrdp_sec_in_mcs_data(struct xrdp_sec *self)
{
    struct stream *s = (struct stream *)NULL;
    struct xrdp_client_info *client_info = (struct xrdp_client_info *)NULL;
    int index = 0;
    char c = 0;

    client_info = &(self->rdp_layer->client_info);
    s = &(self->client_mcs_data);
    /* get hostname, its unicode */
    s->p = s->data;
    if (!s_check_rem(s, 47))
    {
        return 1;
    }
    in_uint8s(s, 47);
    g_memset(client_info->hostname, 0, 32);
    c = 1;
    index = 0;

    while (index < 16 && c != 0)
    {
        if (!s_check_rem(s, 2))
        {
            return 1;
        }
        in_uint8(s, c);
        in_uint8s(s, 1);
        client_info->hostname[index] = c;
        index++;
    }
    /* get build */
    s->p = s->data;
    if (!s_check_rem(s, 43 + 4))
    {
        return 1;
    }
    in_uint8s(s, 43);
    in_uint32_le(s, client_info->build);
    /* get keylayout */
    s->p = s->data;
    if (!s_check_rem(s, 39 + 4))
    {
        return 1;
    }
    in_uint8s(s, 39);
    in_uint32_le(s, client_info->keylayout);
    /* get keyboard type / subtype */
    s->p = s->data;
    if (!s_check_rem(s, 79 + 8))
    {
        return 1;
    }
    in_uint8s(s, 79);
    in_uint32_le(s, client_info->keyboard_type);
    in_uint32_le(s, client_info->keyboard_subtype);
    s->p = s->data;
    return 0;
}

/*****************************************************************************/
int APP_CC
xrdp_sec_incoming(struct xrdp_sec *self)
{
    struct list *items = NULL;
    struct list *values = NULL;
    int index = 0;
    char *item = NULL;
    char *value = NULL;
    char key_file[256];

    g_memset(key_file, 0, sizeof(char) * 256);

    DEBUG((" in xrdp_sec_incoming"));
    g_random(self->server_random, 32);
    items = list_create();
    items->auto_free = 1;
    values = list_create();
    values->auto_free = 1;
    g_snprintf(key_file, 255, "%s/rsakeys.ini", XRDP_CFG_PATH);

    if (file_by_name_read_section(key_file, "keys", items, values) != 0)
    {
        /* this is a show stopper */
        log_message(LOG_LEVEL_ALWAYS, "XRDP cannot read file: %s "
                    "(check permissions)", key_file);
        list_delete(items);
        list_delete(values);
        return 1;
    }

    for (index = 0; index < items->count; index++)
    {
        item = (char *)list_get_item(items, index);
        value = (char *)list_get_item(values, index);

        if (g_strcasecmp(item, "pub_exp") == 0)
        {
            hex_str_to_bin(value, self->pub_exp, 4);
        }
        else if (g_strcasecmp(item, "pub_mod") == 0)
        {
            hex_str_to_bin(value, self->pub_mod, 64);
        }
        else if (g_strcasecmp(item, "pub_sig") == 0)
        {
            hex_str_to_bin(value, self->pub_sig, 64);
        }
        else if (g_strcasecmp(item, "pri_exp") == 0)
        {
            hex_str_to_bin(value, self->pri_exp, 64);
        }
    }

    list_delete(items);
    list_delete(values);

    if (xrdp_mcs_incoming(self->mcs_layer) != 0)
    {
        return 1;
    }

#ifdef XRDP_DEBUG
    g_writeln("client mcs data received");
    g_hexdump(self->client_mcs_data.data,
              (int)(self->client_mcs_data.end - self->client_mcs_data.data));
    g_writeln("server mcs data sent");
    g_hexdump(self->server_mcs_data.data,
              (int)(self->server_mcs_data.end - self->server_mcs_data.data));
#endif
    DEBUG((" out xrdp_sec_incoming"));
    if (xrdp_sec_in_mcs_data(self) != 0)
    {
        return 1;
    }
    return 0;
}

/*****************************************************************************/
int APP_CC
xrdp_sec_disconnect(struct xrdp_sec *self)
{
    int rv;

    DEBUG((" in xrdp_sec_disconnect"));
    rv = xrdp_mcs_disconnect(self->mcs_layer);
    DEBUG((" out xrdp_sec_disconnect"));
    return rv;
}
