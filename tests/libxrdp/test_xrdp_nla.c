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

#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include <string.h>

#include "test_libxrdp.h"
#include "xrdp_client_info.h"
#include "xrdp_nla.h"

START_TEST(test_nla_configuration)
{
    struct xrdp_client_info client_info = {0};

    ck_assert_int_eq(xrdp_nla_is_enabled(&client_info), 0);
    client_info.enable_nla = 1;
    ck_assert_int_ne(xrdp_nla_is_enabled(&client_info), 0);
    client_info.security_layer = SECURITY_LAYER_TLS;
    ck_assert_int_eq(xrdp_nla_is_enabled(&client_info), 0);
    client_info.security_layer = SECURITY_LAYER_NLA;
    client_info.enable_nla = 0;
    ck_assert_int_ne(xrdp_nla_is_enabled(&client_info), 0);
}
END_TEST

START_TEST(test_decode_password_credentials)
{
    static const unsigned char credentials[] =
    {
        0x30, 0x1d,
        0xa0, 0x03, 0x02, 0x01, 0x01,
        0xa1, 0x16, 0x04, 0x14,
        0x30, 0x12,
        0xa0, 0x04, 0x04, 0x02, 0x44, 0x00,
        0xa1, 0x04, 0x04, 0x02, 0x75, 0x00,
        0xa2, 0x04, 0x04, 0x02, 0x70, 0x00
    };
    struct xrdp_client_info client_info = {0};

    ck_assert_int_eq(xrdp_nla_decode_ts_credentials(credentials,
                     sizeof(credentials),
                     &client_info), 0);
    ck_assert_str_eq(client_info.domain, "D");
    ck_assert_str_eq(client_info.username, "u");
    ck_assert_str_eq(client_info.password, "p");
    ck_assert_int_eq(client_info.rdp_autologin, 1);
}
END_TEST

START_TEST(test_prepare_kerberos_wrap_token_for_sspi)
{
    unsigned char token[48] =
    {
        0x05, 0x04, 0x03, 0xff,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    };
    unsigned char original_body[32];
    unsigned int index;

    for (index = 0; index < sizeof(original_body); ++index)
    {
        token[16 + index] = index;
        original_body[index] = index;
    }

    ck_assert_int_eq(xrdp_nla_prepare_wrap_token(token, sizeof(token), 8), 0);
    ck_assert_int_eq(token[6], 0);
    ck_assert_int_eq(token[7], 8);
    ck_assert_mem_eq(token + 16, original_body + 24, 8);
    ck_assert_mem_eq(token + 24, original_body, 24);
}
END_TEST

START_TEST(test_reject_noncanonical_credentials)
{
    static const unsigned char credentials_with_trailing_data[] =
    {
        0x30, 0x1d,
        0xa0, 0x03, 0x02, 0x01, 0x01,
        0xa1, 0x16, 0x04, 0x14,
        0x30, 0x12,
        0xa0, 0x04, 0x04, 0x02, 0x44, 0x00,
        0xa1, 0x04, 0x04, 0x02, 0x75, 0x00,
        0xa2, 0x04, 0x04, 0x02, 0x70, 0x00,
        0x00
    };
    struct xrdp_client_info client_info = {0};

    ck_assert_int_ne(xrdp_nla_decode_ts_credentials(
                         credentials_with_trailing_data,
                         sizeof(credentials_with_trailing_data),
                         &client_info), 0);
    ck_assert_int_eq(client_info.rdp_autologin, 0);
}
END_TEST

Suite *
make_suite_test_xrdp_nla(void)
{
    Suite *suite = suite_create("xrdp_nla");
    TCase *test_case = tcase_create("CredSSP credentials");

    tcase_add_test(test_case, test_nla_configuration);
    tcase_add_test(test_case, test_decode_password_credentials);
    tcase_add_test(test_case, test_reject_noncanonical_credentials);
    tcase_add_test(test_case, test_prepare_kerberos_wrap_token_for_sspi);
    suite_add_tcase(suite, test_case);
    return suite;
}
