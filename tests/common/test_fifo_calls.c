#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include "fifo.h"

#include "os_calls.h"
#include "test_common.h"
#include "string_calls.h"

static const char *strings[] =
{
    "one",
    "two",
    "three",
    "four",
    "five",
    "six",
    "seven",
    "eight",
    "nine",
    "ten",
    "eleven",
    "twelve",
    NULL
};

#define LARGE_TEST_SIZE 10000

/******************************************************************************/
/* Item destructor function for fifo tests involving allocated strings */
static void
string_item_destructor(void *item, void *closure)
{
    free(item);

    if (closure != NULL)
    {
        /* Count the free operation */
        int *c = (int *)closure;
        ++(*c);
    }
}
/******************************************************************************/

START_TEST(test_fifo__null)
{
    struct fifo *f = NULL;
    void *vp;
    int status;

    // These calls should not crash!
    fifo_delete(f, NULL);
    fifo_clear(f, NULL);

    status = fifo_add_item(f, NULL);
    ck_assert_int_eq(status, 0);

    vp = fifo_remove_item(f);
    ck_assert_ptr_eq(vp, NULL);

    status = fifo_is_empty(f);
    ck_assert_int_eq(status, 1);
}

END_TEST

START_TEST(test_fifo__simple)
{
    struct fifo *f = fifo_create(NULL);
    ck_assert_ptr_ne(f, NULL);

    int empty = fifo_is_empty(f);
    ck_assert_int_eq(empty, 1);

    // Check we can't add NULL to the fifo
    int success = fifo_add_item(f, NULL);
    ck_assert_int_eq(success, 0);

    // Check we can't remove anything from an empty fifo
    void *vp = fifo_remove_item(f);
    ck_assert_ptr_eq(vp, NULL);

    // Add some static strings to the FIFO
    const char **s;
    unsigned int n = 0;
    for (s = &strings[0] ; *s != NULL; ++s)
    {
        fifo_add_item(f, (void *)*s);
        ++n;
    }

    empty = fifo_is_empty(f);
    ck_assert_int_eq(empty, 0);

    unsigned int i;
    for (i = 0 ; i < n ; ++i)
    {
        const char *p = (const char *)fifo_remove_item(f);
        ck_assert_ptr_eq(p, strings[i]);
    }

    empty = fifo_is_empty(f);
    ck_assert_int_eq(empty, 1);

    fifo_delete(f, NULL);
}
END_TEST


START_TEST(test_fifo__strdup)
{
    struct fifo *f = fifo_create(string_item_destructor);
    ck_assert_ptr_ne(f, NULL);

    // Add some dynamically allocated strings to the FIFO
    const char **s;
    unsigned int n = 0;
    for (s = &strings[0] ; *s != NULL; ++s)
    {
        int ok = fifo_add_item(f, (void *)strdup(*s));
        ck_assert_int_eq(ok, 1);
        ++n;
    }

    // Delete the fifo, freeing the allocated strings. Check free() is called
    // the expected number of times.
    int c = 0;
    fifo_delete(f, &c);
    ck_assert_int_eq(c, n);
}
END_TEST

START_TEST(test_fifo__large_test_1)
{
    struct fifo *f = fifo_create(string_item_destructor);
    ck_assert_ptr_ne(f, NULL);

    // Fill the fifo with dynamically allocated strings
    int i;
    for (i = 0; i < LARGE_TEST_SIZE; ++i)
    {
        int ok = fifo_add_item(f, (void *)strdup("test item"));
        ck_assert_int_eq(ok, 1);
    }

    // Clear the fifo, checking free is called the expected number of times
    int c = 0;
    fifo_clear(f, &c);
    ck_assert_int_eq(c, LARGE_TEST_SIZE);

    int empty = fifo_is_empty(f);
    ck_assert_int_eq(empty, 1);

    // Finally delete the fifo, checking free is not called this time
    c = 0;
    fifo_delete(f, &c);
    ck_assert_int_eq(c, 0);
}
END_TEST

START_TEST(test_fifo__large_test_2)
{
    char buff[64];

    struct fifo *f = fifo_create(string_item_destructor);
    ck_assert_ptr_ne(f, NULL);

    // Fill the fifo with dynamically allocated strings
    int i;
    for (i = 0; i < LARGE_TEST_SIZE; ++i)
    {
        g_snprintf(buff, sizeof(buff), "%d", i);
        int ok = fifo_add_item(f, (void *)strdup(buff));
        ck_assert_int_eq(ok, 1);
    }

    // Extract all the strings from the fifo, making sure they're
    // as expected
    for (i = 0; i < LARGE_TEST_SIZE; ++i)
    {
        g_snprintf(buff, sizeof(buff), "%d", i);
        char *s = fifo_remove_item(f);
        ck_assert_ptr_ne(s, NULL);
        ck_assert_str_eq(s, buff);
        free(s);
    }

    int empty = fifo_is_empty(f);
    ck_assert_int_eq(empty, 1);

    int c = 0;
    fifo_delete(f, &c);
    ck_assert_int_eq(c, 0);
}
END_TEST

/******************************************************************************/
Suite *
make_suite_test_fifo(void)
{
    Suite *s;
    TCase *tc_simple;

    s = suite_create("Fifo");

    tc_simple = tcase_create("simple");
    suite_add_tcase(s, tc_simple);
    tcase_add_test(tc_simple, test_fifo__null);
    tcase_add_test(tc_simple, test_fifo__simple);
    tcase_add_test(tc_simple, test_fifo__strdup);
    tcase_add_test(tc_simple, test_fifo__large_test_1);
    tcase_add_test(tc_simple, test_fifo__large_test_2);

    return s;
}
