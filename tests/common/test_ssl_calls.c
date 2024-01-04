
#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include "os_calls.h"
#include "string_calls.h"
#include "ssl_calls.h"

#include "test_common.h"

/* Time to allow RSA-based test suites to run on older, slower platforms
 *
 * These platforms are most often seen on build farms (e.g. Debian CI) */
#define RSA_BASED_TEST_SUITE_TIMEOUT 60

START_TEST(test_rc4_enc_ok)
{
    const char *key = "16_byte_key-----";
    char text[] = "xrdp-test-suite-rc4-encryption";
    char *result;

    void *info = ssl_rc4_info_create();
    ssl_rc4_set_key(info, key, g_strlen(key));
    ssl_rc4_crypt(info, text, sizeof(text) - 1);
    ssl_rc4_info_delete(info);
    result = bin_to_hex(text, sizeof(text) - 1);
    ck_assert(result != NULL);
    /* Result should be the same as
     * echo -n '<text>' | \
     *     openssl rc4 -K <hex-key> -e [-provider legacy] | \
     *     xxd -g0
     *
     * where <hex-key> is the string above in hexadecimal */
    ck_assert_str_eq(result, "c080f175b2d85802dbf1042f07180ddc4be1d9bd4a44158f0aebf11c961b");
    g_free(result);
}
END_TEST

