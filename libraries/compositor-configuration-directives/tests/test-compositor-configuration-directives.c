#include <glib.h>

#include "compositor-configuration-directives.h"

static const char configuration_text[] = "font \"Old\" 9\n"
                                         "bar {\n"
                                         "    font \"Bar Font\" 10\n"
                                         "    colors { background #222222 }\n"
                                         "}\n"
                                         "# font commented 1 {\n"
                                         "fonts are not a directive\n"
                                         "  font   pango:Monaspace Neon 10  \n"
                                         "titlebar_padding 7 2\n";

static const struct {
    const char *directive_name;
    const char *expected_arguments;
} lookup_cases[] = {
    {"font",                      "pango:Monaspace Neon 10"},
    {"titlebar_padding",          "7 2"                    },
    {"titlebar_border_thickness", NULL                     },
    {"colors",                    NULL                     },
};

static void
assert_lookup(
    size_t case_index
)
{
    g_autofree char *arguments = compositor_configuration_find_directive(
        configuration_text,
        lookup_cases[case_index].directive_name
    );

    g_assert_cmpstr(arguments, ==, lookup_cases[case_index].expected_arguments);
}

static void
test_top_level_directives_are_found(void)
{
    for (size_t i = 0; i < G_N_ELEMENTS(lookup_cases); i++) {
        assert_lookup(i);
    }
}

/* A stray closing brace must not hide the rest of the file. */
static void
test_unbalanced_braces_are_tolerated(void)
{
    g_autofree char *arguments = compositor_configuration_find_directive("}\n}\nfont Sans 9\n", "font");

    g_assert_cmpstr(arguments, ==, "Sans 9");
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/compositor-configuration-directives/top-level",  test_top_level_directives_are_found },
    {"/compositor-configuration-directives/unbalanced", test_unbalanced_braces_are_tolerated},
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
