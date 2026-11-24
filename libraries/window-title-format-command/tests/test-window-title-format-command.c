#include <glib.h>

#include "window-title-format-command.h"

static const struct compositor_window terminal = {
    .container_id     = 7,
    .application_name = "foot",
    .title            = "~/Programming",
    .workspace_name   = "2",
    .process_id       = 4123,
};

static const struct {
    const char *format;
    const char *expected_command;
} command_cases[] = {
    {"%title ({pid})",                            "[con_id=7] title_format \"%title (4123)\""                         },
    {"{title} {name}",                            "[con_id=7] title_format \"%title foot\""                           },
    {"%title <span alpha=\"60%\">({pid})</span>", "[con_id=7] title_format '%title <span alpha=\"60%\">(4123)</span>'"},
    {"[{workspace}] %title",                      "[con_id=7] title_format \"[2] %title\""                            },
};

static void
assert_command(
    size_t case_index
)
{
    g_autofree char *command = window_title_format_command_build(command_cases[case_index].format, &terminal);

    g_assert_cmpstr(command, ==, command_cases[case_index].expected_command);
}

static void
test_commands_are_built(void)
{
    for (size_t i = 0; i < G_N_ELEMENTS(command_cases); i++) {
        assert_command(i);
    }
}

/* An application name can never close the quotes or inject markup. */
static void
test_window_values_are_escaped(void)
{
    struct compositor_window hostile_window = terminal;

    hostile_window.application_name = "a\"b'<i>&";

    g_autofree char *command = window_title_format_command_build("{name}", &hostile_window);

    g_assert_cmpstr(command, ==, "[con_id=7] title_format \"a&quot;b&apos;&lt;i&gt;&amp;\"");
}

static void
test_unquotable_formats_are_refused(void)
{
    g_assert_false(window_title_format_can_be_quoted("both \" and '"));
    g_assert_false(window_title_format_can_be_quoted("ends in \\"));
    g_assert_null(window_title_format_command_build("both \" and '", &terminal));
    g_assert_true(window_title_format_can_be_quoted(""));
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/window-title-format-command/commands",   test_commands_are_built            },
    {"/window-title-format-command/escaping",   test_window_values_are_escaped     },
    {"/window-title-format-command/unquotable", test_unquotable_formats_are_refused},
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
