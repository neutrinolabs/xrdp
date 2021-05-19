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
 * secure layer
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "libxrdp.h"
#include "ms-rdpbcgr.h"
#include "log.h"
#include "string_calls.h"



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


/*****************************************************************************/
/* Licensing request v2 PDU
 *
 * [MS-RDPBCGR] TS_SECURITY_HEADER - Basic
 * [MS-RDPELE] SERVER_LICENSE_REQUEST with PREAMBLE_VERSION_2_0
 */
/* some compilers need unsigned char to avoid warnings */
static tui8 g_lic1[322] =
{
    /* [MS-RDPBCGR] TS_SECURITY_HEADER - Basic
     * flags (2) = 0x0080 (SEC_LICENSE_PKT)
     * flagsHi (2) = unused (arbitrary data)
     * [MS-RDPBCGR] LICENSE_PREAMBLE
     * bMsgType (1) = 0x01 (LICENSE_REQUEST)
     * flags (1) = 0x02 (PREAMBLE_VERSION_2_0)
     * wMsgSize (2) = 318 (excludes the 4 bytes TS_SECURITY_HEADER Basic)
     */
    0x80, 0x00, 0x3e, 0x01, 0x01, 0x02, 0x3e, 0x01,
    /* [MS-RDPELE] SERVER_LICENSE_REQUEST
     * ServerRandom (32) = <see hex below>
     */
    0x7b, 0x3c, 0x31, 0xa6, 0xae, 0xe8, 0x74, 0xf6,
    0xb4, 0xa5, 0x03, 0x90, 0xe7, 0xc2, 0xc7, 0x39,
    0xba, 0x53, 0x1c, 0x30, 0x54, 0x6e, 0x90, 0x05,
    0xd0, 0x05, 0xce, 0x44, 0x18, 0x91, 0x83, 0x81,
    /* [MS-RDPELE] SERVER_LICENSE_REQUEST - ProductInfo
     * [MS-RDPELE] PRODUCT_INFO
     * dwVersion (4) = 0x00040000
     * cbCompanyName (4) = 0x0000002c (44)
     */
    0x00, 0x00, 0x04, 0x00, 0x2c, 0x00, 0x00, 0x00,
    /*
     * pbCompanyName (44) = UTF-16("Microsoft Corporation")
     * cbProductId (4) = 0x00000008 (8)
     */
    0x4d, 0x00, 0x69, 0x00, 0x63, 0x00, 0x72, 0x00,
    0x6f, 0x00, 0x73, 0x00, 0x6f, 0x00, 0x66, 0x00,
    0x74, 0x00, 0x20, 0x00, 0x43, 0x00, 0x6f, 0x00,
    0x72, 0x00, 0x70, 0x00, 0x6f, 0x00, 0x72, 0x00,
    0x61, 0x00, 0x74, 0x00, 0x69, 0x00, 0x6f, 0x00,
    0x6e, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
    /*
     * pbProductId (8) = UTF-16("236")
     */
    0x32, 0x00, 0x33, 0x00, 0x36, 0x00, 0x00, 0x00,
    /* [MS-RDPELE] SERVER_LICENSE_REQUEST - KeyExchangeList
     * [MS-RDPBCGR] LICENSE_BINARY_BLOB
     * wBlobType (2) = 0x000d (BB_KEY_EXCHG_ALG_BLOB)
     * wBlobLen (2) = 0x0004 (4)
     * blobData (4) = 0x00000001 (KEY_EXCHANGE_ALG_RSA)
     */
    0x0d, 0x00, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00,
    /* [MS-RDPELE] SERVER_LICENSE_REQUEST - ServerCertificate
     * [MS-RDPBCGR] LICENSE_BINARY_BLOB
     * wBlobType (2) = BB_CERTIFICATE_BLOB (0x0003)
     * wBlobLen (2) = 0x00b8 (184)
     * blobData = <SERVER_CERTIFICATE>
     *
     * [MS-RDPBCGR] SERVER_CERTIFICATE
     * dwVersion (31 bits) = 0x00000001 (CERT_CHAIN_VERSION_1)
     * t (1 bit) = 0 (temporary certificate)
     */
    0x03, 0x00, 0xb8, 0x00, 0x01, 0x00, 0x00, 0x00,
    /*
     * certData = <PROPRIETARYSERVERCERTIFICATE>
     *
     * [MS-RDPBCGR] PROPRIETARYSERVERCERTIFICATE
     * dwSigAlgId (4) = 0x00000001 (SIGNATURE_ALG_RSA)
     * dwKeyAlgId (4) = 0x00000001 (KEY_EXCHANGE_ALG_RSA)
     * wPublicKeyBlobType (2) = 0x0006 (BB_RSA_KEY_BLOB)
     * wPublicKeyBlobLen (2) = 0x005c (92)
     * PublicKeyBlob = <RSA_PUBLIC_KEY>
     *
     * [MS-RDPBCGR] RSA_PUBLIC_KEY
     * magic (4) = 0x31415352
     * keylen (4) = 0x00000048 (72)
     * bitlen (4) = 0x00000200 (512)
     * datalen (4) = 0x0000003f (63)
     * pubExp (4) = 0x00010001 (65537)
     */
    0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x5c, 0x00, 0x52, 0x53, 0x41, 0x31,
    0x48, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
    0x3f, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00,
    /*
     * modulus (72) = <see hex below>
     */
    0x01, 0xc7, 0xc9, 0xf7, 0x8e, 0x5a, 0x38, 0xe4,
    0x29, 0xc3, 0x00, 0x95, 0x2d, 0xdd, 0x4c, 0x3e,
    0x50, 0x45, 0x0b, 0x0d, 0x9e, 0x2a, 0x5d, 0x18,
    0x63, 0x64, 0xc4, 0x2c, 0xf7, 0x8f, 0x29, 0xd5,
    0x3f, 0xc5, 0x35, 0x22, 0x34, 0xff, 0xad, 0x3a,
    0xe6, 0xe3, 0x95, 0x06, 0xae, 0x55, 0x82, 0xe3,
    0xc8, 0xc7, 0xb4, 0xa8, 0x47, 0xc8, 0x50, 0x71,
    0x74, 0x29, 0x53, 0x89, 0x6d, 0x9c, 0xed, 0x70,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* [MS-RDPELE] SERVER_LICENSE_REQUEST - ServerCertificate
     * [MS-RDPBCGR] LICENSE_BINARY_BLOB - blobData
     * [MS-RDPBCGR] SERVER_CERTIFICATE - certData
     * [MS-RDPBCGR] PROPRIETARYSERVERCERTIFICATE
     * wSignatureBlobType (2) = 0x0008 (BB_RSA_SIGNATURE_BLOB)
     * wSignatureBlobLen (2) = 0x0048 (72)
     * SignatureBlob (72) = <see hex below, calculated using [MS-RDPBCGR] 5.3.3.1.2>
     */
    0x08, 0x00, 0x48, 0x00, 0xa8, 0xf4, 0x31, 0xb9,
    0xab, 0x4b, 0xe6, 0xb4, 0xf4, 0x39, 0x89, 0xd6,
    0xb1, 0xda, 0xf6, 0x1e, 0xec, 0xb1, 0xf0, 0x54,
    0x3b, 0x5e, 0x3e, 0x6a, 0x71, 0xb4, 0xf7, 0x75,
    0xc8, 0x16, 0x2f, 0x24, 0x00, 0xde, 0xe9, 0x82,
    0x99, 0x5f, 0x33, 0x0b, 0xa9, 0xa6, 0x94, 0xaf,
    0xcb, 0x11, 0xc3, 0xf2, 0xdb, 0x09, 0x42, 0x68,
    0x29, 0x56, 0x58, 0x01, 0x56, 0xdb, 0x59, 0x03,
    0x69, 0xdb, 0x7d, 0x37, 0x00, 0x00, 0x00, 0x00,
    /* <last 4 bytes of SignatureBlob>
     *
     * [MS-RDPELE] SERVER_LICENSE_REQUEST - ScopeList
     * [MS-RDPELE] SCOPE_LIST
     * ScopeCount (4) = 0x00000001 (1)
     * ScopeArray = <LICENSE_BINARY_BLOB>
     *
     * [MS-RDPBCGR] LICENSE_BINARY_BLOB
     * wBlobType (2) = 0x000e (BB_SCOPE_BLOB)
     * wBlobLen (2) = 0x000e (14)
     * blobData (14) = ISO-8859-1("microsoft.com")
     */
    0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x0e, 0x00, 0x0e, 0x00, 0x6d, 0x69, 0x63, 0x72,
    0x6f, 0x73, 0x6f, 0x66, 0x74, 0x2e, 0x63, 0x6f,
    0x6d, 0x00
};

/*****************************************************************************/
/* Licensing success response v2 PDU
 *
 * [MS-RDPBCGR] TS_SECURITY_HEADER - Basic
 * [MS-RDPELE] LICENSE_ERROR_MESSAGE with STATUS_VALID_CLIENT
 */
/* some compilers need unsigned char to avoid warnings */
static tui8 g_lic2[20] =
{
    /* [MS-RDPBCGR] TS_SECURITY_HEADER - Basic
     * flags (2) = 0x0080 (SEC_LICENSE_PKT)
     * flagsHi (2) = unused (arbitrary data)
     * [MS-RDPBCGR] LICENSE_PREAMBLE
     * bMsgType (1) = 0xff (ERROR_ALERT)
     * flags (1) = 0x02 (PREAMBLE_VERSION_2_0)
     * wMsgSize (2) = 0x10 (16, excludes the 4 bytes TS_SECURITY_HEADER Basic)
     */
    0x80, 0x00, 0x10, 0x00, 0xff, 0x02, 0x10, 0x00,
    /*
     * [MS-RDPBCGR] LICENSE_ERROR_MESSAGE
     * dwErrorCode (4) = 0x00000007 (STATUS_VALID_CLIENT)
     * dwStateTransition (4) = 0x00000002 (ST_NO_TRANSITION)
     * bbErrorInfo = <LICENSE_BINARY_BLOB>
     */
    0x07, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
    /*
     * [MS-RDPBCGR] LICENSE_BINARY_BLOB
     * wBlobType (2) = 0x1428 <ignored by client> (should be 0x0004 BB_ERROR_BLOB)
     * wBlobLen (2) = 0x0000 (0)
     */
    0x28, 0x14, 0x00, 0x00
};

/*****************************************************************************/
/* Licensing success response v3 PDU
 *
 * [MS-RDPBCGR] TS_SECURITY_HEADER - Basic
 * [MS-RDPELE] LICENSE_ERROR_MESSAGE with STATUS_VALID_CLIENT
 *
 * used for Media Center Edition
 */
/* some compilers need unsigned char to avoid warnings */
static tui8 g_lic3[20] =
{
    /* S */
    /* [MS-RDPBCGR] TS_SECURITY_HEADER - Basic
     * flags (2) = 0x0280 (SEC_LICENSE_PKT | SEC_LICENSE_ENCRYPT_CS)
     * flagsHi (2) = unused (arbitrary data)
     * [MS-RDPBCGR] LICENSE_PREAMBLE
     * bMsgType (1) = 0xff (ERROR_ALERT)
     * flags (1) = 0x03 (PREAMBLE_VERSION_3_0)
     * wMsgSize (2) = 0x0010 (16, excludes the 4 bytes TS_SECURITY_HEADER Basic)
     */
    0x80, 0x02, 0x10, 0x00, 0xff, 0x03, 0x10, 0x00,
    /*
     * [MS-RDPBCGR] LICENSE_ERROR_MESSAGE
     * dwErrorCode (4) = 0x00000007 (STATUS_VALID_CLIENT)
     * dwStateTransition (4) = 0x00000002 (ST_NO_TRANSITION)
     * bbErrorInfo = <LICENSE_BINARY_BLOB>
     */
    0x07, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
    /*
     * [MS-RDPBCGR] LICENSE_BINARY_BLOB
     * wBlobType (2) = 0x99f3 <ignored by client> (should be 0x0004 BB_ERROR_BLOB)
     * wBlobLen (2) = 0x0000 (0)
     */
    0xf3, 0x99, 0x00, 0x00
};

static const tui8 g_fips_reverse_table[256] =
{
    0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
    0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
    0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
    0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
    0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
    0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
    0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
    0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
    0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
    0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
    0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
    0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
    0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
    0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
    0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
    0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
    0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
    0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
    0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
    0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
    0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
    0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
    0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
    0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
    0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
    0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
    0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
    0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
    0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
    0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
    0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
    0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
};

