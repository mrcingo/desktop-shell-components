#include <glib.h>

#include "rendered-text-width-measurement.h"

static void
test_longer_text_is_wider(void)
{
    struct rendered_text_width_measurement *measurement = rendered_text_width_measurement_create("monospace 10");

    int short_width = rendered_text_width_measurement_measure_markup(measurement, "abc");
    int long_width  = rendered_text_width_measurement_measure_markup(measurement, "abcabcabc");

    g_assert_cmpint(short_width, >, 0);
    g_assert_cmpint(long_width, >, short_width);
    rendered_text_width_measurement_destroy(measurement);
}

/* Tags take no space; entities take the space of their character. */
static void
test_markup_is_measured_as_shown(void)
{
    struct rendered_text_width_measurement *measurement = rendered_text_width_measurement_create("monospace 10");

    int plain_width  = rendered_text_width_measurement_measure_markup(measurement, "a&b");
    int markup_width = rendered_text_width_measurement_measure_markup(measurement, "<span alpha='60%'>a&amp;b</span>");

    g_assert_cmpint(markup_width, ==, plain_width);
    rendered_text_width_measurement_destroy(measurement);
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/rendered-text-width-measurement/longer-is-wider", test_longer_text_is_wider       },
    {"/rendered-text-width-measurement/markup",          test_markup_is_measured_as_shown},
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
