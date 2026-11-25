#include <glib.h>

#include "human-readable-quantities.h"

static const struct {
    uint64_t    bytes;
    const char *expected_text;
} byte_size_cases[] = {
    {0,           "0B"   },
    {1023,        "1023B"},
    {1024,        "1.0K" },
    {1258291,     "1.2M" },
    {850000,      "830K" },
    {16106127360, "15G"  },
    {UINT64_MAX,  "16E"  },
};

static void
assert_byte_size(
    size_t case_index
)
{
    g_autofree char *text = human_readable_byte_size(byte_size_cases[case_index].bytes);

    g_assert_cmpstr(text, ==, byte_size_cases[case_index].expected_text);
}

static void
test_byte_sizes(void)
{
    for (size_t i = 0; i < G_N_ELEMENTS(byte_size_cases); i++) {
        assert_byte_size(i);
    }
}

static const struct {
    int64_t     seconds;
    const char *expected_text;
} duration_cases[] = {
    {-5,    "0m"     },
    {59,    "0m"     },
    {2700,  "45m"    },
    {8100,  "2h 15m" },
    {36060, "10h 01m"},
};

static void
assert_duration(
    size_t case_index
)
{
    g_autofree char *text = human_readable_duration(duration_cases[case_index].seconds);

    g_assert_cmpstr(text, ==, duration_cases[case_index].expected_text);
}

static void
test_durations(void)
{
    for (size_t i = 0; i < G_N_ELEMENTS(duration_cases); i++) {
        assert_duration(i);
    }
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/human-readable-quantities/byte-sizes", test_byte_sizes},
    {"/human-readable-quantities/durations",  test_durations },
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