static const tui8 g_fips_oddparity_table[256] =
{
    0x01, 0x01, 0x02, 0x02, 0x04, 0x04, 0x07, 0x07,
    0x08, 0x08, 0x0b, 0x0b, 0x0d, 0x0d, 0x0e, 0x0e,
    0x10, 0x10, 0x13, 0x13, 0x15, 0x15, 0x16, 0x16,
    0x19, 0x19, 0x1a, 0x1a, 0x1c, 0x1c, 0x1f, 0x1f,
    0x20, 0x20, 0x23, 0x23, 0x25, 0x25, 0x26, 0x26,
    0x29, 0x29, 0x2a, 0x2a, 0x2c, 0x2c, 0x2f, 0x2f,
    0x31, 0x31, 0x32, 0x32, 0x34, 0x34, 0x37, 0x37,
    0x38, 0x38, 0x3b, 0x3b, 0x3d, 0x3d, 0x3e, 0x3e,
    0x40, 0x40, 0x43, 0x43, 0x45, 0x45, 0x46, 0x46,
    0x49, 0x49, 0x4a, 0x4a, 0x4c, 0x4c, 0x4f, 0x4f,
    0x51, 0x51, 0x52, 0x52, 0x54, 0x54, 0x57, 0x57,
    0x58, 0x58, 0x5b, 0x5b, 0x5d, 0x5d, 0x5e, 0x5e,
    0x61, 0x61, 0x62, 0x62, 0x64, 0x64, 0x67, 0x67,
    0x68, 0x68, 0x6b, 0x6b, 0x6d, 0x6d, 0x6e, 0x6e,
    0x70, 0x70, 0x73, 0x73, 0x75, 0x75, 0x76, 0x76,
    0x79, 0x79, 0x7a, 0x7a, 0x7c, 0x7c, 0x7f, 0x7f,
    0x80, 0x80, 0x83, 0x83, 0x85, 0x85, 0x86, 0x86,
    0x89, 0x89, 0x8a, 0x8a, 0x8c, 0x8c, 0x8f, 0x8f,
    0x91, 0x91, 0x92, 0x92, 0x94, 0x94, 0x97, 0x97,
    0x98, 0x98, 0x9b, 0x9b, 0x9d, 0x9d, 0x9e, 0x9e,
    0xa1, 0xa1, 0xa2, 0xa2, 0xa4, 0xa4, 0xa7, 0xa7,
    0xa8, 0xa8, 0xab, 0xab, 0xad, 0xad, 0xae, 0xae,
    0xb0, 0xb0, 0xb3, 0xb3, 0xb5, 0xb5, 0xb6, 0xb6,
    0xb9, 0xb9, 0xba, 0xba, 0xbc, 0xbc, 0xbf, 0xbf,
    0xc1, 0xc1, 0xc2, 0xc2, 0xc4, 0xc4, 0xc7, 0xc7,
    0xc8, 0xc8, 0xcb, 0xcb, 0xcd, 0xcd, 0xce, 0xce,
    0xd0, 0xd0, 0xd3, 0xd3, 0xd5, 0xd5, 0xd6, 0xd6,
    0xd9, 0xd9, 0xda, 0xda, 0xdc, 0xdc, 0xdf, 0xdf,
    0xe0, 0xe0, 0xe3, 0xe3, 0xe5, 0xe5, 0xe6, 0xe6,
    0xe9, 0xe9, 0xea, 0xea, 0xec, 0xec, 0xef, 0xef,
    0xf1, 0xf1, 0xf2, 0xf2, 0xf4, 0xf4, 0xf7, 0xf7,
    0xf8, 0xf8, 0xfb, 0xfb, 0xfd, 0xfd, 0xfe, 0xfe
};

static const tui8 g_fips_ivec[8] =
{
    0x12, 0x34, 0x56, 0x78, 0x90, 0xAB, 0xCD, 0xEF
};

/*****************************************************************************/
static void
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
static void
xrdp_load_keyboard_layout(struct xrdp_client_info *client_info)
{
    int fd;
    int index = 0;
    int bytes;
    struct list *names = (struct list *)NULL;
    struct list *items = (struct list *)NULL;
    struct list *values = (struct list *)NULL;
    char *item = (char *)NULL;
    char *value = (char *)NULL;
    char *q = (char *)NULL;
    char keyboard_cfg_file[256] = { 0 };
    char rdp_layout[256] = { 0 };

    LOG(LOG_LEVEL_INFO, "xrdp_load_keyboard_layout: keyboard_type [%d] keyboard_subtype [%d]",
        client_info->keyboard_type, client_info->keyboard_subtype);

    /* infer model/variant */
    /* TODO specify different X11 keyboard models/variants */
    g_memset(client_info->model, 0, sizeof(client_info->model));
    g_memset(client_info->variant, 0, sizeof(client_info->variant));
    g_strncpy(client_info->layout, "us", sizeof(client_info->layout) - 1);
    if (client_info->keyboard_subtype == 3)
    {
        /* macintosh keyboard */
        bytes = sizeof(client_info->variant);
        g_strncpy(client_info->variant, "mac", bytes - 1);
    }
    else if (client_info->keyboard_subtype == 0)
    {
        /* default - standard subtype */
        client_info->keyboard_subtype = 1;
    }

    g_snprintf(keyboard_cfg_file, 255, "%s/xrdp_keyboard.ini", XRDP_CFG_PATH);
    LOG(LOG_LEVEL_DEBUG, "keyboard_cfg_file %s", keyboard_cfg_file);

    fd = g_file_open(keyboard_cfg_file);

    if (fd >= 0)
    {
        int section_found = -1;
        char section_rdp_layouts[256] = { 0 };
        char section_layouts_map[256] = { 0 };

        names = list_create();
        names->auto_free = 1;
        items = list_create();
        items->auto_free = 1;
        values = list_create();
        values->auto_free = 1;

        file_read_sections(fd, names);
        for (index = 0; index < names->count; index++)
        {
            q = (char *)list_get_item(names, index);
            if (g_strncasecmp("default", q, 8) != 0)
            {
                int i;

                file_read_section(fd, q, items, values);

                for (i = 0; i < items->count; i++)
                {
                    item = (char *)list_get_item(items, i);
                    value = (char *)list_get_item(values, i);
                    LOG(LOG_LEVEL_DEBUG, "xrdp_load_keyboard_layout: item %s value %s",
                        item, value);
                    if (g_strcasecmp(item, "keyboard_type") == 0)
                    {
                        int v = g_atoi(value);
                        if (v == client_info->keyboard_type)
                        {
                            section_found = index;
                        }
                    }
                    else if (g_strcasecmp(item, "keyboard_subtype") == 0)
                    {
                        int v = g_atoi(value);
                        if (v != client_info->keyboard_subtype &&
                                section_found == index)
                        {
                            section_found = -1;
                            break;
                        }
                    }
                    else if (g_strcasecmp(item, "rdp_layouts") == 0)
                    {
                        if (section_found != -1 && section_found == index)
                        {
                            g_strncpy(section_rdp_layouts, value, 255);
                        }
                    }
                    else if (g_strcasecmp(item, "layouts_map") == 0)
                    {
                        if (section_found != -1 && section_found == index)
                        {
                            g_strncpy(section_layouts_map, value, 255);
                        }
                    }
                    else if (g_strcasecmp(item, "model") == 0)
                    {
                        if (section_found != -1 && section_found == index)
                        {
                            bytes = sizeof(client_info->model);
                            g_memset(client_info->model, 0, bytes);
                            g_strncpy(client_info->model, value, bytes - 1);
                        }
                    }
                    else if (g_strcasecmp(item, "variant") == 0)
                    {
                        if (section_found != -1 && section_found == index)
                        {
                            bytes = sizeof(client_info->variant);
                            g_memset(client_info->variant, 0, bytes);
                            g_strncpy(client_info->variant, value, bytes - 1);
                        }
                    }
                    else if (g_strcasecmp(item, "options") == 0)
                    {
                        if (section_found != -1 && section_found == index)
                        {
                            bytes = sizeof(client_info->options);
                            g_memset(client_info->options, 0, bytes);
                            g_strncpy(client_info->options, value, bytes - 1);
                        }
                    }
                    else
                    {
                        /*
                         * mixing items from different sections will result in
                         * skipping over current section.
                         */
                        LOG(LOG_LEVEL_DEBUG, "xrdp_load_keyboard_layout: skipping "
                            "configuration item - %s, continuing to next "
                            "section", item);
                        break;
                    }
                }

                list_clear(items);
                list_clear(values);
            }
        }

        if (section_found == -1)
        {
            g_memset(section_rdp_layouts, 0, sizeof(char) * 256);
            g_memset(section_layouts_map, 0, sizeof(char) * 256);
            // read default section
            file_read_section(fd, "default", items, values);
            for (index = 0; index < items->count; index++)
            {
                item = (char *)list_get_item(items, index);
                value = (char *)list_get_item(values, index);
                if (g_strcasecmp(item, "rdp_layouts") == 0)
                {
                    g_strncpy(section_rdp_layouts, value, 255);
                }
                else if (g_strcasecmp(item, "layouts_map") == 0)
                {
                    g_strncpy(section_layouts_map, value, 255);
                }
            }
            list_clear(items);
            list_clear(values);
        }

        /* load the map */
        file_read_section(fd, section_rdp_layouts, items, values);
        for (index = 0; index < items->count; index++)
        {
            int rdp_layout_id;
            item = (char *)list_get_item(items, index);
            value = (char *)list_get_item(values, index);
            rdp_layout_id = g_htoi(value);
            if (rdp_layout_id == client_info->keylayout)
            {
                g_strncpy(rdp_layout, item, 255);
                break;
            }
        }
        list_clear(items);
        list_clear(values);
        file_read_section(fd, section_layouts_map, items, values);
        for (index = 0; index < items->count; index++)
        {
            item = (char *)list_get_item(items, index);
            value = (char *)list_get_item(values, index);
            if (g_strcasecmp(item, rdp_layout) == 0)
            {
                bytes = sizeof(client_info->layout);
                g_strncpy(client_info->layout, value, bytes - 1);
                break;
            }
        }

        list_delete(names);
        list_delete(items);
        list_delete(values);

        LOG(LOG_LEVEL_INFO, "xrdp_load_keyboard_layout: model [%s] variant [%s] "
            "layout [%s] options [%s]", client_info->model,
            client_info->variant, client_info->layout, client_info->options);
        g_file_close(fd);
    }
    else
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_load_keyboard_layout: error opening %s",
            keyboard_cfg_file);
    }
}

/*****************************************************************************/
struct xrdp_sec *
xrdp_sec_create(struct xrdp_rdp *owner, struct trans *trans)
{
    struct xrdp_sec *self;

    self = (struct xrdp_sec *) g_malloc(sizeof(struct xrdp_sec), 1);
    self->rdp_layer = owner;
    self->crypt_method = CRYPT_METHOD_NONE; /* set later */
    self->crypt_level = CRYPT_LEVEL_NONE;
    self->mcs_layer = xrdp_mcs_create(self, trans, &(self->client_mcs_data),
                                      &(self->server_mcs_data));
    self->fastpath_layer = xrdp_fastpath_create(self, trans);
    self->chan_layer = xrdp_channel_create(self, self->mcs_layer);
    self->is_security_header_present = 1;

    return self;
}

/*****************************************************************************/
void
xrdp_sec_delete(struct xrdp_sec *self)
{
    if (self == 0)
    {
        return;
    }

    xrdp_channel_delete(self->chan_layer);
    xrdp_mcs_delete(self->mcs_layer);
    xrdp_fastpath_delete(self->fastpath_layer);
    ssl_rc4_info_delete(self->decrypt_rc4_info); /* TODO clear all data */
    ssl_rc4_info_delete(self->encrypt_rc4_info); /* TODO clear all data */
    ssl_des3_info_delete(self->decrypt_fips_info);
    ssl_des3_info_delete(self->encrypt_fips_info);
    ssl_hmac_info_delete(self->sign_fips_info);
    g_free(self->client_mcs_data.data);
    g_free(self->server_mcs_data.data);
    /* Crypto information must always be cleared */
    g_memset(self, 0, sizeof(struct xrdp_sec));
    g_free(self);
}

/*****************************************************************************/
/* returns error */
int
xrdp_sec_init(struct xrdp_sec *self, struct stream *s)
{
    if (xrdp_mcs_init(self->mcs_layer, s) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_sec_init: xrdp_mcs_init failed");
        return 1;
    }

    if (self->crypt_level > CRYPT_LEVEL_NONE) /* RDP encryption */
    {
        if (self->crypt_level == CRYPT_LEVEL_FIPS)
        {
            s_push_layer(s, sec_hdr, 4 + 4 + 8);
        }
        else if (self->crypt_level > CRYPT_LEVEL_LOW)
        {
            s_push_layer(s, sec_hdr, 4 + 8);
        }
        else if (self->crypt_level)
        {
            s_push_layer(s, sec_hdr, 4);
        }
    }
    else
    {
        s_push_layer(s, sec_hdr, 0);
    }

    return 0;
}

/*****************************************************************************/
/* Reduce key entropy from 64 to 40 bits */
static void
xrdp_sec_make_40bit(char *key)
{
    key[0] = 0xd1;
    key[1] = 0x26;
    key[2] = 0x9e;
}

