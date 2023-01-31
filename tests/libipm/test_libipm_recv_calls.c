
#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include <unistd.h>

#include "libipm.h"
#include "os_calls.h"
#include "string_calls.h"
#include "trans.h"

#include "test_libipm.h"

/* Random(ish) values for test_libipm_send_recv_all_test */
#define TEST_y_VALUE 89
#define TEST_b_VALUE 1
#define TEST_n_VALUE -143
#define TEST_q_VALUE 329
#define TEST_i_VALUE -150000
#define TEST_u_VALUE 150000
#define TEST_x_VALUE -4500000000L
#define TEST_t_VALUE 8500000000L
#define TEST_s_VALUE "libipm recv test"
#define TEST_B_VALUE  'b', 'y', 'B', 'Y', '8', '2', '/'

/**
 * Type for fields in the message header
 *
 * The type value is the offset of the field within the header
 */
enum header_field
{
    HDR_IPM_VER = 0,
    HDR_MSG_LEN = 2,
    HDR_FACILITY = 4,
    HDR_MSGNO = 6,
    HDR_RESERVED = 8
};

/**
 * Gets a message header field value */
static unsigned int
get_header_field(struct stream *s, enum header_field field)
{
    unsigned int res;
    char *saved_p = s->p;
    s->p = s->data + (unsigned short)field;
    if (field == HDR_RESERVED)
    {
        in_uint32_le(s, res);
    }
    else
    {
        in_uint16_le(s, res);
    }

    s->p = saved_p;

    return res;
}

/**
 * Sets a message header field value */
static void
set_header_field(struct stream *s, enum header_field field, unsigned int val)
{
    char *saved_p = s->p;
    s->p = s->data + (unsigned short)field;
    if (field == HDR_RESERVED)
    {
        out_uint32_le(s, val);
    }
    else
    {
        out_uint16_le(s, val);
    }

    s->p = saved_p;
}

/**
 * Flushes input on a non-blocking socket
 *
 * Returns number of bytes read
 */
static unsigned int
flush_socket(int sck)
{
    char buff[1024];
    unsigned int result = 0;
    int status;
    while (1)
    {
        status = g_sck_recv(g_t_in->sck, buff, sizeof(buff), 0);
        if (status < 0)
        {
            break;
        }
        result += status;
    }

    return result;
}

/**
 * Waits for an expected incoming message */
static void
check_for_incoming_message(unsigned short expected_msgno)
{
    enum libipm_status status;
    unsigned short msgno;

    /* Get the message at the other end */
    libipm_msg_in_reset(g_t_in);
    status = libipm_msg_in_wait_available(g_t_in);
    ck_assert_int_eq(status, E_LI_SUCCESS);

    msgno = libipm_msg_in_get_msgno(g_t_in);
    ck_assert_int_eq(msgno, expected_msgno);
}

/**
 * Test for input truncated by a number of bytes
 *
 * This call assumes the output buffer contains a message of a single
 * type that has already been successfully sent and checked.
 *
 * The send size is truncated, and the message is sent again. This
 * lets us check the input parser won't accept a type for which
 * there is insufficient data.
 */
static void
test_truncated_input_type(char typechar, void *valueptr,
                          int truncate_value,
                          enum libipm_status expected_status)

{
    const char format[] = {typechar, '\0'};
    enum libipm_status status;
    int istatus;
    unsigned int msg_size;

    /* The same message is still in the buffer.
     *
     * reduce the payload length by the truncate value...*/
    msg_size = get_header_field(g_t_out->out_s, HDR_MSG_LEN);
    set_header_field(g_t_out->out_s, HDR_MSG_LEN, msg_size - truncate_value);

    /* ...and re-send it */
    istatus = trans_force_write(g_t_out);
    ck_assert_int_eq(istatus, 0);

    /* Catch the message at the other end */
    check_for_incoming_message(TEST_MESSAGE_NO);

