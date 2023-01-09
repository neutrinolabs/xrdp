
#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include <unistd.h>
#include <ctype.h>

#include "libipm.h"
#include "os_calls.h"
#include "string_calls.h"
#include "trans.h"

#include "test_libipm.h"

/* Random(ish) values for test_libipm_append_all_test */
#define ALL_TEST_y_VALUE 45
#define ALL_TEST_b_VALUE 0
#define ALL_TEST_n_VALUE -14
#define ALL_TEST_q_VALUE 327
#define ALL_TEST_i_VALUE -100000
#define ALL_TEST_u_VALUE 100000
#define ALL_TEST_x_VALUE -4000000000L
#define ALL_TEST_t_VALUE 8000000000L
#define ALL_TEST_s_VALUE \
    'H', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd', '\0'
#define ALL_TEST_B_VALUE  'a', 'z', 'A', 'Z', '0', '9', '#'

/* This macro generates two unsigned char values, separated by a comma,
 * representing a 2's complement little-endian 16-bit integer */
#define BITS16_LE(x) ((x) & 0xff), (((unsigned int)(x) >> 8) & 0xff)

/* This macro generates four unsigned char values, separated by a comma,
 * representing a 2's complement little-endian 32-bit integer */
#define BITS32_LE(x) BITS16_LE((x) & 0xffff), \
    BITS16_LE(((unsigned int)(x) >> 16) & 0xffff)

/* This macro generates eight unsigned char values, separated by a comma,
 * representing a 2's complement little-endian 64-bit integer */
#define BITS64_LE(x) BITS32_LE((uint64_t)(x) & 0xffffffff), \
    BITS32_LE(((uint64_t)(x) >> 32) & 0xffffffff)

/* Expected buffer contents for test_libipm_append_all_test */
static const unsigned char all_test_expected_buff[] =
{
    BITS16_LE(LIBIPM_VERSION),
    BITS16_LE(74),  /* Header : message length */
    BITS16_LE(LIBIPM_FAC_TEST),
    BITS16_LE(TEST_MESSAGE_NO),
    BITS32_LE(0),
    /* ------------------- */
    'y', ALL_TEST_y_VALUE,
    'b', ALL_TEST_b_VALUE,
    'n', BITS16_LE(ALL_TEST_n_VALUE),
    'q', BITS16_LE(ALL_TEST_q_VALUE),
    'i', BITS32_LE(ALL_TEST_i_VALUE),
    'u', BITS32_LE(ALL_TEST_u_VALUE),
    'x', BITS64_LE(ALL_TEST_x_VALUE),
    't', BITS64_LE(ALL_TEST_t_VALUE),
    /* String + terminator */
    's', ALL_TEST_s_VALUE,
    'h', /* No buffer value is needed for 'h' */
    /* Fixed size block */
    'B', BITS16_LE(7) /* length */, ALL_TEST_B_VALUE
};

/* Type used to map small test values (i.e. simple types that fit in an int)
 * to expected status codes if we try to output them for a given type */
struct int_test_value
{
    int value;
    enum libipm_status expected_status;
};

/***************************************************************************//**
 * Compares the data in a stream against an expected data block
 *
 * @param s Stream to check
 * @param expdata Expected data
 * @aram explen Expected data len
 */
static void
check_stream_data_eq(const struct stream *s,
                     const unsigned char *expected_data,
                     unsigned int expected_len)
{
    check_binary_data_eq((unsigned char *)s->data,
                         (unsigned int)(s->end - s->data),
                         expected_data,
                         expected_len);
}


/***************************************************************************//**
 * Value checks for small types (i.e. those that fit in an 'int' */
static void
test_small_type_values(char typechar,
                       const struct int_test_value v[], unsigned int count)
{
    unsigned int i;
    enum libipm_status status;
    char format[] = { typechar, '\0'};

    for (i = 0 ; i < count; ++i)
    {
        status = libipm_msg_out_init(g_t_out, TEST_MESSAGE_NO,
                                     format, v[i].value);
        if (status != v[i].expected_status)
        {
            ck_abort_msg("Test value %d. Expected status %d, got %d",
                         v[i].value, v[i].expected_status, status);
        }
    }
}

/***************************************************************************//**
 * Checks we can add a simple type right at the end of a buffer, but that
 * a buffer overflow is also detected if there's not enough space.
 */