START_TEST(test_rc4_enc_tv0_ok)
{
    /*
     * This is one of the 5 original RC4 test vectors posted in response to
     * the 'RC4 Algorithm revealed' sci.crypt usenet posting */
    unsigned char key[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
    unsigned char text[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
    const char *expected = "75b7878099e0c596";
    char *result;

    void *info = ssl_rc4_info_create();
    ssl_rc4_set_key(info, (char *)key, sizeof(key));
    ssl_rc4_crypt(info, (char *)text, sizeof(text));
    ssl_rc4_info_delete(info);

    result = bin_to_hex((char *)text, sizeof(text));
    ck_assert(result != NULL);
    ck_assert_str_eq(result, expected);
    g_free(result);
}
END_TEST

START_TEST(test_rc4_enc_tv1_ok)
{
    /*
     * This is one of the 5 original RC4 test vectors posted in response to
     * the 'RC4 Algorithm revealed' sci.crypt usenet posting */
    unsigned char key[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
    unsigned char text[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    const char *expected = "7494c2e7104b0879";
    char *result;

    void *info = ssl_rc4_info_create();
    ssl_rc4_set_key(info, (char *)key, sizeof(key));
    ssl_rc4_crypt(info, (char *)text, sizeof(text));
    ssl_rc4_info_delete(info);

    result = bin_to_hex((char *)text, sizeof(text));
    ck_assert(result != NULL);
    ck_assert_str_eq(result, expected);
    g_free(result);
}
END_TEST

START_TEST(test_rc4_enc_tv2_ok)
{
    /*
     * This is one of the 5 original RC4 test vectors posted in response to
     * the 'RC4 Algorithm revealed' sci.crypt usenet posting */
    unsigned char key[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    unsigned char text[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    const char *expected = "de188941a3375d3a";
    char *result;

    void *info = ssl_rc4_info_create();
    ssl_rc4_set_key(info, (char *)key, sizeof(key));
    ssl_rc4_crypt(info, (char *)text, sizeof(text));
    ssl_rc4_info_delete(info);

    result = bin_to_hex((char *)text, sizeof(text));
    ck_assert(result != NULL);
    ck_assert_str_eq(result, expected);
    g_free(result);
}
END_TEST

START_TEST(test_rc4_enc_tv3_ok)
{
    /*
     * This is one of the 5 original RC4 test vectors posted in response to
     * the 'RC4 Algorithm revealed' sci.crypt usenet posting */
    unsigned char key[] = {0xef, 0x01, 0x23, 0x45};
    unsigned char text[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    const char *expected = "d6a141a7ec3c38dfbd61";
    char *result;

    void *info = ssl_rc4_info_create();
    ssl_rc4_set_key(info, (char *)key, sizeof(key));
    ssl_rc4_crypt(info, (char *)text, sizeof(text));
    ssl_rc4_info_delete(info);

    result = bin_to_hex((char *)text, sizeof(text));
    ck_assert(result != NULL);
    ck_assert_str_eq(result, expected);
    g_free(result);
}
END_TEST

START_TEST(test_rc4_enc_tv4_ok)
{
    /*
     * This is one of the 5 original RC4 test vectors posted in response to
     * the 'RC4 Algorithm revealed' sci.crypt usenet posting */
    unsigned char key[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
    unsigned char text[] =
    {
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01
    };
    const char *expected =
        "7595c3e6114a09780c4ad452338e1ffd9a1be9498f813d76533449b6778dca"
        "d8c78a8d2ba9ac66085d0e53d59c26c2d1c490c1ebbe0ce66d1b6b1b13b6"
        "b919b847c25a91447a95e75e4ef16779cde8bf0a95850e32af9689444fd3"
        "77108f98fdcbd4e726567500990bcc7e0ca3c4aaa304a387d20f3b8fbbcd"
        "42a1bd311d7a4303dda5ab078896ae80c18b0af66dff319616eb784e495a"
        "d2ce90d7f772a81747b65f62093b1e0db9e5ba532fafec47508323e67132"
        "7df9444432cb7367cec82f5d44c0d00b67d650a075cd4b70dedd77eb9b10"
        "231b6b5b741347396d62897421d43df9b42e446e358e9c11a9b2184ecbef"
        "0cd8e7a877ef968f1390ec9b3d35a5585cb009290e2fcde7b5ec66d9084b"
        "e44055a619d9dd7fc3166f9487f7cb272912426445998514c15d53a18c86"
        "4ce3a2b7555793988126520eacf2e3066e230c91bee4dd5304f5fd0405b3"
        "5bd99c73135d3d9bc335ee049ef69b3867bf2d7bd1eaa595d8bfc0066ff8"
        "d31509eb0c6caa006c807a623ef84c3d33c195d23ee320c40de0558157c8"
        "22d4b8c569d849aed59d4e0fd7f379586b4b7ff684ed6a189f7486d49b9c"
        "4bad9ba24b96abf924372c8a8fffb10d55354900a77a3db5f205e1b99fcd"
        "8660863a159ad4abe40fa48934163ddde542a6585540fd683cbfd8c00f12"
        "129a284deacc4cdefe58be7137541c047126c8d49e2755ab181ab7e940b0c0";

    char *result;

    void *info = ssl_rc4_info_create();
    ssl_rc4_set_key(info, (char *)key, sizeof(key));
    ssl_rc4_crypt(info, (char *)text, sizeof(text));
    ssl_rc4_info_delete(info);

    result = bin_to_hex((char *)text, sizeof(text));
    ck_assert(result != NULL);
    ck_assert_str_eq(result, expected);
    g_free(result);
}
END_TEST

START_TEST(test_sha1_hash_ok)
{
    const char *hash_string = "xrdp-test-suite-sha1-hash";
    char digest[20];
    char *result1;
    char *result2;

    void *info = ssl_sha1_info_create();
    ssl_sha1_clear(info);
    ssl_sha1_transform(info, hash_string, g_strlen(hash_string));
    ssl_sha1_complete(info, digest);
    result1 = bin_to_hex(digest, sizeof(digest));
    ck_assert(result1 != NULL);
    /* Check result with echo -n '<hash_string>' | sha1sum */
    ck_assert_str_eq(result1, "3ea0ae84e97e6262c7cfe79ccd7ad2094c06885d");

    /* Check a clear has the desired effect */
    ssl_sha1_clear(info);
    ssl_sha1_transform(info, hash_string, g_strlen(hash_string));
    ssl_sha1_complete(info, digest);
    result2 = bin_to_hex(digest, sizeof(digest));
    ck_assert(result2 != NULL);
    ck_assert_str_eq(result1, result2);

    ssl_sha1_info_delete(info);
    g_free(result1);
    g_free(result2);
}
END_TEST

START_TEST(test_md5_hash_ok)
{
    const char *hash_string = "xrdp-test-suite-md5-hash";
    char digest[16];
    char *result1;
    char *result2;

    void *info = ssl_md5_info_create();

    ssl_md5_clear(info);
    ssl_md5_transform(info, hash_string, g_strlen(hash_string));
    ssl_md5_complete(info, digest);
    result1 = bin_to_hex(digest, sizeof(digest));
    ck_assert(result1 != NULL);
    /* Check result with echo -n '<hash_string>' | md5sum */
    ck_assert_str_eq(result1, "ddc599dc7ec62b8f78760b071704c007");

    /* Check a clear has the desired effect */
    ssl_md5_clear(info);
    ssl_md5_transform(info, hash_string, g_strlen(hash_string));
    ssl_md5_complete(info, digest);
    result2 = bin_to_hex(digest, sizeof(digest));
    ck_assert(result2 != NULL);
    ck_assert_str_eq(result1, result2);

    ssl_md5_info_delete(info);
    g_free(result1);
    g_free(result2);
}
END_TEST

START_TEST(test_des3_enc_ok)
{
    const char *key = "24_byte_key-------------";
    char plaintext[] = "xrdp-test-suite-des3-encryption-must-be-multiple-of-8-chars-long--------";
    char ciphertext[sizeof(plaintext) - 1]; /* No terminator needed */
    char plaintext2[sizeof(plaintext)];

    char *result;

    void *info = ssl_des3_encrypt_info_create(key, 0);
    ssl_des3_encrypt(info, sizeof(plaintext) - 1, plaintext, ciphertext);
    ssl_des3_info_delete(info);
    result = bin_to_hex(ciphertext, sizeof(ciphertext));
    ck_assert(result != NULL);
    /* Result should be the same as
     * echo -n '<text>' | \
     *     openssl des3 -iv 0000000000000000 -K <hex-key> -e -nopad | \
     *     od -t x1
     *
     * where <hex-key> is the string above in hexadecimal */
    ck_assert_str_eq(result,
                     "856d70861827365e188781616e4f9dcc3009b2c5dc7785edcbc05fa825a4ea5e10b23735c0e971ca20f895f455b8845418963af6dd8e649719790eed6cbcee0fb97b743c60e32e8b");
    g_free(result);

    /* Let's go back again */
    info = ssl_des3_decrypt_info_create(key, 0);
    ssl_des3_decrypt(info, sizeof(ciphertext), ciphertext, plaintext2);
    ssl_des3_info_delete(info);
    plaintext2[sizeof(plaintext2) - 1] = '\0';

    ck_assert_str_eq(plaintext, plaintext2);
}
END_TEST

START_TEST(test_hmac_sha1_dgst_ok)
{
    const char *key = "20_byte_key---------";
    const char *hash_string = "xrdp-test-suite-hmac-sha1-dgst";
    char hmac[20];

    char *result;

    void *info = ssl_hmac_info_create();
    ssl_hmac_sha1_init(info, key, g_strlen(key));
    ssl_hmac_transform(info, hash_string, g_strlen(hash_string));
    ssl_hmac_complete(info, hmac, sizeof(hmac));
    ssl_hmac_info_delete(info);
    result = bin_to_hex(hmac, sizeof(hmac));
    ck_assert(result != NULL);
    /* Result should be the same as
     * echo -n '<text>' | openssl dgst -sha1 -hmac '<key>'
     *
     * or:-
     *
     * echo -n '<text>' | openssl mac -digest sha1 -macopt key:'<key>' hmac
     */
    ck_assert_str_eq(result, "af8c04e609e9f3cba53ad7815b60160dc69a9936");
    g_free(result);

}
END_TEST

START_TEST(test_gen_key_xrdp1)
{
#define RSA_TEST_BITS 2048
    char modulus[RSA_TEST_BITS / 8] = {0};
    char private_key[RSA_TEST_BITS / 8] = {0};
    unsigned char exponent[4] =
    {
        0x01, 0x00, 0x01, 0x00 /* 65537 in little-endian format */
    };

    /*
     * We can't do much here because of the nature of the call, but we can
     * at least check it completes without error */
    int error;
    error = ssl_gen_key_xrdp1(RSA_TEST_BITS,
                              (const char *)exponent, sizeof(exponent),
                              modulus, sizeof(modulus),
                              private_key, sizeof(private_key));
    ck_assert(error == 0);

    /* Both the modulus and the privatekey should be odd */
    ck_assert((modulus[0] & 1) == 1);
    ck_assert((private_key[0] & 1) == 1);
#undef RSA_TEST_BITS
}
END_TEST

/******************************************************************************/
Suite *
make_suite_test_ssl_calls(void)
{
    Suite *s;
    TCase *tc;

    s = suite_create("SSL-Calls");

    tc = tcase_create("ssl_calls_rc4");
    suite_add_tcase(s, tc);
    tcase_add_test(tc, test_rc4_enc_ok);
    tcase_add_test(tc, test_rc4_enc_tv0_ok);
    tcase_add_test(tc, test_rc4_enc_tv1_ok);
    tcase_add_test(tc, test_rc4_enc_tv2_ok);
    tcase_add_test(tc, test_rc4_enc_tv3_ok);
    tcase_add_test(tc, test_rc4_enc_tv4_ok);

    tc = tcase_create("ssl_calls_sha1");
    suite_add_tcase(s, tc);
    tcase_add_test(tc, test_sha1_hash_ok);

    tc = tcase_create("ssl_calls_md5");
    suite_add_tcase(s, tc);
    tcase_add_test(tc, test_md5_hash_ok);

    tc = tcase_create("ssl_calls_des3");
    suite_add_tcase(s, tc);
    tcase_add_test(tc, test_des3_enc_ok);

    tc = tcase_create("ssl_calls_hmac_sha1");
    suite_add_tcase(s, tc);
    tcase_add_test(tc, test_hmac_sha1_dgst_ok);

    tc = tcase_create("ssl_calls_rsa_key");
    suite_add_tcase(s, tc);
    tcase_set_timeout(tc, RSA_BASED_TEST_SUITE_TIMEOUT);
    tcase_add_test(tc, test_gen_key_xrdp1);

    return s;
}