    /* Check the value */
    switch (typechar)
    {
        case 'B':
            /* For this type, the descriptor is critical to read
             * the value */
            status = libipm_msg_in_parse(g_t_in, format,
                                         (struct fsb_type *)valueptr);
            break;
        default:
            /* For other types, the value shouldn't be needed, as
             * it's only written to if the parse is successful */
            status = libipm_msg_in_parse(g_t_in, format,
                                         (struct fsb_type *)NULL);
            break;
    }
    ck_assert_int_eq(status, expected_status);

    /* There should be 'truncate_value' extra octets left on the input
    * socket which wasn't read when we shrunk the header */
    istatus = flush_socket(g_t_in->sck);
    ck_assert_int_eq(istatus, truncate_value);

    /* Put the message size back to its original value, for further tests */
    set_header_field(g_t_out->out_s, HDR_MSG_LEN, msg_size);
}

/**
 * Test for bad header values
 *
 * This call assumes the output buffer contains a message containing
 * a single 'u' type that has already been send and checked
 *
 * The specified header field is overwritten, and the message is
 * sent. On the receive side we check for a 'bad header' error, and
 * then put everything back to its starting place.
 */
static void
test_bad_header_value(enum header_field field, unsigned int test_val)
{
    unsigned int old_val;
    enum libipm_status status;
    int istatus;

    /* The same message is still in the buffer.
     *
     * Save the field value we are going to change, and replace it */
    old_val = get_header_field(g_t_out->out_s, field);
    set_header_field(g_t_out->out_s, field, test_val);

    /* re-send the message */
    istatus = trans_force_write(g_t_out);
    ck_assert_int_eq(istatus, 0);

    /* Catch the message at the other end. The error is
     * reported when we wait for the incoming message */
    libipm_msg_in_reset(g_t_in);
    status = libipm_msg_in_wait_available(g_t_in);
    ck_assert_int_eq(status, E_LI_BAD_HEADER);

    /* There should be 5 extra octets ('u' + 32-bit value) left on the input
    * socket which wasn't read when we broke the header */
    istatus = flush_socket(g_t_in->sck);
    ck_assert_int_eq(istatus, 1 + sizeof(uint32_t));

    /* Put the message size back to its original value, for further tests */
    set_header_field(g_t_out->out_s, field, old_val);
}

/***************************************************************************//**
 * As the 'append all' test, but data is sent across a link, demarshalled,
 * and validated */
START_TEST(test_libipm_send_recv_all_test)
{
    ck_assert_ptr_ne(g_t_out, NULL);
    ck_assert_ptr_ne(g_t_in, NULL);

    static char bin_out[] = { TEST_B_VALUE };
    struct libipm_fsb binary_desc = { (void *)bin_out, sizeof(bin_out) };
    enum libipm_status status;

    /* variables for received values */
    uint8_t y;
    int b;
    int16_t n;
    uint16_t q;
    int32_t i;
    uint32_t u;
    int64_t x;
    uint64_t t;
    char *s;
    int h = -1;
    char B[sizeof(bin_out)];
    char c;

    /* The message is small enough to fit in the socket buffer */
    status = libipm_msg_out_simple_send(
                 g_t_out, TEST_MESSAGE_NO,
                 "ybnqiuxtshB",
                 TEST_y_VALUE,
                 TEST_b_VALUE,
                 TEST_n_VALUE,
                 TEST_q_VALUE,
                 TEST_i_VALUE,
                 TEST_u_VALUE,
                 TEST_x_VALUE,
                 TEST_t_VALUE,
                 TEST_s_VALUE,
                 g_fd,
                 &binary_desc);
    ck_assert_int_eq(status, E_LI_SUCCESS);

    /* Catch the message at the other end */
    check_for_incoming_message(TEST_MESSAGE_NO);

    /* Re-use our descriptor for the receive operation */
    binary_desc.data = (void *)&B;
    binary_desc.datalen = sizeof(B);

    status = libipm_msg_in_parse(
                 g_t_in,
                 "ybnqiuxtshB",
                 &y, &b, &n, &q, &i, &u, &x, &t, &s, &h, &binary_desc);

    ck_assert_int_eq(status, E_LI_SUCCESS);
    ck_assert_int_eq(y, TEST_y_VALUE);
    ck_assert_int_eq(b, TEST_b_VALUE);
    ck_assert_int_eq(n, TEST_n_VALUE);
    ck_assert_int_eq(q, TEST_q_VALUE);
    ck_assert_int_eq(i, TEST_i_VALUE);
    ck_assert_int_eq(u, TEST_u_VALUE);
    ck_assert_int_eq(x, TEST_x_VALUE);
    ck_assert_int_eq(t, TEST_t_VALUE);
    check_binary_data_eq(TEST_s_VALUE, sizeof(TEST_s_VALUE) - 1,
                         s, g_strlen(s));
    /* The file descriptor should not be -1, neither should it be
     * the value we sent. It should also point to /dev/zero */
    ck_assert_int_ne(h, -1);
    ck_assert_int_ne(h, g_fd);
    check_fd_is_dev_zero(h);
    g_file_close(h);

    check_binary_data_eq(bin_out, sizeof(bin_out), B, sizeof(B));

    /* Check we're at the end of the message */
    c = libipm_msg_in_peek_type(g_t_in);
    ck_assert_int_eq(c, '\0');
}
END_TEST

