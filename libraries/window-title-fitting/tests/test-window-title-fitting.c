#include <glib.h>

#include "window-title-fitting.h"

static const struct compositor_window long_titled_window = {
    .container_id     = 7,
    .application_name = "foot",
    .title            = "a very long window title that keeps going and going",
    .workspace_name   = "1",
    .process_id       = 4123,
};

static void
test_character_limit_adds_ellipsis(void)
{
    const struct window_title_fitting fitting = {.format = "{title} ({pid})", .maximum_title_characters = 10};

    g_autofree char *text = window_title_fitting_expand(&fitting, &long_titled_window);

    g_assert_cmpstr(text, ==, "a very... (4123)");
}

static void
test_no_limit_keeps_the_title(void)
{
    const struct window_title_fitting fitting = {.format = "{title}"};

    g_autofree char *text = window_title_fitting_expand(&fitting, &long_titled_window);

    g_assert_cmpstr(text, ==, long_titled_window.title);
}

/* The process ID stays whole; the title gives way and ends in dots. */
static void
test_text_fits_the_space(void)
{
    struct rendered_text_width_measurement *measurement = rendered_text_width_measurement_create("monospace 10");
    int space = rendered_text_width_measurement_measure_markup(measurement, "a very long... (4123)");

    const struct window_title_fitting fitting = {
        .format           = "{title} <span alpha='60%'>({pid})</span>",
        .measurement      = measurement,
        .text_space_width = space
    };

    g_autofree char *text = window_title_fitting_expand(&fitting, &long_titled_window);

    g_assert_cmpstr(text, ==, "a very long... <span alpha='60%'>(4123)</span>");
    rendered_text_width_measurement_destroy(measurement);
}

static void
test_too_little_space_drops_the_title(void)
{
    struct rendered_text_width_measurement *measurement = rendered_text_width_measurement_create("monospace 10");

    const struct window_title_fitting fitting = {.format = "{title}({pid})", .measurement = measurement, .text_space_width = 1};

    g_autofree char *text = window_title_fitting_expand(&fitting, &long_titled_window);

    g_assert_cmpstr(text, ==, "(4123)");
    rendered_text_width_measurement_destroy(measurement);
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/window-title-fitting/character-limit", test_character_limit_adds_ellipsis   },
    {"/window-title-fitting/no-limit",        test_no_limit_keeps_the_title        },
    {"/window-title-fitting/fits-space",      test_text_fits_the_space             },
    {"/window-title-fitting/too-little",      test_too_little_space_drops_the_title},
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