static void
test_append_at_end_of_message(char typechar, unsigned int wire_size)
{
    char padding[LIBIPM_MAX_PAYLOAD_SIZE] = {0};
    struct libipm_fsb desc;
    enum libipm_status status;
    const char format[] = {'B', typechar, '\0'};

    /* Construct a descriptor for a binary object that fills most of
     * the output buffer, leaving just enough space for our type at the end.
     * Three bytes are needed for the binary descriptor overhead */
    desc.data = padding;
    desc.datalen = LIBIPM_MAX_PAYLOAD_SIZE - 3 - wire_size;

    /* Check we're OK at the end of the buffer... */
    status = libipm_msg_out_init(g_t_out, TEST_MESSAGE_NO, format, &desc, 0);
    ck_assert_int_eq(status, E_LI_SUCCESS);

    /* ..but if the value would overflow the end of the packet it's an error */
    ++desc.datalen;
    status = libipm_msg_out_init(g_t_out, TEST_MESSAGE_NO, format, &desc, 0);
    ck_assert_int_eq(status, E_LI_BUFFER_OVERFLOW);
}

/***************************************************************************//**
 * Appends all data types to a transport, and checks the binary data in
 * the output stream is as expected */
START_TEST(test_libipm_append_all_test)
{
    ck_assert_ptr_ne(g_t_out, NULL);

    static char string[] = { ALL_TEST_s_VALUE };
    static char binary[] = { ALL_TEST_B_VALUE };
    struct libipm_fsb binary_desc = { (void *)binary, sizeof(binary) };
    enum libipm_status status;

    status = libipm_msg_out_init(
                 g_t_out, TEST_MESSAGE_NO,
                 "ybnqiuxtshB",
                 ALL_TEST_y_VALUE,
                 ALL_TEST_b_VALUE,
                 ALL_TEST_n_VALUE,
                 ALL_TEST_q_VALUE,
                 ALL_TEST_i_VALUE,
                 ALL_TEST_u_VALUE,
                 ALL_TEST_x_VALUE,
                 ALL_TEST_t_VALUE,
                 string,
                 g_fd,
                 &binary_desc);
    ck_assert_int_eq(status, E_LI_SUCCESS);

    libipm_msg_out_mark_end(g_t_out);

    check_stream_data_eq(
        g_t_out->out_s,
        all_test_expected_buff, sizeof(all_test_expected_buff));
}
END_TEST

/***************************************************************************//**
 * Checks various send errors for 'y'
 */
START_TEST(test_libipm_send_y_type)
{
    static const struct int_test_value test_values[] =
    {
        {0, E_LI_SUCCESS},
        {255, E_LI_SUCCESS},
        {-1, E_LI_BAD_VALUE},
        {256, E_LI_BAD_VALUE},
        {63336, E_LI_BAD_VALUE},
        {2147483647, E_LI_BAD_VALUE}
    };

    test_small_type_values('y', test_values,
                           sizeof(test_values) / sizeof(test_values[0]));

    test_append_at_end_of_message('y', 2);
}
END_TEST

/***************************************************************************//**
 * Checks various send errors for 'b'
 */
START_TEST(test_libipm_send_b_type)
{
    static const struct int_test_value test_values[] =
    {
        {0, E_LI_SUCCESS},
        {1, E_LI_SUCCESS},
        {2, E_LI_BAD_VALUE},
        {-1, E_LI_BAD_VALUE},
        {256, E_LI_BAD_VALUE}
    };

    test_small_type_values('b', test_values,
                           sizeof(test_values) / sizeof(test_values[0]));

    test_append_at_end_of_message('b', 2);
}
END_TEST

/***************************************************************************//**
 * Checks various send errors for 'n'
 */
START_TEST(test_libipm_send_n_type)
{
    static const struct int_test_value test_values[] =
    {
        {-32768, E_LI_SUCCESS},
        {32767, E_LI_SUCCESS},
        {-32769, E_LI_BAD_VALUE},
        {32768, E_LI_BAD_VALUE},
        {2147483647, E_LI_BAD_VALUE}
    };

    test_small_type_values('n', test_values,
                           sizeof(test_values) / sizeof(test_values[0]));

    test_append_at_end_of_message('n', 3);
}
END_TEST

/***************************************************************************//**
 * Checks various send errors for 'q'
 */
START_TEST(test_libipm_send_q_type)
{
    static const struct int_test_value test_values[] =
    {
        {0, E_LI_SUCCESS},
        {65535, E_LI_SUCCESS},
        {-1, E_LI_BAD_VALUE},
        {65536, E_LI_BAD_VALUE},
        {2147483647, E_LI_BAD_VALUE}
    };

    test_small_type_values('q', test_values,
                           sizeof(test_values) / sizeof(test_values[0]));

    test_append_at_end_of_message('q', 3);
}
END_TEST

/***************************************************************************//**
 * Checks various send errors for 'i'
 */
