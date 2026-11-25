#include <glib.h>

#include "memory-usage-reading.h"

static const char memory_information_text[] = "MemTotal:       32000000 kB\n"
                                              "MemFree:         1000000 kB\n"
                                              "MemAvailable:   20000000 kB\n"
                                              "SwapTotal:       8000000 kB\n"
                                              "SwapFree:        7500000 kB\n";

static void
test_memory_information_is_parsed(void)
{
    struct memory_usage usage = {0};

    g_assert_true(memory_usage_parse(memory_information_text, &usage));
    g_assert_cmpuint(usage.total_kibibytes, ==, 32000000);
    g_assert_cmpuint(usage.available_kibibytes, ==, 20000000);
    g_assert_cmpuint(usage.swap_total_kibibytes, ==, 8000000);
    g_assert_cmpuint(usage.swap_free_kibibytes, ==, 7500000);
}

static void
test_incomplete_information_is_refused(void)
{
    struct memory_usage usage = {0};

    g_assert_false(memory_usage_parse("MemTotal: 100 kB\n", &usage));
    g_assert_false(memory_usage_parse("MemTotal: lots\nMemAvailable: 5 kB\n", &usage));
}

static void
test_real_information_is_readable(void)
{
    struct memory_usage usage = {0};

    g_assert_true(memory_usage_read(&usage));
    g_assert_cmpuint(usage.available_kibibytes, <=, usage.total_kibibytes);
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/memory-usage-reading/parse",      test_memory_information_is_parsed     },
    {"/memory-usage-reading/incomplete", test_incomplete_information_is_refused},
    {"/memory-usage-reading/real",       test_real_information_is_readable     },
};

int
main(
    int   argc,
    char *argv[]
)
{
    g_test_init(&argc, &argv, NULL);

    for (size_t i = 0; i < G_N_ELEMENTS(registered_tests); i++) {
        g_test_add_func(registered_tests[i].test_path, registered_tests[i].test_function);
    }

    return g_test_run();
}