/***************************************************************************//**
 * Checks various receive errors for 'y'
 */
START_TEST(test_libipm_receive_y_type)
{
    enum libipm_status status;
    uint8_t y;

    /* First, a simple send... */
    status = libipm_msg_out_simple_send(
                 g_t_out, TEST_MESSAGE_NO, "y", TEST_y_VALUE);
    ck_assert_int_eq(status, E_LI_SUCCESS);

    check_for_incoming_message(TEST_MESSAGE_NO);

    /* Check the value */
    status = libipm_msg_in_parse( g_t_in, "y", &y);
    ck_assert_int_eq(status, E_LI_SUCCESS);
    ck_assert_int_eq(y, TEST_y_VALUE);

    test_truncated_input_type('y', &y, 1, E_LI_BUFFER_OVERFLOW);
}
END_TEST

/***************************************************************************//**
 * Checks various receive errors for 'b'
 */
START_TEST(test_libipm_receive_b_type)
{
    enum libipm_status status;
    int b;
    int istatus;
    char c;

    /* First, a simple send... */
    status = libipm_msg_out_simple_send(
                 g_t_out, TEST_MESSAGE_NO, "b", TEST_b_VALUE);
    ck_assert_int_eq(status, E_LI_SUCCESS);

    check_for_incoming_message(TEST_MESSAGE_NO);

    /* Check the value */
    status = libipm_msg_in_parse( g_t_in, "b", &b);
    ck_assert_int_eq(status, E_LI_SUCCESS);
    ck_assert_int_eq(b, TEST_b_VALUE);

    test_truncated_input_type('b', &b, 1, E_LI_BUFFER_OVERFLOW);

    /* Resend and re-catch the message */
    istatus = trans_force_write(g_t_out);
    ck_assert_int_eq(istatus, 0);
    check_for_incoming_message(TEST_MESSAGE_NO);

    /* Modify the message to contain a '2' for the boolean */
    c = libipm_msg_in_peek_type(g_t_in);
    ck_assert_int_eq(c, 'b'); /* Next type should be a 'b' */
    c = *(g_t_in->in_s->p + 1); /* This should be the test value */
    ck_assert_int_eq(c, TEST_b_VALUE); /* Check it */
    *(g_t_in->in_s->p + 1) = 2;
    status = libipm_msg_in_parse( g_t_in, "b", &b);
    ck_assert_int_eq(status, E_LI_BAD_VALUE);

    /* Resend and re-catch the message */
    istatus = trans_force_write(g_t_out);
    ck_assert_int_eq(istatus, 0);
    check_for_incoming_message(TEST_MESSAGE_NO);

    /* Modify the message to contain a '-1' for the boolean */
    *(g_t_in->in_s->p + 1) = (char) -1;
    status = libipm_msg_in_parse( g_t_in, "b", &b);
    ck_assert_int_eq(status, E_LI_BAD_VALUE);
}
END_TEST

/***************************************************************************//**
 * Checks various receive errors for 'n'
 */