START_TEST(test_libipm_send_i_type)
{
#if SIZEOF_INT > 4
    static const struct int_test_value test_values[] =
    {
        {-2147483648, E_LI_SUCCESS},
        {2147483647, E_LI_SUCCESS},
        {-2147483649, E_LI_BAD_VALUE},
        {2147483648, E_LI_BAD_VALUE},
    };

    test_small_type_values('i', test_values,
                           sizeof(test_values) / sizeof(test_values[0]));
#endif
    test_append_at_end_of_message('i', 5);
}
END_TEST

/***************************************************************************//**
 * Checks various send errors for 'u'
 */
START_TEST(test_libipm_send_u_type)
{
#if SIZEOF_INT > 4
    static const struct int_test_value test_values[] =
    {
        {0, E_LI_SUCCESS},
        {4294967296, E_LI_SUCCESS},
        {-1, E_LI_BAD_VALUE},
        {4294967297, E_LI_BAD_VALUE},
    };

    test_small_type_values('u', test_values,
                           sizeof(test_values) / sizeof(test_values[0]));
#endif
    test_append_at_end_of_message('u', 5);
}
END_TEST

/***************************************************************************//**
 * Checks various send errors for 'x'
 */
START_TEST(test_libipm_send_x_type)
{
    test_append_at_end_of_message('x', 9);
}
END_TEST

/***************************************************************************//**
 * Checks various send errors for 't'
 */
START_TEST(test_libipm_send_t_type)
{
    test_append_at_end_of_message('t', 9);
}
END_TEST

/***************************************************************************//**
 * Checks various send errors for 's'
 */
START_TEST(test_libipm_send_s_type)
{
    enum libipm_status status;

    /* The maximum string length would be LIBIPM_MAX_PAYLOAD_SIZE-2, as we
     * need one char for the 's' type marker, and another for the string
     * terminator */

    /* Construct a string one character too long to fit in the buffer */
    char str[LIBIPM_MAX_PAYLOAD_SIZE];

    g_memset(str, 'A', sizeof(str));
    str[sizeof(str) - 1] = '\0';

    /* A write should overflow... */
    status = libipm_msg_out_init(g_t_out, TEST_MESSAGE_NO, "s", str);
    ck_assert_int_eq(status, E_LI_BUFFER_OVERFLOW);

    /* .. but a string one character shorter should be OK */
    str[sizeof(str) - 2] = '\0';
    status = libipm_msg_out_init(g_t_out, TEST_MESSAGE_NO, "s", str);
    ck_assert_int_eq(status, E_LI_SUCCESS);

    /* Check passing a NULL string doesn't crash the program */
    status = libipm_msg_out_init(g_t_out, TEST_MESSAGE_NO, "s", NULL);
    ck_assert_int_eq(status, E_LI_PROGRAM_ERROR);
}
END_TEST

/***************************************************************************//**
 * Checks various send errors for 'h'
 */
START_TEST(test_libipm_send_h_type)
{
    enum libipm_status status;
    unsigned int i;

    test_append_at_end_of_message('h', 1);

    status = libipm_msg_out_init(g_t_out, TEST_MESSAGE_NO, NULL);
    ck_assert_int_eq(status, E_LI_SUCCESS);

    for (i = 0 ; i < LIBIPM_MAX_FD_PER_MSG; ++i)
    {
        status = libipm_msg_out_append(g_t_out, "h", g_fd);
        ck_assert_int_eq(status, E_LI_SUCCESS);
    }

    status = libipm_msg_out_append(g_t_out, "h", 1);
    ck_assert_int_eq(status, E_LI_TOO_MANY_FDS);
}
END_TEST

/***************************************************************************//**
 * Checks various send errors for 'B'
 */
START_TEST(test_libipm_send_B_type)
{
    enum libipm_status status;
    char bin[LIBIPM_MAX_PAYLOAD_SIZE] = {0};
    struct libipm_fsb desc;

    /* The maximum binary block length would be LIBIPM_MAX_PAYLOAD_SIZE - 3,
     * as we need one char for the 'B' type marker, and two for the length
     *
     * Construct a descriptor for a binary object that completely fills
     * the output buffer */
    desc.data = bin;
    desc.datalen = LIBIPM_MAX_PAYLOAD_SIZE - 3;

    /* Check it fits in the buffer... */
    status = libipm_msg_out_init(g_t_out, TEST_MESSAGE_NO, "B", &desc);
    ck_assert_int_eq(status, E_LI_SUCCESS);

    /* ..but one byte bigger, and it won't fit */
    ++desc.datalen;
    status = libipm_msg_out_init(g_t_out, TEST_MESSAGE_NO, "B", &desc);
    ck_assert_int_eq(status, E_LI_BUFFER_OVERFLOW);
    --desc.datalen;

    /* Check NULL pointers don't crash the library */
    desc.data = NULL;
    status = libipm_msg_out_init(g_t_out, TEST_MESSAGE_NO, "B", &desc);
    ck_assert_int_eq(status, E_LI_PROGRAM_ERROR);
    status = libipm_msg_out_init(g_t_out, TEST_MESSAGE_NO, "B", NULL);
    ck_assert_int_eq(status, E_LI_PROGRAM_ERROR);
}
END_TEST