/*****************************************************************************/
/* returns error */
/* update an encryption key */
static int
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
static void
xrdp_sec_fips_decrypt(struct xrdp_sec *self, char *data, int len)
{
    LOG_DEVEL(LOG_LEVEL_DEBUG, "xrdp_sec_fips_decrypt:");
    ssl_des3_decrypt(self->decrypt_fips_info, len, data, data);
    self->decrypt_use_count++;
}

/*****************************************************************************/
static void
xrdp_sec_decrypt(struct xrdp_sec *self, char *data, int len)
{
    LOG_DEVEL(LOG_LEVEL_DEBUG, "xrdp_sec_decrypt:");
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
static void
xrdp_sec_fips_encrypt(struct xrdp_sec *self, char *data, int len)
{
    LOG_DEVEL(LOG_LEVEL_DEBUG, "xrdp_sec_fips_encrypt:");
    ssl_des3_encrypt(self->encrypt_fips_info, len, data, data);
    self->encrypt_use_count++;
}

/*****************************************************************************/
static void
xrdp_sec_encrypt(struct xrdp_sec *self, char *data, int len)
{
    LOG_DEVEL(LOG_LEVEL_DEBUG, "xrdp_sec_encrypt:");
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

/*****************************************************************************
 * convert utf-16 encoded string from stream into utf-8 string.
 * note: src_bytes doesn't include the null-terminator char.
 * Copied From: xrdp_sec.c
 */
static int
unicode_utf16_in(struct stream *s, int src_bytes, char *dst, int dst_len)
{
    twchar *src;
    int num_chars;
    int i;
    int bytes;

    LOG_DEVEL(LOG_LEVEL_TRACE, "unicode_utf16_in: uni_len %d, dst_len %d", src_bytes, dst_len);
    if (src_bytes == 0)
    {
        if (!s_check_rem_and_log(s, 2, "Parsing UTF-16"))
        {
            return 1;
        }
        LOG_DEVEL(LOG_LEVEL_TRACE, "unicode_utf16_in: num_chars 0, dst '' (empty string)");
        in_uint8s(s, 2); /* null terminator */
        return 0;
    }

    bytes = src_bytes + 2; /* include the null terminator */
    src = g_new0(twchar, bytes);
    for (i = 0; i < bytes / 2; ++i)
    {
        if (!s_check_rem_and_log(s, 2, "Parsing UTF-16"))
        {
            g_free(src);
            return 1;
        }
        in_uint16_le(s, src[i]);
    }
    num_chars = g_wcstombs(dst, src, dst_len);
    if (num_chars < 0)
    {
        g_memset(dst, '\0', dst_len);
    }
    LOG_DEVEL(LOG_LEVEL_TRACE, "unicode_utf16_in: num_chars %d, dst '%s'", num_chars, dst);
    g_free(src);

    return 0;
}

/*****************************************************************************/
/* Process TS_INFO_PACKET */
/* returns error */
static int
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
    char tmpdata[256];
    const char *sep;

    /* initialize (zero out) local variables */
    g_memset(tmpdata, 0, sizeof(char) * 256);
    if (!s_check_rem_and_log(s, 8, "Parsing [MS-RDPBCGR] TS_INFO_PACKET"))
    {
        return 1;
    }
    in_uint8s(s, 4);
    in_uint32_le(s, flags);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Parsing [MS-RDPBCGR] TS_INFO_PACKET");

    /* this is the first test that the decrypt is working */
    if ((flags & RDP_LOGON_NORMAL) != RDP_LOGON_NORMAL) /* 0x33 */
    {
        /* must be or error */
        LOG(LOG_LEVEL_ERROR, "received wrong flags, likely decrypt not working");
        return 1;
    }

    if (flags & RDP_LOGON_LEAVE_AUDIO)
    {
        self->rdp_layer->client_info.sound_code = 1;
        LOG_DEVEL(LOG_LEVEL_DEBUG, "[MS-RDPBCGR] TS_INFO_PACKET flag INFO_REMOTECONSOLEAUDIO found");
        LOG(LOG_LEVEL_DEBUG,
            "Client requested that audio on the server be played on the server.");
    }

    if (flags & RDP_LOGON_RAIL)
    {
        self->rdp_layer->client_info.rail_enable = 1;
        LOG_DEVEL(LOG_LEVEL_DEBUG, "[MS-RDPBCGR] TS_INFO_PACKET flag INFO_RAIL found");
        LOG(LOG_LEVEL_DEBUG,
            "Client requested Remote Application Integrated Locally (RAIL).");
    }

    if (flags & RDP_LOGON_AUTO)
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "[MS-RDPBCGR] TS_INFO_PACKET flag INFO_AUTOLOGON found");
        /* todo, for now not allowing autologon and mce both */
        if (!self->rdp_layer->client_info.is_mce)
        {
            self->rdp_layer->client_info.rdp_autologin = 1;
            LOG(LOG_LEVEL_DEBUG, "Client requested auto logon.");
        }
        else
        {
            LOG(LOG_LEVEL_WARNING, "Auto logon is not supported with MCE");
        }
    }

    if (flags & RDP_COMPRESSION)
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "[MS-RDPBCGR] TS_INFO_PACKET flag INFO_COMPRESSION found, "
                  "CompressionType 0x%1.1x", (flags & 0x00001E00) >> 9);
        /* TODO: check the client's supported compression type vs the server
           compression used */
        if (self->rdp_layer->client_info.use_bulk_comp)
        {

            self->rdp_layer->client_info.rdp_compression = 1;
            LOG(LOG_LEVEL_DEBUG, "Client requested compression enabled.");
        }
        else
        {
            LOG(LOG_LEVEL_DEBUG, "Client requested compression, but server "
                "compression is disabled.");
        }
    }

    if (!s_check_rem_and_log(s, 2, "Parsing [MS-RDPBCGR] TS_INFO_PACKET cbDomain"))
    {
        return 1;
    }
    in_uint16_le(s, len_domain);

    if (len_domain >= INFO_CLIENT_MAX_CB_LEN)
    {
        LOG(LOG_LEVEL_ERROR,
            "Client supplied domain is too long. Max length %d, domain length %d",
            INFO_CLIENT_MAX_CB_LEN, len_domain);
        return 1;
    }

    if (!s_check_rem_and_log(s, 2, "Parsing [MS-RDPBCGR] TS_INFO_PACKET cbUserName"))
    {
        return 1;
    }
    in_uint16_le(s, len_user);

    /*
     * Microsoft's itap client running on Mac OS/Android
     * always sends autologon credentials, even when user has not
     * configured any
     */
    if (len_user == 0 && self->rdp_layer->client_info.rdp_autologin)
    {
        LOG(LOG_LEVEL_DEBUG, "Client supplied user name is empty, disabling autologin");
        self->rdp_layer->client_info.rdp_autologin = 0;
    }

    if (len_user >= INFO_CLIENT_MAX_CB_LEN)
    {
        LOG(LOG_LEVEL_ERROR,
            "Client supplied user name is too long. Max length %d, user name length %d",
            INFO_CLIENT_MAX_CB_LEN, len_user);
        return 1;
    }

    if (!s_check_rem_and_log(s, 2, "Parsing [MS-RDPBCGR] TS_INFO_PACKET cbPassword"))
    {
        return 1;
    }
    in_uint16_le(s, len_password);

    if (len_password >= INFO_CLIENT_MAX_CB_LEN)
    {
        LOG(LOG_LEVEL_ERROR,
            "Client supplied password is too long. Max length %d, password length %d",
            INFO_CLIENT_MAX_CB_LEN, len_password);
        return 1;
    }

    if (!s_check_rem_and_log(s, 2, "Parsing [MS-RDPBCGR] TS_INFO_PACKET cbAlternateShell"))
    {
        return 1;
    }
    in_uint16_le(s, len_program);

    if (len_program >= INFO_CLIENT_MAX_CB_LEN)
    {
        LOG(LOG_LEVEL_ERROR,
            "Client supplied program name is too long. Max length %d, program name length %d",
            INFO_CLIENT_MAX_CB_LEN, len_program);
        return 1;
    }

    if (!s_check_rem_and_log(s, 2, "Parsing [MS-RDPBCGR] TS_INFO_PACKET cbWorkingDir"))
    {
        return 1;
    }
    in_uint16_le(s, len_directory);

    if (len_directory >= INFO_CLIENT_MAX_CB_LEN)
    {
        LOG(LOG_LEVEL_ERROR,
            "Client supplied directory name is too long. Max length %d, directory name length %d",
            INFO_CLIENT_MAX_CB_LEN, len_directory);
        return 1;
    }

    if (unicode_utf16_in(s, len_domain, self->rdp_layer->client_info.domain, sizeof(self->rdp_layer->client_info.domain) - 1) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "ERROR reading domain");
        return 1;
    }

    if (unicode_utf16_in(s, len_user, self->rdp_layer->client_info.username, sizeof(self->rdp_layer->client_info.username) - 1) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "ERROR reading user name");
        return 1;
    }

    if (flags & RDP_LOGON_AUTO)
    {
        if (unicode_utf16_in(s, len_password, self->rdp_layer->client_info.password, sizeof(self->rdp_layer->client_info.password) - 1) != 0)
        {
            LOG(LOG_LEVEL_ERROR, "ERROR reading password");
            return 1;
        }
    }
    else if (self->rdp_layer->client_info.enable_token_login
             && len_user > 0
             && len_password == 0
             && (sep = g_strchr(self->rdp_layer->client_info.username, '\x1f')) != NULL)
    {
        LOG(LOG_LEVEL_DEBUG, "Client supplied a Logon token. Overwritting password with logon token.");
        g_strncpy(self->rdp_layer->client_info.password, sep + 1,
                  sizeof(self->rdp_layer->client_info.password) - 1);
        self->rdp_layer->client_info.username[sep - self->rdp_layer->client_info.username] = '\0';
        self->rdp_layer->client_info.rdp_autologin = 1;
    }
    else
    {
        if (!s_check_rem_and_log(s, len_password + 2, "Parsing [MS-RDPBCGR] TS_INFO_PACKET Password"))
        {
            return 1;
        }
        in_uint8s(s, len_password + 2);
        if (self->rdp_layer->client_info.require_credentials)
        {
            LOG(LOG_LEVEL_ERROR, "Server is configured to require that the "
                "client enable auto logon with credentials, but the client did "
                "not request auto logon.");
            return 1; /* credentials on cmd line is mandatory */
        }
    }
    if (self->rdp_layer->client_info.domain_user_separator[0] != '\0'
            && self->rdp_layer->client_info.domain[0] != '\0')
    {
        LOG(LOG_LEVEL_DEBUG, "Client supplied domain with user name. Overwritting user name with user name parsed from domain.");
        int size = sizeof(self->rdp_layer->client_info.username);
        g_strncat(self->rdp_layer->client_info.username, self->rdp_layer->client_info.domain_user_separator, size - 1 - g_strlen(self->rdp_layer->client_info.domain_user_separator));
        g_strncat(self->rdp_layer->client_info.username, self->rdp_layer->client_info.domain, size - 1 - g_strlen(self->rdp_layer->client_info.domain));
    }

    if (unicode_utf16_in(s, len_program, self->rdp_layer->client_info.program, sizeof(self->rdp_layer->client_info.program) - 1) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "ERROR reading program");
        return 1;
    }

    if (unicode_utf16_in(s, len_directory, self->rdp_layer->client_info.directory, sizeof(self->rdp_layer->client_info.directory) - 1) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "ERROR reading directory");
        return 1;
    }

    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPBCGR] TS_INFO_PACKET "
              "CodePage (ignored), flags 0x%8.8x, cbDomain %d, cbUserName %d, "
              "cbPassword %d, cbAlternateShell %d, cbWorkingDir %d, Domain %s, "
              "UserName %s, Password (omitted from log), AlternateShell %s, "
              "WorkingDir %s", flags, len_domain,
              len_user, len_password, len_program, len_directory,
              self->rdp_layer->client_info.domain,
              self->rdp_layer->client_info.username,
              self->rdp_layer->client_info.program,
              self->rdp_layer->client_info.directory);
    LOG(LOG_LEVEL_DEBUG, "Client supplied domain: %s", self->rdp_layer->client_info.domain);
    LOG(LOG_LEVEL_DEBUG, "Client supplied username: %s", self->rdp_layer->client_info.username);
    LOG(LOG_LEVEL_DEBUG, "Client supplied password: <omitted from log>");
    LOG(LOG_LEVEL_DEBUG, "Client supplied program: %s", self->rdp_layer->client_info.program);
    LOG(LOG_LEVEL_DEBUG, "Client supplied directory: %s", self->rdp_layer->client_info.directory);

    /* TODO: explain why the windows key flag is used to determine if the
       TS_EXTENDED_INFO_PACKET should be parsed */
    if (flags & RDP_LOGON_BLOB) /* INFO_ENABLEWINDOWSKEY */
    {
        if (!s_check_rem_and_log(s, 4, "Parsing [MS-RDPBCGR] TS_EXTENDED_INFO_PACKET "
                                 "clientAddressFamily and cbClientAddress"))
        {
            return 1;
        }
        /* TS_EXTENDED_INFO_PACKET requiered fields */
        in_uint8s(s, 2);         /* clientAddressFamily */
        in_uint16_le(s, len_ip);
        if (unicode_utf16_in(s, len_ip - 2, tmpdata, sizeof(tmpdata) - 1) != 0)
        {
            LOG(LOG_LEVEL_ERROR, "ERROR reading ip");
            return 1;
        }
        if (!s_check_rem_and_log(s, 2, "Parsing [MS-RDPBCGR] TS_EXTENDED_INFO_PACKET clientDir"))
        {
            return 1;
        }
        in_uint16_le(s, len_dll);
        if (unicode_utf16_in(s, len_dll - 2, tmpdata, sizeof(tmpdata) - 1) != 0)
        {
            LOG(LOG_LEVEL_ERROR, "ERROR reading clientDir");
            return 1;
        }
        LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPBCGR] TS_EXTENDED_INFO_PACKET "
                  "<Requiered Fields> clientAddressFamily (ignored), "
                  "cbClientAddress (ignored), clientAddress (ignored), "
                  "cbClientDir (ignored), clientDir (ignored)");

        /* TODO: MS-BCGR 2.2.1.11.1.1.1 says that all fields after the
           client directory are optional. */
        if (!s_check_rem_and_log(s, 4 + 64 + 20 + 64 + 20 + 4 + 4,
                                 "Parsing [MS-RDPBCGR] TS_EXTENDED_INFO_PACKET "
                                 "clientTimeZone, clientSessionId, and performanceFlags"))
        {
            return 1;
        }
        /* TS_TIME_ZONE_INFORMATION */
        in_uint8s(s, 4);   /* Bias (4) */
        in_uint8s(s, 64);  /* StandardName (64) */
        in_uint8s(s, 20);  /* StandardDate (16), StandardBias (4) */
        in_uint8s(s, 64);  /* DaylightName (64) */
        in_uint8s(s, 20);  /* DaylightDate (16), DaylightBias (4) */
        in_uint8s(s, 4);   /* TS_EXTENDED_INFO_PACKET clientSessionId (4) */

        /* TS_EXTENDED_INFO_PACKET optional fields */
        in_uint32_le(s, self->rdp_layer->client_info.rdp5_performanceflags);

        LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPBCGR] TS_EXTENDED_INFO_PACKET "
                  "<Optional Fields> clientTimeZone (ignored), "
                  "clientSessionId (ignored), performanceFlags 0x%8.8x, "
                  "cbAutoReconnectCookie (ignored), autoReconnectCookie (ignored), "
                  "reserved1 (ignored), reserved2 (ignored), "
                  "cbDynamicDSTTimeZoneKeyName (ignored), "
                  "dynamicDSTTimeZoneKeyName (ignored), "
                  "dynamicDaylightTimeDisabled (ignored)",
                  self->rdp_layer->client_info.rdp5_performanceflags);
    }

    return 0;
}

