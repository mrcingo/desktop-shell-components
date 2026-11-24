#include <glib.h>

#include "system-keyboard-layout.h"

static void
test_layout_becomes_one_input_command(void)
{
    struct system_keyboard_layout layout = {.layout = "es", .variant = "deadtilde", .options = "caps:escape"};

    g_autofree char *command = system_keyboard_layout_to_compositor_command(&layout);

    g_assert_cmpstr(command, ==, "input type:keyboard xkb_layout es xkb_variant deadtilde xkb_options caps:escape");
}

static void
test_unset_layout_gives_no_command(void)
{
    struct system_keyboard_layout layout = {0};

    g_assert_null(system_keyboard_layout_to_compositor_command(&layout));
}

/* A value that could end the command and start another is refused. */
static void
test_unsafe_values_give_no_command(void)
{
    struct system_keyboard_layout layout = {.layout = "us; exec rm -rf ~"};

    g_assert_null(system_keyboard_layout_to_compositor_command(&layout));
}

static const struct {
    const char *test_path;
    GTestFunc   test_function;
} registered_tests[] = {
    {"/system-keyboard-layout/command",       test_layout_becomes_one_input_command},
    {"/system-keyboard-layout/unset-layout",  test_unset_layout_gives_no_command   },
    {"/system-keyboard-layout/unsafe-values", test_unsafe_values_give_no_command   },
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