START_TEST(test_libipm_receive_n_type)
{
    enum libipm_status status;
    int16_t n;

    /* First, a simple send... */
    status = libipm_msg_out_simple_send(
                 g_t_out, TEST_MESSAGE_NO, "n", TEST_n_VALUE);
    ck_assert_int_eq(status, E_LI_SUCCESS);

    check_for_incoming_message(TEST_MESSAGE_NO);

    /* Check the value */
    status = libipm_msg_in_parse( g_t_in, "n", &n);
    ck_assert_int_eq(status, E_LI_SUCCESS);
    ck_assert_int_eq(n, TEST_n_VALUE);

    test_truncated_input_type('n', &n, 1, E_LI_BUFFER_OVERFLOW);
}
END_TEST

/***************************************************************************//**
 * Checks various receive errors for 'q'
 */
START_TEST(test_libipm_receive_q_type)
{
    enum libipm_status status;
    uint16_t q;

    /* First, a simple send... */
    status = libipm_msg_out_simple_send(
                 g_t_out, TEST_MESSAGE_NO, "q", TEST_q_VALUE);
    ck_assert_int_eq(status, E_LI_SUCCESS);

    check_for_incoming_message(TEST_MESSAGE_NO);

    /* Check the value */
    status = libipm_msg_in_parse( g_t_in, "q", &q);
    ck_assert_int_eq(status, E_LI_SUCCESS);
    ck_assert_int_eq(q, TEST_q_VALUE);

    test_truncated_input_type('q', &q, 1, E_LI_BUFFER_OVERFLOW);
}
END_TEST

/***************************************************************************//**
 * Checks various receive errors for 'i'
 */
START_TEST(test_libipm_receive_i_type)
{
    enum libipm_status status;
    int32_t i;

    /* First, a simple send... */
    status = libipm_msg_out_simple_send(
                 g_t_out, TEST_MESSAGE_NO, "i", TEST_i_VALUE);
    ck_assert_int_eq(status, E_LI_SUCCESS);

    check_for_incoming_message(TEST_MESSAGE_NO);

    /* Check the value */
    status = libipm_msg_in_parse( g_t_in, "i", &i);
    ck_assert_int_eq(status, E_LI_SUCCESS);
    ck_assert_int_eq(i, TEST_i_VALUE);

    test_truncated_input_type('i', &i, 1, E_LI_BUFFER_OVERFLOW);
}
END_TEST

/***************************************************************************//**
 * Checks various receive errors for 'u'
 */
START_TEST(test_libipm_receive_u_type)
{
    enum libipm_status status;
    uint32_t u;

    /* First, a simple send... */
    status = libipm_msg_out_simple_send(
                 g_t_out, TEST_MESSAGE_NO, "u", TEST_u_VALUE);
    ck_assert_int_eq(status, E_LI_SUCCESS);

    check_for_incoming_message(TEST_MESSAGE_NO);

    /* Check the value */
    status = libipm_msg_in_parse( g_t_in, "u", &u);
    ck_assert_int_eq(status, E_LI_SUCCESS);
    ck_assert_int_eq(u, TEST_u_VALUE);

    test_truncated_input_type('u', &u, 1, E_LI_BUFFER_OVERFLOW);
}
END_TEST

/***************************************************************************//**
 * Checks various receive errors for 'x'
 */
START_TEST(test_libipm_receive_x_type)
{
    enum libipm_status status;
    int64_t x;

    /* First, a simple send... */
    status = libipm_msg_out_simple_send(
                 g_t_out, TEST_MESSAGE_NO, "x", TEST_x_VALUE);
    ck_assert_int_eq(status, E_LI_SUCCESS);

    check_for_incoming_message(TEST_MESSAGE_NO);

    /* Check the value */
    status = libipm_msg_in_parse( g_t_in, "x", &x);
    ck_assert_int_eq(status, E_LI_SUCCESS);
    ck_assert_int_eq(x, TEST_x_VALUE);

    test_truncated_input_type('x', &x, 1, E_LI_BUFFER_OVERFLOW);
}
END_TEST

/***************************************************************************//**
 * Checks various receive errors for 't'
 */