/*****************************************************************************/
/* returns error */
static int
xrdp_sec_send_lic_initial(struct xrdp_sec *self)
{
    struct stream *s;


    make_stream(s);
    init_stream(s, 8192);

    if (xrdp_mcs_init(self->mcs_layer, s) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_sec_send_lic_initial: xrdp_mcs_init failed");
        free_stream(s);
        return 1;
    }

    out_uint8a(s, g_lic1, sizeof(g_lic1));
    s_mark_end(s);

    LOG_DEVEL(LOG_LEVEL_TRACE, "Sending [MS-RDPELE] SERVER_LICENSE_REQUEST");
    if (xrdp_mcs_send(self->mcs_layer, s, MCS_GLOBAL_CHANNEL) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "Sending [MS-RDPELE] SERVER_LICENSE_REQUEST failed");
        free_stream(s);
        return 1;
    }

    free_stream(s);
    return 0;
}

/*****************************************************************************/
/*
 * Send a [MS-RDPELE] LICENSE_ERROR_MESSAGE with STATUS_VALID_CLIENT
 * See also: [MS-RDPELE] 1.3.3 Licensing PDU Flows
 */
/* returns error */
static int
xrdp_sec_send_lic_response(struct xrdp_sec *self)
{
    struct stream *s;

    make_stream(s);
    init_stream(s, 8192);

    if (xrdp_mcs_init(self->mcs_layer, s) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_sec_send_lic_response: xrdp_mcs_init failed");
        free_stream(s);
        return 1;
    }

    out_uint8a(s, g_lic2, sizeof(g_lic2));
    s_mark_end(s);

    LOG_DEVEL(LOG_LEVEL_TRACE, "Sending [MS-RDPELE] LICENSE_ERROR_MESSAGE with STATUS_VALID_CLIENT");
    if (xrdp_mcs_send(self->mcs_layer, s, MCS_GLOBAL_CHANNEL) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "Sending [MS-RDPELE] LICENSE_ERROR_MESSAGE with STATUS_VALID_CLIENT failed");
        free_stream(s);
        return 1;
    }

    free_stream(s);
    return 0;
}

/*****************************************************************************/
/* returns error */
static int
xrdp_sec_send_media_lic_response(struct xrdp_sec *self)
{
    struct stream *s;

    make_stream(s);
    init_stream(s, 8192);

    if (xrdp_mcs_init(self->mcs_layer, s) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_sec_send_media_lic_response: xrdp_mcs_init failed");
        free_stream(s);
        return 1;
    }

    out_uint8a(s, g_lic3, sizeof(g_lic3));
    s_mark_end(s);

    LOG_DEVEL(LOG_LEVEL_TRACE, "Sending [MS-RDPELE] LICENSE_ERROR_MESSAGE with STATUS_VALID_CLIENT");
    if (xrdp_mcs_send(self->mcs_layer, s, MCS_GLOBAL_CHANNEL) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "Sending [MS-RDPELE] LICENSE_ERROR_MESSAGE with STATUS_VALID_CLIENT failed");
        free_stream(s);
        return 1;
    }

    free_stream(s);
    return 0;
}

/*****************************************************************************/
static void
xrdp_sec_rsa_op(struct xrdp_sec *self, char *out, char *in, int in_bytes,
                char *mod, char *exp)
{
    ssl_mod_exp(out, self->rsa_key_bytes, in, in_bytes,
                mod, self->rsa_key_bytes,
                exp, self->rsa_key_bytes);
}

/*****************************************************************************/
static void
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
static void
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
static void
fips_expand_key_bits(const char *in, char *out)
{
    tui8 buf[32];
    tui8 c;
    int i;
    int b;
    int p;
    int r;

    /* reverse every byte in the key */
    for (i = 0; i < 21; i++)
    {
        c = in[i];
        buf[i] = g_fips_reverse_table[c];
    }
    /* insert a zero-bit after every 7th bit */
    for (i = 0, b = 0; i < 24; i++, b += 7)
    {
        p = b / 8;
        r = b % 8;
        if (r == 0)
        {
            out[i] = buf[p] & 0xfe;
        }
        else
        {
            /* c is accumulator */
            c = buf[p] << r;
            c |= buf[p + 1] >> (8 - r);
            out[i] = c & 0xfe;
        }
    }
    /* reverse every byte */
    /* alter lsb so the byte has odd parity */
    for (i = 0; i < 24; i++)
    {
        c = out[i];
        c = g_fips_reverse_table[c];
        out[i] = g_fips_oddparity_table[c];
    }
}

/****************************************************************************/
static void
xrdp_sec_fips_establish_keys(struct xrdp_sec *self)
{
    char server_encrypt_key[32];
    char server_decrypt_key[32];
    const char *fips_ivec;
    void *sha1;

    LOG_DEVEL(LOG_LEVEL_INFO, "xrdp_sec_fips_establish_keys:");

    sha1 = ssl_sha1_info_create();
    ssl_sha1_clear(sha1);
    ssl_sha1_transform(sha1, self->client_random + 16, 16);
    ssl_sha1_transform(sha1, self->server_random + 16, 16);
    ssl_sha1_complete(sha1, server_decrypt_key);

    server_decrypt_key[20] = server_decrypt_key[0];
    fips_expand_key_bits(server_decrypt_key, self->fips_decrypt_key);
    ssl_sha1_info_delete(sha1);

    sha1 = ssl_sha1_info_create();
    ssl_sha1_clear(sha1);
    ssl_sha1_transform(sha1, self->client_random, 16);
    ssl_sha1_transform(sha1, self->server_random, 16);
    ssl_sha1_complete(sha1, server_encrypt_key);
    server_encrypt_key[20] = server_encrypt_key[0];
    fips_expand_key_bits(server_encrypt_key, self->fips_encrypt_key);
    ssl_sha1_info_delete(sha1);

    sha1 = ssl_sha1_info_create();
    ssl_sha1_clear(sha1);
    ssl_sha1_transform(sha1, server_encrypt_key, 20);
    ssl_sha1_transform(sha1, server_decrypt_key, 20);
    ssl_sha1_complete(sha1, self->fips_sign_key);
    ssl_sha1_info_delete(sha1);

    fips_ivec = (const char *) g_fips_ivec;
    self->encrypt_fips_info =
        ssl_des3_encrypt_info_create(self->fips_encrypt_key, fips_ivec);
    self->decrypt_fips_info =
        ssl_des3_decrypt_info_create(self->fips_decrypt_key, fips_ivec);
    self->sign_fips_info = ssl_hmac_info_create();
}

