#include <glib.h>

#include "clock-boundary-timing.h"

static void
test_next_minute_is_found(void)
{
    g_assert_cmpuint(clock_milliseconds_until_next_boundary(59 * G_USEC_PER_SEC, 60), ==, 1000);
    g_assert_cmpuint(clock_milliseconds_until_next_boundary(60 * G_USEC_PER_SEC, 60), ==, 60000);
    g_assert_cmpuint(clock_milliseconds_until_next_boundary(500, 1), ==, 1000);
    g_assert_cmpuint(clock_milliseconds_until_next_boundary(999999, 1), ==, 1);
}

static void
test_second_formats_are_recognised(void)
{
    g_assert_false(clock_format_shows_seconds("%a %d %b %H:%M"));
    g_assert_true(clock_format_shows_seconds("%H:%M:%S"));
    g_assert_true(clock_format_shows_seconds("%T"));
    g_assert_false(clock_format_shows_seconds("100%%S"));
    g_assert_false(clock_format_shows_seconds("trailing %"));
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/clock-boundary-timing/boundary", test_next_minute_is_found         },
    {"/clock-boundary-timing/seconds",  test_second_formats_are_recognised},
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