START_TEST(test_libipm_receive_t_type)
{
    enum libipm_status status;
    uint64_t t;

    /* First, a simple send... */
    status = libipm_msg_out_simple_send(
                 g_t_out, TEST_MESSAGE_NO, "t", TEST_t_VALUE);
    ck_assert_int_eq(status, E_LI_SUCCESS);

    check_for_incoming_message(TEST_MESSAGE_NO);

    /* Check the value */
    status = libipm_msg_in_parse( g_t_in, "t", &t);
    ck_assert_int_eq(status, E_LI_SUCCESS);
    ck_assert_int_eq(t, TEST_t_VALUE);

    test_truncated_input_type('t', &t, 1, E_LI_BUFFER_OVERFLOW);
}
END_TEST

/***************************************************************************//**
 * Checks various receive errors for 's'
 */
START_TEST(test_libipm_receive_s_type)
{
    enum libipm_status status;
    char *s;

    /* First, a simple send... */
    status = libipm_msg_out_simple_send(
                 g_t_out, TEST_MESSAGE_NO, "s", TEST_s_VALUE);
    ck_assert_int_eq(status, E_LI_SUCCESS);

    check_for_incoming_message(TEST_MESSAGE_NO);

    /* Check the value */
    status = libipm_msg_in_parse( g_t_in, "s", &s);
    ck_assert_int_eq(status, E_LI_SUCCESS);
    check_binary_data_eq(TEST_s_VALUE, sizeof(TEST_s_VALUE) - 1,
                         s, g_strlen(s));

    /* This effectively tests that unterminated strings are not
     * passed back to the user */
    test_truncated_input_type('s', &s, 1, E_LI_BAD_VALUE);
}
END_TEST


/***************************************************************************//**
 * Checks various receive errors for 'h'
 */
START_TEST(test_libipm_receive_h_type)
{
    enum libipm_status status;
    int istatus;
    unsigned int i;
    int fd_count;

    /* Get the number of open file descriptors */
    int base_fd_count = get_open_fd_count();
    ck_assert_int_gt(base_fd_count, 0);

    /* Send the max number of copies of the /dev/zero
     * file descriptor */
    status = libipm_msg_out_init(g_t_out, TEST_MESSAGE_NO, NULL);
    ck_assert_int_eq(status, E_LI_SUCCESS);

    for (i = 0 ; i < LIBIPM_MAX_FD_PER_MSG; ++i)
    {
        status = libipm_msg_out_append(g_t_out, "h", g_fd);
        ck_assert_int_eq(status, E_LI_SUCCESS);
    }
    libipm_msg_out_mark_end(g_t_out);
    istatus = trans_force_write(g_t_out);
    ck_assert_int_eq(istatus, 0);

    check_for_incoming_message(TEST_MESSAGE_NO);

    /* Check the number of file descriptors has gone up as expected */
    fd_count = get_open_fd_count();
    ck_assert_int_eq(fd_count, base_fd_count + LIBIPM_MAX_FD_PER_MSG);

    /* Check half the descriptors work */
    for (i = 0 ; i < LIBIPM_MAX_FD_PER_MSG / 2; ++i)
    {
        int h = -1;
        status = libipm_msg_in_parse(g_t_in, "h", &h);
        ck_assert_int_eq(status, E_LI_SUCCESS);
        check_fd_is_dev_zero(h);
        g_file_close(h);
    }

    /* Close the message without reading the other descriptors */
    libipm_msg_in_reset(g_t_in);

    /* Check all the file descriptors we received have been closed */
    fd_count = get_open_fd_count();
    ck_assert_int_eq(fd_count, base_fd_count);
}
END_TEST

/***************************************************************************//**
 * Checks various receive errors for 'B'
 */
