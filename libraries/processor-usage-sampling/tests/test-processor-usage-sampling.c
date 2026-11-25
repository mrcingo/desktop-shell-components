#include <glib.h>

#include "processor-usage-sampling.h"

static void
test_statistics_are_parsed(void)
{
    struct processor_time_totals totals = {0};

    g_assert_true(processor_time_totals_parse("cpu  10 0 10 70 10 0 0 0 5 0\ncpu0 1 2 3\n", &totals));
    g_assert_cmpuint(totals.total_ticks, ==, 100);
    g_assert_cmpuint(totals.busy_ticks, ==, 20);
}

static void
test_malformed_statistics_are_refused(void)
{
    struct processor_time_totals totals = {0};

    g_assert_false(processor_time_totals_parse("cpu0 1 2 3\n", &totals));
    g_assert_false(processor_time_totals_parse("", &totals));
}

static void
test_usage_between_samples(void)
{
    const struct processor_time_totals earlier = {.busy_ticks = 20, .total_ticks = 100};
    const struct processor_time_totals later   = {.busy_ticks = 45, .total_ticks = 200};

    g_assert_cmpint(processor_usage_percent(&earlier, &later), ==, 25);
    g_assert_cmpint(processor_usage_percent(&later, &earlier), ==, -1);
    g_assert_cmpint(processor_usage_percent(&earlier, &earlier), ==, -1);
}

static void
test_real_statistics_are_readable(void)
{
    struct processor_time_totals totals = {0};

    g_assert_true(processor_time_totals_read(&totals));
    g_assert_cmpuint(totals.total_ticks, >=, totals.busy_ticks);
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/processor-usage-sampling/parse",     test_statistics_are_parsed           },
    {"/processor-usage-sampling/malformed", test_malformed_statistics_are_refused},
    {"/processor-usage-sampling/usage",     test_usage_between_samples           },
    {"/processor-usage-sampling/real",      test_real_statistics_are_readable    },
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