/****************************************************************************/
static void
xrdp_sec_establish_keys(struct xrdp_sec *self)
{
    char session_key[48];
    char temp_hash[48];
    char input[48];

    LOG_DEVEL(LOG_LEVEL_INFO, "xrdp_sec_establish_keys:");

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

    if (self->crypt_method == CRYPT_METHOD_40BIT)
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
int
xrdp_sec_recv_fastpath(struct xrdp_sec *self, struct stream *s)
{
    int ver;
    int len;
    int pad;

#ifndef USE_DEVEL_LOGGING
    /* TODO: remove UNUSED_VAR once the `ver` variable is used for more than
    logging in debug mode */
    UNUSED_VAR(ver);
#endif

    if (xrdp_fastpath_recv(self->fastpath_layer, s) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_sec_recv_fastpath: xrdp_fastpath_recv failed");
        return 1;
    }

    if (self->fastpath_layer->secFlags & FASTPATH_INPUT_ENCRYPTED)
    {
        if (self->crypt_level == CRYPT_LEVEL_FIPS)
        {
            if (!s_check_rem_and_log(s, 12, "Parsing [MS-RDPBCGR] TS_FP_FIPS_INFO"))
            {
                return 1;
            }
            /* TS_FP_FIPS_INFO */
            in_uint16_le(s, len);
            in_uint8(s, ver); /* length (2 bytes) */
            in_uint8(s, pad);
            LOG_DEVEL(LOG_LEVEL_TRACE, "Received header [MS-RDPBCGR] TS_FP_FIPS_INFO "
                      "length %d, version %d, padlen %d", len, ver, pad);
            if (len != 0x10)  /* length MUST set to 0x10 */
            {
                LOG(LOG_LEVEL_ERROR, "Received header [MS-RDPBCGR] TS_FP_FIPS_INFO "
                    "invalid fastpath length. Expected 16, received %d", len);
                return 1;
            }

            /* remainder of TS_FP_INPUT_PDU */
            in_uint8s(s, 8);  /* dataSignature (8 bytes), skip for now */
            LOG_DEVEL(LOG_LEVEL_TRACE, "CRYPT_LEVEL_FIPS - data len %d",
                      (int)(s->end - s->p));
            xrdp_sec_fips_decrypt(self, s->p, (int)(s->end - s->p));
            s->end -= pad;
        }
        else
        {
            if (!s_check_rem_and_log(s, 8,
                                     "Parsing [MS-RDPBCGR] TS_FP_INPUT_PDU dataSignature"))
            {
                return 1;
            }
            /* remainder of TS_FP_INPUT_PDU */
            in_uint8s(s, 8);  /* dataSignature (8 bytes), skip for now */
            xrdp_sec_decrypt(self, s->p, (int)(s->end - s->p));
        }
    }

    if (self->fastpath_layer->numEvents == 0) /* set by xrdp_fastpath_recv() */
    {
        /**
         * If numberEvents is not provided in fpInputHeader, it will be provided
         * as one additional byte here.
         */
        if (!s_check_rem_and_log(s, 8, "Parsing [MS-RDPBCGR] TS_FP_INPUT_PDU numEvents"))
        {
            return 1;
        }
        in_uint8(s, self->fastpath_layer->numEvents); /* numEvents (1 byte) (optional) */
    }

    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPBCGR] TS_FP_INPUT_PDU "
              "fpInputHeader.action (ignored), "
              "fpInputHeader.numEvents (see final numEvents), "
              "fpInputHeader.flags %d, length1 (ToDo), length2 (ToDo), "
              "fipsInformation %s, dataSignature (ignored), numEvents %d",
              self->fastpath_layer->secFlags,
              (self->fastpath_layer->secFlags & FASTPATH_INPUT_ENCRYPTED) ? "(see above)" : "(not present)",
              self->fastpath_layer->numEvents);

    return 0;
}
/*****************************************************************************/
/* returns error */
int
xrdp_sec_recv(struct xrdp_sec *self, struct stream *s, int *chan)
{
    int flags;
    int len;
    int ver;
    int pad;


    if (xrdp_mcs_recv(self->mcs_layer, s, chan) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_sec_recv: xrdp_mcs_recv failed");
        return 1;
    }

    /* TODO: check if moving this check until after the is_security_header_present
    causes any issues.
    the security header is optional (eg. TLS connections), so this
    check should really be after the check if the security header is present,
    this currently seems to be working by coincidence at the moment. */
    if (!s_check_rem_and_log(s, 4, "Parsing [MS-RDPBCGR] TS_SECURITY_HEADER"))
    {
        return 1;
    }

    if (!(self->is_security_header_present))
    {
        /* noisy log statement with no real info since this is an
           expected state for TLS connections
        */
        LOG_DEVEL(LOG_LEVEL_TRACE, "xrdp_sec_recv: security header NOT present");
        return 0;
    }

    /* TS_SECURITY_HEADER */
    in_uint32_le(s, flags);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received header [MS-RDPBCGR] TS_SECURITY_HEADER "
              "flags 0x%8.8x, flagsHi (merged with flags)", flags);

    if (flags & SEC_ENCRYPT) /* 0x08 */
    {
        if (self->crypt_level == CRYPT_LEVEL_FIPS)
        {
            if (!s_check_rem_and_log(s, 12, "Parsing [MS-RDPBCGR] TS_SECURITY_HEADER2"))
            {
                return 1;
            }
            /* TS_SECURITY_HEADER2 */
            in_uint16_le(s, len); /* length */
            in_uint8(s, ver); /* version */
            in_uint8(s, pad); /* padlen */
            in_uint8s(s, 8); /* signature(8) */
            LOG_DEVEL(LOG_LEVEL_TRACE, "Received header [MS-RDPBCGR] TS_SECURITY_HEADER2 "
                      "length %d, version %d, padlen %d, dataSignature (ignored)",
                      len, ver, pad);
            if (len != 16)
            {
                LOG(LOG_LEVEL_ERROR, "Received header [MS-RDPBCGR] TS_SECURITY_HEADER2 "
                    "has unexpected length. Expected 16, actual %d", len);
                return 1;
            }
            if (ver != 1)
            {
                LOG(LOG_LEVEL_ERROR, "Received header [MS-RDPBCGR] TS_SECURITY_HEADER2 "
                    "has unexpected version. Expected 1, actual %d", ver);
                return 1;
            }
            xrdp_sec_fips_decrypt(self, s->p, (int)(s->end - s->p));
            s->end -= pad;
        }
        else if (self->crypt_level > CRYPT_LEVEL_NONE)
        {
            if (!s_check_rem_and_log(s, 8, "Parsing [MS-RDPBCGR] TS_SECURITY_HEADER1"))
            {
                return 1;
            }
            /* TS_SECURITY_HEADER1 */
            in_uint8s(s, 8); /* signature(8) */
            LOG_DEVEL(LOG_LEVEL_TRACE, "Received header [MS-RDPBCGR] TS_SECURITY_HEADER1 "
                      "dataSignature (ignored)");
            xrdp_sec_decrypt(self, s->p, (int)(s->end - s->p));
        }
    }

    if (flags & SEC_CLIENT_RANDOM) /* 0x01 TS_SECURITY_PACKET */
    {
        if (!s_check_rem_and_log(s, 4, "Parsing [MS-RDPBCGR] TS_SECURITY_PACKET"))
        {
            return 1;
        }
        in_uint32_le(s, len);
        /* 512, 2048 bit */
        if ((len != 64 + 8) && (len != 256 + 8))
        {
            LOG(LOG_LEVEL_ERROR, "xrdp_sec_recv : error - unexpected length %d", len);
            return 1;
        }
        if (!s_check_rem_and_log(s, len - 8,
                                 "Parsing [MS-RDPBCGR] TS_SECURITY_PACKET encryptedClientRandom"))
        {
            return 1;
        }
        in_uint8a(s, self->client_crypt_random, len - 8);

        xrdp_sec_rsa_op(self, self->client_random, self->client_crypt_random,
                        len - 8, self->pub_mod, self->pri_exp);
        LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPBCGR] TS_SECURITY_PACKET "
                  "length %d, encryptedClientRandom (see below)", len);
        LOG_DEVEL_HEXDUMP(LOG_LEVEL_TRACE, "encryptedClientRandom", self->client_crypt_random, len - 8);
        LOG_DEVEL_HEXDUMP(LOG_LEVEL_TRACE, "decrypted encryptedClientRandom", self->client_random, 256);
        if (self->crypt_level == CRYPT_LEVEL_FIPS)
        {
            xrdp_sec_fips_establish_keys(self);
        }
        else if (self->crypt_method != CRYPT_METHOD_NONE)
        {
            xrdp_sec_establish_keys(self);
        }
        *chan = 1; /* just set a non existing channel and exit */
        LOG_DEVEL(LOG_LEVEL_DEBUG, "xrdp_sec_recv: out channel 1 (non-existing channel)");
        return 0;
    }

    if (flags & SEC_LOGON_INFO) /* 0x40 SEC_INFO_PKT */
    {
        if (xrdp_sec_process_logon_info(self, s) != 0)
        {
            LOG(LOG_LEVEL_ERROR, "xrdp_sec_recv: xrdp_sec_process_logon_info failed");
            return 1;
        }

        if (self->rdp_layer->client_info.is_mce)
        {
            if (xrdp_sec_send_media_lic_response(self) != 0)
            {
                LOG(LOG_LEVEL_ERROR, "xrdp_sec_recv: xrdp_sec_send_media_lic_response failed");
                return 1;
            }

            LOG_DEVEL(LOG_LEVEL_DEBUG, "xrdp_sec_recv: out 'send demand active'");
            return -1; /* special error that means send demand active */
        }

        if (xrdp_sec_send_lic_initial(self) != 0)
        {
            LOG(LOG_LEVEL_ERROR, "xrdp_sec_recv: xrdp_sec_send_lic_initial failed");
            return 1;
        }

        *chan = 1; /* just set a non existing channel and exit */
        LOG_DEVEL(LOG_LEVEL_DEBUG, "xrdp_sec_recv: out channel 1 (non-existing channel)");
        return 0;
    }

    if (flags & SEC_LICENCE_NEG) /* 0x80 SEC_LICENSE_PKT */
    {
        if (xrdp_sec_send_lic_response(self) != 0)
        {
            LOG(LOG_LEVEL_ERROR, "xrdp_sec_recv: xrdp_sec_send_lic_response failed");
            return 1;
        }

        if (self->crypt_level == CRYPT_LEVEL_NONE
                && self->crypt_method == CRYPT_METHOD_NONE)
        {
            /* in tls mode, no more security header from now on */
            self->is_security_header_present = 0;
        }

        LOG_DEVEL(LOG_LEVEL_DEBUG, "xrdp_sec_recv: out 'send demand active'");
        return -1; /* special error that means send demand active */
    }

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
static void
xrdp_sec_fips_sign(struct xrdp_sec *self, char *out, int out_len,
                   char *data, int data_len)
{
    char buf[20];
    char lenhdr[4];

    buf_out_uint32(lenhdr, self->encrypt_use_count);
    ssl_hmac_sha1_init(self->sign_fips_info, self->fips_sign_key, 20);
    ssl_hmac_transform(self->sign_fips_info, data, data_len);
    ssl_hmac_transform(self->sign_fips_info, lenhdr, 4);
    ssl_hmac_complete(self->sign_fips_info, buf, 20);
    g_memcpy(out, buf, out_len);
}

/*****************************************************************************/
/* Generate a MAC hash (5.2.3.1), using a combination of SHA1 and MD5 */
static void
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
int
xrdp_sec_send(struct xrdp_sec *self, struct stream *s, int chan)
{
    int datalen;
    int pad;

    s_pop_layer(s, sec_hdr);

    if (self->crypt_level > CRYPT_LEVEL_NONE)
    {
        if (self->crypt_level == CRYPT_LEVEL_FIPS)
        {
            out_uint32_le(s, SEC_ENCRYPT);
            datalen = (int)((s->end - s->p) - 12);
            out_uint16_le(s, 16); /* crypto header size */
            out_uint8(s, 1); /* fips version */
            pad = (8 - (datalen % 8)) & 7;
            g_memset(s->end, 0, pad);
            s->end += pad;
            out_uint8(s, pad); /* fips pad */
            xrdp_sec_fips_sign(self, s->p, 8, s->p + 8, datalen);
            xrdp_sec_fips_encrypt(self, s->p + 8, datalen + pad);
            LOG_DEVEL(LOG_LEVEL_TRACE, "Adding header [MS-RDPBCGR] TS_SECURITY_HEADER2 "
                      "flags 0x%4.4x, flagsHi 0x%4.4x, length 16, version 1, "
                      "padlen %d, dataSignature 0x%8.8x 0x%8.8x",
                      SEC_ENCRYPT & 0xffff, (SEC_ENCRYPT & 0xffff0000) >> 16,
                      pad, *((uint32_t *) s->p), *((uint32_t *) (s->p + 4)));
        }
        else if (self->crypt_level > CRYPT_LEVEL_LOW)
        {
            out_uint32_le(s, SEC_ENCRYPT);
            datalen = (int)((s->end - s->p) - 8);
            xrdp_sec_sign(self, s->p, 8, s->p + 8, datalen);
            xrdp_sec_encrypt(self, s->p + 8, datalen);
            LOG_DEVEL(LOG_LEVEL_TRACE, "Adding header [MS-RDPBCGR] TS_SECURITY_HEADER1 "
                      "flags 0x%4.4x, flagsHi 0x%4.4x, dataSignature 0x%8.8x 0x%8.8x",
                      SEC_ENCRYPT & 0xffff, (SEC_ENCRYPT & 0xffff0000) >> 16,
                      *((uint32_t *) s->p), *((uint32_t *) (s->p + 4)));
        }
        else
        {
            out_uint32_le(s, 0);
            LOG_DEVEL(LOG_LEVEL_TRACE, "Adding header [MS-RDPBCGR] TS_SECURITY_HEADER "
                      "flags 0x0000, flagsHi 0x0000");
        }
    }

    if (xrdp_mcs_send(self->mcs_layer, s, chan) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_sec_send: xrdp_mcs_send failed");
        return 1;
    }

    return 0;
}

/*****************************************************************************/
/* returns the fastpath sec byte count */
int
xrdp_sec_get_fastpath_bytes(struct xrdp_sec *self)
{
    if (self->crypt_level == CRYPT_LEVEL_FIPS)
    {
        return 3 + 4 + 8;
    }
    else if (self->crypt_level > CRYPT_LEVEL_LOW)
    {
        return 3 + 8;
    }
    return 3;
}

/*****************************************************************************/
/* returns error */
int
xrdp_sec_init_fastpath(struct xrdp_sec *self, struct stream *s)
{
    if (xrdp_fastpath_init(self->fastpath_layer, s) != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "xrdp_sec_init_fastpath: xrdp_fastpath_init failed");
        return 1;
    }
    if (self->crypt_level == CRYPT_LEVEL_FIPS)
    {
        s_push_layer(s, sec_hdr, 3 + 4 + 8);
    }
    else if (self->crypt_level > CRYPT_LEVEL_LOW)
    {
        s_push_layer(s, sec_hdr, 3 + 8);
    }
    else
    {
        s_push_layer(s, sec_hdr, 3);
    }
    return 0;
}

/*****************************************************************************/
/* returns error */
/* 2.2.9.1.2 Server Fast-Path Update PDU (TS_FP_UPDATE_PDU)
 * http://msdn.microsoft.com/en-us/library/cc240621.aspx */