START_TEST(test_libipm_receive_B_type)
{
    enum libipm_status status;
    static char bin_out[] = { TEST_B_VALUE };
    char bin_in[sizeof(bin_out)];

    struct libipm_fsb binary_desc = { (void *)bin_out, sizeof(bin_out) };

    /* First, a simple send... */
    status = libipm_msg_out_simple_send(
                 g_t_out, TEST_MESSAGE_NO, "B", &binary_desc);
    ck_assert_int_eq(status, E_LI_SUCCESS);

    check_for_incoming_message(TEST_MESSAGE_NO);

    /* Check the value */
    binary_desc.data = (void *)bin_in;
    binary_desc.datalen = sizeof(bin_in);

    status = libipm_msg_in_parse( g_t_in, "B", &binary_desc);
    ck_assert_int_eq(status, E_LI_SUCCESS);
    check_binary_data_eq(bin_out, sizeof(bin_out),
                         bin_in, sizeof(bin_in));

    /* Check a truncated binary object is rejected */
    test_truncated_input_type('B', &binary_desc, 1, E_LI_BUFFER_OVERFLOW);

    /* Check a binary object without a complete length field is rejected */
    test_truncated_input_type('B', &binary_desc, sizeof(bin_out) + 1,
                              E_LI_BUFFER_OVERFLOW);

    /* Check a binary object with a mismatching FSB length field is rejected */
    --binary_desc.datalen;
    test_truncated_input_type('B', &binary_desc, 0, E_LI_BAD_VALUE);
    ++binary_desc.datalen;

    /* Check a binary object with a null FSB data field is rejected */
    binary_desc.data = NULL;
    test_truncated_input_type('B', &binary_desc, 0, E_LI_PROGRAM_ERROR);
    test_truncated_input_type('B', NULL, 0, E_LI_PROGRAM_ERROR);
}
END_TEST


/***************************************************************************//**
 * Checks for a message with a completely missing type
 */
START_TEST(test_libipm_receive_no_type)
{
    enum libipm_status status;
    uint32_t u;

    /* First, a simple send... */
    status = libipm_msg_out_simple_send(
                 g_t_out, TEST_MESSAGE_NO, "u", TEST_u_VALUE);
    ck_assert_int_eq(status, E_LI_SUCCESS);

    check_for_incoming_message(TEST_MESSAGE_NO);

    /* Check the value */
    status = libipm_msg_in_parse( g_t_in, "u", &u);
    ck_assert_int_eq(status, E_LI_SUCCESS);
    ck_assert_int_eq(u, TEST_u_VALUE);

    /* Completely remove the type flag and the value from the message */
    test_truncated_input_type('u', &u, sizeof(uint32_t) + 1,
                              E_LI_BUFFER_OVERFLOW);
}
END_TEST

/***************************************************************************//**
 * Checks for a message with an unexpected type
 */
START_TEST(test_libipm_receive_unexpected_type)
{
    enum libipm_status status;
    int istatus;
    uint32_t u;
    int32_t i;

    /* First, a simple send... */
    status = libipm_msg_out_simple_send(
                 g_t_out, TEST_MESSAGE_NO, "u", TEST_u_VALUE);
    ck_assert_int_eq(status, E_LI_SUCCESS);

    check_for_incoming_message(TEST_MESSAGE_NO);

    /* Check the value */
    status = libipm_msg_in_parse( g_t_in, "u", &u);
    ck_assert_int_eq(status, E_LI_SUCCESS);
    ck_assert_int_eq(u, TEST_u_VALUE);

    /* Resend and re-catch the message */
    istatus = trans_force_write(g_t_out);
    ck_assert_int_eq(istatus, 0);
    check_for_incoming_message(TEST_MESSAGE_NO);

    /* Try parsing with a type mismatch */
    status = libipm_msg_in_parse( g_t_in, "i", &i);
    ck_assert_int_eq(status, E_LI_UNEXPECTED_TYPE);
}
END_TEST

/***************************************************************************//**
 * Checks for a message with an unsupported type
 */
