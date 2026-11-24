#include <glib.h>

#include "window-label-format.h"

static const struct window_label_values terminal = {"foot", 4123, "~/Programming", "2"};

static const struct {
    const char *format;
    const char *expected_label;
} expansion_cases[] = {
    {"{name} {pid}",         "foot 4123"     },
    {"{title}",              "~/Programming" },
    {"[{workspace}] {name}", "[2] foot"      },
    {"{name} {unknown}",     "foot {unknown}"},
    {"unclosed {name",       "unclosed {name"},
    {"plain text",           "plain text"    },
};

static void
assert_expansion(
    size_t case_index
)
{
    g_autofree char *label = window_label_format_expand(expansion_cases[case_index].format, &terminal);

    g_assert_cmpstr(label, ==, expansion_cases[case_index].expected_label);
}

static void
test_placeholders_expand(void)
{
    for (size_t i = 0; i < G_N_ELEMENTS(expansion_cases); i++) {
        assert_expansion(i);
    }
}

/* A window without a title shows an empty title, not the placeholder. */
static void
test_missing_values_expand_to_nothing(void)
{
    const struct window_label_values untitled = {"calc", 7, NULL, NULL};
    g_autofree char                 *label    = window_label_format_expand("{name}:{title}:{workspace}", &untitled);

    g_assert_cmpstr(label, ==, "calc::");
}

static void
test_title_use_is_detected(void)
{
    g_assert_true(window_label_format_uses_title("{name} - {title}"));
    g_assert_false(window_label_format_uses_title("{name} {pid}"));
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/window-label-format/placeholders",   test_placeholders_expand             },
    {"/window-label-format/missing-values", test_missing_values_expand_to_nothing},
    {"/window-label-format/uses-title",     test_title_use_is_detected           },
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