int
xrdp_sec_send_fastpath(struct xrdp_sec *self, struct stream *s)
{
    int secFlags;
    int fpOutputHeader;
    int datalen;
    int pdulen;
    int pad;
    int error;
    char save[8];

    error = 0;
    s_pop_layer(s, sec_hdr);
    if (self->crypt_level == CRYPT_LEVEL_FIPS)
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "xrdp_sec_send_fastpath: fips");
        pdulen = (int)(s->end - s->p);
        datalen = pdulen - 15;
        pad = (8 - (datalen % 8)) & 7;
        secFlags = 0x2;
        fpOutputHeader = secFlags << 6;
        out_uint8(s, fpOutputHeader);
        pdulen += pad;
        pdulen |= 0x8000;
        out_uint16_be(s, pdulen);
        out_uint16_le(s, 16); /* crypto header size */
        out_uint8(s, 1); /* fips version */
        s->end += pad;
        out_uint8(s, pad); /* fips pad */
        xrdp_sec_fips_sign(self, s->p, 8, s->p + 8, datalen);
        g_memcpy(save, s->p + 8 + datalen, pad);
        g_memset(s->p + 8 + datalen, 0, pad);
        xrdp_sec_fips_encrypt(self, s->p + 8, datalen + pad);
        LOG_DEVEL(LOG_LEVEL_TRACE, "Sending [MS-RDPBCGR] TS_FP_UPDATE_PDU "
                  "fpOutputHeader.action 0, fpOutputHeader.reserved 0, "
                  "fpOutputHeader.flags 0x2, length1 0x%2.2x, length2 0x%2.2x, "
                  "fipsInformation.length 16, fipsInformation.version 1, "
                  "fipsInformation.padlen %d, dataSignature 0x%8.8x 0x%8.8x, ",
                  pdulen >> 4, pdulen & 0xff, pad,
                  *((uint32_t *) s->p), *((uint32_t *) (s->p + 4)));
        error = xrdp_fastpath_send(self->fastpath_layer, s);
        g_memcpy(s->p + 8 + datalen, save, pad);
    }
    else if (self->crypt_level > CRYPT_LEVEL_LOW)
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "xrdp_sec_send_fastpath: crypt");
        pdulen = (int)(s->end - s->p);
        datalen = pdulen - 11;
        secFlags = 0x2;
        fpOutputHeader = secFlags << 6;
        out_uint8(s, fpOutputHeader);
        pdulen |= 0x8000;
        out_uint16_be(s, pdulen);
        xrdp_sec_sign(self, s->p, 8, s->p + 8, datalen);
        xrdp_sec_encrypt(self, s->p + 8, datalen);
        LOG_DEVEL(LOG_LEVEL_TRACE, "Sending [MS-RDPBCGR] TS_FP_UPDATE_PDU "
                  "fpOutputHeader.action 0, fpOutputHeader.reserved 0, "
                  "fpOutputHeader.flags 0x2, length1 0x%2.2x, length2 0x%2.2x, "
                  "dataSignature 0x%8.8x 0x%8.8x, ",
                  pdulen >> 4, pdulen & 0xff,
                  *((uint32_t *) s->p), *((uint32_t *) (s->p + 4)));
        error = xrdp_fastpath_send(self->fastpath_layer, s);
    }
    else
    {
        LOG_DEVEL(LOG_LEVEL_DEBUG, "xrdp_sec_send_fastpath: no crypt");
        pdulen = (int)(s->end - s->p);
        secFlags = 0x0;
        fpOutputHeader = secFlags << 6;
        out_uint8(s, fpOutputHeader);
        pdulen |= 0x8000;
        out_uint16_be(s, pdulen);
        LOG_DEVEL(LOG_LEVEL_TRACE, "Sending [MS-RDPBCGR] TS_FP_UPDATE_PDU "
                  "fpOutputHeader.action 0, fpOutputHeader.reserved 0, "
                  "fpOutputHeader.flags 0, length1 0x%2.2x, length2 0x%2.2x",
                  pdulen >> 4, pdulen & 0xff);
        error = xrdp_fastpath_send(self->fastpath_layer, s);
    }
    if (error != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "xrdp_sec_send_fastpath: xrdp_fastpath_send failed");
        return 1;
    }
    return 0;
}

/*****************************************************************************/
/* http://msdn.microsoft.com/en-us/library/cc240510.aspx
   2.2.1.3.2 Client Core Data (TS_UD_CS_CORE) */
static int
xrdp_sec_process_mcs_data_CS_CORE(struct xrdp_sec *self, struct stream *s)
{
    int colorDepth;
    int postBeta2ColorDepth;
    int highColorDepth;
    int supportedColorDepths;
    int earlyCapabilityFlags;
    char clientName[INFO_CLIENT_NAME_BYTES / 2] = { '\0' };

    /* TS_UD_CS_CORE requiered fields */
    in_uint8s(s, 4); /* version */
    in_uint16_le(s, self->rdp_layer->client_info.width);
    in_uint16_le(s, self->rdp_layer->client_info.height);
    in_uint16_le(s, colorDepth);
    switch (colorDepth)
    {
        case RNS_UD_COLOR_4BPP:
            self->rdp_layer->client_info.bpp = 4;
            break;
        case RNS_UD_COLOR_8BPP:
            self->rdp_layer->client_info.bpp = 8;
            break;
    }
    in_uint8s(s, 2); /* SASSequence */
    in_uint8s(s, 4); /* keyboardLayout */
    in_uint8s(s, 4); /* clientBuild */
    unicode_utf16_in(s, INFO_CLIENT_NAME_BYTES - 2, clientName, sizeof(clientName) - 1);  /* clientName */
    LOG(LOG_LEVEL_INFO, "Connected client computer name: %s", clientName);
    in_uint8s(s, 4); /* keyboardType */
    in_uint8s(s, 4); /* keyboardSubType */
    in_uint8s(s, 4); /* keyboardFunctionKey */
    in_uint8s(s, 64); /* imeFileName */
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPBCGR] TS_UD_CS_CORE "
              "<Requiered fields> version (ignored), desktopWidth %d, "
              "desktopHeight %d, colorDepth %s, SASSequence (ingored), "
              "keyboardLayout (ignored), clientBuild (ignored), "
              "clientName %s, keyboardType (ignored), "
              "keyboardSubType (ignored), keyboardFunctionKey (ignored), "
              "imeFileName (ignroed)",
              self->rdp_layer->client_info.width,
              self->rdp_layer->client_info.height,
              (colorDepth == 0xca00 ? "RNS_UD_COLOR_4BPP" :
               colorDepth == 0xca01 ? "RNS_UD_COLOR_8BPP" : "unknown"),
              clientName);

    /* TS_UD_CS_CORE optional fields */
    in_uint16_le(s, postBeta2ColorDepth);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPBCGR] TS_UD_CS_CORE "
              "<Optional Field> postBeta2ColorDepth %s",
              postBeta2ColorDepth == 0xca00 ? "RNS_UD_COLOR_4BPP" :
              postBeta2ColorDepth == 0xca01 ? "RNS_UD_COLOR_8BPP" :
              postBeta2ColorDepth == 0xca02 ? "RNS_UD_COLOR_16BPP_555" :
              postBeta2ColorDepth == 0xca03 ? "RNS_UD_COLOR_16BPP_565" :
              postBeta2ColorDepth == 0xca04 ? "RNS_UD_COLOR_24BPP" :
              "unknown");

    switch (postBeta2ColorDepth)
    {
        case RNS_UD_COLOR_4BPP:
            self->rdp_layer->client_info.bpp = 4;
            break;
        case RNS_UD_COLOR_8BPP :
            self->rdp_layer->client_info.bpp = 8;
            break;
        case RNS_UD_COLOR_16BPP_555:
            self->rdp_layer->client_info.bpp = 15;
            break;
        case RNS_UD_COLOR_16BPP_565:
            self->rdp_layer->client_info.bpp = 16;
            break;
        case RNS_UD_COLOR_24BPP:
            self->rdp_layer->client_info.bpp = 24;
            break;
    }
    if (!s_check_rem(s, 2))
    {
        return 0;
    }
    in_uint8s(s, 2); /* clientProductId */
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPBCGR] TS_UD_CS_CORE "
              "<Optional Field> clientProductId (ignored)");

    if (!s_check_rem(s, 4))
    {
        return 0;
    }
    in_uint8s(s, 4); /* serialNumber */
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPBCGR] TS_UD_CS_CORE "
              "<Optional Field> serialNumber (ignored)");

    if (!s_check_rem(s, 2))
    {
        return 0;
    }
    in_uint16_le(s, highColorDepth);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPBCGR] TS_UD_CS_CORE "
              "<Optional Field> highColorDepth %s",
              highColorDepth == 0x0004 ? "HIGH_COLOR_4BPP" :
              highColorDepth == 0x0008 ? "HIGH_COLOR_8BPP" :
              highColorDepth == 0x000F ? "HIGH_COLOR_15BPP" :
              highColorDepth == 0x0010 ? "HIGH_COLOR_16BPP" :
              highColorDepth == 0x0018 ? "HIGH_COLOR_24BPP" :
              "unknown");
    self->rdp_layer->client_info.bpp = highColorDepth;

    if (!s_check_rem(s, 2))
    {
        return 0;
    }
    in_uint16_le(s, supportedColorDepths);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPBCGR] TS_UD_CS_CORE "
              "<Optional Field> supportedColorDepths %s",
              supportedColorDepths == 0x0001 ? "RNS_UD_24BPP_SUPPORT" :
              supportedColorDepths == 0x0002 ? "RNS_UD_16BPP_SUPPORT" :
              supportedColorDepths == 0x0004 ? "RNS_UD_15BPP_SUPPORT" :
              supportedColorDepths == 0x0008 ? "RNS_UD_32BPP_SUPPORT" :
              "unknown");

    if (!s_check_rem(s, 2))
    {
        return 0;
    }
    in_uint16_le(s, earlyCapabilityFlags);
    self->rdp_layer->client_info.mcs_early_capability_flags = earlyCapabilityFlags;
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPBCGR] TS_UD_CS_CORE "
              "<Optional Field> earlyCapabilityFlags 0x%4.4x",
              earlyCapabilityFlags);
    if ((earlyCapabilityFlags & 0x0002) && (supportedColorDepths & 0x0008))
    {
        self->rdp_layer->client_info.bpp = 32;
    }

    if (!s_check_rem(s, 64))
    {
        return 0;
    }
    in_uint8s(s, 64); /* clientDigProductId */
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPBCGR] TS_UD_CS_CORE "
              "<Optional Field> clientDigProductId (ignored)");

    if (!s_check_rem(s, 1))
    {
        return 0;
    }
    in_uint8(s, self->rdp_layer->client_info.mcs_connection_type); /* connectionType */
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPBCGR] TS_UD_CS_CORE "
              "<Optional Field> connectionType 0x%2.2x",
              self->rdp_layer->client_info.mcs_connection_type);

    if (!s_check_rem(s, 1))
    {
        return 0;
    }
    in_uint8s(s, 1); /* pad1octet */
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPBCGR] TS_UD_CS_CORE "
              "<Optional Field> pad1octet (ignored)");

    if (!s_check_rem(s, 4))
    {
        return 0;
    }
    in_uint8s(s, 4); /* serverSelectedProtocol */
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPBCGR] TS_UD_CS_CORE "
              "<Optional Field> serverSelectedProtocol (ignored)");

    if (!s_check_rem(s, 4))
    {
        return 0;
    }
    in_uint8s(s, 4); /* desktopPhysicalWidth */
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPBCGR] TS_UD_CS_CORE "
              "<Optional Field> desktopPhysicalWidth (ignored)");

    if (!s_check_rem(s, 4))
    {
        return 0;
    }
    in_uint8s(s, 4); /* desktopPhysicalHeight */
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPBCGR] TS_UD_CS_CORE "
              "<Optional Field> desktopPhysicalHeight (ignored)");

    if (!s_check_rem(s, 2))
    {
        return 0;
    }
    in_uint8s(s, 2); /* reserved */
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPBCGR] TS_UD_CS_CORE "
              "<Optional Field> desktopOrientation (ignored)");

    return 0;
}