START_TEST(test_libipm_receive_unsupported_type)
{
    enum libipm_status status;
    int istatus;
    uint32_t u;
    char c;

    /* First, a simple send... */
    status = libipm_msg_out_simple_send(
                 g_t_out, TEST_MESSAGE_NO, "u", TEST_u_VALUE);
    ck_assert_int_eq(status, E_LI_SUCCESS);

    check_for_incoming_message(TEST_MESSAGE_NO);

    /* Check the value */
    status = libipm_msg_in_parse( g_t_in, "u", &u);
    ck_assert_int_eq(status, E_LI_SUCCESS);
    ck_assert_int_eq(u, TEST_u_VALUE);

    /* Resend and re-catch the message */
    istatus = trans_force_write(g_t_out);
    ck_assert_int_eq(istatus, 0);
    check_for_incoming_message(TEST_MESSAGE_NO);

    /* Modify the message to contain an unsupported type */
    c = libipm_msg_in_peek_type(g_t_in);
    ck_assert_int_eq(c, 'u'); /* Next type should be a 'u' */
    *g_t_in->in_s->p = 'A'; /* unsupported type */
    c = libipm_msg_in_peek_type(g_t_in);
    ck_assert_int_eq(c, '?'); /* peek should say this is an error */

    status = libipm_msg_in_parse( g_t_in, "A", NULL); /* Parse it anyway */
    ck_assert_int_eq(status, E_LI_UNSUPPORTED_TYPE);
}
END_TEST


/***************************************************************************//**
 * Checks for a message with an unimplemented type
 */
START_TEST(test_libipm_receive_unimplemented_type)
{
    enum libipm_status status;
    int istatus;
    uint32_t u;
    char c;

    /* First, a simple send... */
    status = libipm_msg_out_simple_send(
                 g_t_out, TEST_MESSAGE_NO_STRING_NO, "u", TEST_u_VALUE);
    ck_assert_int_eq(status, E_LI_SUCCESS);

    check_for_incoming_message(TEST_MESSAGE_NO_STRING_NO);

    /* Check the value */
    status = libipm_msg_in_parse( g_t_in, "u", &u);
    ck_assert_int_eq(status, E_LI_SUCCESS);
    ck_assert_int_eq(u, TEST_u_VALUE);

    /* Resend and re-catch the message */
    istatus = trans_force_write(g_t_out);
    ck_assert_int_eq(istatus, 0);
    check_for_incoming_message(TEST_MESSAGE_NO_STRING_NO);

    /* Try parsing with an unimplemented type.
     * To do this we need to modify the type marker in the input stream */
    c = libipm_msg_in_peek_type(g_t_in);
    ck_assert_int_eq(c, 'u'); /* Next type should be a 'u' */
    *g_t_in->in_s->p = 'd'; /* reserved type */
    c = libipm_msg_in_peek_type(g_t_in);
    ck_assert_int_eq(c, 'd');
    status = libipm_msg_in_parse( g_t_in, "d", NULL);
    ck_assert_int_eq(status, E_LI_UNIMPLEMENTED_TYPE);
}
END_TEST

/***************************************************************************//**
 * Checks for bad header values
 */
START_TEST(test_libipm_receive_bad_header)
{
    enum libipm_status status;
    uint32_t u;
    unsigned short hdr_ipm_ver;
    unsigned short hdr_facility;
    unsigned int i;

    /* First, a simple send... */
    status = libipm_msg_out_simple_send(
                 g_t_out, TEST_MESSAGE_NO, "u", TEST_u_VALUE);
    ck_assert_int_eq(status, E_LI_SUCCESS);

    check_for_incoming_message(TEST_MESSAGE_NO);

    /* Check the value */
    status = libipm_msg_in_parse( g_t_in, "u", &u);
    ck_assert_int_eq(status, E_LI_SUCCESS);
    ck_assert_int_eq(u, TEST_u_VALUE);

    /* Save existing header values */
    hdr_ipm_ver = get_header_field(g_t_out->out_s, HDR_IPM_VER);
    hdr_facility = get_header_field(g_t_out->out_s, HDR_FACILITY);

    test_bad_header_value(HDR_IPM_VER, hdr_ipm_ver + 1);
    test_bad_header_value(HDR_FACILITY, hdr_facility - 1);
    test_bad_header_value(HDR_RESERVED, 0xff);
    test_bad_header_value(HDR_MSG_LEN, LIBIPM_MAX_MESSAGE_SIZE + 1);
    test_bad_header_value(HDR_MSG_LEN, 0xffff);
    for (i = 0 ; i < LIBIPM_HEADER_SIZE; ++i)
    {
        test_bad_header_value(HDR_MSG_LEN, i);
    }
}
END_TEST