/***************************************************************************//**
 * Checks send errors for unsupported and unimplemented types
 */
START_TEST(test_libipm_send_bad_types)
{
    enum libipm_status status;
    enum libipm_status expected_status;
    char format[2] = {0};
    char c;

    for (c = 0x1; c < 0x7f; ++c)
    {
        if (!isprint(c))
        {
            continue;
        }

        if (g_strchr(g_supported_types, c) == NULL)
        {
            expected_status = E_LI_UNSUPPORTED_TYPE;
        }
        else if (g_strchr(g_unimplemented_types, c) != NULL)
        {
            expected_status = E_LI_UNIMPLEMENTED_TYPE;
        }
        else
        {
            continue;
        }

        format[0] = c;
        status = libipm_msg_out_init(g_t_out,
                                     TEST_MESSAGE_NO_STRING_NO,
                                     format, NULL);
        if (status != expected_status)
        {
            ck_abort_msg("Output char '%c'. Expected status %d, got %d",
                         c, expected_status, status);
        }
    }
}
END_TEST

/***************************************************************************//**
 * Checks message erase works as expected
 *
 * Also calls libipm_msg_out_append() which isn't exercised anywhere else
 */
START_TEST(test_libipm_send_msg_erase)
{
    enum libipm_status status;
    const char *username = "username";
    const char *password = "password";

    status = libipm_msg_out_init(g_t_out, TEST_MESSAGE_NO, NULL);
    ck_assert_int_eq(status, E_LI_SUCCESS);

    status = libipm_msg_out_append(g_t_out, "ss", username, password);
    ck_assert_int_eq(status, E_LI_SUCCESS);

    ck_assert_int_ne(does_stream_contain_string(g_t_out->out_s, password), 0);
    libipm_msg_out_erase(g_t_out);
    ck_assert_int_eq(does_stream_contain_string(g_t_out->out_s, password), 0);
}
END_TEST

/***************************************************************************//**
 * Exercises codepaths that shouldn't be called (programming errors)
 */
START_TEST(test_libipm_send_programming_errors)
{
    enum libipm_status status;

    status = libipm_msg_out_init(g_t_vanilla, TEST_MESSAGE_NO, NULL);
    ck_assert_int_eq(status, E_LI_PROGRAM_ERROR);

    status = libipm_msg_out_append(g_t_vanilla, "i", 0);
    ck_assert_int_eq(status, E_LI_PROGRAM_ERROR);

    libipm_msg_out_mark_end(g_t_vanilla); /* No status to check */

    status = libipm_msg_out_simple_send(g_t_vanilla, TEST_MESSAGE_NO, "i", 0);
    ck_assert_int_eq(status, E_LI_PROGRAM_ERROR);

}
END_TEST

/******************************************************************************/
Suite *
make_suite_test_libipm_send_calls(void)
{
    Suite *s;
    TCase *tc;

    s = suite_create("libipm_send");

    tc = tcase_create("libipm_send");
    suite_add_tcase(s, tc);
    tcase_add_test(tc, test_libipm_append_all_test);
    tcase_add_test(tc, test_libipm_send_y_type);
    tcase_add_test(tc, test_libipm_send_b_type);
    tcase_add_test(tc, test_libipm_send_n_type);
    tcase_add_test(tc, test_libipm_send_q_type);
    tcase_add_test(tc, test_libipm_send_i_type);
    tcase_add_test(tc, test_libipm_send_u_type);
    tcase_add_test(tc, test_libipm_send_x_type);
    tcase_add_test(tc, test_libipm_send_t_type);
    tcase_add_test(tc, test_libipm_send_s_type);
    tcase_add_test(tc, test_libipm_send_h_type);
    tcase_add_test(tc, test_libipm_send_B_type);
    tcase_add_test(tc, test_libipm_send_y_type);
    tcase_add_test(tc, test_libipm_send_bad_types);
    tcase_add_test(tc, test_libipm_send_msg_erase);
    tcase_add_test(tc, test_libipm_send_programming_errors);

    return s;
}