/*****************************************************************************/
/* Process a [MS-RDPBCGR] TS_UD_CS_SEC message */
static int
xrdp_sec_process_mcs_data_CS_SECURITY(struct xrdp_sec *self, struct stream *s)
{
    int crypt_method;
    int found;

    in_uint32_le(s, crypt_method);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPBCGR] TS_UD_CS_SEC "
              "encryptionMethods 0x%8.8x, extEncryptionMethods (ignored)",
              crypt_method);
    if (crypt_method & CRYPT_METHOD_40BIT)
    {
        LOG(LOG_LEVEL_DEBUG, "Client supports 40 bit encryption");
    }
    if (crypt_method & CRYPT_METHOD_128BIT)
    {
        LOG(LOG_LEVEL_DEBUG, "Client supports 128 bit encryption");
    }
    if (crypt_method & CRYPT_METHOD_56BIT)
    {
        LOG(LOG_LEVEL_DEBUG, "Client supports 56 bit encryption");
    }
    if (crypt_method & CRYPT_METHOD_FIPS)
    {
        LOG(LOG_LEVEL_DEBUG, "Client supports fips encryption");
    }
    found = 0;
    if ((found == 0) &&
            (self->mcs_layer->iso_layer->selectedProtocol == PROTOCOL_SSL))
    {
        LOG(LOG_LEVEL_DEBUG,
            "The connection is using TLS, skipping RDP crypto negotiation");
        found = 1;
    }
    if ((found == 0) &&
            (self->crypt_method & CRYPT_METHOD_FIPS) &&
            (self->crypt_level == CRYPT_LEVEL_FIPS))
    {
        if (crypt_method & CRYPT_METHOD_FIPS)
        {
            LOG(LOG_LEVEL_DEBUG,
                "Client and server both support fips encryption, "
                "using RDP fips encryption.");
            self->crypt_method = CRYPT_METHOD_FIPS;
            self->crypt_level = CRYPT_LEVEL_FIPS;
            found = 1;
        }
    }
    if ((found == 0) &&
            (self->crypt_method & CRYPT_METHOD_128BIT) &&
            (self->crypt_level == CRYPT_LEVEL_HIGH))
    {
        if (crypt_method & CRYPT_METHOD_128BIT)
        {
            LOG(LOG_LEVEL_DEBUG,
                "Client and server both support high encryption, "
                "using RDP 128-bit encryption.");
            self->crypt_method = CRYPT_METHOD_128BIT;
            self->crypt_level = CRYPT_LEVEL_HIGH;
            found = 1;
        }
    }
    /* TODO: figure out why both "COMPATIBLE" and "LOW" crypto level both use
        40-bit encryption, and why there is no cypto method for 56-bit
        encryption even though there is code for checking for 56-bit
        encryption */
    if ((found == 0) &&
            (self->crypt_method & CRYPT_METHOD_40BIT) &&
            (self->crypt_level == CRYPT_LEVEL_CLIENT_COMPATIBLE))
    {
        if (crypt_method & CRYPT_METHOD_40BIT)
        {
            LOG(LOG_LEVEL_DEBUG,
                "Client and server both support medium encryption, "
                "using RDP 40-bit encryption.");
            self->crypt_method = CRYPT_METHOD_40BIT;
            self->crypt_level = CRYPT_LEVEL_CLIENT_COMPATIBLE;
            found = 1;
        }
    }
    if ((found == 0) &&
            (self->crypt_method & CRYPT_METHOD_40BIT) &&
            (self->crypt_level == CRYPT_LEVEL_LOW))
    {
        if (crypt_method & CRYPT_METHOD_40BIT)
        {
            LOG(LOG_LEVEL_DEBUG,
                "Client and server both support low encryption, "
                "using RDP 40-bit encryption.");
            self->crypt_method = CRYPT_METHOD_40BIT;
            self->crypt_level = CRYPT_LEVEL_LOW;
            found = 1;
        }
    }
    if ((found == 0) &&
            (self->crypt_level == CRYPT_LEVEL_NONE))
    {
        if (crypt_method == CRYPT_METHOD_NONE)
        {
            LOG(LOG_LEVEL_DEBUG,
                "Client and server both support no encryption, "
                "RDP encryption is disabled.");
            self->crypt_method = CRYPT_METHOD_NONE;
            self->crypt_level = CRYPT_LEVEL_NONE;
            found = 1;
        }
    }
    if (found == 0)
    {
        /* TODO: figure out why failing to find a shared encryption level
            does not return an error? */
        /* TODO: does the connection fail now or is the default server
            encryption used? */
        LOG(LOG_LEVEL_WARNING,
            "Client and server do not both support the same encryption.");
        //        return 1;
    }
    return 0;
}

/*****************************************************************************/
/* Process a [MS-RDPBCGR] TS_UD_CS_NET message.
   This adds the mcs channels in the list of channels to be used when
   creating the server mcs data */
static int
xrdp_sec_process_mcs_data_channels(struct xrdp_sec *self, struct stream *s)
{
    int num_channels;
    int index;
    struct xrdp_client_info *client_info;
    struct mcs_channel_item *channel_item;

    client_info = &(self->rdp_layer->client_info);
    /* this is an option set in xrdp.ini */
    if (client_info->channels_allowed == 0) /* are channels on? */
    {
        LOG(LOG_LEVEL_DEBUG, "All channels are disabled by configuration");
        return 0;
    }
    if (!s_check_rem_and_log(s, 4, "Parsing [MS-RDPBCGR] TS_UD_CS_NET"))
    {
        return 1;
    }
    in_uint32_le(s, num_channels);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPBCGR] TS_UD_CS_NET "
              "channelCount %d", num_channels);
    if (num_channels > 31)
    {
        LOG(LOG_LEVEL_ERROR, "[MS-RDPBCGR] Protocol error: too many channels requested. "
            "max 31, received %d", num_channels);
        return 1;
    }
    for (index = 0; index < num_channels; index++)
    {
        channel_item = g_new0(struct mcs_channel_item, 1);
        if (!s_check_rem_and_log(s, 12, "Parsing [MS-RDPBCGR] TS_UD_CS_NET.CHANNEL_DEF"))
        {
            g_free(channel_item);
            return 1;
        }
        in_uint8a(s, channel_item->name, 8);
        in_uint32_le(s, channel_item->flags);

        if (g_strlen(channel_item->name) > 0 && g_strlen(channel_item->name) < 8)
        {
            LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPBCGR] "
                      "TS_UD_CS_NET.CHANNEL_DEF %d, name %s, options 0x%8.8x",
                      index, channel_item->name, channel_item->flags);
            channel_item->chanid = MCS_GLOBAL_CHANNEL + (index + 1);
            list_add_item(self->mcs_layer->channel_list,
                          (intptr_t) channel_item);
            LOG(LOG_LEVEL_DEBUG,
                "Adding channel: name %s, channel id %d, flags 0x%8.8x",
                channel_item->name, channel_item->chanid, channel_item->flags);
        }
        else
        {
            LOG_DEVEL(LOG_LEVEL_WARNING, "Received [MS-RDPBCGR] "
                      "TS_UD_CS_NET.CHANNEL_DEF %d, skipped because of "
                      "malformed channel name.", index);
            LOG_DEVEL_HEXDUMP(LOG_LEVEL_WARNING,
                              "[MS-RDPBCGR] TS_UD_CS_NET.CHANNEL_DEF name",
                              channel_item->name, 8);
            g_free(channel_item);
        }
    }
    return 0;
}

/*****************************************************************************/
/* Process a [MS-RDPBCGR] TS_UD_CS_MONITOR message.
   reads the client monitors data */
static int
xrdp_sec_process_mcs_data_monitors(struct xrdp_sec *self, struct stream *s)
{
    int index;
    int monitorCount;
    int flags;
    int x1;
    int y1;
    int x2;
    int y2;
    int got_primary;
    struct xrdp_client_info *client_info = (struct xrdp_client_info *)NULL;

    client_info = &(self->rdp_layer->client_info);

    /* this is an option set in xrdp.ini */
    if (client_info->multimon != 1) /* are multi-monitors allowed ? */
    {
        LOG(LOG_LEVEL_INFO, "Multi-monitor is disabled by server config");
        return 0;
    }
    if (!s_check_rem_and_log(s, 8, "Parsing [MS-RDPBCGR] TS_UD_CS_MONITOR"))
    {
        return 1;
    }
    in_uint32_le(s, flags); /* flags */
    in_uint32_le(s, monitorCount);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPBCGR] TS_UD_CS_MONITOR "
              "flags 0x%8.8x, monitorCount %d", flags, monitorCount);

    //verify flags - must be 0x0
    if (flags != 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "[MS-RDPBCGR] Protocol error: TS_UD_CS_MONITOR flags MUST be zero, "
            "received: 0x%8.8x", flags);
        return 1;
    }
    //verify monitorCount - max 16
    if (monitorCount > 16)
    {
        LOG(LOG_LEVEL_ERROR,
            "[MS-RDPBCGR] Protocol error: TS_UD_CS_MONITOR monitorCount "
            "MUST be less than 16, received: %d", monitorCount);
        return 1;
    }

    client_info->monitorCount = monitorCount;

    x1 = 0;
    y1 = 0;
    x2 = 0;
    y2 = 0;
    got_primary = 0;
    /* Add client_monitor_data to client_info struct, will later pass to X11rdp */
    for (index = 0; index < monitorCount; index++)
    {
        if (!s_check_rem_and_log(s, 20, "Parsing [MS-RDPBCGR] TS_UD_CS_MONITOR.TS_MONITOR_DEF"))
        {
            return 1;
        }
        in_uint32_le(s, client_info->minfo[index].left);
        in_uint32_le(s, client_info->minfo[index].top);
        in_uint32_le(s, client_info->minfo[index].right);
        in_uint32_le(s, client_info->minfo[index].bottom);
        in_uint32_le(s, client_info->minfo[index].is_primary);

        LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPBCGR] "
                  "TS_UD_CS_MONITOR.TS_MONITOR_DEF %d "
                  "left %d, top %d, right %d, bottom %d, flags 0x%8.8x",
                  index,
                  client_info->minfo[index].left,
                  client_info->minfo[index].top,
                  client_info->minfo[index].right,
                  client_info->minfo[index].bottom,
                  client_info->minfo[index].is_primary);

        if (index == 0)
        {
            x1 = client_info->minfo[index].left;
            y1 = client_info->minfo[index].top;
            x2 = client_info->minfo[index].right;
            y2 = client_info->minfo[index].bottom;
        }
        else
        {
            x1 = MIN(x1, client_info->minfo[index].left);
            y1 = MIN(y1, client_info->minfo[index].top);
            x2 = MAX(x2, client_info->minfo[index].right);
            y2 = MAX(y2, client_info->minfo[index].bottom);
        }

        if (client_info->minfo[index].is_primary)
        {
            got_primary = 1;
        }

        LOG(LOG_LEVEL_DEBUG,
            "Client monitor [%d]: left= %d, top= %d, right= %d, bottom= %d, "
            "is_primary?= %d",
            index,
            client_info->minfo[index].left,
            client_info->minfo[index].top,
            client_info->minfo[index].right,
            client_info->minfo[index].bottom,
            client_info->minfo[index].is_primary);
    }

    if (!got_primary)
    {
        /* no primary monitor was set, choose the leftmost monitor as primary */
        for (index = 0; index < monitorCount; index++)
        {
            if (client_info->minfo[index].left == x1 &&
                    client_info->minfo[index].top == y1)
            {
                client_info->minfo[index].is_primary = 1;
                break;
            }
        }
    }

    /* set wm geometry */
    if ((x2 > x1) && (y2 > y1))
    {
        client_info->width = (x2 - x1) + 1;
        client_info->height = (y2 - y1) + 1;
    }
    /* make sure virtual desktop size is ok */
    if (client_info->width > 0x7FFE || client_info->width < 0xC8 ||
            client_info->height > 0x7FFE || client_info->height < 0xC8)
    {
        LOG(LOG_LEVEL_ERROR,
            "Client supplied virtual desktop width or height is invalid. "
            "Allowed width range: min %d, max %d. Width received: %d. "
            "Allowed height range: min %d, max %d. Height received: %d",
            0xC8, 0x7FFE, client_info->width,
            0xC8, 0x7FFE, client_info->height);
        return 1; /* error */
    }

    /* keep a copy of non negative monitor info values for xrdp_wm usage */
    for (index = 0; index < monitorCount; index++)
    {
        client_info->minfo_wm[index].left =  client_info->minfo[index].left - x1;
        client_info->minfo_wm[index].top =  client_info->minfo[index].top - y1;
        client_info->minfo_wm[index].right =  client_info->minfo[index].right - x1;
        client_info->minfo_wm[index].bottom =  client_info->minfo[index].bottom - y1;
        client_info->minfo_wm[index].is_primary =  client_info->minfo[index].is_primary;
    }

    return 0;
}

/*****************************************************************************/
/* Process a Client MCS Connect Initial PDU with GCC Conference Create Request.
   process client mcs data, we need some things in here to create the server
   mcs data */