/***************************************************************************//**
 * Checks message erase works as expected
 */
START_TEST(test_libipm_receive_msg_erase)
{
    enum libipm_status status;
    int istatus;
    const char *username = "username";
    const char *password = "password";

    /* Send a message containing sensitive information */
    status = libipm_msg_out_simple_send(g_t_out, TEST_MESSAGE_NO, "ss",
                                        username, password);
    ck_assert_int_eq(status, E_LI_SUCCESS);

    check_for_incoming_message(TEST_MESSAGE_NO);
    libipm_set_flags(g_t_in, LIBIPM_E_MSG_IN_ERASE_AFTER_USE);

    ck_assert_int_ne(does_stream_contain_string(g_t_in->in_s, password), 0);
    libipm_msg_in_reset(g_t_in);
    ck_assert_msg(does_stream_contain_string(g_t_in->in_s, password) == 0,
                  "Auto buffer reset on input not working");

    /* The flag should be reset automatically */
    /* Resend and re-catch the message */
    istatus = trans_force_write(g_t_out);
    ck_assert_int_eq(istatus, 0);
    check_for_incoming_message(TEST_MESSAGE_NO);

    ck_assert_int_ne(does_stream_contain_string(g_t_in->in_s, password), 0);
    libipm_msg_in_reset(g_t_in);
    ck_assert_msg(does_stream_contain_string(g_t_in->in_s, password) != 0,
                  "LIBIPM_E_MSG_IN_ERASE_AFTER_USE not automatically cleared");
}
END_TEST
/***************************************************************************//**
 * Exercises codepaths that shouldn't be called (programming errors)
 */
START_TEST(test_libipm_receive_programming_errors)
{
    enum libipm_status status;
    int available;
    int32_t dummy;

    status = libipm_msg_in_wait_available(g_t_vanilla);
    ck_assert_int_eq(status, E_LI_PROGRAM_ERROR);

    status = libipm_msg_in_check_available(g_t_vanilla, &available);
    ck_assert_int_eq(status, E_LI_PROGRAM_ERROR);

    status = libipm_msg_in_parse(g_t_vanilla, "i", &dummy);
    ck_assert_int_eq(status, E_LI_PROGRAM_ERROR);

    libipm_msg_in_reset(g_t_vanilla); /* No status to check */

}
END_TEST

/******************************************************************************/
Suite *
make_suite_test_libipm_recv_calls(void)
{
    Suite *s;
    TCase *tc;

    s = suite_create("libipm_recv");

    tc = tcase_create("libipm_recv");
    suite_add_tcase(s, tc);
    tcase_add_test(tc, test_libipm_send_recv_all_test);
    tcase_add_test(tc, test_libipm_receive_y_type);
    tcase_add_test(tc, test_libipm_receive_b_type);
    tcase_add_test(tc, test_libipm_receive_n_type);
    tcase_add_test(tc, test_libipm_receive_q_type);
    tcase_add_test(tc, test_libipm_receive_i_type);
    tcase_add_test(tc, test_libipm_receive_u_type);
    tcase_add_test(tc, test_libipm_receive_x_type);
    tcase_add_test(tc, test_libipm_receive_t_type);
    tcase_add_test(tc, test_libipm_receive_s_type);
    tcase_add_test(tc, test_libipm_receive_h_type);
    tcase_add_test(tc, test_libipm_receive_B_type);
    tcase_add_test(tc, test_libipm_receive_no_type);
    tcase_add_test(tc, test_libipm_receive_unexpected_type);
    tcase_add_test(tc, test_libipm_receive_unsupported_type);
    tcase_add_test(tc, test_libipm_receive_unimplemented_type);
    tcase_add_test(tc, test_libipm_receive_bad_header);
    tcase_add_test(tc, test_libipm_receive_msg_erase);
    tcase_add_test(tc, test_libipm_receive_programming_errors);

    return s;
}