int
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
    if (!s_check_rem_and_log(s, 23, "Parsing [ITU T.124] ConferenceCreateRequest"))
    {
        return 1;
    }
    in_uint8s(s, 23); /* skip [ITU T.124] ConferenceCreateRequest fields until userData */

    while (s_check_rem(s, 4))
    {
        hold_p = s->p;
        in_uint16_le(s, tag);
        in_uint16_le(s, size);
        LOG_DEVEL(LOG_LEVEL_TRACE, "Received header [MS-RDPBCGR] TS_UD_HEADER "
                  "type 0x%4.4x, length %d", tag, size);

        if (size < 4)
        {
            LOG(LOG_LEVEL_WARNING, "[MS-RDPBCGR] Protocol error: Invalid TS_UD_HEADER length value. "
                "expected >= 4, actual %d", size);
            break;
        }
        if (!s_check_rem_and_log(s, size - 4,
                                 "Parsing [MS-RDPBCGR] GCC Conference Create Request client data field"))
        {
            break;
        }

        switch (tag)
        {
            case SEC_TAG_CLI_INFO:     /* CS_CORE           0xC001 */
                if (xrdp_sec_process_mcs_data_CS_CORE(self, s) != 0)
                {
                    LOG(LOG_LEVEL_ERROR,
                        "Processing [MS-RDPBCGR] TS_UD_CS_CORE failed");
                    return 1;
                }
                break;
            case SEC_TAG_CLI_CRYPT:    /* CS_SECURITY       0xC002 */
                if (xrdp_sec_process_mcs_data_CS_SECURITY(self, s) != 0)
                {
                    LOG(LOG_LEVEL_ERROR,
                        "Processing [MS-RDPBCGR] TS_UD_CS_SEC failed");
                    return 1;
                }
                break;
            case SEC_TAG_CLI_CHANNELS: /* CS_NET            0xC003 */
                if (xrdp_sec_process_mcs_data_channels(self, s) != 0)
                {
                    LOG(LOG_LEVEL_ERROR,
                        "Processing [MS-RDPBCGR] TS_UD_CS_NET failed");
                    return 1;
                }
                break;
            case SEC_TAG_CLI_4:        /* CS_CLUSTER        0xC004 */
                LOG_DEVEL(LOG_LEVEL_DEBUG, "Received [MS-RDPBCGR] TS_UD_CS_CLUSTER - no-op");
                break;
            case SEC_TAG_CLI_MONITOR:  /* CS_MONITOR        0xC005 */
                if (xrdp_sec_process_mcs_data_monitors(self, s) != 0)
                {
                    LOG(LOG_LEVEL_ERROR,
                        "Processing [MS-RDPBCGR] TS_UD_CS_MONITOR failed");
                    return 1;
                }
                break;
            /* CS_MCS_MSGCHANNEL 0xC006
               CS_MONITOR_EX     0xC008
               CS_MULTITRANSPORT 0xC00A
               SC_CORE           0x0C01
               SC_SECURITY       0x0C02
               SC_NET            0x0C03
               SC_MCS_MSGCHANNEL 0x0C04
               SC_MULTITRANSPORT 0x0C08 */
            default:
                LOG(LOG_LEVEL_WARNING,
                    "Received [MS-RDPBCGR] TS_UD_HEADER type 0x%4.4x "
                    "is unknown (ignored)", tag);
                break;
        }

        s->p = hold_p + size;
    }

    if (self->rdp_layer->client_info.max_bpp > 0)
    {
        if (self->rdp_layer->client_info.bpp >
                self->rdp_layer->client_info.max_bpp)
        {
            LOG(LOG_LEVEL_WARNING, "Client requested %d bpp color depth, "
                "but the server configuration is limited to %d bpp. "
                "Downgrading the color depth to %d bits-per-pixel.",
                self->rdp_layer->client_info.bpp,
                self->rdp_layer->client_info.max_bpp,
                self->rdp_layer->client_info.max_bpp);
            self->rdp_layer->client_info.bpp =
                self->rdp_layer->client_info.max_bpp;
        }
    }

    /* set p to beginning */
    s->p = s->data;
    return 0;
}

/*****************************************************************************/
/* Process the mcs client data [ITU T.124] ConferenceCreateRequest userData field
   as a [MS-RDPBCGR] TS_UD_CS_CORE struct */
/* TODO: why does this method parse the strust from back to front (resetting
   after each field) instead of from front to back like the rest of the parsing
   code? */
/* TODO: why does this method exist when the same struct is parsed in
   xrdp_sec_process_mcs_data_CS_CORE and there does not seem to be any
   dependencies preventing a call to that function. */
/* TODO: this is a brittle function that assumes field offsets in the stream
   instead of parsing the variable length fields of [ITU T.124] ConferenceCreateRequest */
static int
xrdp_sec_in_mcs_data(struct xrdp_sec *self)
{
    struct stream *s = (struct stream *)NULL;
    struct xrdp_client_info *client_info = (struct xrdp_client_info *)NULL;
    int index = 0;
    char c = 0;

    client_info = &(self->rdp_layer->client_info);
    s = &(self->client_mcs_data);
    /* get hostname, it's unicode */
    s->p = s->data;
    if (!s_check_rem_and_log(s, 47, "Parsing [ITU T.124] ConferenceCreateRequest"))
    {
        return 1;
    }
    in_uint8s(s, 47); /* skip [ITU T.124] ConferenceCreateRequest up to the
                         userData field, and skip [MS-RDPBCGR] TS_UD_CS_CORE
                         up to the clientName field */
    g_memset(client_info->hostname, 0, 32);
    c = 1;
    index = 0;

    /* TODO: why aren't we using unicode_utf16_in to parse the client name
       like we do in xrdp_sec_process_mcs_data_CS_CORE? */
    while (index < 16 && c != 0)
    {
        if (!s_check_rem_and_log(s, 2, "Parsing [MS-RDPBCGR] TS_UD_CS_CORE clientName"))
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
    if (!s_check_rem_and_log(s, 43 + 4, "Parsing [MS-RDPBCGR] TS_UD_CS_CORE clientBuild"))
    {
        return 1;
    }
    in_uint8s(s, 43);
    in_uint32_le(s, client_info->build); /* [MS-RDPBCGR] TS_UD_CS_CORE clientBuild */
    /* get keylayout */
    s->p = s->data;
    if (!s_check_rem_and_log(s, 39 + 4, "Parsing [MS-RDPBCGR] TS_UD_CS_CORE keyboardLayout"))
    {
        return 1;
    }
    in_uint8s(s, 39);
    in_uint32_le(s, client_info->keylayout); /* [MS-RDPBCGR] TS_UD_CS_CORE keyboardLayout */
    /* get keyboard type / subtype */
    s->p = s->data;
    if (!s_check_rem_and_log(s, 79 + 8, "Parsing [MS-RDPBCGR] TS_UD_CS_CORE keyboardType"))
    {
        return 1;
    }
    in_uint8s(s, 79);
    in_uint32_le(s, client_info->keyboard_type); /* [MS-RDPBCGR] TS_UD_CS_CORE keyboardType */
    in_uint32_le(s, client_info->keyboard_subtype); /* [MS-RDPBCGR] TS_UD_CS_CORE keyboardSubType */
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [MS-RDPBCGR] TS_UD_CS_CORE "
              "<Requiered fields> version (ignored), desktopWidth (ignored), "
              "desktopHeight (ignored), colorDepth (ignored), SASSequence (ingored), "
              "keyboardLayout 0x%8.8x, clientBuild %d, "
              "clientName %s, keyboardType 0x%8.8x, "
              "keyboardSubType 0x%8.8x, keyboardFunctionKey (ignored), "
              "imeFileName (ignroed)",
              client_info->keylayout,
              client_info->build,
              client_info->hostname,
              client_info->keyboard_type,
              client_info->keyboard_subtype);

    xrdp_load_keyboard_layout(client_info);
    s->p = s->data;

    return 0;
}

/*****************************************************************************/
/* returns error */
int
xrdp_sec_init_rdp_security(struct xrdp_sec *self)
{
    switch (self->rdp_layer->client_info.crypt_level)
    {
        case 0: /* none */
            self->crypt_method = CRYPT_METHOD_NONE;
            self->crypt_level = CRYPT_LEVEL_NONE;
            break;
        case 1: /* low */
            self->crypt_method = CRYPT_METHOD_40BIT;
            self->crypt_level = CRYPT_LEVEL_LOW;
            break;
        case 2: /* medium */
            self->crypt_method = CRYPT_METHOD_40BIT;
            self->crypt_level = CRYPT_LEVEL_CLIENT_COMPATIBLE;
            break;
        case 3: /* high */
            self->crypt_method = CRYPT_METHOD_128BIT;
            self->crypt_level = CRYPT_LEVEL_HIGH;
            break;
        case 4: /* fips */
            self->crypt_method = CRYPT_METHOD_FIPS;
            self->crypt_level = CRYPT_LEVEL_FIPS;
            break;
        default:
            LOG_DEVEL(LOG_LEVEL_WARNING, "Fatal : Illegal crypt_level %d",
                      self->rdp_layer->client_info.crypt_level);
            break ;
    }

    if (self->decrypt_rc4_info != NULL)
    {
        LOG_DEVEL(LOG_LEVEL_WARNING,
                  "xrdp_sec_init_rdp_security: decrypt_rc4_info already created !!!");
    }
    else
    {
        self->decrypt_rc4_info = ssl_rc4_info_create();
    }

    if (self->encrypt_rc4_info != NULL)
    {
        LOG_DEVEL(LOG_LEVEL_WARNING,
                  "xrdp_sec_init_rdp_security: encrypt_rc4_info already created !!!");
    }
    else
    {
        self->encrypt_rc4_info = ssl_rc4_info_create();
    }

    return 0;
}

/*****************************************************************************/
int
xrdp_sec_incoming(struct xrdp_sec *self)
{
    struct list *items = NULL;
    struct list *values = NULL;
    struct xrdp_iso *iso;
    int index = 0;
    char *item = NULL;
    char *value = NULL;
    char key_file[256];

    iso = self->mcs_layer->iso_layer;

    /* negotiate security layer */
    if (xrdp_iso_incoming(iso) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_sec_incoming: xrdp_iso_incoming failed");
        return 1;
    }

    /* initialize selected security layer */
    if (iso->selectedProtocol > PROTOCOL_RDP)
    {
        /* init tls security */

        if (trans_set_tls_mode(self->mcs_layer->iso_layer->trans,
                               self->rdp_layer->client_info.key_file,
                               self->rdp_layer->client_info.certificate,
                               self->rdp_layer->client_info.ssl_protocols,
                               self->rdp_layer->client_info.tls_ciphers) != 0)
        {
            LOG(LOG_LEVEL_ERROR, "xrdp_sec_incoming: trans_set_tls_mode failed");
            return 1;
        }

        LOG(LOG_LEVEL_DEBUG, "Using TLS security, and "
            "setting RDP security crypto to LEVEL_NONE and METHOD_NONE");
        self->crypt_level = CRYPT_LEVEL_NONE;
        self->crypt_method = CRYPT_METHOD_NONE;
        self->rsa_key_bytes = 0;

    }
    else
    {
        /* init rdp security */
        if (xrdp_sec_init_rdp_security(self) != 0)
        {
            LOG(LOG_LEVEL_ERROR, "xrdp_sec_incoming: xrdp_sec_init_rdp_security failed");
            return 1;
        }
        if (self->crypt_method != CRYPT_METHOD_NONE)
        {
            LOG(LOG_LEVEL_DEBUG, "Using RDP security, and "
                "reading the server configuration");

            g_memset(key_file, 0, sizeof(char) * 256);
            g_random(self->server_random, 32);
            items = list_create();
            items->auto_free = 1;
            values = list_create();
            values->auto_free = 1;
            g_snprintf(key_file, 255, "%s/rsakeys.ini", XRDP_CFG_PATH);

            if (file_by_name_read_section(key_file, "keys", items, values) != 0)
            {
                /* this is a show stopper */
                LOG(LOG_LEVEL_ERROR, "XRDP cannot read file: %s "
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
                    self->rsa_key_bytes = (g_strlen(value) + 1) / 5;
                    LOG_DEVEL(LOG_LEVEL_DEBUG, "Server config: pub_mod bytes %d",
                              self->rsa_key_bytes);
                    hex_str_to_bin(value, self->pub_mod, self->rsa_key_bytes);
                }
                else if (g_strcasecmp(item, "pub_sig") == 0)
                {
                    hex_str_to_bin(value, self->pub_sig, 64);
                }
                else if (g_strcasecmp(item, "pri_exp") == 0)
                {
                    self->rsa_key_bytes = (g_strlen(value) + 1) / 5;
                    LOG_DEVEL(LOG_LEVEL_DEBUG, "Server config: pri_exp %d",
                              self->rsa_key_bytes);
                    hex_str_to_bin(value, self->pri_exp, self->rsa_key_bytes);
                }
            }

            if (self->rsa_key_bytes <= 64)
            {
                LOG(LOG_LEVEL_WARNING, "warning, RSA key len 512 "
                    "bits or less, consider creating a 2048 bit key");
            }

            list_delete(items);
            list_delete(values);
        }
    }

    /* negotiate mcs layer */
    if (xrdp_mcs_incoming(self->mcs_layer) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_sec_incoming: xrdp_mcs_incoming failed");
        return 1;
    }

    LOG_DEVEL_HEXDUMP(LOG_LEVEL_TRACE, "client mcs data received",
                      self->client_mcs_data.data,
                      (int)(self->client_mcs_data.end - self->client_mcs_data.data));
    LOG_DEVEL_HEXDUMP(LOG_LEVEL_TRACE, "server mcs data sent",
                      self->server_mcs_data.data,
                      (int)(self->server_mcs_data.end - self->server_mcs_data.data));

    if (xrdp_sec_in_mcs_data(self) != 0)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_sec_incoming: xrdp_sec_in_mcs_data failed");
        return 1;
    }

    return 0;
}

/*****************************************************************************/
int
xrdp_sec_disconnect(struct xrdp_sec *self)
{
    int rv;

    rv = xrdp_mcs_disconnect(self->mcs_layer);
    return rv;
}
